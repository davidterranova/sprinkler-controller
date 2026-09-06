#pragma once
//
//  The pure half of the over-run guard.
//
//  The impure half -- an esp_timer callback that toggles the deadman pin and
//  writes GPIOs directly -- lives in components/irrigation_guard/. Everything
//  that can be decided without touching hardware is decided here so it can be
//  tested without hardware.
//
#include "types.h"

namespace irrigation_core {

// The compile-time ceiling on any single valve run, independent of the
// schedule, the sprinkler component and every number entity.
//
// docs/07-firmware.md documents why this constant has to exist: the stock
// component's own limits allow `multiplier_number` = 10 and
// `run_duration_number` = 86400 s, so **a 240-hour run is expressible within
// the component's own validation**. Clamping in YAML is necessary and not
// sufficient; this is the value the independent code path enforces.
inline constexpr uint32_t MAX_RUN_SECONDS = 130u * 60;  // 2 h 10, just over the 2 h schedule cap

// How long the main loop may be silent before the deadman stops being fed.
// The hardware side (a retriggerable monostable, ~1 s) then drops the valve
// rail. Sized well above ESPHome's 16 ms loop interval and above the worst
// legitimate loop stall we expect, and well below the ~1 s monostable period.
inline constexpr uint32_t LIVENESS_TIMEOUT_MS = 400;

// Rollover-safe elapsed-time comparison. Written as unsigned subtraction on
// purpose: `now >= start + max` breaks once at every 2^32 ms (~49.7 days), and
// a controller that runs a whole season without rebooting will cross that
// boundary mid-season, in August, with everyone away.
inline bool deadline_expired(uint32_t now_ms, uint32_t start_ms, uint32_t max_ms) {
  return static_cast<uint32_t>(now_ms - start_ms) >= max_ms;
}

inline uint32_t elapsed_ms(uint32_t now_ms, uint32_t start_ms) { return static_cast<uint32_t>(now_ms - start_ms); }

// FR-3.7a: the per-day per-zone budget is what makes a resume bug survivable.
// The run counter is not the real stop condition -- this is. Expressed in
// seconds for v0 (no flow meter yet); v1 replaces or supplements it with a
// litre budget once the meter exists.
struct DailyBudget {
  int32_t day{-1};  // local day number the counters below belong to
  uint32_t seconds_used[MAX_ZONES]{};
  uint32_t seconds_per_zone{0};  // 0 = no budget configured

  // Rolls the counters over when the civil day changes. Safe to call often.
  void roll_to(int32_t day_number) {
    if (day_number == this->day)
      return;
    this->day = day_number;
    for (uint8_t z = 0; z < MAX_ZONES; z++)
      this->seconds_used[z] = 0;
  }

  bool would_exceed(uint8_t zone, uint32_t seconds) const {
    if (this->seconds_per_zone == 0 || zone >= MAX_ZONES)
      return false;
    return this->seconds_used[zone] + seconds > this->seconds_per_zone;
  }

  uint32_t remaining(uint8_t zone) const {
    if (this->seconds_per_zone == 0 || zone >= MAX_ZONES)
      return MAX_RUN_SECONDS;
    if (this->seconds_used[zone] >= this->seconds_per_zone)
      return 0;
    return this->seconds_per_zone - this->seconds_used[zone];
  }

  void charge(uint8_t zone, uint32_t seconds) {
    if (zone < MAX_ZONES)
      this->seconds_used[zone] += seconds;
  }
};

// The one place a requested duration is turned into an enforceable one. Every
// path -- scheduled, catch-up, manual, commissioning test -- goes through it.
inline uint32_t clamp_run_seconds(uint32_t requested, uint8_t zone, const DailyBudget &budget) {
  uint32_t seconds = requested > MAX_RUN_SECONDS ? MAX_RUN_SECONDS : requested;
  const uint32_t left = budget.remaining(zone);
  if (seconds > left)
    seconds = left;
  return seconds;
}

}  // namespace irrigation_core
