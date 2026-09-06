//
//  The schedule resolver, against a table of the dates that break schedulers.
//
//  Every case here is one that a naive implementation gets wrong: the spring
//  hour that does not exist, the autumn hour that happens twice, the leap day,
//  the 31st followed by the 1st, and the year boundary.
//
#include "../components/irrigation_core/resolver.h"
#include "../components/irrigation_core/tz.h"
#include "test_util.h"

using namespace irrigation_core;

// Europe/Paris, as a POSIX TZ string: CET-1, CEST from the last Sunday in
// March at 02:00 to the last Sunday in October at 03:00.
static const char *PARIS = "CET-1CEST,M3.5.0,M10.5.0/3";

static Schedule two_zone_schedule() {
  Schedule s = default_schedule(4);
  Program &p = s.programs[0];
  p.enabled = true;
  p.start_minute = 5 * 60;  // 05:00
  p.days.weekday_mask = 0x7F;
  p.days.interval_days = 0;
  p.repeats = 1;
  p.zone_minutes[0] = 10;
  p.zone_minutes[1] = 20;
  return s;
}

static LocalTime at(int y, int mo, int d, int h, int mi) {
  return local_from_epoch(epoch_from_local(y, mo, d, h, mi, 0));
}

// ---------------------------------------------------------------------------
//  Duration arithmetic
// ---------------------------------------------------------------------------

TEST(effective_seconds_applies_seasonal_percentage) {
  EXPECT_EQ(effective_seconds(10, 100, 1), 600);
  EXPECT_EQ(effective_seconds(10, 50, 1), 300);
  EXPECT_EQ(effective_seconds(10, 200, 1), 1200);
}

TEST(effective_seconds_divides_by_cycle_and_soak_passes) {
  // FR-5.10: N passes of duration/N, not N passes of duration.
  EXPECT_EQ(effective_seconds(30, 100, 3), 600);
  EXPECT_EQ(effective_seconds(30, 100, 1), 1800);
}

TEST(effective_seconds_does_not_overflow_at_the_limits) {
  // 120 min * 200 % * 60 s = 14400 s, which overflows a uint16_t if the
  // multiply is done narrow. It must not.
  EXPECT_EQ(effective_seconds(MAX_ZONE_MINUTES, 200, 1), MAX_ZONE_MINUTES * 60);
}

TEST(effective_seconds_rounds_a_pointless_pass_up_to_thirty_seconds) {
  // 1 min at 10 % over 6 passes = 1 s. A 1 s pass barely lifts the diaphragm.
  EXPECT_EQ(effective_seconds(1, 10, 6), 30);
  // But zero stays zero -- "this zone is not in this program" must survive.
  EXPECT_EQ(effective_seconds(0, 100, 1), 0);
}

// ---------------------------------------------------------------------------
//  Validation
// ---------------------------------------------------------------------------

TEST(validation_rejects_a_start_time_inside_the_dst_window) {
  Schedule s = two_zone_schedule();
  s.programs[0].start_minute = 2 * 60 + 30;  // 02:30 -- does not exist in spring
  const ValidationResult r = validate(s);
  EXPECT_FALSE(r.ok());
  EXPECT_TRUE(r.error == ValidationError::DST_START_WINDOW);
}

TEST(validation_accepts_the_edges_just_outside_the_dst_window) {
  Schedule s = two_zone_schedule();
  s.programs[0].start_minute = 1 * 60 + 29;
  EXPECT_TRUE(validate(s).ok());
  s.programs[0].start_minute = 3 * 60 + 31;
  EXPECT_TRUE(validate(s).ok());
}

TEST(validation_rejects_an_over_long_duration_and_names_the_zone) {
  Schedule s = two_zone_schedule();
  s.programs[0].zone_minutes[1] = MAX_ZONE_MINUTES + 1;
  const ValidationResult r = validate(s);
  EXPECT_TRUE(r.error == ValidationError::DURATION_TOO_LONG);
  EXPECT_EQ(r.zone, 1);
}

TEST(validation_ignores_incoherent_disabled_programs) {
  // A disabled program is a draft. Refusing to commit the whole schedule
  // because of a program that cannot run would be needlessly obstructive.
  Schedule s = two_zone_schedule();
  s.programs[1].enabled = false;
  s.programs[1].start_minute = 2 * 60;  // inside the DST window
  EXPECT_TRUE(validate(s).ok());
}

TEST(validation_rejects_an_unknown_schema_version) {
  Schedule s = two_zone_schedule();
  s.schema_version = SCHEMA_VERSION + 1;
  EXPECT_TRUE(validate(s).error == ValidationError::BAD_SCHEMA);
}

TEST(default_schedule_boots_with_everything_disabled) {
  // FR-2.4: a device that has never been configured must boot with a valid
  // schedule -- and inventing a watering plan is not "valid".
  const Schedule s = default_schedule(4);
  EXPECT_TRUE(validate(s).ok());
  for (uint8_t p = 0; p < MAX_PROGRAMS; p++)
    EXPECT_FALSE(s.programs[p].enabled);
}

// ---------------------------------------------------------------------------
//  Due-ness and idempotency
// ---------------------------------------------------------------------------

TEST(a_program_is_due_at_its_start_minute) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 0), {60}, &plan) == DueResult::DUE);
  EXPECT_EQ(plan.count, 2);
  EXPECT_EQ(plan.entries[0].seconds, 600);
  EXPECT_EQ(plan.entries[1].seconds, 1200);
}

TEST(a_program_is_not_due_before_its_start_minute) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 4, 59), {60}, &plan) == DueResult::NOT_DUE);
}

TEST(resolving_is_idempotent_once_the_zones_are_marked_done) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;
  const LocalTime now = at(2026, 6, 10, 5, 0);

  EXPECT_TRUE(resolve(s, st, now, {60}, &plan) == DueResult::DUE);
  for (uint8_t i = 0; i < plan.count; i++)
    st.mark_ran(plan.program, plan.entries[i].zone, now.day_number);

  // Asked again a minute later, and an hour later, the answer is no.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 1), {60}, &plan) == DueResult::NOT_DUE);
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 59), {60}, &plan) == DueResult::NOT_DUE);
  // But tomorrow it is due again.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 11, 5, 0), {60}, &plan) == DueResult::DUE);
}

TEST(an_interrupted_cycle_resumes_only_the_zones_that_did_not_finish) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;
  const LocalTime now = at(2026, 6, 10, 5, 0);

  EXPECT_TRUE(resolve(s, st, now, {60}, &plan) == DueResult::DUE);
  st.mark_ran(plan.program, 0, now.day_number);  // zone 0 completed, then a reboot

  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 12), {60}, &plan) == DueResult::DUE_CATCH_UP);
  EXPECT_EQ(plan.count, 1);
  EXPECT_EQ(plan.entries[0].zone, 1);
}

TEST(catch_up_is_bounded_and_never_reaches_across_midnight) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;

  // Inside the window.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 6, 0), {60}, &plan) == DueResult::DUE_CATCH_UP);
  // One minute outside it: a device that was off all morning does not start
  // the 05:00 program at 06:01.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 6, 1), {60}, &plan) == DueResult::NOT_DUE);
  // And certainly not at 22:00.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 22, 0), {60}, &plan) == DueResult::NOT_DUE);
  // catch_up_minutes = 0 means exact starts only.
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 0), {0}, &plan) == DueResult::DUE);
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 5), {0}, &plan) == DueResult::NOT_DUE);
}

TEST(an_untrusted_clock_never_produces_a_scheduled_run) {
  // Fault-policy row 6. Before SNTP or the RTC lands, the clock reads 1970 --
  // which is a Thursday, and a naive weekday mask would happily fire on it.
  const Schedule s = two_zone_schedule();
  RunState st;
  st.reset();
  Plan plan;
  const LocalTime epoch_zero = local_from_epoch(5 * 3600);  // 1970-01-01, near enough
  EXPECT_FALSE(epoch_zero.valid);
  EXPECT_TRUE(resolve(s, st, epoch_zero, {60}, &plan) == DueResult::NOT_DUE);
}

// ---------------------------------------------------------------------------
//  Day predicates
// ---------------------------------------------------------------------------

TEST(weekday_mask_selects_days) {
  tz_set(PARIS);
  Schedule s = two_zone_schedule();
  s.programs[0].days.weekday_mask = (1u << 3);  // Wednesday only
  RunState st;
  st.reset();
  Plan plan;
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 10, 5, 0), {60}, &plan) == DueResult::DUE);       // Wed
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 11, 5, 0), {60}, &plan) == DueResult::NOT_DUE);   // Thu
}

TEST(every_n_days_is_measured_from_the_last_actual_watering) {
  // FR-5.2. If the interval were measured from the last *scheduled* day, a
  // skipped day (rain, hold, a fault) would silently shift the whole rhythm.
  tz_set(PARIS);
  Schedule s = two_zone_schedule();
  s.programs[0].days.interval_days = 3;
  RunState st;
  st.reset();
  Plan plan;

  const LocalTime d10 = at(2026, 6, 10, 5, 0);
  // Never watered: eligible immediately. A freshly commissioned controller
  // should not sit idle for three days for no stated reason.
  EXPECT_TRUE(resolve(s, st, d10, {60}, &plan) == DueResult::DUE);

  st.mark_actual(0, d10.day_number);
  st.mark_actual(1, d10.day_number);
  st.mark_ran(0, 0, d10.day_number);
  st.mark_ran(0, 1, d10.day_number);

  EXPECT_TRUE(resolve(s, st, at(2026, 6, 11, 5, 0), {60}, &plan) == DueResult::NOT_DUE);  // +1
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 12, 5, 0), {60}, &plan) == DueResult::NOT_DUE);  // +2
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 13, 5, 0), {60}, &plan) == DueResult::DUE);      // +3
}

TEST(a_manual_run_credits_the_interval_the_same_as_a_scheduled_one) {
  // D10 / FR-10.7, seen from the resolver's side: mark_actual is what the
  // interval reads, and a manual run writes it.
  tz_set(PARIS);
  Schedule s = two_zone_schedule();
  s.programs[0].days.interval_days = 2;
  RunState st;
  st.reset();
  Plan plan;

  const LocalTime d10 = at(2026, 6, 10, 14, 0);
  st.mark_actual(0, d10.day_number);
  st.mark_actual(1, d10.day_number);

  EXPECT_TRUE(resolve(s, st, at(2026, 6, 11, 5, 0), {60}, &plan) == DueResult::NOT_DUE);
  EXPECT_TRUE(resolve(s, st, at(2026, 6, 12, 5, 0), {60}, &plan) == DueResult::DUE);
}

// ---------------------------------------------------------------------------
//  Ordering
// ---------------------------------------------------------------------------

TEST(priority_orders_the_plan_and_rotation_never_overrides_it) {
  tz_set(PARIS);
  Schedule s = default_schedule(4);
  Program &p = s.programs[0];
  p.enabled = true;
  p.start_minute = 5 * 60;
  p.days.weekday_mask = 0x7F;
  for (uint8_t z = 0; z < 4; z++)
    p.zone_minutes[z] = 10;

  s.zone_priority[0] = 1;
  s.zone_priority[1] = 0;  // must always be first
  s.zone_priority[2] = 1;
  s.zone_priority[3] = 1;

  RunState st;
  st.reset();
  Plan plan;

  // Over four consecutive days the priority-0 zone stays first every time,
  // while the priority-1 group rotates through every starting position.
  bool zone0_seen_second = false, zone2_seen_second = false, zone3_seen_second = false;
  for (int d = 10; d < 14; d++) {
    EXPECT_TRUE(resolve(s, st, at(2026, 6, d, 5, 0), {60}, &plan) == DueResult::DUE);
    EXPECT_EQ(plan.count, 4);
    EXPECT_EQ(plan.entries[0].zone, 1);
    if (plan.entries[1].zone == 0)
      zone0_seen_second = true;
    if (plan.entries[1].zone == 2)
      zone2_seen_second = true;
    if (plan.entries[1].zone == 3)
      zone3_seen_second = true;
  }
  EXPECT_TRUE(zone0_seen_second);
  EXPECT_TRUE(zone2_seen_second);
  EXPECT_TRUE(zone3_seen_second);
}

// ---------------------------------------------------------------------------
//  The date table
// ---------------------------------------------------------------------------

struct DayCase {
  const char *label;
  int y, mo, d;
  int expected_weekday;
};

TEST(civil_day_numbers_advance_by_exactly_one_across_the_awkward_boundaries) {
  tz_set(PARIS);
  static const DayCase table[] = {
      {"spring forward (23 h day)", 2026, 3, 29, 0},
      {"day after spring forward", 2026, 3, 30, 1},
      {"fall back (25 h day)", 2026, 10, 25, 0},
      {"day after fall back", 2026, 10, 26, 1},
      {"leap day", 2028, 2, 29, 2},
      {"day after leap day", 2028, 3, 1, 3},
      {"28 Feb in a non-leap year", 2027, 2, 28, 0},
      {"1 Mar in a non-leap year", 2027, 3, 1, 1},
      {"31-day month boundary", 2026, 7, 31, 5},
      {"1 Aug", 2026, 8, 1, 6},
      {"new year's eve", 2026, 12, 31, 4},
      {"new year's day", 2027, 1, 1, 5},
  };

  for (const DayCase &c : table) {
    const LocalTime noon = at(c.y, c.mo, c.d, 12, 0);
    EXPECT_TRUE(noon.valid);
    EXPECT_EQ(noon.weekday, c.expected_weekday);
    // 23:59 the same evening and 00:00 the next morning must differ by one.
    const LocalTime late = at(c.y, c.mo, c.d, 23, 59);
    EXPECT_EQ(late.day_number, noon.day_number);
    const LocalTime next = local_from_epoch(epoch_from_local(c.y, c.mo, c.d, 12, 0, 0) + 24 * 3600);
    // On a 23 h or 25 h day, +24 h from noon lands at 11:00 or 13:00 the next
    // day -- still the next civil day, which is the property that matters.
    EXPECT_EQ(next.day_number, noon.day_number + 1);
  }
}

// Walks a whole civil day minute by minute and counts how many times the
// program fires. This is the test that a cron-edge design fails: on the autumn
// day 02:xx local happens twice, and an edge trigger fires twice with it.
static int fire_count_over_day(const Schedule &s, int y, int mo, int d, uint16_t catch_up) {
  RunState st;
  st.reset();
  Plan plan;
  int fires = 0;

  const Epoch start = epoch_from_local(y, mo, d, 0, 0, 0);
  // 26 h of wall-clock, so the 25-hour autumn day is fully covered and the
  // 23-hour spring day spills harmlessly into the next.
  const int32_t day_number = local_from_epoch(start).day_number;

  for (int minute = 0; minute < 26 * 60; minute++) {
    const LocalTime now = local_from_epoch(start + minute * 60);
    if (now.day_number != day_number)
      break;
    const DueResult r = resolve(s, st, now, {catch_up}, &plan);
    if (r == DueResult::NOT_DUE)
      continue;
    fires++;
    for (uint8_t i = 0; i < plan.count; i++) {
      st.mark_ran(plan.program, plan.entries[i].zone, now.day_number);
      st.mark_actual(plan.entries[i].zone, now.day_number);
    }
  }
  return fires;
}

TEST(a_program_fires_exactly_once_on_both_dst_transition_days) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();  // 05:00, every day

  EXPECT_EQ(fire_count_over_day(s, 2026, 3, 29, 60), 1);   // 23-hour day
  EXPECT_EQ(fire_count_over_day(s, 2026, 10, 25, 60), 1);  // 25-hour day
  EXPECT_EQ(fire_count_over_day(s, 2026, 6, 10, 60), 1);   // ordinary day, control
}

TEST(a_program_scheduled_just_outside_the_dst_band_still_fires_once) {
  tz_set(PARIS);
  Schedule s = two_zone_schedule();

  s.programs[0].start_minute = 1 * 60 + 29;  // 01:29, the earliest allowed
  EXPECT_EQ(fire_count_over_day(s, 2026, 3, 29, 60), 1);
  EXPECT_EQ(fire_count_over_day(s, 2026, 10, 25, 60), 1);

  s.programs[0].start_minute = 3 * 60 + 31;  // 03:31, the latest allowed
  EXPECT_EQ(fire_count_over_day(s, 2026, 3, 29, 60), 1);
  EXPECT_EQ(fire_count_over_day(s, 2026, 10, 25, 60), 1);
}

TEST(a_program_fires_once_on_a_leap_day_and_on_new_years_day) {
  tz_set(PARIS);
  const Schedule s = two_zone_schedule();
  EXPECT_EQ(fire_count_over_day(s, 2028, 2, 29, 60), 1);
  EXPECT_EQ(fire_count_over_day(s, 2027, 1, 1, 60), 1);
  EXPECT_EQ(fire_count_over_day(s, 2026, 12, 31, 60), 1);
}

TEST(two_programs_in_one_day_both_fire_once) {
  tz_set(PARIS);
  Schedule s = two_zone_schedule();  // 05:00
  Program &p2 = s.programs[1];
  p2 = s.programs[0];
  p2.start_minute = 20 * 60;  // 20:00
  EXPECT_TRUE(validate(s).ok());
  EXPECT_EQ(fire_count_over_day(s, 2026, 6, 10, 60), 2);
  EXPECT_EQ(fire_count_over_day(s, 2026, 10, 25, 60), 2);
}

int main() { return ::testing::run_all("resolver"); }
