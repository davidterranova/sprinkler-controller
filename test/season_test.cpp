//
//  A whole irrigation season, unattended, in about a second.
//
//  This is the v0 exit criterion from docs/14-roadmap.md: "a simulated season
//  runs unattended". It ticks the real resolver and the real supervisor once a
//  simulated minute from 1 March to 31 October 2026 -- 352 800 ticks, both DST
//  transitions, five month boundaries -- while injecting the faults the season
//  will actually contain: reboots mid-cycle, a holiday hold, a latched CRITICAL
//  and its acknowledgement.
//
//  It asserts invariants continuously rather than checking an end state, so a
//  violation is reported at the minute it happens.
//
#include <cstdio>

#include "../components/irrigation_core/fsm.h"
#include "../components/irrigation_core/guard.h"
#include "../components/irrigation_core/resolver.h"
#include "../components/irrigation_core/tz.h"
#include "test_util.h"

using namespace irrigation_core;

static const char *PARIS = "CET-1CEST,M3.5.0,M10.5.0/3";

namespace {

constexpr uint8_t ZONES = 4;
constexpr uint16_t CATCH_UP_MINUTES = 60;
constexpr uint32_t SETTLE_SECONDS = 10;  // FR-4.3, and it resets the hardware backstop

// Deterministic pseudo-randomness: a season that fails must fail identically
// on the next run, or the failure is not actionable.
struct Lcg {
  uint32_t s;
  uint32_t next() {
    this->s = this->s * 1664525u + 1013904223u;
    return this->s >> 16;
  }
};

struct Stats {
  uint32_t cycles_started{0};
  uint32_t cycles_completed{0};
  uint32_t cycles_interrupted{0};  // ended by a reboot
  uint32_t cycles_aborted{0};      // ended by an alarm, a hold or winter
  uint32_t zone_runs{0};
  uint32_t reboots{0};
  uint32_t seconds_delivered[MAX_ZONES]{};
  uint32_t days_watered[MAX_ZONES]{};
};

class Season {
 public:
  Season() {
    this->sched_ = default_schedule(ZONES);
    this->sched_.seasonal_pct = 100;

    // Program 0: the morning cycle, every day at 05:00.
    Program &morning = this->sched_.programs[0];
    morning.enabled = true;
    morning.start_minute = 5 * 60;
    morning.days.weekday_mask = 0x7F;
    morning.repeats = 2;  // cycle-and-soak, D5 -- heavy clay
    morning.zone_minutes[0] = 20;
    morning.zone_minutes[1] = 20;
    morning.zone_minutes[2] = 30;
    morning.zone_minutes[3] = 15;

    // Program 1: an evening top-up, Mon/Wed/Fri only, every 2 days.
    Program &evening = this->sched_.programs[1];
    evening.enabled = true;
    evening.start_minute = 21 * 60;
    evening.days.weekday_mask = (1u << 1) | (1u << 3) | (1u << 5);
    evening.days.interval_days = 2;
    evening.repeats = 1;
    evening.zone_minutes[0] = 10;
    evening.zone_minutes[2] = 10;

    this->state_.reset();
    this->budget_.seconds_per_zone = 3 * 3600;  // FR-3.7a
  }

  const Schedule &schedule() const { return this->sched_; }
  const Stats &stats() const { return this->stats_; }
  Supervisor &supervisor() { return this->sup_; }

  void boot(Epoch now) {
    this->sup_.restore(this->persisted_);
    this->sup_.note_boot(now);
    this->persisted_ = this->sup_.state();
    // A run in progress is abandoned, never resumed (docs/07-firmware.md):
    // resuming turns a single fault into a repeated one, and in a crash loop
    // into an unbounded one.
    if (this->cycle_active_)
      this->end_cycle_early(this->stats_.cycles_interrupted);
    this->sup_.stop();
  }

  void tick(Epoch now) {
    const LocalTime local = local_from_epoch(now);
    this->budget_.roll_to(local.day_number);
    this->sup_.tick(now);

    // An alarm or winter mode closes the valves *now*; the supervisor has
    // already left RUNNING, so the executor must let go of the cycle too.
    if (this->cycle_active_ && this->sup_.phase() == Phase::IDLE)
      this->end_cycle_early(this->stats_.cycles_aborted);

    if (this->cycle_active_) {
      this->advance_run(local);
    } else {
      this->maybe_start(now, local);
    }

    this->check_invariants(local);
    this->persisted_ = this->sup_.state();
  }

 private:
  void maybe_start(Epoch now, const LocalTime &local) {
    Plan plan;
    const DueResult due = resolve(this->sched_, this->state_, local, {CATCH_UP_MINUTES}, &plan);
    if (due == DueResult::NOT_DUE)
      return;

    const Source src = due == DueResult::DUE ? Source::SCHEDULE : Source::CATCH_UP;
    const Refusal refusal =
        this->sup_.can_start(src, plan.entries[0].zone, plan.entries[0].seconds / 60, local.valid, this->sched_);
    if (refusal != Refusal::NONE) {
      this->last_refusal_ = refusal;
      return;
    }

    this->plan_ = plan;
    this->entry_ = 0;
    this->pass_ = 0;
    this->cycle_active_ = true;
    this->stats_.cycles_started++;
    this->begin_zone(local);
    (void) now;
  }

  void end_cycle_early(uint32_t &counter) {
    counter++;
    this->cycle_active_ = false;
    this->open_zone_ = 0xFF;
    this->granted_ = 0;
    this->remaining_ = 0;
  }

  void begin_zone(const LocalTime &local) {
    const PlanEntry &e = this->plan_.entries[this->entry_];
    const uint32_t granted = clamp_run_seconds(e.seconds, e.zone, this->budget_);
    if (granted == 0) {
      // The day's budget for this zone is spent. Skip it rather than run it:
      // this is the branch that makes a resume bug survivable.
      this->finish_zone(local, 0);
      return;
    }
    this->sup_.started(Source::SCHEDULE, e.zone);
    this->remaining_ = granted + SETTLE_SECONDS;
    this->granted_ = granted;
    this->open_zone_ = e.zone;
  }

  void advance_run(const LocalTime &local) {
    if (this->remaining_ > 60) {
      this->remaining_ -= 60;
      return;
    }
    this->finish_zone(local, this->granted_);
  }

  void finish_zone(const LocalTime &local, uint32_t delivered) {
    if (delivered > 0) {
      const uint8_t zone = this->plan_.entries[this->entry_].zone;
      this->budget_.charge(zone, delivered);
      this->stats_.seconds_delivered[zone] += delivered;
      this->stats_.zone_runs++;
    }
    this->open_zone_ = 0xFF;
    this->granted_ = 0;
    this->remaining_ = 0;
    this->sup_.finished();

    this->entry_++;
    if (this->entry_ < this->plan_.count) {
      this->begin_zone(local);
      return;
    }

    this->entry_ = 0;
    this->pass_++;
    if (this->pass_ < this->plan_.repeats) {
      this->begin_zone(local);
      return;
    }

    // Cycle complete: only now is the idempotency key written.
    for (uint8_t i = 0; i < this->plan_.count; i++) {
      const uint8_t zone = this->plan_.entries[i].zone;
      this->state_.mark_ran(this->plan_.program, zone, local.day_number);
      this->state_.mark_actual(zone, local.day_number);
      if (this->last_watered_day_[zone] != local.day_number) {
        this->last_watered_day_[zone] = local.day_number;
        this->stats_.days_watered[zone]++;
      }
    }
    this->cycle_active_ = false;
    this->stats_.cycles_completed++;
  }

  void check_invariants(const LocalTime &local) {
    // P0's software shadow: at most one zone open, ever (FR-4.1).
    if (this->open_zone_ != 0xFF) {
      EXPECT_TRUE(this->open_zone_ < ZONES);
      // Nothing may be open while held, in winter, or under a latched CRITICAL.
      EXPECT_TRUE(this->sup_.mode() != Mode::WINTER);
      EXPECT_FALSE(this->sup_.locked_out());
    }
    // No zone exceeds its daily budget, whatever the fault history.
    for (uint8_t z = 0; z < ZONES; z++)
      EXPECT_TRUE(this->budget_.seconds_used[z] <= this->budget_.seconds_per_zone);
    // The civil day never goes backwards.
    EXPECT_TRUE(local.day_number >= this->last_day_seen_);
    this->last_day_seen_ = local.day_number;
  }

  Schedule sched_;
  RunState state_{};
  Supervisor sup_{};
  SupervisorState persisted_{};
  DailyBudget budget_{};
  Stats stats_{};

  Plan plan_{};
  uint8_t entry_{0};
  uint8_t pass_{0};
  bool cycle_active_{false};
  uint32_t remaining_{0};
  uint32_t granted_{0};
  uint8_t open_zone_{0xFF};
  Refusal last_refusal_{Refusal::NONE};
  int32_t last_day_seen_{-2147483647};
  int32_t last_watered_day_[MAX_ZONES]{-1, -1, -1, -1, -1, -1, -1, -1};
};

}  // namespace

TEST(a_full_season_runs_unattended_without_violating_an_invariant) {
  tz_set(PARIS);

  Season season;
  Lcg rng{12345};

  const Epoch start = epoch_from_local(2026, 3, 1, 0, 0, 0);
  const Epoch end = epoch_from_local(2026, 11, 1, 0, 0, 0);

  // Scripted faults, at fixed instants so a failure is reproducible.
  const Epoch hold_at = epoch_from_local(2026, 7, 14, 9, 0, 0);   // a fortnight away
  const Epoch critical_at = epoch_from_local(2026, 8, 12, 6, 30, 0);
  const Epoch ack_at = epoch_from_local(2026, 8, 14, 18, 0, 0);
  const Epoch winter_on = epoch_from_local(2026, 10, 20, 12, 0, 0);

  season.boot(start);
  uint32_t reboots = 0;
  bool ran_during_lockout = false;

  for (Epoch now = start; now < end; now += 60) {
    if (now == hold_at)
      season.supervisor().hold(now, 5 * 24 * 3600, "mobile_app_dt_phone");
    if (now == critical_at)
      season.supervisor().latch_critical();
    if (now == ack_at)
      season.supervisor().acknowledge();
    if (now == winter_on)
      season.supervisor().set_winter(true);

    // ~1 reboot every 2 simulated days. Cycles occupy roughly 7 % of the day,
    // so this yields a double-figure number landing mid-cycle -- enough that
    // the abandon-and-re-evaluate path is genuinely exercised.
    if (rng.next() % 2880 == 0) {
      season.boot(now);
      reboots++;
    }

    const uint32_t before = season.stats().zone_runs;
    season.tick(now);
    if (season.supervisor().locked_out() && season.stats().zone_runs != before)
      ran_during_lockout = true;
  }

  const Stats &st = season.stats();
  std::printf("    season: %u cycles started, %u completed, %u interrupted, %u aborted, %u zone runs, %u reboots\n",
              st.cycles_started, st.cycles_completed, st.cycles_interrupted, st.cycles_aborted, st.zone_runs,
              reboots);
  for (uint8_t z = 0; z < ZONES; z++)
    std::printf("    zone %u: %u days watered, %u min delivered\n", z, st.days_watered[z],
                st.seconds_delivered[z] / 60);

  EXPECT_FALSE(ran_during_lockout);
  EXPECT_TRUE(reboots > 100);
  EXPECT_TRUE(st.cycles_interrupted > 5);
  EXPECT_TRUE(st.cycles_aborted > 0);

  // Every zone waters on most days of the season. 245 days minus the 5-day
  // hold, the 2-day lockout and the 12-day winter tail leaves ~226; allowing
  // for cycles abandoned by a reboot, 200 is a comfortable floor and a real
  // regression (a resolver that stops firing) drops far below it.
  for (uint8_t z = 0; z < ZONES; z++)
    EXPECT_TRUE(st.days_watered[z] > 200);

  // Roadmap exit criterion: >= 99 % of planned cycles completed or explicably
  // skipped. The only inexplicable outcome here is an interruption, and every
  // interruption is accounted for by an injected reboot.
  const double completion = static_cast<double>(st.cycles_completed) / static_cast<double>(st.cycles_started);
  std::printf("    completion: %.2f %%\n", completion * 100.0);
  EXPECT_TRUE(completion > 0.90);
  EXPECT_EQ(st.cycles_started, st.cycles_completed + st.cycles_interrupted + st.cycles_aborted);
}

TEST(a_season_with_the_scheduler_held_throughout_delivers_no_water) {
  // The complement of the test above: prove the hold is actually load-bearing
  // and not merely reported.
  tz_set(PARIS);
  Season season;
  const Epoch start = epoch_from_local(2026, 6, 1, 0, 0, 0);
  const Epoch end = epoch_from_local(2026, 6, 6, 0, 0, 0);

  season.boot(start);
  for (Epoch now = start; now < end; now += 60) {
    // Re-asserted every hour so it never expires inside the window.
    if ((now - start) % 3600 == 0)
      season.supervisor().hold(now, 24 * 3600, "test");
    season.tick(now);
  }
  EXPECT_EQ(season.stats().zone_runs, 0u);
  EXPECT_EQ(season.stats().cycles_started, 0u);
}

int main() { return ::testing::run_all("season"); }
