#pragma once
//
//  L1 -- the supervisor. Two jobs, both of which must work when nothing else
//  does:
//
//    1. Feed the hardware deadman, but ONLY while there is evidence the
//       application is alive.
//    2. Enforce an absolute per-run deadline from a code path the scheduler
//       cannot reach, and close the valves by writing GPIOs directly.
//
//  ## Why the heartbeat is an esp_timer callback and not LEDC, and not loop()
//
//  docs/05-hardware.md carries a CRITICAL correction that this file is the
//  implementation of, and it is worth restating because getting it wrong
//  produces a very elegant device that protects nothing:
//
//    * **LEDC free-runs in hardware.** Start it once and the square wave
//      continues whether or not any software is alive. A hung main loop, a
//      crash loop, safe mode -- the wave carries on, the monostable stays
//      retriggered, and the valve rail stays enabled. The circuit would
//      protect nothing at all.
//
//    * **Toggling in loop() cannot go fast enough.** ESPHome's loop_interval_
//      defaults to 16 ms, capping the rate at ~31 Hz, which is below what the
//      original passive charge pump needed to assert its output.
//
//  So: an independent esp_timer callback toggles the pin, and it checks a
//  counter that only the main loop increments. That makes BOTH failure modes
//  trip -- the main loop hanging (the counter goes stale, so the callback stops
//  toggling) and the whole runtime wedging (the callback stops running). If you
//  can stop the code and the wave continues, the design is wrong.
//
//  ## Why the GPIO writes are raw
//
//  The stock sprinkler component's all_valves_off_() only writes a pin **if the
//  switch's cached state is true** [V]. So exactly when cache and reality have
//  diverged -- the case worth defending against -- "close everything" closes
//  nothing. Everything in this file writes the pin unconditionally, through
//  gpio_set_level(), with no cache in the path.
//
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <vector>

#ifdef USE_ESP32
#include <driver/gpio.h>
#include <esp_timer.h>
#endif

#include "../irrigation_core/guard.h"

namespace esphome::irrigation_guard {

// Free functions, deliberately not methods: they must be callable from
// `safe_mode: on_safe_mode:`, where no component has been constructed and the
// switch platform will never run. In safe mode this is the ONLY thing that will
// ever put the pins in a safe state (risk R1).
//
// The pin table is emitted by the component's codegen from the SAME list the
// guard itself uses, so the safe-mode sweep cannot drift out of step with the
// running config -- which a hand-written list in YAML certainly would.
extern const uint8_t PANIC_PINS[];
extern const uint8_t PANIC_PIN_COUNT;
extern const bool PANIC_ACTIVE_LOW;

void panic_close(uint8_t pin, bool active_low);
void panic_close_all();

enum class TripReason : uint8_t {
  NONE = 0,
  DEADLINE,   // a run exceeded its absolute maximum
  LIVENESS,   // the main loop stopped feeding the guard
  MANUAL,     // a bench test asked for it
};

class IrrigationGuard : public Component {
 public:
  // Earlier than every other component, including the GPIO switches. FR-3.2:
  // all valve outputs are de-energised before anything else initialises.
  float get_setup_priority() const override { return setup_priority::BUS + 100.0f; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // --- configuration, from codegen -------------------------------------
  void set_heartbeat_pin(uint8_t pin) { this->heartbeat_pin_ = pin; }
  void set_heartbeat_period_us(uint32_t us) { this->period_us_ = us; }
  void set_master_pin(uint8_t pin) { this->master_pin_ = pin; }
  void add_zone_pin(uint8_t pin) { this->zone_pins_.push_back(pin); }
  void set_active_low(bool active_low) { this->active_low_ = active_low; }
  void set_max_run_seconds(uint32_t seconds) {
    this->max_run_seconds_ = seconds > irrigation_core::MAX_RUN_SECONDS ? irrigation_core::MAX_RUN_SECONDS : seconds;
  }
  void set_liveness_timeout_ms(uint32_t ms) { this->liveness_timeout_ms_ = ms; }

  Trigger<uint8_t> *get_trip_trigger() { return &this->trip_trigger_; }

  // --- the scheduler's interface ----------------------------------------
  // Arms the absolute deadline. `seconds` is clamped here as well as by the
  // caller, because "clamped in two independent places" is the whole point.
  void arm(uint8_t zone, uint32_t seconds);
  void disarm();

  bool armed() const { return this->armed_; }
  uint8_t armed_zone() const { return this->armed_zone_; }
  uint32_t seconds_remaining() const;

  // --- always available --------------------------------------------------
  void hard_close_all();

  // Holds every output closed regardless of what any switch believes. Used by
  // Stop, by a latched CRITICAL and by winter mode -- the states where the
  // sequencer must be unable to open anything even if it tries.
  void inhibit(bool on);
  bool inhibited() const { return this->inhibited_; }

  bool tripped() const { return this->trip_reason_ != TripReason::NONE; }
  TripReason trip_reason() const { return this->trip_reason_; }
  const char *trip_reason_str() const;
  void clear_trip();

  // --- bench instrumentation (v0) ---------------------------------------
  // Deliberately stops feeding the liveness counter WITHOUT hanging the CPU,
  // so the deadman can be exercised on a desk without a reset button. The real
  // proof is still a genuinely hung task -- see verification item E6.
  void bench_stop_feeding(uint32_t ms) { this->bench_starve_until_ms_ = millis() + ms; }
  uint32_t heartbeat_edges() const { return this->heartbeat_edges_; }
  bool heartbeat_alive() const { return this->heartbeat_alive_; }

 protected:
#ifdef USE_ESP32
  static void timer_cb(void *arg);
#endif
  // esp_timer's own clock. millis() happens to be the same base on ESP-IDF,
  // but this path must not depend on anything the stalled main loop maintains.
  static uint32_t now_ms_();
  void configure_output_(uint8_t pin);
  void write_raw_(uint8_t pin, bool energised);

  uint8_t heartbeat_pin_{0xFF};
  uint8_t master_pin_{0xFF};
  std::vector<uint8_t> zone_pins_;
  bool active_low_{true};

  uint32_t period_us_{25000};  // 20 Hz square wave => 40 edges/s
  uint32_t max_run_seconds_{irrigation_core::MAX_RUN_SECONDS};
  uint32_t liveness_timeout_ms_{irrigation_core::LIVENESS_TIMEOUT_MS};

  // Written by loop(), read by the timer callback. The single piece of
  // evidence that the application is still alive.
  volatile uint32_t loop_counter_{0};
  volatile uint32_t last_seen_counter_{0};
  volatile uint32_t last_progress_ms_{0};

  volatile bool armed_{false};
  // Start + length rather than an absolute instant, so the comparison in the
  // callback is the rollover-safe unsigned subtraction from irrigation_core.
  volatile uint32_t armed_start_ms_{0};
  volatile uint32_t armed_len_ms_{0};
  uint8_t armed_zone_{0};
  uint32_t armed_seconds_{0};

  bool inhibited_{false};
  bool heartbeat_level_{false};
  volatile bool heartbeat_alive_{true};
  volatile uint32_t heartbeat_edges_{0};

  // Set in the timer callback, consumed in loop(). ESPHome's logger and
  // automation machinery are not safe to call from another task, so the
  // callback only ever sets a flag and writes pins.
  volatile TripReason pending_trip_{TripReason::NONE};
  TripReason trip_reason_{TripReason::NONE};
  uint32_t bench_starve_until_ms_{0};

#ifdef USE_ESP32
  esp_timer_handle_t timer_{nullptr};
#endif
  Trigger<uint8_t> trip_trigger_;
};

template<typename... Ts> class ArmAction : public Action<Ts...>, public Parented<IrrigationGuard> {
 public:
  TEMPLATABLE_VALUE(uint8_t, zone)
  TEMPLATABLE_VALUE(uint32_t, duration)
  void play(Ts... x) override { this->parent_->arm(this->zone_.value(x...), this->duration_.value(x...)); }
};

template<typename... Ts> class DisarmAction : public Action<Ts...>, public Parented<IrrigationGuard> {
 public:
  void play(Ts...) override { this->parent_->disarm(); }
};

template<typename... Ts> class HardCloseAction : public Action<Ts...>, public Parented<IrrigationGuard> {
 public:
  void play(Ts...) override { this->parent_->hard_close_all(); }
};

}  // namespace esphome::irrigation_guard
