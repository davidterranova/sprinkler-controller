#include "irrigation_scheduler.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>

namespace esphome::irrigation_scheduler {

static const char *const TAG = "irrigation_scheduler";

using irrigation_core::Epoch;
using irrigation_core::LocalTime;
using irrigation_core::Mode;
using irrigation_core::Phase;
using irrigation_core::Plan;
using irrigation_core::Refusal;
using irrigation_core::Source;

const char *to_string(Outcome o) {
  switch (o) {
    case Outcome::OK:
      return "ok";
    case Outcome::INTERRUPTED:
      return "interrupted";
    case Outcome::ABORTED:
      return "aborted";
    case Outcome::SKIPPED_HELD:
      return "skipped_held";
    case Outcome::SKIPPED_WINTER:
      return "skipped_winter";
    case Outcome::SKIPPED_STILL_RUNNING:
      return "skipped_still_running";
    case Outcome::SKIPPED_LOCKED_OUT:
      return "skipped_locked_out";
    case Outcome::SKIPPED_CLOCK:
      return "skipped_clock";
    case Outcome::SKIPPED_BUDGET:
      return "skipped_budget";
    case Outcome::SATISFIED_BY_MANUAL:
      return "satisfied_by_manual";
  }
  return "?";
}

// ---------------------------------------------------------------------------
//  Persistence
//
//  Own the record rather than trusting the preference layer alone: ESPHome
//  calls nvs_flash_erase() when NVS fails to open, so a corrupt NVS silently
//  wipes every preference [V]. The magic + version + CRC is what lets us tell
//  "never written" from "written and damaged", and the two banks are what let
//  us fall back to the previous good schedule instead of to a blank one.
// ---------------------------------------------------------------------------

void IrrigationScheduler::load_() {
  this->pref_sched_a_ = global_preferences->make_preference<irrigation_core::Record<irrigation_core::Schedule>>(
      fnv1_hash("irrigation_schedule_a"));
  this->pref_sched_b_ = global_preferences->make_preference<irrigation_core::Record<irrigation_core::Schedule>>(
      fnv1_hash("irrigation_schedule_b"));
  this->pref_state_ = global_preferences->make_preference<irrigation_core::Record<irrigation_core::SupervisorState>>(
      fnv1_hash("irrigation_supervisor"));
  this->pref_runstate_ = global_preferences->make_preference<irrigation_core::Record<irrigation_core::RunState>>(
      fnv1_hash("irrigation_runstate"));
  this->pref_active_run_ = global_preferences->make_preference<irrigation_core::Record<ActiveRun>>(
      fnv1_hash("irrigation_active_run"));

  irrigation_core::Record<irrigation_core::Schedule> a{}, b{};
  const bool a_ok = this->pref_sched_a_.load(&a) && a.valid(irrigation_core::SCHEMA_VERSION);
  const bool b_ok = this->pref_sched_b_.load(&b) && b.valid(irrigation_core::SCHEMA_VERSION);

  const irrigation_core::Record<irrigation_core::Schedule> *winner = nullptr;
  if (a_ok)
    winner = &a;
  if (b_ok && (winner == nullptr || b.sequence > winner->sequence))
    winner = &b;

  if (winner != nullptr) {
    this->active_ = winner->payload;
    this->schedule_sequence_ = winner->sequence;
    ESP_LOGI(TAG, "schedule restored from bank %c, sequence %" PRIu32, winner == &a ? 'A' : 'B',
             winner->sequence);
  } else {
    // FR-2.4: boot with a VALID schedule even if HA has never connected. A
    // device that has never been configured must not invent a watering plan,
    // so the default has every program disabled.
    this->active_ = irrigation_core::default_schedule(this->zone_count_);
    this->schedule_sequence_ = 0;
    if (a_ok || b_ok)
      ESP_LOGE(TAG, "both schedule banks failed validation -- falling back to defaults (all programs off)");
    else
      ESP_LOGI(TAG, "no schedule stored yet -- defaults, all programs off");
  }
  this->staged_ = this->active_;

  irrigation_core::Record<irrigation_core::SupervisorState> st{};
  if (this->pref_state_.load(&st) && st.valid(irrigation_core::SCHEMA_VERSION)) {
    this->sup_.restore(st.payload);
  }
  // Whatever happened, hold defaults to OFF when nothing valid is stored
  // (FR-8.6): wrongly running costs water and is loudly visible, wrongly held
  // kills a garden silently.

  irrigation_core::Record<irrigation_core::RunState> rs{};
  if (this->pref_runstate_.load(&rs) && rs.valid(irrigation_core::SCHEMA_VERSION)) {
    this->run_state_ = rs.payload;
  } else {
    this->run_state_.reset();
  }

  // Fault-policy row 4: a run that was open when the power went is logged
  // `interrupted` and then abandoned. Never resumed -- resuming turns a single
  // fault into a repeated one, and in a crash loop into an unbounded one.
  irrigation_core::Record<ActiveRun> ar{};
  if (this->pref_active_run_.load(&ar) && ar.valid(irrigation_core::SCHEMA_VERSION) && ar.payload.open) {
    ESP_LOGW(TAG, "zone %u was open at the last reset -- logging the run as interrupted, not resuming it",
             ar.payload.zone);
    this->record_(ar.payload.zone, ar.payload.program, ar.payload.source, ar.payload.planned_seconds, 0,
                  Outcome::INTERRUPTED, ar.payload.started);
    this->clear_active_run_();
  }
}

void IrrigationScheduler::save_active_run_() {
  irrigation_core::Record<ActiveRun> rec;
  rec.version = irrigation_core::SCHEMA_VERSION;
  rec.payload = this->active_run_;
  rec.seal();
  this->pref_active_run_.save(&rec);
}

void IrrigationScheduler::clear_active_run_() {
  this->active_run_ = ActiveRun{};
  this->save_active_run_();
}

void IrrigationScheduler::save_schedule_() {
  this->schedule_sequence_++;
  irrigation_core::Record<irrigation_core::Schedule> rec;
  rec.version = irrigation_core::SCHEMA_VERSION;
  rec.sequence = this->schedule_sequence_;
  rec.payload = this->active_;
  rec.seal();
  // Alternate banks so a power cut during the write always leaves one intact
  // record behind.
  if (this->schedule_sequence_ % 2 == 1)
    this->pref_sched_a_.save(&rec);
  else
    this->pref_sched_b_.save(&rec);
}

void IrrigationScheduler::save_state_() {
  irrigation_core::Record<irrigation_core::SupervisorState> st;
  st.version = irrigation_core::SCHEMA_VERSION;
  st.payload = this->sup_.state();
  st.seal();
  this->pref_state_.save(&st);

  irrigation_core::Record<irrigation_core::RunState> rs;
  rs.version = irrigation_core::SCHEMA_VERSION;
  rs.payload = this->run_state_;
  rs.seal();
  this->pref_runstate_.save(&rs);
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

void IrrigationScheduler::setup() {
  this->load_();

  const Epoch now = this->time_ != nullptr ? static_cast<Epoch>(this->time_->timestamp_now()) : 0;
  this->sup_.note_boot(now);
  if (this->sup_.state().crash_loop) {
    // Fault-policy row 14: all closed, no irrigation, advertise the fault only.
    this->enter_lockout_("crash-loop brake engaged");
  }
  this->save_state_();

  // Belt and braces on top of the guard's own boot sweep: whatever the
  // sequencer's cached switch states say, close everything.
  if (this->sprinkler_ != nullptr)
    this->sprinkler_->shutdown(true);
  if (this->guard_ != nullptr)
    this->guard_->hard_close_all();
}

void IrrigationScheduler::loop() {
  if (this->time_ == nullptr || this->sprinkler_ == nullptr)
    return;

  const Epoch now = static_cast<Epoch>(this->time_->timestamp_now());
  const LocalTime local = irrigation_core::local_from_epoch(now);

  this->follow_sprinkler_(now);

  // Exactly one evaluation per civil minute. Cheap, idempotent, and it means a
  // slow loop cannot make the scheduler fire twice.
  const int32_t minute = local.valid ? local.day_number * 1440 + local.minute_of_day() : -1;
  if (minute != this->last_tick_minute_) {
    this->last_tick_minute_ = minute;
    this->tick_(now, local);
  }
}

// ---------------------------------------------------------------------------
//  The tick
// ---------------------------------------------------------------------------

void IrrigationScheduler::tick_(Epoch now, const LocalTime &local) {
  if (local.valid)
    this->budget_.roll_to(local.day_number);

  switch (this->sup_.tick(now)) {
    case irrigation_core::Event::HOLD_EXPIRING_SOON:
      ESP_LOGW(TAG, "hold expires in 24 h (held by %s)", this->sup_.state().held_by);
      this->alarm_ = "hold expires in 24 h";
      this->save_state_();
      break;
    case irrigation_core::Event::HOLD_EXPIRED:
      ESP_LOGI(TAG, "hold expired -- resuming the schedule");
      this->alarm_ = "";
      this->save_state_();
      break;
    case irrigation_core::Event::PAUSE_TIMEOUT:
      ESP_LOGW(TAG, "pause exceeded its maximum -- stopping instead of resuming");
      this->stop();
      break;
    case irrigation_core::Event::NONE:
      break;
  }

  if (this->cycle_active_ || !local.valid)
    return;

  Plan plan;
  const irrigation_core::DueResult due = irrigation_core::resolve(this->active_, this->run_state_, local, this->opts_, &plan);
  if (due == irrigation_core::DueResult::NOT_DUE)
    return;

  const Source src = due == irrigation_core::DueResult::DUE ? Source::SCHEDULE : Source::CATCH_UP;

  // Drop zones that are individually locked out (fault-policy rows 7, 9, 10,
  // 11 all lock out the offending zone, not the system). The resolver has
  // already excluded disabled zones, so this is the only per-zone refusal left
  // -- and it has to happen HERE rather than in can_start(), which reasons
  // about one zone: asking it about entries[0] alone would let one locked-out
  // zone silently cancel the whole cycle.
  {
    Plan filtered;
    filtered.program = plan.program;
    filtered.repeats = plan.repeats;
    for (uint8_t i = 0; i < plan.count; i++) {
      const uint8_t zone = plan.entries[i].zone;
      if (this->sup_.zone_locked_out(zone)) {
        this->record_(zone, plan.program, src, plan.entries[i].seconds, 0, Outcome::SKIPPED_LOCKED_OUT, now);
        this->run_state_.mark_ran(plan.program, zone, local.day_number);
        continue;
      }
      filtered.entries[filtered.count++] = plan.entries[i];
    }
    if (filtered.count == 0) {
      this->save_state_();
      return;
    }
    plan = filtered;
  }

  const uint16_t first_minutes = static_cast<uint16_t>(plan.entries[0].seconds / 60);
  const Refusal refusal = this->sup_.can_start(src, plan.entries[0].zone, first_minutes, local.valid, this->active_);

  if (refusal != Refusal::NONE) {
    // FR-7.2: a skip is an outcome with a name, not silence. Recorded once per
    // due evaluation so the gap in the history is explicable.
    Outcome outcome;
    switch (refusal) {
      case Refusal::HELD:
        outcome = Outcome::SKIPPED_HELD;
        break;
      case Refusal::WINTER:
        outcome = Outcome::SKIPPED_WINTER;
        break;
      case Refusal::ALREADY_RUNNING:
        outcome = Outcome::SKIPPED_STILL_RUNNING;
        break;
      case Refusal::CLOCK_UNTRUSTED:
        outcome = Outcome::SKIPPED_CLOCK;
        break;
      default:
        outcome = Outcome::SKIPPED_LOCKED_OUT;
        break;
    }
    ESP_LOGI(TAG, "program %u due but not started: %s", plan.program, irrigation_core::to_string(refusal));
    this->record_(plan.entries[0].zone, plan.program, src, plan.entries[0].seconds, 0, outcome, now);
    // Mark the whole program done for the day so the skip is recorded once and
    // the log does not fill with one line a minute for the catch-up window.
    for (uint8_t i = 0; i < plan.count; i++)
      this->run_state_.mark_ran(plan.program, plan.entries[i].zone, local.day_number);
    this->save_state_();
    return;
  }

  this->start_cycle_(plan, src, now, local);
}

// ---------------------------------------------------------------------------
//  Driving the stock sequencer
// ---------------------------------------------------------------------------

uint32_t IrrigationScheduler::scaled_(uint32_t seconds) const {
  if (this->sim_speed_ <= 1)
    return seconds;
  const uint32_t scaled = seconds / this->sim_speed_;
  return scaled < 1 ? 1 : scaled;
}

bool IrrigationScheduler::start_cycle_(const Plan &plan, Source src, Epoch now, const LocalTime &local) {
  this->plan_ = plan;
  this->source_ = src;
  this->cycle_started_ = now;

  // FR-4.4: the resolved plan is published before the cycle executes, so
  // "scheduled vs actual" has something to compare against.
  ESP_LOGI(TAG, "starting %s: %s", irrigation_core::to_string(src), this->plan_text().c_str());

  // Only the planned zones take part in this cycle. `start_full_cycle()`
  // iterates the ENABLED valves, so the enable switches are how the plan is
  // communicated to the stock component.
  for (uint8_t z = 0; z < this->zone_count_; z++) {
    auto *en = this->sprinkler_->enable_switch(z);
    if (en == nullptr)
      continue;
    bool in_plan = false;
    for (uint8_t i = 0; i < plan.count; i++)
      if (plan.entries[i].zone == z)
        in_plan = true;
    if (in_plan)
      en->turn_on();
    else
      en->turn_off();
  }

  for (uint8_t i = 0; i < plan.count; i++) {
    const uint8_t zone = plan.entries[i].zone;
    const uint32_t granted = irrigation_core::clamp_run_seconds(plan.entries[i].seconds, zone, this->budget_);
    if (granted == 0) {
      ESP_LOGW(TAG, "zone %u has spent its daily budget -- excluded from this cycle", zone);
      this->record_(zone, plan.program, src, plan.entries[i].seconds, 0, Outcome::SKIPPED_BUDGET, now);
      auto *en = this->sprinkler_->enable_switch(zone);
      if (en != nullptr)
        en->turn_off();
      continue;
    }
    this->sprinkler_->set_valve_run_duration(zone, this->scaled_(granted));
  }

  // The seasonal percentage and the cycle-and-soak division are already baked
  // into the plan's per-pass durations by irrigation_core, where they are unit
  // tested. The multiplier therefore stays at exactly 1: two places applying
  // the same adjustment is how a 200 % season becomes a 400 % one.
  this->sprinkler_->set_multiplier(1.0f);
  this->sprinkler_->set_repeat(plan.repeats);
  this->sprinkler_->set_auto_advance(true);
  this->sprinkler_->start_full_cycle();

  this->cycle_active_ = true;
  this->sup_.started(src, plan.entries[0].zone);
  (void) local;
  return true;
}

void IrrigationScheduler::follow_sprinkler_(Epoch now) {
  const auto active = this->sprinkler_->active_valve();
  const int16_t current = active.has_value() ? static_cast<int16_t>(*active) : -1;

  if (current == this->last_active_valve_)
    return;

  // A zone just closed: record what it actually delivered.
  if (this->last_active_valve_ >= 0) {
    const uint8_t zone = static_cast<uint8_t>(this->last_active_valve_);
    const uint32_t actual = now > this->zone_started_ ? static_cast<uint32_t>(now - this->zone_started_) : 0;
    this->budget_.charge(zone, actual * this->sim_speed_);
    this->record_(zone, this->plan_.program, this->source_, this->zone_planned_seconds_,
                  static_cast<uint16_t>(actual > 65535 ? 65535 : actual), Outcome::OK, this->zone_started_);
    if (this->source_ == Source::MANUAL) {
      const LocalTime closed_at = irrigation_core::local_from_epoch(now);
      if (closed_at.valid)
        this->credit_manual_run_(zone, actual * this->sim_speed_, closed_at.day_number);
    }
    this->clear_active_run_();
    if (this->guard_ != nullptr)
      this->guard_->disarm();
  }

  this->last_active_valve_ = current;

  if (current >= 0) {
    const uint8_t zone = static_cast<uint8_t>(current);
    this->zone_started_ = now;
    this->zone_planned_seconds_ = static_cast<uint16_t>(this->sprinkler_->valve_run_duration_adjusted(zone));
    this->sup_.started(this->source_, zone);
    this->active_run_ = ActiveRun{now, this->zone_planned_seconds_, zone, this->plan_.program, this->source_, true};
    this->save_active_run_();
    if (this->guard_ != nullptr)
      this->guard_->arm(zone, this->zone_planned_seconds_ + GUARD_MARGIN_SECONDS);
    return;
  }

  // Nothing active. If the sequencer has genuinely finished, close the cycle.
  if (this->cycle_active_ && this->sprinkler_->controller_state() == sprinkler::IDLE) {
    const LocalTime local = irrigation_core::local_from_epoch(now);
    this->complete_cycle_(local);
  }
}

void IrrigationScheduler::complete_cycle_(const LocalTime &local) {
  if (local.valid) {
    for (uint8_t i = 0; i < this->plan_.count; i++) {
      const uint8_t zone = this->plan_.entries[i].zone;
      this->run_state_.mark_ran(this->plan_.program, zone, local.day_number);
      this->run_state_.mark_actual(zone, local.day_number);
    }
  }
  this->cycle_active_ = false;
  this->sup_.finished();
  if (this->guard_ != nullptr)
    this->guard_->disarm();
  this->save_state_();
  ESP_LOGI(TAG, "cycle complete (program %u)", this->plan_.program);
}

// FR-10.7 / D10. A manual run that delivered at least 80 % of what a program
// planned for that zone today satisfies that program's zone for the day. This
// prevents the most common irrigation mistake -- watering by hand on a hot
// afternoon and then watering again automatically at five the next morning.
// Scoped per zone, not per cycle, exactly as D10 settled it.
void IrrigationScheduler::credit_manual_run_(uint8_t zone, uint32_t delivered_seconds, int32_t day) {
  for (uint8_t p = 0; p < irrigation_core::MAX_PROGRAMS; p++) {
    const irrigation_core::Program &prog = this->active_.programs[p];
    if (!prog.enabled || prog.zone_minutes[zone] == 0)
      continue;
    if (this->run_state_.last_done_day[p][zone] == day)
      continue;
    const uint32_t planned =
        static_cast<uint32_t>(irrigation_core::effective_seconds(prog.zone_minutes[zone], this->active_.seasonal_pct,
                                                            prog.repeats)) *
        prog.repeats;
    if (planned == 0 || delivered_seconds * 100 < planned * 80)
      continue;
    this->run_state_.mark_ran(p, zone, day);
    this->record_(zone, p, Source::MANUAL, static_cast<uint16_t>(planned),
                  static_cast<uint16_t>(delivered_seconds > 65535 ? 65535 : delivered_seconds),
                  Outcome::SATISFIED_BY_MANUAL, this->cycle_started_);
    ESP_LOGI(TAG, "zone %u: today's program %u is satisfied by the manual run", zone, p);
  }
  this->run_state_.mark_actual(zone, day);
  this->save_state_();
}

void IrrigationScheduler::abandon_cycle_(Outcome outcome) {
  if (!this->cycle_active_)
    return;
  this->cycle_active_ = false;
  // Deliberately NOT marking the zones done: the next tick re-evaluates
  // due-ness and, if still inside the catch-up window, picks up the zones that
  // did not finish. That is re-evaluation, not resumption -- resuming an
  // interrupted run turns a single fault into a repeated one.
  ESP_LOGW(TAG, "cycle abandoned: %s", to_string(outcome));
  if (this->last_active_valve_ >= 0)
    this->record_(static_cast<uint8_t>(this->last_active_valve_), this->plan_.program, this->source_,
                  this->zone_planned_seconds_, 0, outcome, this->zone_started_);
  this->last_active_valve_ = -1;
  this->clear_active_run_();
}

// ---------------------------------------------------------------------------
//  Verbs
// ---------------------------------------------------------------------------

void IrrigationScheduler::hold(uint32_t seconds, const std::string &by) {
  this->sup_.hold(static_cast<Epoch>(this->time_->timestamp_now()), seconds, by.c_str());
  this->save_state_();
  ESP_LOGI(TAG, "hold until %lld, requested by %s", static_cast<long long>(this->sup_.hold_until()),
           this->sup_.state().held_by);
}

void IrrigationScheduler::release_hold() {
  this->sup_.release_hold(static_cast<Epoch>(this->time_->timestamp_now()));
  this->save_state_();
  ESP_LOGI(TAG, "hold released");
}

void IrrigationScheduler::pause() {
  if (!this->sup_.pause(static_cast<Epoch>(this->time_->timestamp_now()))) {
    ESP_LOGW(TAG, "pause: nothing is running");
    return;
  }
  this->sprinkler_->pause();
  if (this->guard_ != nullptr)
    this->guard_->disarm();
  ESP_LOGI(TAG, "paused");
}

void IrrigationScheduler::resume() {
  if (!this->sup_.resume(static_cast<Epoch>(this->time_->timestamp_now()))) {
    ESP_LOGW(TAG, "resume: nothing is paused");
    return;
  }
  this->sprinkler_->resume();
  ESP_LOGI(TAG, "resumed");
}

void IrrigationScheduler::stop() {
  this->sprinkler_->shutdown(true);
  this->sprinkler_->reset_resume();
  if (this->guard_ != nullptr) {
    this->guard_->hard_close_all();
    this->guard_->disarm();
  }
  this->abandon_cycle_(Outcome::ABORTED);
  this->sup_.stop();
  this->last_active_valve_ = -1;
  ESP_LOGI(TAG, "stopped");
}

void IrrigationScheduler::set_winter(bool on) {
  if (this->sup_.set_winter(on))
    this->abandon_cycle_(Outcome::SKIPPED_WINTER);
  if (on) {
    this->sprinkler_->shutdown(true);
    if (this->guard_ != nullptr)
      this->guard_->inhibit(true);
  } else if (this->guard_ != nullptr && !this->sup_.locked_out()) {
    this->guard_->inhibit(false);
  }
  this->save_state_();
  ESP_LOGI(TAG, "winter mode %s", on ? "ON -- all irrigation blocked" : "off");
}

void IrrigationScheduler::enter_lockout_(const char *reason) {
  if (this->sup_.latch_critical())
    this->abandon_cycle_(Outcome::ABORTED);
  this->alarm_ = reason;
  if (this->sprinkler_ != nullptr)
    this->sprinkler_->shutdown(true);
  if (this->guard_ != nullptr) {
    this->guard_->hard_close_all();
    this->guard_->inhibit(true);
  }
  this->save_state_();
  ESP_LOGE(TAG, "CRITICAL latched: %s", reason);
}

void IrrigationScheduler::latch_critical(const std::string &reason) {
  this->lockout_reason_ = reason;
  this->enter_lockout_(this->lockout_reason_.c_str());
}

void IrrigationScheduler::acknowledge(const std::string &by) {
  this->sup_.acknowledge();
  this->alarm_ = "";
  if (this->guard_ != nullptr) {
    this->guard_->clear_trip();
    if (this->sup_.mode() != Mode::WINTER)
      this->guard_->inhibit(false);
  }
  this->save_state_();
  ESP_LOGI(TAG, "alarms acknowledged by %s", by.c_str());
}

// ---------------------------------------------------------------------------
//  Manual runs
// ---------------------------------------------------------------------------

bool IrrigationScheduler::manual_run(uint8_t zone, uint16_t minutes) {
  const Epoch now = static_cast<Epoch>(this->time_->timestamp_now());
  const LocalTime local = irrigation_core::local_from_epoch(now);

  const Refusal refusal = this->sup_.can_start(Source::MANUAL, zone, minutes, local.valid, this->active_);
  if (refusal != Refusal::NONE) {
    // FR-10.6: report a visible outcome rather than silently swallowing the
    // press.
    ESP_LOGW(TAG, "manual run of zone %u refused: %s", zone, irrigation_core::to_string(refusal));
    this->alarm_ = irrigation_core::to_string(refusal);
    return false;
  }
  if (!this->sup_.take_diagnostic_allowance()) {
    ESP_LOGW(TAG, "manual run refused: the single diagnostic run has already been used");
    return false;
  }

  if (local.valid)
    this->budget_.roll_to(local.day_number);
  const uint32_t granted = irrigation_core::clamp_run_seconds(static_cast<uint32_t>(minutes) * 60, zone, this->budget_);
  if (granted == 0) {
    ESP_LOGW(TAG, "manual run of zone %u refused: the day's budget for that zone is spent", zone);
    return false;
  }

  this->plan_ = Plan{};
  this->plan_.program = 0xFF;  // "not a program"
  this->plan_.repeats = 1;
  this->plan_.count = 1;
  this->plan_.entries[0].zone = zone;
  this->plan_.entries[0].seconds = static_cast<uint16_t>(granted);
  this->source_ = Source::MANUAL;
  this->cycle_started_ = now;
  this->cycle_active_ = true;

  this->sprinkler_->start_single_valve(zone, this->scaled_(granted));
  this->sup_.started(Source::MANUAL, zone);
  ESP_LOGI(TAG, "manual run: zone %u for %" PRIu32 " s", zone, granted);
  return true;
}

void IrrigationScheduler::test_all_zones(uint16_t seconds_each) {
  // FR-10.8, the commissioning action. Queued rather than cycled, because
  // queued valves run regardless of the enable switches -- which is exactly
  // what "test all zones" means.
  const Epoch now = static_cast<Epoch>(this->time_->timestamp_now());
  const LocalTime local = irrigation_core::local_from_epoch(now);

  // Ask about the first zone that could actually take part, not about zone 0:
  // a disabled zone 1 must not make "test all zones" report ZONE_DISABLED and
  // refuse the whole commissioning run.
  uint8_t first = 0xFF;
  for (uint8_t z = 0; z < this->zone_count_; z++) {
    if (this->active_.zone_enabled[z] && !this->sup_.zone_locked_out(z)) {
      first = z;
      break;
    }
  }
  if (first == 0xFF) {
    ESP_LOGW(TAG, "zone test: no zone is both enabled and clear of lockouts");
    return;
  }

  const Refusal refusal =
      this->sup_.can_start(Source::TEST, first, seconds_each / 60 + 1, local.valid, this->active_);
  if (refusal != Refusal::NONE) {
    ESP_LOGW(TAG, "zone test refused: %s", irrigation_core::to_string(refusal));
    return;
  }

  this->sprinkler_->clear_queued_valves();
  this->plan_ = Plan{};
  this->plan_.program = 0xFF;
  this->plan_.repeats = 1;
  for (uint8_t z = 0; z < this->zone_count_; z++) {
    if (!this->active_.zone_enabled[z] || this->sup_.zone_locked_out(z))
      continue;
    this->sprinkler_->queue_valve(z, this->scaled_(seconds_each));
    this->plan_.entries[this->plan_.count].zone = z;
    this->plan_.entries[this->plan_.count].seconds = seconds_each;
    this->plan_.count++;
  }
  if (this->plan_.count == 0) {
    ESP_LOGW(TAG, "zone test: no zone is both enabled and clear of lockouts");
    return;
  }

  this->source_ = Source::TEST;
  this->cycle_started_ = now;
  this->cycle_active_ = true;
  this->sprinkler_->start_from_queue();
  this->sup_.started(Source::TEST, this->plan_.entries[0].zone);
  ESP_LOGI(TAG, "testing %u zone(s) for %u s each", this->plan_.count, seconds_each);
}

void IrrigationScheduler::set_simulation_speed(uint16_t speed) {
  if (speed < 1)
    speed = 1;
  if (speed > 600)
    speed = 600;
  this->sim_speed_ = speed;
  ESP_LOGI(TAG, "simulation speed x%u (durations are divided; runs can only get shorter)", speed);
}

// ---------------------------------------------------------------------------
//  Staged configuration
// ---------------------------------------------------------------------------

void IrrigationScheduler::stage_zone_enabled(uint8_t zone, bool on) {
  if (zone >= irrigation_core::MAX_ZONES)
    return;
  this->staged_.zone_enabled[zone] = on;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_zone_priority(uint8_t zone, uint8_t priority) {
  if (zone >= irrigation_core::MAX_ZONES)
    return;
  this->staged_.zone_priority[zone] = priority;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_seasonal_pct(uint16_t pct) {
  this->staged_.seasonal_pct = pct;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_program_enabled(uint8_t program, bool on) {
  if (program >= irrigation_core::MAX_PROGRAMS)
    return;
  this->staged_.programs[program].enabled = on;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_program_start(uint8_t program, uint16_t minute_of_day) {
  if (program >= irrigation_core::MAX_PROGRAMS)
    return;
  this->staged_.programs[program].start_minute = minute_of_day;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_program_weekday_mask(uint8_t program, uint8_t mask) {
  if (program >= irrigation_core::MAX_PROGRAMS)
    return;
  this->staged_.programs[program].days.weekday_mask = mask;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_program_interval(uint8_t program, uint8_t days) {
  if (program >= irrigation_core::MAX_PROGRAMS)
    return;
  this->staged_.programs[program].days.interval_days = days;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_program_repeats(uint8_t program, uint8_t repeats) {
  if (program >= irrigation_core::MAX_PROGRAMS)
    return;
  this->staged_.programs[program].repeats = repeats;
  this->dirty_ = true;
}

void IrrigationScheduler::stage_zone_minutes(uint8_t program, uint8_t zone, uint16_t minutes) {
  if (program >= irrigation_core::MAX_PROGRAMS || zone >= irrigation_core::MAX_ZONES)
    return;
  this->staged_.programs[program].zone_minutes[zone] = minutes;
  this->dirty_ = true;
}

bool IrrigationScheduler::apply() {
  // FR-2.2: the commit is atomic. Validate the whole staged schedule first --
  // a partially-applied schedule must never exist.
  this->staged_.schema_version = irrigation_core::SCHEMA_VERSION;
  this->staged_.zone_count = this->zone_count_;

  const irrigation_core::ValidationResult r = irrigation_core::validate(this->staged_);
  if (!r.ok()) {
    this->validation_message_ = r.message();
    ESP_LOGW(TAG, "apply refused: program %u, zone %u -- %s", r.program, r.zone, r.message());
    return false;
  }

  this->active_ = this->staged_;
  this->dirty_ = false;
  this->validation_message_ = "ok";
  this->save_schedule_();
  ESP_LOGI(TAG, "schedule applied (sequence %" PRIu32 ")", this->schedule_sequence_);
  return true;
}

void IrrigationScheduler::revert() {
  this->staged_ = this->active_;
  this->dirty_ = false;
  this->validation_message_ = "ok";
  ESP_LOGI(TAG, "staged edits discarded");
}

// ---------------------------------------------------------------------------
//  Run records
// ---------------------------------------------------------------------------

void IrrigationScheduler::record_(uint8_t zone, uint8_t program, Source src, uint16_t planned, uint16_t actual,
                                 Outcome outcome, Epoch started) {
  RunRecord &rec = this->records_[this->record_head_];
  rec.started = started;
  rec.planned_seconds = planned;
  rec.actual_seconds = actual;
  rec.zone = zone;
  rec.program = program;
  rec.source = src;
  rec.outcome = outcome;

  this->record_head_ = (this->record_head_ + 1) % RECORD_RING_SIZE;
  if (this->record_count_ < RECORD_RING_SIZE)
    this->record_count_++;

  // FR-7.5: an append-only line that is readable in ten years without this
  // software. The log is the archive until the SD/JSONL sink of v1 exists.
  ESP_LOGI(TAG, "run,%lld,%u,%u,%s,%u,%u,%s", static_cast<long long>(started), zone, program,
           irrigation_core::to_string(src), planned, actual, to_string(outcome));
}

void IrrigationScheduler::log_records() const {
  ESP_LOGI(TAG, "epoch,zone,program,source,planned_s,actual_s,outcome");
  for (uint8_t i = 0; i < this->record_count_; i++) {
    const uint8_t idx = (this->record_head_ + RECORD_RING_SIZE - this->record_count_ + i) % RECORD_RING_SIZE;
    const RunRecord &r = this->records_[idx];
    ESP_LOGI(TAG, "%lld,%u,%u,%s,%u,%u,%s", static_cast<long long>(r.started), r.zone, r.program,
             irrigation_core::to_string(r.source), r.planned_seconds, r.actual_seconds, to_string(r.outcome));
  }
}

// ---------------------------------------------------------------------------
//  Presentation
// ---------------------------------------------------------------------------

bool IrrigationScheduler::clock_trusted() const {
  if (this->time_ == nullptr)
    return false;
  return irrigation_core::local_from_epoch(static_cast<Epoch>(this->time_->timestamp_now())).valid;
}

int IrrigationScheduler::active_zone() const {
  return this->last_active_valve_;
}

uint32_t IrrigationScheduler::seconds_remaining() const {
  if (this->sprinkler_ == nullptr)
    return 0;
  const auto remaining = this->sprinkler_->time_remaining_current_operation();
  return remaining.has_value() ? *remaining : 0;
}

uint32_t IrrigationScheduler::zone_seconds_today(uint8_t zone) const {
  return zone < irrigation_core::MAX_ZONES ? this->budget_.seconds_used[zone] : 0;
}

std::string IrrigationScheduler::mode_text() const { return irrigation_core::to_string(this->sup_.mode()); }

std::string IrrigationScheduler::state_text() const {
  char buf[96];
  if (this->sup_.state().crash_loop)
    return "crash-loop brake";
  if (this->sup_.state().critical_latched)
    return "locked out";
  if (this->sup_.mode() == Mode::WINTER)
    return "winter";
  if (this->sup_.phase() == Phase::PAUSED)
    return "paused";
  if (this->last_active_valve_ >= 0) {
    std::snprintf(buf, sizeof(buf), "watering zone %d (%s)", this->last_active_valve_ + 1,
                  irrigation_core::to_string(this->source_));
    return buf;
  }
  if (this->sup_.mode() == Mode::HOLD)
    return "held";
  if (!this->clock_trusted())
    return "clock not trusted";
  return "idle";
}

std::string IrrigationScheduler::plan_text() const {
  if (this->plan_.count == 0)
    return "no plan";
  std::string out;
  char buf[32];
  for (uint8_t i = 0; i < this->plan_.count; i++) {
    std::snprintf(buf, sizeof(buf), "%sZ%u:%us", i == 0 ? "" : " ", this->plan_.entries[i].zone + 1,
                  this->plan_.entries[i].seconds);
    out += buf;
  }
  if (this->plan_.repeats > 1) {
    std::snprintf(buf, sizeof(buf), " x%u passes", this->plan_.repeats);
    out += buf;
  }
  return out;
}

std::string IrrigationScheduler::next_run_text() const {
  if (this->time_ == nullptr)
    return "unknown";
  const LocalTime local = irrigation_core::local_from_epoch(static_cast<Epoch>(this->time_->timestamp_now()));
  if (!local.valid)
    return "clock not trusted";
  if (this->sup_.mode() == Mode::WINTER)
    return "winter -- nothing scheduled";
  if (this->sup_.mode() == Mode::HOLD)
    return "held";

  const int32_t next = irrigation_core::next_start_today(this->active_, local);
  if (next < 0)
    return "nothing further today";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "today %02d:%02d", static_cast<int>(next / 60), static_cast<int>(next % 60));
  return buf;
}

std::string IrrigationScheduler::hold_text() const {
  if (this->sup_.mode() != Mode::HOLD)
    return "not held";
  char buf[96];
  const Epoch now = this->time_ != nullptr ? static_cast<Epoch>(this->time_->timestamp_now()) : 0;
  const long long hours = static_cast<long long>((this->sup_.hold_until() - now) / 3600);
  std::snprintf(buf, sizeof(buf), "%lld h left, held by %s", hours,
                this->sup_.state().held_by[0] ? this->sup_.state().held_by : "unknown");
  return buf;
}

std::string IrrigationScheduler::last_record_text() const {
  if (this->record_count_ == 0)
    return "no runs yet";
  const uint8_t idx = (this->record_head_ + RECORD_RING_SIZE - 1) % RECORD_RING_SIZE;
  const RunRecord &r = this->records_[idx];
  char buf[96];
  std::snprintf(buf, sizeof(buf), "Z%u %s %us/%us %s", r.zone + 1, irrigation_core::to_string(r.source),
                r.actual_seconds, r.planned_seconds, to_string(r.outcome));
  return buf;
}

std::string IrrigationScheduler::alarm_text() const {
  if (this->guard_ != nullptr && this->guard_->tripped())
    return std::string("guard tripped: ") + this->guard_->trip_reason_str();
  if (this->alarm_[0] != '\0')
    return this->alarm_;
  return "none";
}

void IrrigationScheduler::dump_config() {
  ESP_LOGCONFIG(TAG, "Irrigation scheduler:");
  ESP_LOGCONFIG(TAG, "  zones: %u, catch-up window: %u min", this->zone_count_, this->opts_.catch_up_minutes);
  ESP_LOGCONFIG(TAG, "  daily per-zone budget: %" PRIu32 " s", this->budget_.seconds_per_zone);
  ESP_LOGCONFIG(TAG, "  schedule sequence: %" PRIu32, this->schedule_sequence_);
  for (uint8_t p = 0; p < irrigation_core::MAX_PROGRAMS; p++) {
    const irrigation_core::Program &prog = this->active_.programs[p];
    if (!prog.enabled)
      continue;
    ESP_LOGCONFIG(TAG, "  program %u: %02u:%02u mask 0x%02X every %u day(s), %u pass(es)", p,
                  prog.start_minute / 60, prog.start_minute % 60, prog.days.weekday_mask,
                  prog.days.interval_days, prog.repeats);
  }
}

}  // namespace esphome::irrigation_scheduler
