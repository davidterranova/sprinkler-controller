[← Documentation index](../README.md)

# 8. Firmware architecture

**Recommendation: ESPHome on the classic ESP32 with the `esp-idf` framework**, using the stock
`sprinkler` component as the sequencer, three external C++ components, and a pure ESPHome-free core.

**Choose the classic ESP32, not C3/S3:** the classic has **8 PCNT units**; the **ESP32-C3 has no pulse
counter hardware at all [V]**. Two cores also leaves the door open to moving the supervisor into its
own FreeRTOS task.

| Layer | Responsibility | Implementation |
|---|---|---|
| **L0 Hardware** | Water cannot flow when de-energised | N/C solenoids + N/C master in series + deadman-gated rail |
| **L1 Supervisor** | Absolute deadline, invariant enforcement, watchdog, deadman refresh, direct-GPIO hard close | Custom `irrigation_guard` |
| **L2 Sequencer** | One zone at a time, master-valve delays, run timing | **Stock ESPHome `sprinkler`** |
| **L3 Scheduler** | Persisted schedule, due evaluation, catch-up, hold, deviation, alarm latches *(no window/overrun policy — D4)* | Custom `irrigation_scheduler` |
| **L4 Metering** | Pulse capture, K-factor, per-zone attribution, totals, volume termination | Custom `water_meter` |
| **L5 HA** | Config UI, dashboards, statistics, notifications. **Never in the control path.** | ESPHome native API |
| **L6 Core** | Schedule resolver + FSM as pure functions over an injected clock | `components/irrigation_core/` — zero ESPHome includes, host-unit-tested |

## What the stock `sprinkler` component already gives you [V]

Better than expected. Verified from source:

- **`setup()` is literally `all_valves_off_(true); disable_loop();`** — every valve and the master are
  commanded off at boot before anything else runs.
- The per-run deadline is a **polled, rollover-safe absolute `millis()` comparison in
  `SprinklerValveOperator::loop()`**, not a one-shot scheduler callback. A dropped or leaked timer
  cannot extend a run. This is exactly the property FR-3.4 wants.
- Strict sequencing (one `active_req_`, `valve_overlap` defaults to 0), master/pump with four
  independent start/stop delay knobs, `valve_open_delay`, per-zone `enable_switch`, `standby_switch`,
  a 100-entry queue, a `multiplier_number` for seasonal scaling, and a full C++ API to drive it from
  a custom component.
- Requirements satisfied **out of the box**: sequential execution, manual run with auto-stop, pause
  mechanism, master valve, all-closed-at-boot, per-run deadline, persisted per-zone durations.

> **Two v1 additions from D5 and D8**, both cheaper than they look:
> - **Cycle-and-soak** is `repeat_number` — the stock component already runs the whole enabled-zone
>   list N times with each zone getting `duration/N`, and sequential execution supplies the soak
>   interval for free. Multiple **programs** per day still needs the scheduler's own program list.
> - **OTA safety, sharpened.** OTA not only blocks the main loop — it **calls `App.feed_wdt()` inside
  its own blocking loop [V]**, so the task watchdog is *deliberately silenced* while the run supervisor
  is stopped. Neither the software deadline nor the watchdog is enforcing anything during an update.
  **Forbid OTA while any valve is open** (`ota: on_begin:` → shutdown, plus an HA-side gate), and rely
  on the hardware deadman for the rest.
- **Bounded resume + progress %** (FR-3.7a/b/c) needs run progress persisted every ~30–60 s. Note
>   **RTC_SLOW_MEM does not survive a power cut**, so this must go to NVS or FRAM: ~480 writes/day
>   worst case ≈ 4 sector erases/day ≈ 70 years of flash, so acceptable — and free on FRAM.

## What must be built

No time-of-day awareness at all, no weekday / every-N-days logic, no volume, no alarms, no persistence
of an in-progress run, no crash recovery, no plan-vs-actual. **Those gaps are this project.**

Three sharp edges in the stock component to guard against **[V]**:

- `all_valves_off_()` only writes the pin **if the switch's cached state is true** — so if cache and
  reality diverge, "close everything" closes nothing. Mitigate with an unconditional raw-GPIO sweep at
  `on_boot: priority: 800` and a `hard_close_all_()` in the guard that writes pins directly.
- `multiplier_number` max is 10.0 and `run_duration_number` max is 86400 s, so **a 240-hour run is
  expressible within the component's own limits.** Clamp both in YAML *and* against a compile-time
  `MAX_RUN_MS` in the guard.
- Queued valves ignore `enable_switch`, and `multiplier = 0` silently prevents any valve starting.

## Time and scheduling

- **Never use a schedule edge as the source of truth.** ESPHome's `on_time` cron trigger does bounded
  catch-up only within **900 s**, logs "Time has jumped ahead!" and **skips catch-up entirely** beyond
  that, keeps `last_check_` in RAM only (so **no catch-up at all across a reboot**), and ESPHome's own
  docs state it does not reschedule around DST-skipped or duplicated times. **[V]**
- Instead: use `on_time: seconds: 0, minutes: '*'` purely as a **1-minute tick**, and let the scheduler
  decide "is zone N due?" by comparing persisted `last_completed_epoch` against the schedule model.
  Idempotent, survives outages of any length, immune to the 900 s cliff and to DST double-fires.
- **Store UTC in the RTC; never store local time.** Apply `Europe/Paris`
  (`CET-1CEST,M3.5.0,M10.5.0/3`) at the point of use. If the RTC held local time, a DST transition
  becomes a *write* to the RTC and every reboot near the transition must work out whether that write
  already happened — a genuinely nasty class of bug.
- Idempotency key: `(zone, schedule_id, local_date)`.
- **Clock-invalid behaviour:** `is_valid()` in ESPHome means `year >= 2019` **[V]**; before sync the
  clock reads 1970-01-01 and nothing fires. Refuse *scheduled* runs while the clock is untrusted;
  allow *manual* runs, because a duration is a stopwatch, not a clock.
- **DS3231 caveat [V]:** it is not a natively supported ESPHome platform. The `ds1307` platform works
  (registers 0x00–0x06 are identical) but ESPHome tests the DS1307 **CH bit**, which is unused on a
  DS3231, and cannot read the DS3231's **OSF** oscillator-stop flag — so **ESPHome can never detect
  "this RTC lost time"**, and a dead backup cell can yield a plausible-looking wrong time. Either use
  the natively-supported **PCF8563** (which has a voltage-low flag) or keep the DS3231's superior
  oscillator and add your own sanity band + monotonicity check against a persisted last-known-good
  epoch. Also note ESPHome writes 8 bytes from 0x00, so **register 0x07 — Alarm-1 seconds on a
  DS3231 — gets written**; harmless if you never use the alarms.
- Watering at **05:00–06:00 local** is safe on both DST transitions and agronomically better anyway.
  Better still, schedule relative to sunrise — computable offline from lat/long, and immune to the EU
  possibly abolishing seasonal clock changes.

## Persistence and flash wear

- Flash wear is **a non-issue at ESPHome's default 60 s `flash_write_interval`** (~47 k writes/year
  against a ~120 M budget). ⚠️ **Correction 2026-09-05:** the claim that *"Arduino's 5-sector NVS is
  20× weaker than esp-idf's"* is **wrong and now obsolete** — ESPHome allocates **112 sectors under
  esp-idf and 96 under Arduino**, changed after 2025.12 **[V, read from `esp32/__init__.py`]**. The
  wear argument for `esp-idf` is dead; the framework choice still stands on **binary size** (≥135 KB
  smaller for an identical config) and peripheral access.
- **The trap is specific and quantifiable: never write the total per turbine pulse.** At 476 pulses/L
  and 500 L/day that is 238 k writes/day ⇒ **flash destroyed in ~52 days [V]**. Per-litre writes are
  fine (~68 years).
- Four-tier counter durability: RAM working total → **`RTC_SLOW_MEM` staging** (survives software and
  watchdog resets at zero flash cost, though not power loss) → non-volatile store on meaningful events
  (every 10 L, zone start/end, mains-fail ISR) → **FRAM (MB85RC256V, ~10¹² cycles, ~€7)** if you want
  to stop thinking about wear forever. The AT24C32 already bundled on ZS-042 DS3231 modules is a good
  free alternative.
- Own the schedule record yourself: **fixed-size binary struct, schema-versioned, CRC32, dual-bank
  atomic write.** ESPHome's preference layer calls `nvs_flash_erase()` on open failure — i.e. NVS
  corruption silently wipes every preference. **[V]**
- Add a **CI grep gate** forbidding a `save()` call in the pulse path.

## Crash recovery

The boot order is: raw GPIO sweep + deadman off at priority 800 → guard `setup()` with
`guard_armed_ = false`, read `esp_reset_reason()` and the persisted run record → metering restores its
total and re-seeds the counter *before* counting resumes → scheduler evaluates due-ness.

**Do not auto-resume an interrupted run.** It is the "obvious" design and it turns a single fault into
a repeated one — and in a crash loop, into an unbounded one. Abandon, log, re-evaluate, and bound
everything with a per-day per-zone volume budget so even a resume bug cannot flood. A **crash-loop
brake** at 2 and 3 occurrences is the backstop.

## Is the ESP32-WROOM-32 big enough? — yes, with 28–43 % flash headroom

Verified against ESPHome's own sources at tag **2026.7.2** (the version already running on the pool
controller), not estimated.

| Resource | Budget | This project | Headroom |
|---|---|---|---|
| **Flash (app partition)** | **1 835 008 B = 1.75 MB** — ESPHome computes `(4 MiB − 0x80000) / 2` in `esp32/__init__.py`, two app slots for OTA **[V]** | **≈1.02–1.29 MB [E]** | **28–43 %** |
| **RAM (entities)** | **200–240 KB** free heap measured on real small ESP32 configs; 262–293 KB on the maintainers' 148-entity reference device **[V, community + PR data]** | **≈13–32 KB** for ~130 entities **[E]** | **~7–15×** |
| **API connections** | `max_connections` = 5 on esp32; ESPHome's own comment says *"~500–1000 bytes per connection"* **[V]** | HA + a logs session | ample |
| **Reconnect state dump** | 32 KiB cap | 260 messages, batched ≤34 ⇒ **~6.8 KB peak, ~130 ms** | ample |
| **CPU** | 2 × 240 MHz | 1-minute tick, hardware PCNT, hardware I²C | not close to bound |

`sizeof(EntityBase)` is **≈20 bytes** — names live in flash via `StringRef`, flags are bit-packed, and
device class / unit / icon are `uint8_t` table indices **[V]**. ⚠️ **Corrected:** an earlier draft
quoted ~11 KB for 130 entities by counting only that mixin. The *instantiated* object also carries its
`Component`/`PollingComponent` base and any lambdas, so a realistic live entity is **~100–250 B [E]** —
call it **13–32 KB** for this design. Still 7–15× headroom, but the earlier figure was too flattering.

### ⚠️ The real entity-count constraint is not RAM — it is the ListEntities dump

This is the one genuine caution in the whole capacity picture, and it lands close to this project's
~130 entities.

**What actually fails at high entity counts** is the initial enumeration, not memory:

- ESPHome PR **#18577** (merged 2026-08-23), verbatim: *"`ListEntitiesIterator::on_service` could spin
  while the TCP buffer was full, **starving the main loop and triggering the task watchdog**… field
  crash reports sample the loop task exactly here when the task watchdog fires."* **[V]**
- PR **#9790**: *"Users with high entity counts have reported connection issues where not all entities
  are visible… particularly beneficial for devices with **50+ entities**"* — and, after the fix,
  *"even with 300 entities reconnects are almost instant now."* **[V]**
- **home-assistant/core#145293**: ~250 entities on one ESP32, and **only 120–140 became visible in HA**
  after connect. Worked around with `update_interval: never` plus staggered polling. **[V, from the
  issue body]**
- **esphome/issues#3020**: an ESP32 with only binary sensors and switches that *"keeps disconnecting
  from Home Assistant if too many entities"*, with HA logging `Timeout waiting for response for
  ListEntitiesRequest` — and static RAM at link time was only **13 %**, so it was **not** a RAM
  ceiling. A comment reports a sharp cliff — fine ≤74 entities, flaky 75–77, refuses to connect at 78+
  **[reported, not verified verbatim]**.

**Against that, the positive existence proof:** the ESPHome maintainers' own "large config, many
entities" benchmark device (**Apollo R PRO-1, ~148 entities, ESP32-S3, Ethernet**) runs comfortably
with **262–293 KB free heap [V]**. So ~130 entities is demonstrably fine — *on current firmware*.

**Three consequences for this project:**

1. **Run ESPHome 2026.9 or later.** That release carries the ListEntities loop-stall fix (#18577) and
   makes an API out-of-memory condition **drop the connection instead of rebooting the device**
   (#18802/#18803). The pool controller currently runs **2026.7.2**, which predates both. This is the
   single highest-value version constraint in the project.
2. **Treat the entity count as a budget, not a free variable.** The HA design already proposes a
   `recorder: exclude:` list for the purely-echoing config entities; the same reasoning says to think
   twice before adding the 131st entity. If the initial dump ever misbehaves, the documented workaround
   is `update_interval: never` plus staggered publishing from a lambda.
3. **On ESP-IDF a failed `new` calls `abort()` and reboots [V]** — so a memory problem presents as a
   spontaneous reset, not a logged error. That is precisely the crash-loop case the deadman exists for.

**Watch-thresholds — the measurable triggers for "you have outgrown this board":**

| Metric | Threshold | Action |
|---|---|---|
| Binary size | **≥1 550 KB (86 %)** | Move to **WROOM-32E-N16** (16 MB, ~$6) — a one-line `flash_size: 16MB` change |
| `debug.min_free` heap | < 50 KB | Investigate before adding features. **Requires ESPHome ≥ 2026.2.0** — the sensor does not exist before that. |
| **`debug.block` (largest free block)** | **< 20 KB** | **Alarm on this, not on `free`.** Every documented real failure is a *contiguity* failure at a size specific to the config's largest single allocation — never a total-free-bytes failure. ESPHome's own guidance: *"failures happen when the largest contiguous block shrinks, even if total free heap is still large. We have seen field crashes caused by this."* **[V]** |
| `min_free` trend | declining > 10 % / 30 days | **NFR-R8 breach** — a leak |
| `loop_time` | > 200 ms | Something is blocking the supervisory loop |

**Reject WROVER** if you ever swap boards: its PSRAM occupies **GPIO16/17**, and **GPIO17 is the
master valve** in the pin map. Cheap ways to buy headroom first, in order: disable `web_server` in
production (~150 KB **[E]**, and NFR-SEC4 already wants it off), and keep the `esp-idf` framework
(≥135 KB smaller than Arduino for an identical config **[V]**).

## Testing

- **Extract the pure logic** into `components/irrigation_core/` — the single highest-leverage architectural
  decision here. Unit-test the resolver and FSM against a table covering DST spring-forward and
  fall-back, month/year boundaries, leap day, a 31-day month, interval-vs-weekday interaction, window
  overflow, and rotation.
- **A simulation/dry-run mode with a time-acceleration factor** so a season runs in minutes. This one
  feature will save more time than it costs, several times over.
- **CI** (fits your branch-per-change, green-before-PR workflow): host unit tests, `esphome config`
  validation for all three YAML variants, a compile check, the purity gate (no ESPHome includes in
  `lib/`), and the flash-write grep gate.
- Hardware-in-the-loop, on the bench with LEDs: reboot 20× mid-run, hold the board in reset,
  deliberately hang a task, pull the AP's power mid-cycle, unplug the device to prove the HA
  availability watchdog fires.

## The two settings that silently destroy autonomy [V]

```yaml
api:
  encryption: { key: !secret arrosage_api_key }
  reboot_timeout: 0s      # default 15min — see below
wifi:
  reboot_timeout: 0s      # default 15min
  ap: {}                  # fallback hotspot so the node stays configurable
```

Both default to **15 minutes**. Out of the box, the controller **reboots every quarter hour for as long
as HA or the access point is down**, and every reboot lands in IDLE with all valves closed — so an
afternoon's HA outage silently ends the season's watering. Verified in `api_server.cpp` and
`api/__init__.py`, and documented on esphome.io.

Reassuringly, a nested verification pass confirmed **nothing else in ESPHome is gated on API
connectivity** — `application.cpp` has zero references to `api`, and `APIServer` has no
`can_proceed()` override — so switches, intervals, scripts and `on_time` all keep running with no
client connected. The reboot timer is the *only* consequence, and `0s` removes it. **[V]**

---
