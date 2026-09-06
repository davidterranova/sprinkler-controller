#pragma once
//
//  L3 -- the scheduler. Owns the persisted schedule, decides what is due,
//  drives the stock `sprinkler` component, and holds the Hold/Pause/Stop verbs.
//
//  It contains as little logic as possible: every decision worth testing lives
//  in ../irrigation_core/ and is exercised on the host. What is left here is
//  the glue that cannot be tested without a device -- persistence, entity
//  publishing, and talking to the sequencer.
//
//  ## Why there is a tick and not a cron trigger
//
//  ESPHome's `on_time` cron is edge-triggered, and docs/07-firmware.md lists
//  the three separate ways that is wrong for irrigation [V]: catch-up is
//  bounded at 900 s, `last_check_` lives in RAM so a reboot loses catch-up
//  entirely, and it does not reschedule around DST-skipped or duplicated
//  times. This component instead ticks once a minute and ASKS the resolver
//  whether anything is due, comparing persisted state against the model. That
//  is idempotent, survives an outage of any length, and cannot double-fire on
//  the autumn repeat.
//
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "esphome/components/sprinkler/sprinkler.h"
#include "esphome/components/time/real_time_clock.h"

#include "../irrigation_core/crc.h"
#include "../irrigation_core/fsm.h"
#include "../irrigation_core/guard.h"
#include "../irrigation_core/resolver.h"
#include "../irrigation_core/tz.h"
#include "../irrigation_guard/irrigation_guard.h"

#include <string>

namespace esphome::irrigation_scheduler {

// FR-7.2. The outcome enum is deliberately explicit: "it did not water" is
// never a satisfying answer, and each of these says something different about
// what to do next.
enum class Outcome : uint8_t {
  OK = 0,
  INTERRUPTED,
  ABORTED,
  SKIPPED_HELD,
  SKIPPED_WINTER,
  SKIPPED_STILL_RUNNING,
  SKIPPED_LOCKED_OUT,
  SKIPPED_CLOCK,
  SKIPPED_BUDGET,
  SATISFIED_BY_MANUAL,
};

const char *to_string(Outcome o);

// FR-7.2 / FR-7.5. Retained on-device in a ring buffer and replayed to HA on
// reconnect -- the interesting run is precisely the one that happened while
// Home Assistant was down.
struct RunRecord {
  irrigation_core::Epoch started{0};
  uint16_t planned_seconds{0};
  uint16_t actual_seconds{0};
  uint8_t zone{0};
  uint8_t program{0};
  irrigation_core::Source source{irrigation_core::Source::SCHEDULE};
  Outcome outcome{Outcome::OK};
};

inline constexpr uint8_t RECORD_RING_SIZE = 24;

// How much longer than the sequencer's own deadline the guard is armed for.
// The guard is a BACKSTOP: if it fires during a run that is merely finishing
// normally, the alarm is false and the next real one gets ignored. The margin
// has to cover the inter-zone `valve_open_delay` (which the sequencer counts
// inside the active-valve window) plus the master's lead/lag, with room to
// spare -- while staying far below the guard's own absolute ceiling.
inline constexpr uint16_t GUARD_MARGIN_SECONDS = 60;

// FR-3.7 / fault-policy row 4. Persisted the moment a zone opens and cleared
// the moment it closes, so a reboot mid-run leaves a breadcrumb: on the next
// boot the run is logged `interrupted` rather than vanishing. This is the
// minimum that makes "the ESP rebooted mid-run" a *recorded* behaviour instead
// of an inferred one. It is NOT a resume record -- the run is still abandoned.
struct ActiveRun {
  irrigation_core::Epoch started{0};
  uint16_t planned_seconds{0};
  uint8_t zone{0};
  uint8_t program{0};
  irrigation_core::Source source{irrigation_core::Source::SCHEDULE};
  bool open{false};
};

class IrrigationScheduler : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // --- configuration, from codegen -------------------------------------
  void set_sprinkler(sprinkler::Sprinkler *s) { this->sprinkler_ = s; }
  void set_guard(irrigation_guard::IrrigationGuard *g) { this->guard_ = g; }
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
  void set_zone_count(uint8_t n) { this->zone_count_ = n; }
  void set_catch_up_minutes(uint16_t m) { this->opts_.catch_up_minutes = m; }
  void set_daily_zone_budget(uint32_t seconds) { this->budget_.seconds_per_zone = seconds; }

  // --- staged configuration (FR-2.2: staged in RAM, committed by Apply) ---
  irrigation_core::Schedule &staged() { return this->staged_; }
  const irrigation_core::Schedule &active() const { return this->active_; }
  void mark_staged_dirty() { this->dirty_ = true; }
  bool config_pending() const { return this->dirty_; }  // FR-2.3
  bool apply();                                          // false => validation failed
  void revert();
  const char *last_validation_message() const { return this->validation_message_; }

  // Convenience setters so the YAML template entities stay one-liners.
  void stage_zone_enabled(uint8_t zone, bool on);
  void stage_zone_priority(uint8_t zone, uint8_t priority);
  void stage_seasonal_pct(uint16_t pct);
  void stage_program_enabled(uint8_t program, bool on);
  void stage_program_start(uint8_t program, uint16_t minute_of_day);
  void stage_program_weekday_mask(uint8_t program, uint8_t mask);
  void stage_program_interval(uint8_t program, uint8_t days);
  void stage_program_repeats(uint8_t program, uint8_t repeats);
  void stage_zone_minutes(uint8_t program, uint8_t zone, uint16_t minutes);

  // --- the three verbs, plus winter -------------------------------------
  void hold(uint32_t seconds, const std::string &by);
  void release_hold();
  void pause();
  void resume();
  void stop();
  void set_winter(bool on);

  // --- runs --------------------------------------------------------------
  bool manual_run(uint8_t zone, uint16_t minutes);  // FR-10.1: never "open zone N"
  void test_all_zones(uint16_t seconds_each);       // FR-10.8
  void acknowledge(const std::string &by);          // FR-9.3
  // Called from the guard's on_trip automation and from any other device-side
  // CRITICAL detector. Closes everything, latches, and inhibits the outputs.
  void latch_critical(const std::string &reason);

  // --- simulation (v0 bench) --------------------------------------------
  // Divides every run duration. Can only ever SHORTEN a run, so a value left
  // behind by accident under-waters -- visible, and not a P0 event.
  void set_simulation_speed(uint16_t speed);
  uint16_t simulation_speed() const { return this->sim_speed_; }

  // --- state, for the template entities ---------------------------------
  std::string state_text() const;
  std::string mode_text() const;
  std::string plan_text() const;        // FR-4.4, published BEFORE the cycle runs
  std::string next_run_text() const;
  std::string hold_text() const;
  std::string last_record_text() const;
  std::string alarm_text() const;
  bool clock_trusted() const;
  bool locked_out() const { return this->sup_.locked_out(); }
  bool running() const { return this->sup_.phase() == irrigation_core::Phase::RUNNING; }
  bool held() const { return this->sup_.mode() == irrigation_core::Mode::HOLD; }
  bool winter() const { return this->sup_.mode() == irrigation_core::Mode::WINTER; }
  int active_zone() const;
  uint32_t seconds_remaining() const;
  uint32_t zone_seconds_today(uint8_t zone) const;
  void log_records() const;

  irrigation_core::Supervisor &supervisor() { return this->sup_; }

 protected:
  void tick_(irrigation_core::Epoch now, const irrigation_core::LocalTime &local);
  void follow_sprinkler_(irrigation_core::Epoch now);
  bool start_cycle_(const irrigation_core::Plan &plan, irrigation_core::Source src, irrigation_core::Epoch now,
                    const irrigation_core::LocalTime &local);
  void complete_cycle_(const irrigation_core::LocalTime &local);
  void abandon_cycle_(Outcome outcome);
  void enter_lockout_(const char *reason);
  void save_active_run_();
  void clear_active_run_();
  void credit_manual_run_(uint8_t zone, uint32_t delivered_seconds, int32_t day);
  void record_(uint8_t zone, uint8_t program, irrigation_core::Source src, uint16_t planned, uint16_t actual,
               Outcome outcome, irrigation_core::Epoch started);
  uint32_t scaled_(uint32_t seconds) const;

  void load_();
  void save_schedule_();
  void save_state_();

  sprinkler::Sprinkler *sprinkler_{nullptr};
  irrigation_guard::IrrigationGuard *guard_{nullptr};
  time::RealTimeClock *time_{nullptr};

  uint8_t zone_count_{4};
  irrigation_core::ResolveOptions opts_{};
  irrigation_core::DailyBudget budget_{};

  irrigation_core::Schedule active_{};
  irrigation_core::Schedule staged_{};
  bool dirty_{false};
  const char *validation_message_{"ok"};
  uint32_t schedule_sequence_{0};

  irrigation_core::RunState run_state_{};
  irrigation_core::Supervisor sup_{};

  irrigation_core::Plan plan_{};
  bool cycle_active_{false};
  irrigation_core::Source source_{irrigation_core::Source::SCHEDULE};
  irrigation_core::Epoch cycle_started_{0};
  irrigation_core::Epoch zone_started_{0};
  uint16_t zone_planned_seconds_{0};
  int16_t last_active_valve_{-1};
  uint16_t sim_speed_{1};

  int32_t last_tick_minute_{-1};
  const char *alarm_{""};

  RunRecord records_[RECORD_RING_SIZE]{};
  uint8_t record_head_{0};
  uint8_t record_count_{0};

  ESPPreferenceObject pref_sched_a_;
  ESPPreferenceObject pref_sched_b_;
  ESPPreferenceObject pref_state_;
  ESPPreferenceObject pref_runstate_;
  ESPPreferenceObject pref_active_run_;

  ActiveRun active_run_{};
  std::string lockout_reason_{};
};

}  // namespace esphome::irrigation_scheduler
