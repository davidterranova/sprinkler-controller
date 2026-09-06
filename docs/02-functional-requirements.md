[← Documentation index](../README.md)

# 4. Functional requirements

Your ten original requirements, refined into testable statements. Traceability to your wording is in
the *Origin* column.

## FR-1 — Autonomy

| ID | Requirement | Origin |
|---|---|---|
| FR-1.1 | The controller **shall execute its full schedule with no Wi-Fi, no Home Assistant, and no internet, indefinitely.** | R1 |
| FR-1.2 | The schedule, pause state, calibration, cumulative volumes and alarm latches **shall be persisted on the device** and restored on boot. | R1 |
| FR-1.3 | Wall-clock time **shall come from a battery-backed RTC**, with SNTP as an opportunistic correction only. A power cut of any length must not lose the date. | R1 |
| FR-1.4 | An SNTP result deviating from the RTC by more than a configurable band **shall be rejected**, and shall never trigger a run by itself. | new |
| FR-1.5 | The controller **shall survive ≥30 consecutive days offline** while continuing to execute the last committed schedule. | R1 |

> **Autonomy is not free and it is easy to fake.** An HA automation calling `valve.open_valve` is not
> autonomy, it is a remote control. FR-1.1 is the requirement that forces the whole on-device
> architecture; if you decide you do not want it, [§14 D3](13-decisions.md) says
> how much simpler v1 becomes.

## FR-2 — Home Assistant as the rule authority

| ID | Requirement | Origin |
|---|---|---|
| FR-2.1 | All schedule rules **shall be authored and reviewed in Home Assistant**, and **owned and executed by the ESP32**. HA is the editing authority; the device is the execution authority. | R2 |
| FR-2.2 | A configuration edit **shall be atomic**: staged in RAM, then committed to non-volatile storage by an explicit *Apply* action. A partially-applied schedule must never exist. | new |
| FR-2.3 | An "edited but not applied" indicator **shall be exposed** to HA, mirroring your existing `binary_sensor.piscine_pool_controller_filtration_config_pending`. **[V]** | new |
| FR-2.4 | The device **shall boot with a valid schedule** even if HA has never connected (factory defaults, or the last committed config). | new |
| FR-2.5 | Every HA-supplied *modifier* (rain hint, seasonal %, forecast) **shall carry an age**, and the device **shall ignore it once stale**, reverting to the permissive default. | P6 |
| FR-2.5a | **Rain-skip shall be autonomous (D11).** A tipping-bucket rain gauge wired to the device is the **primary** rain input; the HA-pushed hint of FR-2.5 is demoted to a **fallback** used only when the gauge is absent or faulty. *This removes the one structural HA dependency the integration analysis identified.* | **D11** |
| FR-2.6 | The device **shall not depend on calling HA services.** Your ESPHome config entry has `allow_service_calls: false`. **[V]** | new |

## FR-3 — Fail closed

| ID | Requirement | Origin |
|---|---|---|
| FR-3.1 | **Hardware:** all zone valves and the master valve **shall be normally closed**, such that de-energising closes them with no stored energy, spring, or capacitor. Latching/bistable solenoids and position-holding motorised ball valves are **prohibited** on zones. | R3 |
| FR-3.1a | **Valves shall be commodity low-voltage irrigation solenoids in the 24–48 V band, driven by relays.** Specifically **24 VAC normally-closed diaphragm solenoids** (Hunter PGV, Rain Bird DV, Toro and equivalents), switched by **mechanical relay contacts**. *Already satisfied by the existing design — see the note below.* | **owner, 2026-09-05** |
| FR-3.2 | **Boot:** all valve outputs **shall be de-energised before any other component initialises**, and no valve state shall ever be restored from persistence. | R3 |
| FR-3.3 | **Crash:** valves **shall de-energise within ~1 s of the application ceasing to run**, including panic, task-watchdog reset, boot loop, and ESPHome safe mode. This **requires a hardware deadman** — see [§16 R1](15-risk-register.md). | R3 |
| FR-3.4 | **Absolute deadline:** every run **shall have an absolute maximum duration enforced in a code path independent of the scheduler**, plus a hardware max-runtime backstop. | P4 |
| FR-3.5 | **Interlock:** at most one zone drive signal shall be active at a time, enforced in firmware (the `sprinkler` component holds a single `active_req_`) and **bounded hydraulically by the master valve in series**. *Revised by D2: the 74HC238 topological interlock was dropped with the custom PCB — with a master valve, two zones opening at once is a pressure problem, not a safety one.* | R4, revised D2 |
| FR-3.6 | **Network faults are not water faults.** Loss of Wi-Fi, loss of HA, or loss of internet **shall not stop a running cycle and shall not suspend the schedule.** See [§5](03-fault-policy.md). | **challenges R3** |
| FR-3.7 | A run interrupted by reboot or power loss **shall be logged with the volume actually delivered**, and an alert raised. *Revised by D8 — the earlier "abandon, never resume" is replaced by bounded resume, below.* | new, revised D8 |
| FR-3.7a | **Bounded resume (D8).** The remainder of an interrupted run **may** be resumed, but **only if all of:** the outage was shorter than a configurable threshold (default 15 min); the clock is trusted; the crash-loop brake is not engaged; and the per-day per-zone volume budget is not exceeded. Otherwise the run is **abandoned**. *The budget, not the run counter, is the real stop condition — that is what makes a resume bug survivable.* | **D8** |
| FR-3.7b | **Progress shall be persisted during a run** (~30–60 s cadence) so that elapsed time, delivered litres and a **completion percentage** survive a power cut and are reportable. Written to NVS/FRAM — **not** RTC_SLOW_MEM, which does not survive loss of power. | **D8** |
| FR-3.7c | Where resume is ambiguous, HA **shall raise an actionable notification** (*resume / skip / restart*) reporting the progress percentage. **If unanswered within a timeout, the safe default applies: abandon.** A run shall never wait indefinitely on a human. | **D8** |
| FR-3.8 | A **crash-loop brake** shall exist: N resets within M minutes puts the device in a safe mode that closes everything and only advertises the fault. | new |

## FR-4 — Sequential execution

| ID | Requirement | Origin |
|---|---|---|
| FR-4.1 | At most **one zone valve shall be energised at any instant**, enforced in firmware *and* by hardware topology. | R4 |
| FR-4.2 | The constraint **shall be expressed as a hydraulic budget** (per-zone L/min plus a `max_concurrent_zones` cap **fixed at 1**), not as a hard-coded "always one". *D1 settles this: a DN25 mains feed already running multiple sprinklers per zone has no spare budget for a second zone, so the cap stays at 1 — but keeping it as a budget rather than a constant means the flow data is collected and the constraint is explicit and testable.* | **challenges R4**, revised D1 |
| FR-4.3 | A **master valve shall sequence with configurable lead/lag delays**, and a settle delay shall separate zones so zone N is fully closed before N+1 opens. **The settle delay is also a safety requirement**: it is what resets the hardware max-runtime backstop between zones, so it must comfortably exceed that timer's reset time (see [hardware](05-hardware.md#where-the-max-runtime-backstop-is-wired--and-why-it-matters)). | new |
| FR-4.4 | The **resolved plan for a cycle shall be published to HA before the cycle executes**, so "scheduled vs actual" has something to compare against. | **challenges R7** |
| FR-4.5 | Zone **ordering shall support priority and rotation**, so the same zone is not always last. | new |
| ~~FR-4.6~~ | ~~An explicit **overrun policy** (`TRUNCATE` / `SPILL` / `SKIP`) for when the queue cannot fit the window.~~ **Withdrawn by D4 — there is no window to overrun.** The operator sets start times and durations; the cycle runs to completion. | ~~challenges R4~~, **withdrawn D4** |
| FR-4.6a | **Cycle re-entrancy:** if a cycle is still running when the next scheduled start fires, the new start **shall be skipped, not queued**, and recorded as `skipped_still_running`. *Replaces FR-4.6. Without a window this is the only remaining collision case, and skipping is right: queueing would let a slow cycle cascade into permanent back-to-back watering.* | **new, D4** |
| FR-4.7 | **A cycle may legitimately run for several hours, and is not bounded.** No component may assume a bounded total cycle duration — in particular the max-runtime backstop must bound a *single zone run*, not the cycle (D4). | new |

> ℹ️ **Cycle length is unbounded, by decision (D4).**
>
> Because zones are strictly sequential, one full cycle costs the **sum** of all zone runs (plus ~5–10 s
> of settle time per zone — ~2 min total, negligible). Depending on head type that is plausibly
> **~1 h 15** (pop-up sprays, ~35–50 mm/h) to **~4 h 30** (gear-drive rotors, ~10–12 mm/h) for eight
> zones at 6 mm each *[E — confirm by measurement: run a zone 15 min with straight-sided tins in the
> spray and read the depth]*.
>
> **The operator owns this.** They set start times and durations; the cycle runs to completion. There
> is no window, no latest-finish, and no cap. An earlier draft asserted "8 × 20 min = 2 h 40 does not
> fit a 05:00–07:00 window" — **both** the 20 min/zone and the two-hour window were analysis
> assumptions, and neither survived contact with the owner.
>
> **What this removes:** the overrun policy, truncation, `min_useful_duration`, spill grace periods,
> and the whole `latest_finish` half of the schedule model. That is the most underspecified corner of
> the original requirement 5, deleted rather than solved.
>
> **What it does not remove — and cannot, because P0 does not delegate:**
> - the **per-run hard cap** (FR-3.4, FR-10.3) and the hardware max-runtime backstop. These are
>   *safety* limits on a single zone, not scheduling windows.
> - **cycle re-entrancy** (FR-4.6a) — the one collision case that survives having no window.
> - the optional **drought-restriction window** (NFR-LEG4), which is a legal constraint rather than a
>   preference. Ships **disabled by default**; enabling it is the operator's call.

> ### Note on FR-3.1a — this requirement was already met, and here is why the choice is right
>
> **24 VAC normally-closed diaphragm solenoids are the most commodity component in the entire
> project.** They have been the irrigation industry standard for 40+ years, cost **€19.80 each [V]**,
> are stocked by every garden centre and irrigation supplier in France, and every brand is
> mechanically and electrically interchangeable — a failed valve is a Saturday-morning fix, not a
> two-week order. They are also **SELV**, so gel-filled direct-burial splices in a wet valve box are
> entirely normal practice. Using an identical valve for all 8 zones *and* the master means **one
> spare part covers everything**.
>
> **Do not go above 24 V.** 48 V is still within SELV limits, but **48 V irrigation solenoids are
> rare**, which defeats the "common, cheap, easily replaceable" requirement outright. There is no
> engineering gain — the coil is sized for the valve, not the other way round.
>
> **Avoid DC solenoids** for a different reason: in irrigation, most 12/24 V **DC** solenoids sold
> are **latching** types for battery timers, which **FR-3.1 prohibits** — they hold their last
> position with no power, so a power cut mid-run leaves the valve **open** with the controller dead.
> ⚠️ Concretely: **Hunter 458200 is the 9 V DC latching solenoid [V]** and must never be ordered as a
> spare coil. The 24 VAC coil is €14.14.
>
> Relay drive was already chosen over triacs for the same class of reason: **a relay fails *open*
> (fail-closed); a triac fails *shorted* (fail-water-on)**, and at ~730–4 400 operations per zone per
> year against a 10⁵ rating, relay wear is not a design constraint.

## FR-11 — Physical controls on the box *(optional, owner request 2026-09-05)*

**Intent:** someone with no Home Assistant access — a house-sitter, a guest, family — can water the
garden or stop it, without a phone.

| ID | Requirement | Origin |
|---|---|---|
| FR-11.1 | The box **may** carry a **master enable/disable switch** and **one momentary button per zone**. | owner |
| FR-11.2 | ⚠️ **Physical controls shall be inputs to the firmware, not direct valve wiring.** Pressing a zone button **requests a manual run** exactly as FR-10 does — so it inherits the duration cap, the sequential interlock, flow monitoring, alarm lockout and the whole safety layer. **A switch that energises a valve directly would bypass the deadman, the max-runtime backstop and the interlock at once — a direct P0 violation.** | **P0, P4** |
| FR-11.3 | A zone button **shall start a bounded run** (default duration, per FR-10.3's hard cap). Pressing it again **shall stop that zone early**. Press-and-hold is not used — every press must resolve to a bounded action. | new |
| FR-11.4 | The master switch **shall map to the existing Hold verb** (FR-8.1), not to cutting power. Its position **shall be readable by the firmware and published to HA**, so a physically-held system is visible remotely and can raise the stuck-hold alert (FR-8.4). | new |
| FR-11.5 | Physical control **shall remain subject to a latched CRITICAL alarm lockout** (FR-9.3), with the same single bounded diagnostic run allowed. | new |
| FR-11.6 | A **status LED** shall indicate at a glance: idle / watering / held / fault. This is what makes the box legible to someone who has never seen it before. | new |
| FR-11.7 | **The dead-controller case is covered by the manual bypass ball valve (A5), not by electrical switches.** If the ESP32 is dead, no amount of button-pressing helps; the bypass is the answer, and the laminated card (NFR-D1) must say so. | new |

> **If you want a true hardware manual mode** — water flowing with the firmware entirely out of the
> loop — the safe way to build it is a switch that feeds the zone solenoid **through the latching
> max-runtime backstop and the NC kill switch**, so it is still bounded to one timer period and still
> killable. That is defensible. A switch wired straight across the relay contact is not: it is
> unbounded watering with no supervision, which is the one genuinely dangerous failure in
> [§5](03-fault-policy.md).

## FR-5 — Per-zone schedule configuration

| ID | Requirement | Origin |
|---|---|---|
| FR-5.1 | Each zone **shall support a day predicate** composed of independent, composable filters: weekday mask, optional `every N days` with an epoch anchor, and an optional day-parity filter. | R5 |
| FR-5.2 | The day predicate for `every N days` **shall be measured from the last *actual* watering**, not the last scheduled one, so skips do not cause drift. | new |
| FR-5.3 | Each zone **shall have a start time and a duration**. **No `latest_finish`, no window, no cap** — *D4: scheduling is fully delegated to the operator.* This resolves the three-way "range of time" ambiguity in the original requirement 5 by removing the construct entirely. | R5, **settled D4** |
| FR-5.4 | Durations **shall be in minutes in v1.** Volume (litres) and depth (mm) targets are v2/v3 — so the core loop never depends on the least reliable component. | **challenges R5** |
| FR-5.5 | A **volume target, when introduced, shall always be bounded by a hard maximum duration.** An under-reading flow meter with an unbounded volume target is the exact dangerous case. | **challenges R5** |
| FR-5.6 | A **global seasonal adjustment percentage** shall scale all durations, clamped to a safe maximum (≤200 %). | new |
| FR-5.7 | Multiple programs per day **shall be supported in v1**, not merely representable. *Upgraded by D5: the soil is heavy clay.* | **challenges R5**, upgraded D5 |
| FR-5.10 | **Cycle-and-soak shall ship in v1** (D5). A zone's daily duration is delivered as **N passes of `duration/N`** rather than one continuous run, so water has time to infiltrate instead of running off. Clay infiltrates at roughly 3–6 mm/h *[E]* against a rotor precipitation rate of 10–12 mm/h, so a single long pass largely runs off. **Sequential execution provides the soak interval for free** — while zone 1 rests, zones 2–8 are watering — so this is *N passes over the zone list*, which is ESPHome `sprinkler`'s `repeat_number`. Little new firmware, and D4's removal of the window makes the extra wall-clock free. | **D5** |
| FR-5.8 | Schedule storage **shall be schema-versioned and CRC-protected**, with dual-bank atomic writes. A firmware update must reject an incompatible config and alarm, never guess. | new |
| FR-5.9 | No schedule **start time** shall be permitted inside **01:30–03:30 local**, because the Europe/Paris DST transitions delete and duplicate that hour. Validation shall reject it. | new |

## FR-6 — Water metering

| ID | Requirement | Origin |
|---|---|---|
| FR-6.1 | The device **shall totalise volume per zone and in aggregate**, in **litres**, with `device_class: water` and `state_class: total_increasing`, matching your pool sensors exactly. **[V]** | R6 |
| FR-6.2 | Totals **shall be accumulated on the device and persisted**, monotonic across reboots. *(Your pool counter is not — see [§9](08-home-assistant.md).)* | R6 |
| FR-6.3 | Day / month / year totals **shall be derived in HA via `utility_meter` helpers**, mirroring `Piscine - Volume aujourd'hui` / `... ce mois`, plus the yearly meter the pool lacks. **[V]** | R6 |
| FR-6.4 | Per-zone volumes **shall appear in the HA Energy dashboard's Water section** as devices under an aggregate parent, using the currently-empty `device_consumption_water` slot. **[V]** | R6 |
| FR-6.5 | Season totals **shall be read from long-term statistics** with a fixed-period `statistic` card, not a fourth helper — `utility_meter` has no season cycle and tariff-less meters cannot be reset by service call. **[V]** | new |
| FR-6.6 | Zones whose flow is below the meter's measurable floor **shall be flagged `flow_unmetered`**, with volume estimated from `nominal_flow × runtime` and **labelled as estimated** in the UI. *Demoted by D1 — no zone runs below ~1 L/min, so this is now a defensive edge case (a partially-blocked zone), not a design driver.* | **challenges R6**, revised D1 |
| FR-6.7 | The meter **shall be plumbed to see only the irrigation circuit**, so a hosepipe wash does not trigger the unexpected-flow alarm. | new |

## FR-7 — Scheduled vs actual

| ID | Requirement | Origin |
|---|---|---|
| FR-7.1 | The daily plan **shall be materialised and stored** before execution: per zone, planned start, planned duration, planned volume. | R7 |
| FR-7.2 | Each run **shall produce a durable record**: zone, requested/actual start, actual duration, delivered litres, trigger source (schedule / manual / catch-up), and an **outcome enum** (`OK`, `interrupted`, `no_flow`, `low_flow`, `excess_flow`, `skipped_pause`, `skipped_rain`, `skipped_disabled`, `never_ran`). | R7 |
| FR-7.3 | Plan and actual **shall both be exposed as numeric sensors with a `state_class`**, because HA's recorder purges detail after ~10 days and only `state_class` sensors reach long-term statistics. **[V]** | R7 |
| FR-7.4 | Actual runtime **shall be reported by the device**, not derived by an HA `history_stats` helper — the interesting case is precisely a run that happened while HA was down. | new |
| FR-7.5 | Run records **shall be retained on-device in a ring buffer** and replayed to HA on reconnect. Archival format **shall be append-only JSONL/CSV**, readable in 10 years without this software. | new |
| FR-7.6 | A **deviation** metric per zone (delivered − planned) shall exist, with a documented threshold for "worth flagging". | R7 |

## FR-8 — Pause / Hold / Stop

*The two state diagrams in [§8](07-firmware.md#modes-phases-and-the-three-verbs) show why these are three verbs and not one: Hold changes the mode, Pause and Stop change the phase, and Winter does both.*

| ID | Requirement | Origin |
|---|---|---|
| FR-8.1 | Three distinct verbs shall exist and be named unambiguously: **Hold** (let the current cycle finish, schedule nothing new), **Pause** (suspend the current run, resumable, with a max-suspend timeout), **Stop** (close everything now, clear the queue, no resume). | **challenges R8** |
| FR-8.2 | Missed cycles **shall be skipped, never queued.** Unpausing must not dump three days of water at once. Skips are recorded as `skipped_pause` so FR-7 shows the gap. | **challenges R8** |
| FR-8.3 | Hold **shall take a duration.** Presets (end of day / 24 h / 48 h / until Sunday / until date) plus an *indefinite* option that is deliberately harder to select. Default **24 h**, not indefinite. | **challenges R8** |
| FR-8.4 | A **maximum hold duration** (e.g. 7 days) shall exist, with an actionable notification 24 h before expiry, then auto-resume. *The forgotten pause is how gardens die, and its failure mode is silent.* | **challenges R8** |
| FR-8.5 | Hold **shall not block manual runs.** A separate **Winter/Off mode** blocks everything including manual, for when the pipes are drained. | **challenges R8** |
| FR-8.6 | Hold state and its expiry **shall persist on the device.** If no persisted value exists, the device **shall default to NOT held** — because wrongly running costs some water and is loudly visible, while wrongly held kills a garden silently. An HA-side alert shall fire if hold stays on >24 h. | new |
| FR-8.7 | Hold **shall be attributed** ("held by mobile_app_dt_phone at 14:02") so the digest can say who to ask. | new |

## FR-9 — Alarms

See [§11](10-alarms.md) for the full taxonomy and the [detection-to-acknowledgement flow](10-alarms.md#from-detection-to-acknowledgement).

| ID | Requirement | Origin |
|---|---|---|
| FR-9.1 | Alarms **shall have three severities**: `INFO`, `WARNING` (never locks out), `CRITICAL` (latches and locks out irrigation). | R9 |
| FR-9.2 | Detection **shall happen on the device** (because the action is closing a valve); interpretation and notification **shall happen in HA** (because HA owns the only notification channel, and the firmware cannot call HA services). **[V]** | R9 |
| FR-9.3 | `CRITICAL` alarms **shall latch** and require an explicit, attributed acknowledgement to clear. Manual runs are blocked while a system-level `CRITICAL` is latched, except for one bounded diagnostic run so a repair can be tested. | R9 |
| FR-9.4 | An **HA-side availability watchdog shall exist**, because a dead ESP32 cannot raise its own alarm. It shall trigger on `unavailable` with explicit `from:`/`to:` (your own automation notes that the controller flaps). **[V]** | new |
| FR-9.5 | A **"system healthy" weekly heartbeat shall exist, whose absence is itself the signal.** This is the only alarm design that survives total silence. | new |
| FR-9.6 | Every alert **shall have a matching `clear_notification` with the same `tag`**, mirroring your pool automation. **[V]** | new |

## FR-10 — Manual run

| ID | Requirement | Origin |
|---|---|---|
| FR-10.1 | A manual run **shall be expressed as "run zone N for M minutes"**, never as "open zone N". | R10 |
| FR-10.2 | The duration **shall be enforced on the device.** A Wi-Fi drop, HA restart, or HA upgrade during the run must still result in auto-stop. | R10 |
| FR-10.3 | A **hard maximum manual duration** (default 60 min, per-zone configurable) shall be enforced independently of the run timer. | new |
| FR-10.4 | A manual request **shall preempt the current zone**, keep the cycle's position, and resume the cycle afterwards if still inside the window. | **challenges R10** |
| FR-10.5 | Concurrent manual requests **shall queue**, never open multiple valves. | new |
| FR-10.6 | Manual runs **shall ignore** rain-skip and the day predicate (a human overrode deliberately) but **shall respect** hold, zone-disabled, the interlock and the flow alarms — reporting a visible outcome rather than silently swallowing the press. | new |
| FR-10.7 | A manual run **shall count toward all water accounting unconditionally**, and shall mark the day's scheduled run `satisfied_by_manual` **per zone** if it delivered ≥80 % of that zone's plan — preventing the most common irrigation mistake, double-watering on a hot day. *Confirmed by D10, scoped per zone rather than per cycle.* | **challenges R10**, confirmed D10 |
| FR-10.8 | A **"test all zones for 2 minutes each" commissioning action** shall exist. | new |

---
