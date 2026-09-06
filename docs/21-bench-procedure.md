[← Documentation index](../README.md)

# 21. v0 bench procedure — the exit criteria, as a checklist

The [roadmap](14-roadmap.md) sets three exit criteria for v0:

> Every row of [§5](03-fault-policy.md) has a demonstrated, recorded behaviour; unit tests green
> including DST; a simulated season runs unattended.

Two of those are automated and take about a second (`make test`). The third is this document. Nothing
here needs water, a valve, or the relay board's output side — LEDs and a serial log are enough for
every row except the two marked ⚡, which need the deadman circuit built.

**Eight zones are wired** (GPIO 32, 33, 25, 26, 27, 23, 19, 18) even though four are planted to start
with. Test all eight: an unplanted zone still has a relay, and a relay that has never been commanded
is a relay whose failure mode is unknown.

> **Record the result of each row in this file as you go**, with the date and what you actually saw.
> A checklist with no evidence in it is a wish list. That evidence is also the commissioning record
> NFR-D4 asks for.

## Before anything else — the three go/no-go board tests

These gate everything below, and **T3 can disqualify the board outright.**

| ID | Test | Pass condition | Result |
|---|---|---|---|
| **T1** (E1) | DMM continuity across the relay COM terminals | **COM↔COM open.** Each channel has a 3-way NO/COM/NC terminal, not 2-way. If the COMs are bussed the board cannot switch 24 VAC and is unusable. | ☐ |
| **T3** (E2) | Power the board with **nothing** connected to the IN pins, and listen | **Silence.** Any relay that pulls in on a floating input means the board is high-trigger or has a pull-down, and `relay_active_low` in `sprinkler.yaml` is wrong — as is the assumption underneath the whole boot-safety argument. | ☐ |
| **T4** (E3) | Drive the opto from 3.3 V, sweep the logic rail down, then repeat with the board in a freezer bag | Relays pull in reliably **cold**. Optocoupler CTR falls at both low current and low temperature, so a board that works on a warm September bench can fail on a February morning. If marginal, change R1 to **470 Ω** — *not* 220 Ω. | ☐ |
| **E5** | Read the part number off the relay can | SRD-05VDC-SL-**C** (71.4 mA) or **-D** (89.3 mA). Sets the 5 V supply budget. | ☐ |

## Fault-policy rows

Row numbers are [§5](03-fault-policy.md)'s. "Expected" restates what that table promises; the point of
the test is to make the promise observable.

### Rows 1–3 — network faults are not water faults

| Row | Test | Expected | Result |
|---|---|---|---|
| 1 | Start a cycle, then **pull the access point's power** | The cycle **runs to completion.** No reboot. `make logs-usb` keeps showing zone transitions. | ☐ |
| 2 | Stop Home Assistant while a cycle runs | Cycle continues; the device keeps sequencing with no API client connected. | ☐ |
| 2 | Leave HA down across a scheduled start | The next cycle **starts normally.** This is the one that proves autonomy rather than remote control. | ☐ |
| 3 | Leave HA down >24 h (or set the clock forward) | Still watering. A WARNING is the only consequence. | ☐ |

> **Why this is the first block.** `reboot_timeout: 0s` on both `api:` and `wifi:` is the single
> highest-consequence line in the config, and its default of 15 min would make all four rows fail
> silently — the device would reboot every quarter hour and land in IDLE with everything closed.

### Rows 4–5 — reboots and power cuts

| Row | Test | Expected | Result |
|---|---|---|---|
| 4 | **Reboot 20× mid-run** (the Restart button, or the board's EN button) | Every time: outputs de-energise, the device boots IDLE, and the log carries `run,…,interrupted`. **Never auto-resumes.** The remaining zones are picked up only if the tick still falls inside the catch-up window. | ☐ |
| 4 | Check the record after one such reboot | `Last run` reads `… interrupted`. This comes from the persisted `ActiveRun` breadcrumb, so it survives a power cut, not just a soft reset. | ☐ |
| 5 | Pull the USB power mid-run, wait 10 s, reconnect | As row 4. Note that no water flowed either — the supply was off. | ☐ |
| — | Reboot 3× inside 10 min | **Crash-loop brake engages.** `Locked out` goes true, `State` reads `crash-loop brake`, and nothing will run until *Acknowledge alarms* is pressed. | ☐ |

### Row 6 — the clock

| Row | Test | Expected | Result |
|---|---|---|---|
| 6 | Boot with no Wi-Fi (or block NTP) and watch before any sync | `Clock trusted` is false, `State` reads `clock not trusted`, **no scheduled run fires**. | ☐ |
| 6 | With the clock still untrusted, press **Start manual run** | **It runs.** A duration is a stopwatch, not a clock — this is the asymmetry the row exists to state. | ☐ |
| 6 ⚑ | Fit the DS3231, set the time, **pull all power for 24 h**, reboot with no network | Time and date correct, DST correct. **This is the row the RTC exists for**, and until it passes, FR-1.3 is unmet — see the caveats in `packages/rtc-ds3231.yaml`. | ☐ |

### Rows 7–9 — flow

**Out of scope for v0 by design.** There is no flow meter on the bench, so rows 7, 8 and 9 cannot be
demonstrated and are deferred to v1 with the meter. What v0 *can* prove is the machinery those rows
depend on — that a latched CRITICAL closes everything and locks out irrigation — which rows 10 and 11
exercise below. Note that **row 8 is the only failure in the whole table that causes unbounded
damage**, and it is the reason the flow meter and the master valve are both v1-mandatory.

### Rows 10–11 — the guard ⚡

| Row | Test | Expected | Result |
|---|---|---|---|
| 10 | Set `max_run_duration` to 30 s in `packages/safety.yaml`, rebuild, start a 5-minute manual run | At 30 s the guard **forces everything closed**, logs `OVER-RUN`, latches a CRITICAL, and `Locked out` goes true. Restore the real value afterwards. | ☐ |
| 10 | Press **Start manual run** while locked out | Allowed **exactly once** (the diagnostic run, FR-9.3), refused thereafter until acknowledged. | ☐ |
| 10 | Reboot while locked out | **Still locked out.** A latch that a power cycle clears is not a latch. | ☐ |
| 11 | — | Coil current sensing is v1 hardware. Deferred. | — |

### Row 14 — the deadman ⚡ (verification E6)

**This is the single most important circuit validation in the project**, and it is the one that the
original design would have failed.

| Test | Expected | Result |
|---|---|---|
| Press **BENCH starve the deadman (3 s)** | The heartbeat pin stops toggling within ~400 ms (scope it), `Deadman alive` goes false, and the guard hard-closes the outputs. The main loop is still running throughout — this proves the *counter-staleness* half. | ☐ |
| Press **BENCH hang the main loop (E6)** — with a scope on the deadman pin and a meter on the valve rail | The heartbeat stops, the monostable times out, and **the rail de-energises in ~1 s.** The task watchdog then resets the board. | ☐ |
| Measure the decay time from last edge to rail drop | ~1 s. | ☐ |
| Hold the board in reset (EN to GND) | Rail de-energised, all outputs high-impedance. | ☐ |

> ⚠️ **Do not substitute "pull the power" for the hang test.** Every design passes that one, including
> the LEDC + charge-pump design that provably would *not* have protected anything. The distinguishing
> case is a firmware that has stopped while the hardware keeps running, and the only way to see it is
> to stop the firmware on purpose.

### Row 15 — corrupt config

| Test | Expected | Result |
|---|---|---|
| Configure a schedule, Apply, then run `esptool erase_region` over the NVS partition (or flash a build with a bumped `SCHEMA_VERSION`) | The device boots, **refuses the record rather than guessing**, logs the fallback, and comes up on `default_schedule` with every program disabled. It does not water on a schedule it cannot read. | ☐ |
| Apply an invalid schedule — e.g. set **P1 start hour** to 2 | Apply is **refused whole**: `start time falls in 01:30-03:30, which DST deletes or repeats`. The previously committed schedule is untouched, and `Config pending` stays true. | ☐ |

## Scheduler and verb behaviour

| Test | Expected | Result |
|---|---|---|
| Set **Simulation speed** to 60, configure P1 for the next minute, watch a full cycle | Zones sequence in plan order with the 10 s settle gap between them; the master leads by 2 s and lags by 2 s. A 20-minute zone runs for 20 s. | ☐ |
| Watch `Current plan` before the cycle starts | The resolved plan is published **before** execution (FR-4.4). | ☐ |
| Press **Hold 24 h**, then wait for a scheduled start | `skipped_held` in the log; nothing runs. `Hold status` names who held it. | ☐ |
| While held, press **Start manual run** | **It runs.** Hold does not block manual (FR-8.5). | ☐ |
| Press **Pause run** mid-cycle, then **Resume run** | The same zone continues where it left off. | ☐ |
| Pause and leave it for 30 min | **Auto-stops**, does not auto-resume. `PAUSE_TIMEOUT` in the log. | ☐ |
| Turn on **Winter mode** mid-run | Everything closes immediately, and a manual run is now refused too. | ☐ |
| Run zone 1 manually for ≥80 % of its planned daily minutes, then wait for the scheduled start | Zone 1 is recorded `satisfied_by_manual` and **is not watered again**; the other zones run normally (FR-10.7 / D10, scoped per zone). | ☐ |
| Set **P1 every N days** to 3, run once, then wait three days (or move the clock) | Runs on day 0 and day 3, not on days 1 and 2, and the interval is measured from the **actual** run. | ☐ |
| Leave the device off across a scheduled start, boot it 20 min later | Runs as `catch_up`. Boot it 90 min later instead: it does **not** run. | ☐ |
| Let a zone accumulate 3 h of run time in one day | Further runs for that zone are refused `skipped_budget`; other zones are unaffected; it resets at local midnight. | ☐ |
| Press **Test all zones** | **All eight** run for 2 min each, in order, regardless of the enable switches. This is also the commissioning check that every relay channel and every LED actually works. | ☐ |
| Set **P1 zone 5-8 minutes** to 0, leave zones 1-4 non-zero, and watch a cycle | Only zones 1-4 appear in `Current plan` and only they run — a zone that is wired but has no duration is left out of the plan entirely. This is the whole basis for declaring eight zones while planting four. | ☐ |
| Give zone 5 a duration and Apply | It joins the plan on the next cycle, with no firmware change. | ☐ |
| Watch which zone is first across eight consecutive days | **It rotates.** All eight zones take the first slot once (FR-4.5, "so the same zone is not always last"). Set a zone's priority lower to pin it first and confirm rotation no longer moves it. | ☐ |

## Home Assistant integration

| Test | Expected | Result |
|---|---|---|
| Unplug the device | `binary_sensor.…_status` goes unavailable. **Build the HA-side availability automation now** (FR-9.4) — a dead controller cannot raise its own alarm, and this is the only alarm that survives total silence. | ☐ |
| Change a duration in HA without pressing Apply | `Config pending` true; the running schedule is unchanged. | ☐ |
| Press **Apply**, then reboot | The new schedule survives, restored from the dual-bank record with its sequence number in the log. | ☐ |
| Count the entities in HA | **91.** The budget is ~130, and the binding constraint is the ListEntities dump rather than RAM — this build runs ESPHome 2026.8.2, which predates the loop-stall fix in #18577, so watch for entities silently missing after a reconnect. | ☐ |
| Look for a `switch.…_zone_1` entity in HA | **There isn't one, and there must not be.** The sequencer's per-valve switches are internal: turning one on calls `start_single_valve()` directly, bypassing hold, winter, lockout and the daily budget. FR-10.1 says a manual run is "run zone N for M minutes", never "open zone N". | ☐ |

## What v0 deliberately does not cover

Stated so the gaps are visible rather than assumed closed:

- **Rows 7, 8, 9 and 11** — need the flow meter and coil current sensing. v1.
- **Water volumes of any kind.** v0 measures run *time* only; the per-zone budget is in seconds.
- **The hardware max-runtime backstop** (timer relay + Set/Reset bistable on the zone-side common).
  It is a physical part, and the settle-gap reset (FR-4.3) must be measured on it before any valve is
  connected.
- **The rain gauge, freeze interlock and restriction window.** v1.
- **`satisfied_by_manual` credited against a *volume*** rather than a duration. v1, with the meter.

---
