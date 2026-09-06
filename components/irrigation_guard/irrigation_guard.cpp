#include "irrigation_guard.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::irrigation_guard {

static const char *const TAG = "irrigation_guard";

void panic_close(uint8_t pin, bool active_low) {
#ifdef USE_ESP32
  const gpio_num_t num = static_cast<gpio_num_t>(pin);
  // reset_pin first: it returns the pin to a known input state with the
  // internal pull-up on, which on an active-low opto relay board is already
  // "relay off" -- so even if the subsequent calls were to fail, the pin is
  // safe. ESP32 datasheet v5.3 A.1 note 9: during reset all pins are
  // output-disabled, and this reproduces that state on demand.
  gpio_reset_pin(num);
  gpio_set_direction(num, GPIO_MODE_OUTPUT);
  gpio_set_level(num, active_low ? 1 : 0);
#else
  (void) pin;
  (void) active_low;
#endif
}

uint32_t IrrigationGuard::now_ms_() {
#ifdef USE_ESP32
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
#else
  return millis();
#endif
}

void panic_close_all() {
  for (uint8_t i = 0; i < PANIC_PIN_COUNT; i++)
    panic_close(PANIC_PINS[i], PANIC_ACTIVE_LOW);
}

void IrrigationGuard::configure_output_(uint8_t pin) {
  if (pin == 0xFF)
    return;
#ifdef USE_ESP32
  const gpio_num_t num = static_cast<gpio_num_t>(pin);
  gpio_reset_pin(num);
  gpio_set_direction(num, GPIO_MODE_OUTPUT);
#endif
}

void IrrigationGuard::write_raw_(uint8_t pin, bool energised) {
  if (pin == 0xFF)
    return;
#ifdef USE_ESP32
  gpio_set_level(static_cast<gpio_num_t>(pin), (energised != this->active_low_) ? 1 : 0);
#else
  (void) pin;
  (void) energised;
#endif
}

void IrrigationGuard::setup() {
  // Before anything else in the firmware runs: every output de-energised,
  // unconditionally, with no reference to any cached switch state.
  for (uint8_t pin : this->zone_pins_) {
    this->configure_output_(pin);
    this->write_raw_(pin, false);
  }
  this->configure_output_(this->master_pin_);
  this->write_raw_(this->master_pin_, false);

  this->configure_output_(this->heartbeat_pin_);
#ifdef USE_ESP32
  if (this->heartbeat_pin_ != 0xFF)
    gpio_set_level(static_cast<gpio_num_t>(this->heartbeat_pin_), 0);
#endif

  this->last_progress_ms_ = IrrigationGuard::now_ms_();

#ifdef USE_ESP32
  esp_timer_create_args_t args{};
  args.callback = &IrrigationGuard::timer_cb;
  args.arg = this;
  // ESP_TIMER_TASK, not ISR: the callback writes GPIOs and reads shared state,
  // and the esp_timer task runs at a high FreeRTOS priority, so it keeps
  // running while the ESPHome main loop is stuck. That asymmetry is the whole
  // mechanism.
  args.dispatch_method = ESP_TIMER_TASK;
  args.name = "irrigation_deadman";
  if (esp_timer_create(&args, &this->timer_) != ESP_OK) {
    // Without the deadman there is no P0 guarantee, so this is not a warning.
    ESP_LOGE(TAG, "could not create the deadman timer -- marking the component failed");
    this->mark_failed();
    return;
  }
  esp_timer_start_periodic(this->timer_, this->period_us_);
#endif
}

#ifdef USE_ESP32
void IrrigationGuard::timer_cb(void *arg) {
  auto *self = static_cast<IrrigationGuard *>(arg);

  // Deliberately esp_timer's own clock rather than millis(): millis() is
  // maintained by code the main loop shares, and this callback exists precisely
  // for the case where that code has stopped.
  const uint32_t now_ms = IrrigationGuard::now_ms_();

  const uint32_t counter = self->loop_counter_;
  if (counter != self->last_seen_counter_) {
    self->last_seen_counter_ = counter;
    self->last_progress_ms_ = now_ms;
  }

  const bool alive =
      irrigation_core::elapsed_ms(now_ms, self->last_progress_ms_) < self->liveness_timeout_ms_;
  self->heartbeat_alive_ = alive;

  if (alive) {
    // The only place the heartbeat pin is ever toggled. No LEDC, no hardware
    // that could keep running on its own.
    self->heartbeat_level_ = !self->heartbeat_level_;
    gpio_set_level(static_cast<gpio_num_t>(self->heartbeat_pin_), self->heartbeat_level_ ? 1 : 0);
    self->heartbeat_edges_++;
  } else {
    // Stop feeding, and close the valves ourselves as well. Belt and braces:
    // the monostable will drop the rail in ~1 s regardless, but if it has been
    // miswired we would rather still be right.
    gpio_set_level(static_cast<gpio_num_t>(self->heartbeat_pin_), 0);
    if (self->pending_trip_ == TripReason::NONE)
      self->pending_trip_ = TripReason::LIVENESS;
    self->hard_close_all();
    return;
  }

  // The absolute deadline, in a code path the scheduler cannot reach. Polled
  // and rollover-safe (see irrigation_core/guard.h), so a dropped or leaked
  // software timer cannot extend a run.
  if (self->armed_ && irrigation_core::deadline_expired(now_ms, self->armed_start_ms_, self->armed_len_ms_)) {
    self->armed_ = false;
    if (self->pending_trip_ == TripReason::NONE)
      self->pending_trip_ = TripReason::DEADLINE;
    self->hard_close_all();
  }

  // A held-closed state is re-asserted on every callback, so a switch that
  // turns itself on cannot win a race against it.
  if (self->inhibited_)
    self->hard_close_all();
}
#endif

void IrrigationGuard::loop() {
  // The evidence. Nothing else writes it.
  if (this->bench_starve_until_ms_ == 0 || millis() >= this->bench_starve_until_ms_) {
    this->bench_starve_until_ms_ = 0;
    this->loop_counter_++;
  }

  const TripReason pending = this->pending_trip_;
  if (pending != TripReason::NONE && this->trip_reason_ == TripReason::NONE) {
    this->trip_reason_ = pending;
    switch (pending) {
      case TripReason::DEADLINE:
        ESP_LOGE(TAG, "OVER-RUN: zone %u exceeded its absolute deadline of %" PRIu32 " s -- forced closed",
                 this->armed_zone_, this->armed_seconds_);
        break;
      case TripReason::LIVENESS:
        ESP_LOGE(TAG, "DEADMAN: the main loop stopped for more than %" PRIu32 " ms -- heartbeat withheld",
                 this->liveness_timeout_ms_);
        break;
      case TripReason::MANUAL:
        ESP_LOGW(TAG, "guard tripped on request (bench test)");
        break;
      case TripReason::NONE:
        break;
    }
    this->trip_trigger_.trigger(this->armed_zone_);
  }
}

void IrrigationGuard::arm(uint8_t zone, uint32_t seconds) {
  if (seconds > this->max_run_seconds_)
    seconds = this->max_run_seconds_;

  // Disarm FIRST, then write start/length, then arm. The timer task can
  // preempt between any two of these lines, and the one ordering that must
  // never happen is `armed_` true against a half-written deadline.
  this->armed_ = false;
  this->armed_zone_ = zone;
  this->armed_seconds_ = seconds;
  this->armed_start_ms_ = IrrigationGuard::now_ms_();
  this->armed_len_ms_ = seconds * 1000;
  this->armed_ = true;
  ESP_LOGD(TAG, "armed: zone %u for %" PRIu32 " s", zone, seconds);
}

void IrrigationGuard::disarm() {
  this->armed_ = false;
  ESP_LOGD(TAG, "disarmed");
}

uint32_t IrrigationGuard::seconds_remaining() const {
  if (!this->armed_)
    return 0;
  const uint32_t elapsed = irrigation_core::elapsed_ms(IrrigationGuard::now_ms_(), this->armed_start_ms_) / 1000;
  return elapsed >= this->armed_seconds_ ? 0 : this->armed_seconds_ - elapsed;
}

void IrrigationGuard::hard_close_all() {
  for (uint8_t pin : this->zone_pins_)
    this->write_raw_(pin, false);
  this->write_raw_(this->master_pin_, false);
}

void IrrigationGuard::inhibit(bool on) {
  this->inhibited_ = on;
  if (on)
    this->hard_close_all();
}

const char *IrrigationGuard::trip_reason_str() const {
  switch (this->trip_reason_) {
    case TripReason::NONE:
      return "none";
    case TripReason::DEADLINE:
      return "over-run";
    case TripReason::LIVENESS:
      return "deadman";
    case TripReason::MANUAL:
      return "bench test";
  }
  return "?";
}

void IrrigationGuard::clear_trip() {
  this->trip_reason_ = TripReason::NONE;
  this->pending_trip_ = TripReason::NONE;
}

void IrrigationGuard::dump_config() {
  ESP_LOGCONFIG(TAG, "Irrigation guard:");
  ESP_LOGCONFIG(TAG, "  heartbeat: GPIO%u every %" PRIu32 " us (liveness timeout %" PRIu32 " ms)",
                this->heartbeat_pin_, this->period_us_, this->liveness_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  master: GPIO%u, %u zone pin(s), active %s", this->master_pin_,
                static_cast<unsigned>(this->zone_pins_.size()), this->active_low_ ? "low" : "high");
  ESP_LOGCONFIG(TAG, "  absolute run limit: %" PRIu32 " s", this->max_run_seconds_);
}

}  // namespace esphome::irrigation_guard
