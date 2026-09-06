#include "fsm.h"

namespace irrigation_core {

const char *to_string(Mode m) {
  switch (m) {
    case Mode::NORMAL:
      return "normal";
    case Mode::HOLD:
      return "hold";
    case Mode::WINTER:
      return "winter";
  }
  return "?";
}

const char *to_string(Phase p) {
  switch (p) {
    case Phase::IDLE:
      return "idle";
    case Phase::RUNNING:
      return "running";
    case Phase::PAUSED:
      return "paused";
  }
  return "?";
}

const char *to_string(Source s) {
  switch (s) {
    case Source::SCHEDULE:
      return "schedule";
    case Source::CATCH_UP:
      return "catch_up";
    case Source::MANUAL:
      return "manual";
    case Source::TEST:
      return "test";
  }
  return "?";
}

const char *to_string(Refusal r) {
  switch (r) {
    case Refusal::NONE:
      return "ok";
    case Refusal::WINTER:
      return "winter mode -- all irrigation blocked";
    case Refusal::HELD:
      return "held";
    case Refusal::LOCKED_OUT:
      return "locked out by a latched CRITICAL alarm";
    case Refusal::ZONE_LOCKED_OUT:
      return "zone locked out by a latched CRITICAL alarm";
    case Refusal::CLOCK_UNTRUSTED:
      return "clock not trusted";
    case Refusal::ALREADY_RUNNING:
      return "already running";
    case Refusal::ZONE_DISABLED:
      return "zone disabled";
    case Refusal::INVALID_ZONE:
      return "no such zone";
    case Refusal::DURATION_TOO_LONG:
      return "duration exceeds the hard maximum";
    case Refusal::CRASH_LOOP:
      return "crash-loop brake engaged";
  }
  return "?";
}

static void copy_attribution(char *dst, const char *src) {
  uint8_t i = 0;
  if (src != nullptr)
    for (; i < ATTRIBUTION_LEN - 1 && src[i] != '\0'; i++)
      dst[i] = src[i];
  for (; i < ATTRIBUTION_LEN; i++)
    dst[i] = '\0';
}

void Supervisor::note_boot(Epoch now) {
  // Reboots inside the window accumulate; the first one outside it starts a
  // fresh window. Note this counts *reboots*, not crashes -- an OTA and a
  // deliberate restart look identical from here, which is why the threshold is
  // 3 and the window is short.
  if (this->s_.boot_window_start > 0 &&
      now - this->s_.boot_window_start <= static_cast<Epoch>(CRASH_LOOP_WINDOW_SECONDS)) {
    if (this->s_.boot_count < 255)
      this->s_.boot_count++;
  } else {
    this->s_.boot_count = 1;
    this->s_.boot_window_start = now;
  }

  if (this->s_.boot_count >= CRASH_LOOP_THRESHOLD)
    this->s_.crash_loop = true;

  // FR-3.2: never restore a valve state. Always boot idle.
  this->phase_ = Phase::IDLE;
  this->paused_at_ = 0;
}

void Supervisor::hold(Epoch now, uint32_t duration_seconds, const char *by) {
  if (duration_seconds == 0 || duration_seconds > MAX_HOLD_SECONDS)
    duration_seconds = MAX_HOLD_SECONDS;

  this->s_.mode = Mode::HOLD;
  this->s_.hold_until = now + static_cast<Epoch>(duration_seconds);
  this->s_.hold_warned = false;
  copy_attribution(this->s_.held_by, by);
}

void Supervisor::release_hold(Epoch now) {
  (void) now;
  if (this->s_.mode == Mode::HOLD)
    this->s_.mode = Mode::NORMAL;
  this->s_.hold_until = 0;
  this->s_.hold_warned = false;
  copy_attribution(this->s_.held_by, "");
}

bool Supervisor::set_winter(bool on) {
  bool aborted = false;
  if (on) {
    this->s_.mode = Mode::WINTER;
    aborted = this->phase_ != Phase::IDLE;
    this->stop();
  } else if (this->s_.mode == Mode::WINTER) {
    this->s_.mode = Mode::NORMAL;
  }
  return aborted;
}

bool Supervisor::pause(Epoch now) {
  if (this->phase_ != Phase::RUNNING)
    return false;
  this->phase_ = Phase::PAUSED;
  this->paused_at_ = now;
  return true;
}

bool Supervisor::resume(Epoch now) {
  (void) now;
  if (this->phase_ != Phase::PAUSED)
    return false;
  this->phase_ = Phase::RUNNING;
  this->paused_at_ = 0;
  return true;
}

void Supervisor::stop() {
  this->phase_ = Phase::IDLE;
  this->paused_at_ = 0;
}

Refusal Supervisor::can_start(Source src, uint8_t zone, uint16_t minutes, bool clock_trusted,
                              const Schedule &sched) const {
  const bool human = src == Source::MANUAL || src == Source::TEST;

  // Fault-policy row 14. Nothing runs; the device only advertises the fault.
  if (this->s_.crash_loop)
    return Refusal::CRASH_LOOP;

  // FR-8.5. Winter blocks manual too: the pipes are drained.
  if (this->s_.mode == Mode::WINTER)
    return Refusal::WINTER;

  // FR-9.3. A latched CRITICAL locks out irrigation, with exactly one bounded
  // diagnostic run allowed so a repair can actually be tested. Note this only
  // *permits* the attempt -- take_diagnostic_allowance() spends it.
  if (this->s_.critical_latched && !(human && !this->s_.diagnostic_used))
    return Refusal::LOCKED_OUT;

  if (zone >= sched.zone_count)
    return Refusal::INVALID_ZONE;
  if (this->zone_locked_out(zone))
    return Refusal::ZONE_LOCKED_OUT;
  if (!sched.zone_enabled[zone])
    return Refusal::ZONE_DISABLED;

  if (human) {
    if (minutes > MAX_MANUAL_MINUTES)
      return Refusal::DURATION_TOO_LONG;
  } else {
    // Fault-policy row 6: refuse scheduled runs while the clock is untrusted.
    if (!clock_trusted)
      return Refusal::CLOCK_UNTRUSTED;
    // FR-8.1: Hold means "schedule nothing new". It does not touch manual runs.
    if (this->s_.mode == Mode::HOLD)
      return Refusal::HELD;
    if (minutes > MAX_ZONE_MINUTES)
      return Refusal::DURATION_TOO_LONG;
  }

  // FR-4.6a: a start that collides with a run in progress is SKIPPED, never
  // queued. Queueing lets one slow cycle cascade into permanent back-to-back
  // watering, which is the failure this rule exists to prevent.
  if (this->phase_ != Phase::IDLE)
    return Refusal::ALREADY_RUNNING;

  return Refusal::NONE;
}

void Supervisor::started(Source src, uint8_t zone) {
  this->phase_ = Phase::RUNNING;
  this->source_ = src;
  this->active_zone_ = zone;
}

void Supervisor::finished() {
  this->phase_ = Phase::IDLE;
  this->paused_at_ = 0;
}

bool Supervisor::latch_critical() {
  this->s_.critical_latched = true;
  const bool aborted = this->phase_ != Phase::IDLE;
  this->stop();
  return aborted;
}

void Supervisor::lock_out_zone(uint8_t zone) {
  if (zone < MAX_ZONES)
    this->s_.zone_lockout_mask |= static_cast<uint8_t>(1u << zone);
}

bool Supervisor::zone_locked_out(uint8_t zone) const {
  if (zone >= MAX_ZONES)
    return true;
  return (this->s_.zone_lockout_mask & (1u << zone)) != 0;
}

void Supervisor::acknowledge() {
  this->s_.critical_latched = false;
  this->s_.diagnostic_used = false;
  this->s_.zone_lockout_mask = 0;
  // The crash-loop brake is cleared too: acknowledging is a human saying "I
  // have looked". Leaving it latched across an acknowledgement would make the
  // device unrecoverable without a reflash.
  this->s_.crash_loop = false;
  this->s_.boot_count = 0;
  this->s_.boot_window_start = 0;
}

bool Supervisor::take_diagnostic_allowance() {
  if (!this->s_.critical_latched)
    return true;  // nothing to spend
  if (this->s_.diagnostic_used)
    return false;
  this->s_.diagnostic_used = true;
  return true;
}

Event Supervisor::tick(Epoch now) {
  if (this->phase_ == Phase::PAUSED && this->paused_at_ > 0 &&
      now - this->paused_at_ >= static_cast<Epoch>(MAX_PAUSE_SECONDS)) {
    // A pause that outlives its timeout becomes a Stop, not a silent resume.
    // Resuming would restart a valve nobody is watching; stopping is the only
    // choice consistent with P0.
    this->stop();
    return Event::PAUSE_TIMEOUT;
  }

  if (this->s_.mode == Mode::HOLD && this->s_.hold_until > 0) {
    if (now >= this->s_.hold_until) {
      this->s_.mode = Mode::NORMAL;
      this->s_.hold_until = 0;
      this->s_.hold_warned = false;
      return Event::HOLD_EXPIRED;
    }
    if (!this->s_.hold_warned && now >= this->s_.hold_until - static_cast<Epoch>(HOLD_WARN_BEFORE_SECONDS)) {
      this->s_.hold_warned = true;
      return Event::HOLD_EXPIRING_SOON;
    }
  }

  return Event::NONE;
}

}  // namespace irrigation_core
