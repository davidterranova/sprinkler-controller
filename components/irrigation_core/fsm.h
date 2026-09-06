#pragma once
//
//  The supervisor: modes, the three verbs, latches and lockouts.
//
//  Hold / Pause / Stop are three DIFFERENT things (FR-8.1), and the single
//  most common way irrigation UIs go wrong is collapsing them into one
//  "pause" button whose meaning nobody can state:
//
//    Hold  -- let the current cycle finish; schedule nothing new. Has an
//             expiry, always. Does NOT block manual runs.
//    Pause -- suspend the run in progress; resumable; bounded by a
//             max-suspend timeout so a forgotten pause cannot become a
//             permanently half-watered garden.
//    Stop  -- close everything now, clear the queue, no resume.
//
//  Winter is the fourth, deliberately separate: it blocks manual runs too,
//  because the pipes are drained and there is nothing safe to do.
//
#include "schedule.h"
#include "types.h"

namespace irrigation_core {

// FR-8.4 / D9. Even an "indefinite" hold expires here and auto-resumes with a
// warning 24 h ahead. The forgotten hold is how gardens die, and its failure
// mode is silent -- so the design refuses to offer a truly unbounded one.
// Winter mode is the unbounded option, and it is named for what it is.
inline constexpr uint32_t MAX_HOLD_SECONDS = 7u * 24 * 3600;
inline constexpr uint32_t HOLD_WARN_BEFORE_SECONDS = 24u * 3600;

// FR-8.1. A pause is a suspension, not a state to live in.
inline constexpr uint32_t MAX_PAUSE_SECONDS = 30u * 60;

// FR-3.8. N resets within M minutes and the device stops irrigating and only
// advertises the fault (fault-policy row 14).
inline constexpr uint8_t CRASH_LOOP_THRESHOLD = 3;
inline constexpr uint32_t CRASH_LOOP_WINDOW_SECONDS = 10u * 60;

inline constexpr uint8_t ATTRIBUTION_LEN = 24;

enum class Mode : uint8_t { NORMAL = 0, HOLD = 1, WINTER = 2 };
enum class Phase : uint8_t { IDLE = 0, RUNNING = 1, PAUSED = 2 };
enum class Source : uint8_t { SCHEDULE = 0, CATCH_UP = 1, MANUAL = 2, TEST = 3 };

enum class Refusal : uint8_t {
  NONE = 0,
  WINTER,             // everything is blocked, including manual
  HELD,               // scheduled runs only
  LOCKED_OUT,         // a CRITICAL alarm is latched
  ZONE_LOCKED_OUT,    // this specific zone is latched out
  CLOCK_UNTRUSTED,    // scheduled runs only -- a duration is a stopwatch
  ALREADY_RUNNING,    // FR-4.6a: skip, never queue
  ZONE_DISABLED,
  INVALID_ZONE,
  DURATION_TOO_LONG,
  CRASH_LOOP,
};

// Emitted by tick(). The caller turns these into alarms and HA notifications --
// the core never talks to anything.
enum class Event : uint8_t {
  NONE = 0,
  HOLD_EXPIRING_SOON,
  HOLD_EXPIRED,
  PAUSE_TIMEOUT,
};

const char *to_string(Mode m);
const char *to_string(Phase p);
const char *to_string(Refusal r);
const char *to_string(Source s);

// The half of supervisor state that must survive a reboot (FR-1.2, FR-8.6).
// Deliberately a POD: it is written to NVS byte-for-byte with a CRC around it.
struct SupervisorState {
  Mode mode{Mode::NORMAL};
  Epoch hold_until{0};
  bool hold_warned{false};
  char held_by[ATTRIBUTION_LEN]{};  // FR-8.7 -- so the digest can say who to ask

  bool critical_latched{false};
  bool diagnostic_used{false};  // FR-9.3: exactly one bounded run to test a repair
  uint8_t zone_lockout_mask{0};

  uint8_t boot_count{0};
  Epoch boot_window_start{0};
  bool crash_loop{false};
};

class Supervisor {
 public:
  // FR-8.6: with nothing persisted the device boots NOT held. Wrongly running
  // costs some water and is loudly visible; wrongly held kills a garden
  // silently. The asymmetry decides the default.
  void restore(const SupervisorState &s) { this->s_ = s; }
  const SupervisorState &state() const { return this->s_; }
  SupervisorState &mutable_state() { return this->s_; }

  Mode mode() const { return this->s_.mode; }
  Phase phase() const { return this->phase_; }
  Source source() const { return this->source_; }
  bool locked_out() const { return this->s_.critical_latched || this->s_.crash_loop; }

  // ---- boot ------------------------------------------------------------
  // Call once, early, with the persisted state already restored.
  void note_boot(Epoch now);

  // ---- the three verbs -------------------------------------------------
  // duration_seconds == 0 means "as long as allowed", which is MAX_HOLD_SECONDS
  // and not forever. See the header comment.
  void hold(Epoch now, uint32_t duration_seconds, const char *by);
  void release_hold(Epoch now);
  bool pause(Epoch now);   // false if there was nothing running to pause
  bool resume(Epoch now);  // false if nothing was paused
  void stop();

  // Returns true if a run was in progress and has just been aborted, so the
  // caller knows it must close valves. Winter is not merely a refusal to start.
  bool set_winter(bool on);

  // ---- run lifecycle ---------------------------------------------------
  Refusal can_start(Source src, uint8_t zone, uint16_t minutes, bool clock_trusted, const Schedule &sched) const;
  void started(Source src, uint8_t zone);
  void finished();

  // ---- alarms ----------------------------------------------------------
  // Latching a CRITICAL is a Stop, not just a refusal to start again: every
  // CRITICAL row in docs/03-fault-policy.md says "abort" or "force closed".
  // Returns true if a run was aborted, so the caller closes valves and logs the
  // run as terminated by the alarm rather than as completed.
  bool latch_critical();
  void lock_out_zone(uint8_t zone);
  // FR-9.3: clearing requires an explicit, attributed acknowledgement. The
  // attribution is the caller's to log; the core just refuses to self-clear.
  void acknowledge();
  bool zone_locked_out(uint8_t zone) const;

  // Consumes the one diagnostic run allowance. Returns false if already used.
  bool take_diagnostic_allowance();

  // ---- time -------------------------------------------------------------
  // Call once a minute (and after any state change). Returns at most one event
  // per call; call again if you want to drain them.
  Event tick(Epoch now);

  Epoch hold_until() const { return this->s_.hold_until; }
  uint8_t active_zone() const { return this->active_zone_; }

 private:
  SupervisorState s_{};

  // Runtime-only. A valve state is NEVER restored from persistence (FR-3.2):
  // the device always boots IDLE with everything closed.
  Phase phase_{Phase::IDLE};
  Source source_{Source::SCHEDULE};
  uint8_t active_zone_{0};
  Epoch paused_at_{0};
};

}  // namespace irrigation_core
