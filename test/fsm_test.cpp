//
//  The supervisor: that Hold, Pause and Stop stay three different things, and
//  that every latch behaves the way docs/03-fault-policy.md says it does.
//
#include "../components/irrigation_core/fsm.h"
#include "test_util.h"

using namespace irrigation_core;

static constexpr Epoch T0 = 1780000000;  // an arbitrary but plausible 2026 instant

static Schedule four_zones() {
  Schedule s = default_schedule(4);
  return s;
}

static Supervisor fresh() {
  Supervisor sup;
  sup.note_boot(T0);
  return sup;
}

// ---------------------------------------------------------------------------
//  Defaults
// ---------------------------------------------------------------------------

TEST(a_device_with_nothing_persisted_boots_not_held) {
  // FR-8.6. Wrongly running costs some water and is loudly visible; wrongly
  // held kills a garden silently. That asymmetry decides the default.
  const Supervisor sup = fresh();
  EXPECT_TRUE(sup.mode() == Mode::NORMAL);
  EXPECT_TRUE(sup.phase() == Phase::IDLE);
  EXPECT_FALSE(sup.locked_out());
}

TEST(a_reboot_never_restores_a_running_state) {
  // FR-3.2: no valve state is ever restored from persistence.
  Supervisor sup = fresh();
  sup.started(Source::SCHEDULE, 0);
  EXPECT_TRUE(sup.phase() == Phase::RUNNING);

  SupervisorState persisted = sup.state();
  Supervisor after_reboot;
  after_reboot.restore(persisted);
  after_reboot.note_boot(T0 + 30);
  EXPECT_TRUE(after_reboot.phase() == Phase::IDLE);
}

// ---------------------------------------------------------------------------
//  Hold
// ---------------------------------------------------------------------------

TEST(hold_blocks_scheduled_runs_but_not_manual_ones) {
  // FR-8.5, and the reason the two verbs are separate at all.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.hold(T0, 24 * 3600, "mobile_app_dt_phone");

  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::HELD);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::NONE);
}

TEST(hold_is_attributed) {
  // FR-8.7 -- so the weekly digest can say who to ask.
  Supervisor sup = fresh();
  sup.hold(T0, 3600, "mobile_app_dt_phone");
  EXPECT_STREQ(sup.state().held_by, "mobile_app_dt_phone");
}

TEST(an_indefinite_hold_is_still_capped_at_seven_days) {
  // FR-8.4 / D9. There is deliberately no truly unbounded hold: the forgotten
  // hold is how gardens die, and it fails silently. Winter mode is the
  // unbounded option, and it is named for what it is.
  Supervisor sup = fresh();
  sup.hold(T0, 0, "someone");
  EXPECT_EQ(sup.hold_until(), T0 + static_cast<Epoch>(MAX_HOLD_SECONDS));

  sup.hold(T0, 30 * 24 * 3600, "someone");
  EXPECT_EQ(sup.hold_until(), T0 + static_cast<Epoch>(MAX_HOLD_SECONDS));
}

TEST(hold_warns_24h_before_expiry_then_auto_resumes) {
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.hold(T0, 48 * 3600, "someone");

  EXPECT_TRUE(sup.tick(T0 + 3600) == Event::NONE);
  EXPECT_TRUE(sup.tick(T0 + 24 * 3600) == Event::HOLD_EXPIRING_SOON);
  // Warned exactly once, not on every subsequent tick.
  EXPECT_TRUE(sup.tick(T0 + 24 * 3600 + 60) == Event::NONE);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::HELD);

  EXPECT_TRUE(sup.tick(T0 + 48 * 3600) == Event::HOLD_EXPIRED);
  EXPECT_TRUE(sup.mode() == Mode::NORMAL);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::NONE);
}

TEST(releasing_a_hold_early_works_and_clears_the_attribution) {
  Supervisor sup = fresh();
  sup.hold(T0, 48 * 3600, "someone");
  sup.release_hold(T0 + 60);
  EXPECT_TRUE(sup.mode() == Mode::NORMAL);
  EXPECT_STREQ(sup.state().held_by, "");
}

// ---------------------------------------------------------------------------
//  Pause and Stop
// ---------------------------------------------------------------------------

TEST(pause_only_applies_to_a_run_in_progress) {
  Supervisor sup = fresh();
  EXPECT_FALSE(sup.pause(T0));
  sup.started(Source::SCHEDULE, 0);
  EXPECT_TRUE(sup.pause(T0));
  EXPECT_TRUE(sup.phase() == Phase::PAUSED);
  EXPECT_TRUE(sup.resume(T0 + 60));
  EXPECT_TRUE(sup.phase() == Phase::RUNNING);
}

TEST(a_pause_that_outlives_its_timeout_becomes_a_stop_not_a_resume) {
  // Resuming would restart a valve nobody is watching. Only stopping is
  // consistent with P0.
  Supervisor sup = fresh();
  sup.started(Source::SCHEDULE, 0);
  sup.pause(T0);
  EXPECT_TRUE(sup.tick(T0 + MAX_PAUSE_SECONDS - 1) == Event::NONE);
  EXPECT_TRUE(sup.tick(T0 + MAX_PAUSE_SECONDS) == Event::PAUSE_TIMEOUT);
  EXPECT_TRUE(sup.phase() == Phase::IDLE);
}

TEST(stop_returns_to_idle_from_any_phase) {
  Supervisor sup = fresh();
  sup.started(Source::MANUAL, 2);
  sup.pause(T0);
  sup.stop();
  EXPECT_TRUE(sup.phase() == Phase::IDLE);
}

// ---------------------------------------------------------------------------
//  Collisions
// ---------------------------------------------------------------------------

TEST(a_start_that_collides_with_a_running_cycle_is_refused_not_queued) {
  // FR-4.6a. Queueing lets one slow cycle cascade into permanent back-to-back
  // watering, which is exactly the failure the rule exists to prevent.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.started(Source::SCHEDULE, 0);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 1, 10, true, s) == Refusal::ALREADY_RUNNING);
}

// ---------------------------------------------------------------------------
//  Clock trust
// ---------------------------------------------------------------------------

TEST(an_untrusted_clock_blocks_scheduled_runs_but_allows_manual_ones) {
  // Fault-policy row 6: a duration is a stopwatch, not a clock.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, false, s) == Refusal::CLOCK_UNTRUSTED);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, false, s) == Refusal::NONE);
}

// ---------------------------------------------------------------------------
//  Winter, latches and lockouts
// ---------------------------------------------------------------------------

TEST(winter_blocks_everything_including_manual) {
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.set_winter(true);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::WINTER);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::WINTER);
  sup.set_winter(false);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::NONE);
}

TEST(a_latched_critical_allows_exactly_one_diagnostic_run) {
  // FR-9.3. Locking out irrigation entirely would mean a repair could never be
  // tested without first clearing the alarm that the repair is meant to fix.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.latch_critical();

  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::LOCKED_OUT);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::NONE);
  EXPECT_TRUE(sup.take_diagnostic_allowance());

  // The allowance is spent; a second attempt is refused.
  EXPECT_FALSE(sup.take_diagnostic_allowance());
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::LOCKED_OUT);
}

TEST(latching_a_critical_aborts_the_run_in_progress) {
  // Every CRITICAL row in docs/03-fault-policy.md says "abort" or "force
  // closed" -- a latch that only refused the *next* run would leave the valve
  // that raised the alarm still open.
  Supervisor sup = fresh();
  sup.started(Source::SCHEDULE, 1);
  EXPECT_TRUE(sup.latch_critical());
  EXPECT_TRUE(sup.phase() == Phase::IDLE);

  // And with nothing running there is nothing to abort.
  Supervisor idle = fresh();
  EXPECT_FALSE(idle.latch_critical());
}

TEST(switching_to_winter_aborts_the_run_in_progress) {
  Supervisor sup = fresh();
  sup.started(Source::MANUAL, 0);
  EXPECT_TRUE(sup.set_winter(true));
  EXPECT_TRUE(sup.phase() == Phase::IDLE);
}

TEST(a_critical_latch_survives_a_reboot) {
  // Which is the whole point of latching: the fault outlives the power cycle
  // that would otherwise appear to have fixed it.
  Supervisor sup = fresh();
  sup.latch_critical();
  const SupervisorState persisted = sup.state();

  Supervisor after;
  after.restore(persisted);
  after.note_boot(T0 + 3600);
  EXPECT_TRUE(after.locked_out());
}

TEST(only_an_explicit_acknowledgement_clears_a_latch) {
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.latch_critical();
  sup.take_diagnostic_allowance();
  sup.acknowledge();
  EXPECT_FALSE(sup.locked_out());
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::NONE);
}

TEST(a_zone_lockout_is_per_zone_and_does_not_stop_the_others) {
  // Fault-policy rows 7, 9, 10 and 11 all lock out the offending zone only.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.lock_out_zone(2);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 2, 10, true, s) == Refusal::ZONE_LOCKED_OUT);
  EXPECT_TRUE(sup.can_start(Source::SCHEDULE, 1, 10, true, s) == Refusal::NONE);
}

TEST(zone_lockouts_are_independent_of_each_other) {
  // The scheduler filters a locked-out zone out of the plan rather than
  // cancelling the cycle; this is the property that makes that safe.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  sup.lock_out_zone(0);
  sup.lock_out_zone(3);
  EXPECT_TRUE(sup.zone_locked_out(0));
  EXPECT_FALSE(sup.zone_locked_out(1));
  EXPECT_FALSE(sup.zone_locked_out(2));
  EXPECT_TRUE(sup.zone_locked_out(3));

  // And they survive a reboot, like every other latch.
  const SupervisorState persisted = sup.state();
  Supervisor after;
  after.restore(persisted);
  after.note_boot(T0 + 60);
  EXPECT_TRUE(after.can_start(Source::SCHEDULE, 0, 10, true, s) == Refusal::ZONE_LOCKED_OUT);
  EXPECT_TRUE(after.can_start(Source::SCHEDULE, 1, 10, true, s) == Refusal::NONE);
}

TEST(a_disabled_or_nonexistent_zone_is_refused_by_name) {
  Supervisor sup = fresh();
  Schedule s = four_zones();
  s.zone_enabled[1] = false;
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 1, 10, true, s) == Refusal::ZONE_DISABLED);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 6, 10, true, s) == Refusal::INVALID_ZONE);
}

TEST(a_manual_run_longer_than_the_hard_cap_is_refused) {
  // FR-10.3. Enforced here as well as by the guard, because two independent
  // refusals is the point.
  Supervisor sup = fresh();
  const Schedule s = four_zones();
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, MAX_MANUAL_MINUTES, true, s) == Refusal::NONE);
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, MAX_MANUAL_MINUTES + 1, true, s) == Refusal::DURATION_TOO_LONG);
}

// ---------------------------------------------------------------------------
//  Crash-loop brake
// ---------------------------------------------------------------------------

TEST(three_reboots_inside_the_window_engage_the_crash_loop_brake) {
  // FR-3.8 / fault-policy row 14: all closed, no irrigation, advertise the
  // fault only.
  SupervisorState st;
  Supervisor sup;
  sup.restore(st);
  const Schedule s = four_zones();

  sup.note_boot(T0);
  EXPECT_FALSE(sup.locked_out());
  st = sup.state();

  sup.restore(st);
  sup.note_boot(T0 + 60);
  EXPECT_FALSE(sup.locked_out());
  st = sup.state();

  sup.restore(st);
  sup.note_boot(T0 + 120);
  EXPECT_TRUE(sup.locked_out());
  EXPECT_TRUE(sup.can_start(Source::MANUAL, 0, 10, true, s) == Refusal::CRASH_LOOP);
}

TEST(reboots_spread_out_in_time_do_not_engage_the_brake) {
  SupervisorState st;
  Supervisor sup;
  for (int i = 0; i < 10; i++) {
    sup.restore(st);
    sup.note_boot(T0 + i * static_cast<Epoch>(CRASH_LOOP_WINDOW_SECONDS + 60));
    st = sup.state();
  }
  EXPECT_FALSE(sup.locked_out());
}

TEST(acknowledging_clears_the_crash_loop_brake) {
  // Otherwise the only way out of a boot loop would be a reflash, which turns
  // a safety feature into a brick.
  SupervisorState st;
  Supervisor sup;
  for (int i = 0; i < 3; i++) {
    sup.restore(st);
    sup.note_boot(T0 + i * 30);
    st = sup.state();
  }
  EXPECT_TRUE(sup.locked_out());
  sup.acknowledge();
  EXPECT_FALSE(sup.locked_out());
}

int main() { return ::testing::run_all("fsm"); }
