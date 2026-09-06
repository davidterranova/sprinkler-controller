#include "resolver.h"

namespace irrigation_core {

void RunState::reset() {
  for (uint8_t p = 0; p < MAX_PROGRAMS; p++)
    for (uint8_t z = 0; z < MAX_ZONES; z++)
      this->last_done_day[p][z] = -1;
  for (uint8_t z = 0; z < MAX_ZONES; z++)
    this->last_actual_day[z] = -1;
}

void RunState::mark_ran(uint8_t program, uint8_t zone, int32_t day) {
  if (program >= MAX_PROGRAMS || zone >= MAX_ZONES)
    return;
  this->last_done_day[program][zone] = day;
}

void RunState::mark_actual(uint8_t zone, int32_t day) {
  if (zone >= MAX_ZONES)
    return;
  this->last_actual_day[zone] = day;
}

uint32_t Plan::total_seconds() const {
  uint32_t total = 0;
  for (uint8_t i = 0; i < this->count; i++)
    total += this->entries[i].seconds;
  return total * (this->repeats < 1 ? 1 : this->repeats);
}

bool zone_day_matches(const Schedule &s, const RunState &state, uint8_t program, uint8_t zone,
                      const LocalTime &now) {
  if (program >= MAX_PROGRAMS || zone >= s.zone_count)
    return false;

  const Program &prog = s.programs[program];

  if (!prog.enabled || !s.zone_enabled[zone] || prog.zone_minutes[zone] == 0)
    return false;

  // Weekday mask.
  if ((prog.days.weekday_mask & (1u << now.weekday)) == 0)
    return false;

  // "Every N days", measured from the last ACTUAL watering (FR-5.2). A zone
  // that has never run is always eligible -- otherwise a freshly commissioned
  // controller would sit idle for N days for no stated reason.
  if (prog.days.interval_days > 0) {
    const int32_t last = state.last_actual_day[zone];
    if (last >= 0 && (now.day_number - last) < static_cast<int32_t>(prog.days.interval_days))
      return false;
  }

  return true;
}

// Orders the plan: priority ascending, and within one priority group the start
// point rotates with the civil day (FR-4.5). Rotating *within* a group rather
// than across the whole list means rotation can never override priority --
// which would be a surprising way for a low-priority zone to jump the queue.
static void order_entries(const Schedule &s, PlanEntry *entries, uint8_t count, int32_t day_number) {
  // Insertion sort by priority, then by zone number. n <= 8, and a stable
  // in-place sort with no allocation is exactly what this needs.
  for (uint8_t i = 1; i < count; i++) {
    PlanEntry key = entries[i];
    int j = static_cast<int>(i) - 1;
    while (j >= 0) {
      const uint8_t pj = s.zone_priority[entries[j].zone];
      const uint8_t pk = s.zone_priority[key.zone];
      if (pj > pk || (pj == pk && entries[j].zone > key.zone)) {
        entries[j + 1] = entries[j];
        j--;
      } else {
        break;
      }
    }
    entries[j + 1] = key;
  }

  // Rotate each equal-priority run by the day number.
  uint8_t start = 0;
  while (start < count) {
    uint8_t end = start + 1;
    while (end < count && s.zone_priority[entries[end].zone] == s.zone_priority[entries[start].zone])
      end++;

    const uint8_t len = end - start;
    if (len > 1) {
      // day_number can be negative in principle; normalise before the modulo so
      // the shift is always a forward rotation.
      int32_t shift = day_number % len;
      if (shift < 0)
        shift += len;
      if (shift != 0) {
        PlanEntry tmp[MAX_ZONES];
        for (uint8_t i = 0; i < len; i++)
          tmp[i] = entries[start + (i + static_cast<uint8_t>(shift)) % len];
        for (uint8_t i = 0; i < len; i++)
          entries[start + i] = tmp[i];
      }
    }
    start = end;
  }
}

DueResult resolve(const Schedule &s, const RunState &state, const LocalTime &now, const ResolveOptions &opts,
                  Plan *out) {
  if (out == nullptr)
    return DueResult::NOT_DUE;

  *out = Plan{};

  // Fault-policy row 6: no scheduled run while the clock is untrusted. The
  // caller enforces this too; belt and braces, because this is the function
  // that would otherwise happily decide 1970-01-01 is a Thursday.
  if (!now.valid)
    return DueResult::NOT_DUE;

  const uint16_t minute_now = now.minute_of_day();

  for (uint8_t p = 0; p < MAX_PROGRAMS; p++) {
    const Program &prog = s.programs[p];
    if (!prog.enabled)
      continue;

    // Only ever forward within the same civil day. Because the catch-up window
    // is bounded by "the start minute has already passed today", it can never
    // reach across midnight -- which removes an entire class of DST and
    // month-boundary bugs rather than trying to reason about them.
    if (minute_now < prog.start_minute)
      continue;

    const uint16_t elapsed = minute_now - prog.start_minute;
    if (elapsed > opts.catch_up_minutes)
      continue;

    PlanEntry entries[MAX_ZONES];
    uint8_t count = 0;
    for (uint8_t z = 0; z < s.zone_count; z++) {
      if (!zone_day_matches(s, state, p, z, now))
        continue;
      // The idempotency key: (zone, program, local_date). Already done today
      // for this program means not due -- however many times we are asked.
      if (state.last_done_day[p][z] == now.day_number)
        continue;

      const uint16_t seconds = effective_seconds(prog.zone_minutes[z], s.seasonal_pct, prog.repeats);
      if (seconds == 0)
        continue;

      entries[count].zone = z;
      entries[count].seconds = seconds;
      count++;
    }

    if (count == 0)
      continue;

    order_entries(s, entries, count, now.day_number);

    out->program = p;
    out->repeats = prog.repeats < 1 ? 1 : prog.repeats;
    out->count = count;
    for (uint8_t i = 0; i < count; i++)
      out->entries[i] = entries[i];

    // One minute of slack, because the tick that should land exactly on the
    // start minute may land a second or two late.
    return elapsed <= 1 ? DueResult::DUE : DueResult::DUE_CATCH_UP;
  }

  return DueResult::NOT_DUE;
}

int32_t next_start_today(const Schedule &s, const LocalTime &now) {
  if (!now.valid)
    return -1;

  const uint16_t minute_now = now.minute_of_day();
  int32_t best = -1;

  for (uint8_t p = 0; p < MAX_PROGRAMS; p++) {
    const Program &prog = s.programs[p];
    if (!prog.enabled)
      continue;
    if ((prog.days.weekday_mask & (1u << now.weekday)) == 0)
      continue;
    if (prog.start_minute <= minute_now)
      continue;
    if (best < 0 || prog.start_minute < best)
      best = prog.start_minute;
  }
  return best;
}

}  // namespace irrigation_core
