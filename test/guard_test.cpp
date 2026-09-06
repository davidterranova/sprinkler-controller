//
//  The pure half of the over-run guard, plus the persisted-record wrapper.
//
#include "../components/irrigation_core/crc.h"
#include "../components/irrigation_core/guard.h"
#include "../components/irrigation_core/schedule.h"
#include "test_util.h"

using namespace irrigation_core;

// ---------------------------------------------------------------------------
//  Deadline arithmetic
// ---------------------------------------------------------------------------

TEST(a_deadline_expires_when_it_should) {
  EXPECT_FALSE(deadline_expired(1000, 0, 2000));
  EXPECT_TRUE(deadline_expired(2000, 0, 2000));
  EXPECT_TRUE(deadline_expired(5000, 0, 2000));
}

TEST(a_deadline_survives_the_millis_rollover) {
  // uint32_t milliseconds wrap every ~49.7 days. A controller that runs a whole
  // season without rebooting crosses that boundary mid-season, in August, with
  // everyone away -- so the naive `now >= start + max` is not an option.
  const uint32_t near_wrap = 0xFFFFF000u;
  const uint32_t max_ms = 60u * 1000;

  EXPECT_FALSE(deadline_expired(near_wrap + 1000, near_wrap, max_ms));
  // 0x1000 (4096) ms before the wrap plus 60 s after it.
  EXPECT_TRUE(deadline_expired(near_wrap + max_ms, near_wrap, max_ms));
  EXPECT_EQ(elapsed_ms(0x00000100u, near_wrap), 0x1100u);
}

// ---------------------------------------------------------------------------
//  Clamping
// ---------------------------------------------------------------------------

TEST(any_requested_duration_is_clamped_to_the_compile_time_ceiling) {
  // docs/07-firmware.md: the stock component's own limits allow a 240-hour run
  // to be *expressible* (multiplier 10 x run_duration 86400). Clamping in YAML
  // is necessary and not sufficient.
  DailyBudget budget;
  budget.roll_to(20000);
  EXPECT_EQ(clamp_run_seconds(240u * 3600, 0, budget), MAX_RUN_SECONDS);
  EXPECT_EQ(clamp_run_seconds(600, 0, budget), 600u);
}

TEST(the_daily_per_zone_budget_is_the_real_stop_condition) {
  // FR-3.7a. The run counter is not what makes a resume bug survivable -- this
  // is. A bug that restarts a zone forever still cannot deliver more than the
  // day's budget.
  DailyBudget budget;
  budget.seconds_per_zone = 3600;
  budget.roll_to(20000);

  EXPECT_FALSE(budget.would_exceed(0, 1800));
  budget.charge(0, 1800);
  EXPECT_FALSE(budget.would_exceed(0, 1800));
  budget.charge(0, 1800);
  EXPECT_TRUE(budget.would_exceed(0, 1));

  // Once exhausted, every further request clamps to zero -- which the caller
  // reads as "refuse", not as "run forever".
  EXPECT_EQ(clamp_run_seconds(600, 0, budget), 0u);
  // Other zones are unaffected.
  EXPECT_EQ(clamp_run_seconds(600, 1, budget), 600u);
}

TEST(the_budget_rolls_over_with_the_civil_day) {
  DailyBudget budget;
  budget.seconds_per_zone = 3600;
  budget.roll_to(20000);
  budget.charge(0, 3600);
  EXPECT_EQ(clamp_run_seconds(600, 0, budget), 0u);

  budget.roll_to(20001);
  EXPECT_EQ(clamp_run_seconds(600, 0, budget), 600u);
}

TEST(an_unconfigured_budget_does_not_block_anything) {
  DailyBudget budget;  // seconds_per_zone == 0
  budget.roll_to(20000);
  budget.charge(0, 10 * 3600);
  EXPECT_FALSE(budget.would_exceed(0, 3600));
  EXPECT_EQ(clamp_run_seconds(600, 0, budget), 600u);
}

// ---------------------------------------------------------------------------
//  The persisted record
// ---------------------------------------------------------------------------

TEST(a_sealed_record_validates_and_a_corrupted_one_does_not) {
  Record<Schedule> rec;
  rec.version = SCHEMA_VERSION;
  rec.sequence = 7;
  rec.payload = default_schedule(4);
  rec.seal();

  EXPECT_TRUE(rec.valid(SCHEMA_VERSION));

  // A single flipped bit anywhere in the payload must be caught. ESPHome's own
  // preference layer calls nvs_flash_erase() when NVS fails to open, so a
  // silently-blanked schedule is a real outcome -- the CRC is how we tell
  // "never written" from "written and damaged".
  rec.payload.programs[0].start_minute ^= 1;
  EXPECT_FALSE(rec.valid(SCHEMA_VERSION));
}

TEST(a_record_from_another_schema_version_is_refused_not_reinterpreted) {
  Record<Schedule> rec;
  rec.version = SCHEMA_VERSION;
  rec.payload = default_schedule(4);
  rec.seal();
  EXPECT_FALSE(rec.valid(SCHEMA_VERSION + 1));
}

TEST(an_all_zero_record_is_refused) {
  // "Never written" must not read as "written, and everything is off". The
  // magic number is what distinguishes them.
  Record<Schedule> rec;
  rec.magic = 0;
  rec.crc = 0;
  EXPECT_FALSE(rec.valid(SCHEMA_VERSION));
}

TEST(the_higher_sequence_number_wins_between_two_intact_banks) {
  Record<Schedule> bank_a, bank_b;
  bank_a.version = bank_b.version = SCHEMA_VERSION;
  bank_a.payload = bank_b.payload = default_schedule(4);
  bank_a.sequence = 41;
  bank_b.sequence = 42;
  bank_a.payload.seasonal_pct = 80;
  bank_b.payload.seasonal_pct = 120;
  bank_a.seal();
  bank_b.seal();

  const Record<Schedule> *winner = nullptr;
  if (bank_a.valid(SCHEMA_VERSION))
    winner = &bank_a;
  if (bank_b.valid(SCHEMA_VERSION) && (winner == nullptr || bank_b.sequence > winner->sequence))
    winner = &bank_b;
  EXPECT_EQ(winner->payload.seasonal_pct, 120);

  // Damage the newer bank: the older intact one must win, rather than the
  // firmware falling back to a blank schedule.
  bank_b.crc ^= 0xFFu;
  winner = nullptr;
  if (bank_a.valid(SCHEMA_VERSION))
    winner = &bank_a;
  if (bank_b.valid(SCHEMA_VERSION) && (winner == nullptr || bank_b.sequence > winner->sequence))
    winner = &bank_b;
  EXPECT_EQ(winner->payload.seasonal_pct, 80);
}

int main() { return ::testing::run_all("guard"); }
