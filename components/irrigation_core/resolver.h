#pragma once
//
//  "Is anything due right now, and if so what exactly should run?"
//
//  The single most important property of this file: it is a PURE FUNCTION of
//  (schedule, persisted run state, local time). It is never driven by a
//  schedule edge.
//
//  docs/07-firmware.md explains why at length, and it is worth restating: an
//  edge-triggered cron is wrong here in three separate ways. It does bounded
//  catch-up only within 900 s, it keeps its "last checked" marker in RAM so a
//  reboot loses catch-up entirely, and it double-fires on the autumn DST
//  repeat. Comparing persisted state against the model instead is idempotent,
//  survives an outage of any length, and cannot double-fire.
//
#include "schedule.h"
#include "types.h"

namespace irrigation_core {

// The persisted half of the idempotency key (zone, program, local_date).
struct RunState {
  // Local day number on which (program, zone) last completed. -1 = never.
  int32_t last_done_day[MAX_PROGRAMS][MAX_ZONES];
  // Local day number on which the zone last ACTUALLY watered, from any source
  // -- scheduled, catch-up or manual. FR-5.2: "every N days" is measured from
  // the last actual watering, not the last scheduled one, so a skipped day does
  // not silently shift the whole rhythm. -1 = never.
  int32_t last_actual_day[MAX_ZONES];

  void reset();
  // FR-10.7 / D10: a manual run credits the day per zone.
  void mark_ran(uint8_t program, uint8_t zone, int32_t day);
  void mark_actual(uint8_t zone, int32_t day);
};

struct PlanEntry {
  uint8_t zone{0};
  uint16_t seconds{0};  // per pass, seasonal adjustment and cycle-and-soak applied
};

struct Plan {
  uint8_t program{0};
  uint8_t repeats{1};
  uint8_t count{0};
  PlanEntry entries[MAX_ZONES]{};

  uint32_t total_seconds() const;
  bool empty() const { return this->count == 0; }
};

enum class DueResult : uint8_t {
  NOT_DUE = 0,
  DUE,           // the start minute is now (within tick granularity)
  DUE_CATCH_UP,  // the start minute has passed but is still inside the catch-up window
};

struct ResolveOptions {
  // How long after a missed start a program may still begin. Bounded on
  // purpose: a device that was off all day should NOT start the 05:00 program
  // at 22:00, and "catch up whatever you missed, whenever" is how a fortnight's
  // outage turns into a night of continuous watering.
  //
  // 0 disables catch-up entirely (exact-minute starts only).
  uint16_t catch_up_minutes{60};
};

// Fills `out` and returns DUE / DUE_CATCH_UP when a program should start now.
// Zones already completed today for that program are excluded, so calling this
// again after an interrupted cycle resumes the remainder without ever
// "resuming a run" -- which docs/07-firmware.md rules out.
DueResult resolve(const Schedule &s, const RunState &state, const LocalTime &now, const ResolveOptions &opts,
                  Plan *out);

// Exposed for the "next scheduled run" sensor and for tests. Returns the
// minute-of-day of the next enabled program that could fire strictly after
// `now`, or -1 if none fires again today.
int32_t next_start_today(const Schedule &s, const LocalTime &now);

// True if the program's day filters accept this civil day for this zone.
bool zone_day_matches(const Schedule &s, const RunState &state, uint8_t program, uint8_t zone, const LocalTime &now);

}  // namespace irrigation_core
