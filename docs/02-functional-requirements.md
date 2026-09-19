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
| FR-5.1 | Each zone **shall support a day predicate** composed of independent, composable filters: weekday mask, optional `every N days` with an epoch anchor, and an optional day-parity filter. *Confirmed and relocated by FR-5.11: the predicate belongs to the **zone**, which is what makes "frequency per week" a per-zone field (FR-12.6.3).* | R5, relocated FR-5.11 |
| FR-5.2 | The day predicate for `every N days` **shall be measured from the last *actual* watering**, not the last scheduled one, so skips do not cause drift. | new |
| FR-5.3 | Each zone **shall have a start time and a duration**. **No `latest_finish`, no window, no cap** — *D4: scheduling is fully delegated to the operator.* This resolves the three-way "range of time" ambiguity in the original requirement 5 by removing the construct entirely. | R5, **settled D4** |
| FR-5.4 | Durations **shall be in minutes in v1.** Volume (litres) and depth (mm) targets are v2/v3 — so the core loop never depends on the least reliable component. | **challenges R5** |
| FR-5.5 | A **volume target, when introduced, shall always be bounded by a hard maximum duration.** An under-reading flow meter with an unbounded volume target is the exact dangerous case. | **challenges R5** |
| FR-5.6 | A **global seasonal adjustment percentage** shall scale all durations, clamped to a safe maximum (≤200 %). | new |
| FR-5.7 | Multiple programs per day **shall be supported in v1**, not merely representable. *Upgraded by D5: the soil is heavy clay.* The count of programs a zone participates in **is** its "runs per day" (FR-12.6.4), which is a different quantity from its passes (FR-5.10). | **challenges R5**, upgraded D5 |
| FR-5.10 | **Cycle-and-soak shall ship in v1** (D5). A zone's daily duration is delivered as **N passes of `duration/N`** rather than one continuous run, so water has time to infiltrate instead of running off. Clay infiltrates at roughly 3–6 mm/h *[E]* against a rotor precipitation rate of 10–12 mm/h, so a single long pass largely runs off. **Sequential execution provides the soak interval for free** — while zone 1 rests, zones 2–8 are watering — so this is *N passes over the zone list*, which is ESPHome `sprinkler`'s `repeat_number`. Little new firmware, and D4's removal of the window makes the extra wall-clock free. | **D5** |
| FR-5.8 | Schedule storage **shall be schema-versioned and CRC-protected**, with dual-bank atomic writes. A firmware update must reject an incompatible config and alarm, never guess. | new |
| FR-5.11 | The **day predicate shall be stored per zone, not per program.** A `Program` keeps its start time, enabled flag and pass count; the weekday mask and `every N days` move to the zone, and a zone runs in a program iff the program is enabled, the zone's predicate accepts the day, and the zone's minutes in that program are non-zero. *This is what lets the operator think in “this bed, twice a week” rather than in “which programs did I put this bed in”, and it is the model FR-12.6.2 requires.* **It changes the persisted layout: `SCHEMA_VERSION` shall be bumped and the old record refused rather than reinterpreted (FR-5.8).** | **owner, 2026-09-16** |
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

## FR-12 — The Home Assistant interface *(owner request, 2026-09-16)*

**Intent:** Home Assistant is where the system is *observed* and where rules are *authored*
(FR-2.1). It is never in the control path. This section specifies **what the interface must
answer**, not what the dashboard looks like; the entity model and the card-by-card layout are in
[§9](08-home-assistant.md#the-interface-specified).

Three questions, and the whole interface is an answer to one of them: **what is it doing now**,
**what did it do**, and **how do I change it**.

> ⚠️ **The binding constraint on this section is the entity budget, not effort.** The device
> publishes **91** HA-visible entities today against a **~130** ceiling that is an *enumeration*
> limit, not a memory one ([§8](07-firmware.md#-the-real-entity-count-constraint-is-not-ram--it-is-the-listentities-dump)).
> The read-outs below cost ~32 of the ~39 remaining. That is why FR-12.7 exists, why the averages
> are HA-derived, and why the whole editing surface (FR-12.6) had to be moved off the device
> (**D18**).

### FR-12.1 — Global status

| ID | Requirement | Origin |
|---|---|---|
| FR-12.1.1 | The interface **shall answer "is any valve open, and which one" without scrolling and without a tap**: watering yes/no, the active zone by **label**, its time remaining, and **the master valve as an item distinct from the zone** — a master open with no zone open is a fault, not a detail. | owner |
| FR-12.1.2 | **Total water consumed** shall be shown as a lifetime figure owned by the device (monotonic, `total_increasing`, litres) alongside its day / month / year / season derivations (FR-6.3, FR-6.5). | owner |
| FR-12.1.3 | **Average water per run** shall be shown globally as **litres per *cycle***, not per zone-run — at system level the meaningful unit is "what one watering costs". The per-zone-run average is FR-12.2.4. Both definitions shall be stated on the card, because an unlabelled average is a number nobody can act on. | owner |
| FR-12.1.4 | A **run** counts toward an average **iff it delivered water** (`actual_seconds > 0`); outcomes `skipped_*` and `satisfied_by_manual` count toward the *skip* statistics (FR-12.5.2) and **never** toward the denominator. A cycle counts when at least one of its zone runs did. | new |
| FR-12.1.5 | Averages **shall be derived in HA from two device-owned counters** (total litres, completed-run count) and **never accumulated in HA**. HA's recorder purges detail after ~10 days **[V]** and HA is routinely restarted; a mean computed from device counters is exact across any outage, one computed in HA is not. **Cost: zero device entities.** | new |
| FR-12.1.6 | Every figure **shall carry its own staleness.** When the controller is `unavailable` the interface shall show the **last known value marked stale**, never a blank and never a zero. *"Zero litres today" and "I cannot see the controller" must never render the same* — the second is the one that needs a human. | new |

### FR-12.2 — Per-zone status

| ID | Requirement | Origin |
|---|---|---|
| FR-12.2.1 | Each zone **shall show a state**: `watering` / `next in queue` / `idle` / `disabled` / `retired` / `faulted`. **Derived in HA** from the active zone, the published plan and the zone-enabled flag — the device already publishes every input, so this costs **no device entities**. **This requires the device's alarm text to name the zone it concerns**; without that, `faulted` costs eight `binary_sensor`s the budget does not have. | owner |
| FR-12.2.2 | Each zone **shall show its previous run**: when it started, **actual vs planned** minutes, litres delivered, and the outcome enum of FR-7.2 in plain language. | owner |
| FR-12.2.3 | Each zone **shall show its next run**: projected start timestamp, planned minutes, planned litres, and the passes it will be split into (FR-5.10). Where a zone has no next run the interface **shall say why** (`disabled`, `retired`, `no enabled program`, `held until …`, `winter`), never show a blank. | owner |
| FR-12.2.4 | Each zone **shall show its lifetime total** and its **average per run**, the latter derived per FR-12.1.5 from the zone's total and its completed-run count. | owner |
| FR-12.2.5 | **Zones shall be identified by their label everywhere a human reads them**, and by their index everywhere a machine does. **Entity IDs shall remain index-based and immutable for the life of the installation** — renaming an ESPHome entity changes its `entity_id` and **silently breaks `utility_meter` sources and Energy-dashboard preferences** (finding #6 in [§9](08-home-assistant.md#things-in-the-live-instance-that-complicate-this)). The label is presentation; it is never an identifier. | new |
| FR-12.2.6 | Where a zone's volume is estimated rather than metered (FR-6.6), **every figure derived from it shall be visibly labelled estimated**, including the averages. | new |

### FR-12.3 — What happens next

| ID | Requirement | Origin |
|---|---|---|
| FR-12.3.1 | The **resolved plan for the next cycle** (FR-4.4) shall be shown as an ordered list: position, zone label, **projected** start, minutes, passes, planned litres, and the cycle's projected total duration and volume. | owner |
| FR-12.3.2 | The horizon **shall be at least 7 days**, so a weekly rhythm is visible as a rhythm. A zone on `every N days` with N > 7 shall still show its next occurrence beyond the horizon as a date. | owner |
| FR-12.3.3 | Projected starts **shall be labelled as projections.** Only the first zone's start is a schedule; every later one is the sum of the durations before it, and a single zone running long moves all of them. Presenting a computed start as a commitment is how an interface lies. | new |
| FR-12.3.4 | When nothing is planned, the interface **shall state the cause** — held until *T* and by whom (FR-8.7), winter, latched alarm, rain-skip with the gauge reading that caused it, clock untrusted, no enabled program — **never render an empty card**. *An empty schedule and a suppressed schedule look identical and mean opposite things.* | new |
| FR-12.3.5 | Anything the operator can still do shall be reachable from this view: **run zone N for M minutes** (FR-10.1), hold, release, stop, and test-all-zones. | new |

### FR-12.4 — Statistics for a single run

| ID | Requirement | Origin |
|---|---|---|
| FR-12.4.1 | Any individual run **shall be inspectable**: zone, program, trigger source, requested vs actual start, planned vs actual duration, litres delivered, **deviation** (FR-7.6), passes completed, and outcome. | owner |
| FR-12.4.2 | Two retention tiers **shall be stated on the view itself**, because they differ by three orders of magnitude: **full detail — including the flow trace through the run — for as long as the recorder holds it (~10 days [V])**; **summary rows forever**, from the append-only archive of FR-7.5 and from long-term statistics. A run older than the recorder window shall show its summary and **say the detail is gone**, not fail to render. | new |
| FR-12.4.3 | The run list **shall be assembled from the device's ring buffer replayed on reconnect** (FR-7.5), so runs that happened while HA was down appear once HA returns. **Each run shall be recorded exactly once**, keyed on `(zone, program, start epoch)` — a reconnect replays records HA may already hold, and a duplicated run corrupts every average on the page. | new |
| FR-12.4.4 | Runs that **did not** water shall appear in the list with their skip reason, not be omitted. **A gap in the record must be distinguishable from a deliberate skip** — that distinction is the whole point of FR-7. | new |

### FR-12.5 — Statistics over a range

| ID | Requirement | Origin |
|---|---|---|
| FR-12.5.1 | A **single period control shall drive every chart on the view** — `1 jour / 7 jours / 30 jours / 90 jours / 1 an`, reusing the `input_select` idiom already in `dashboard-piscine` **[V]**. Per-chart period pickers are prohibited: comparing two charts on different periods is the most common way to read a dashboard wrong. | owner |
| FR-12.5.2 | Over the selected period the interface **shall report**: litres per zone and in total, run count, minutes watered, **average litres per run**, deviation against plan, and **skipped runs broken down by reason**. | owner |
| FR-12.5.3 | Range statistics **shall be read from long-term statistics**, not raw history — `type: change` on the counters, `type: max` on plan levels — which mandates `state_class` on every figure that appears here (FR-7.3). Anything without one is invisible beyond ~10 days and shall not be charted. | new |
| FR-12.5.4 | Zones **shall be comparable on one axis** for the same period, so "which bed drinks the most" is a glance and not an arithmetic exercise. | owner |
| FR-12.5.5 | The **season** figure shall come from a fixed-period `statistic` card (FR-6.5), not a fourth `utility_meter` — tariff-less meters cannot be reset by service call **[V]**. | new |

### FR-12.6 — Adding, configuring and deleting a zone

| ID | Requirement | Origin |
|---|---|---|
| FR-12.6.1 | **Eight zone slots exist permanently in firmware. "Adding a zone" is claiming a slot, never creating an entity.** ESPHome's entity set is fixed at compile time, and the guard must own all eight pins from the first boot — *a pin that can reach a relay coil is a pin the safety layer must be able to close, planted or not.* The interface shall present slots as `in service` or `free`, and shall never imply that a ninth is possible. | new |
| FR-12.6.2 | Putting a zone into service **shall require exactly four fields**: **label**, **frequency per week**, **runs per day**, **minutes per run**. Everything else — priority, passes, seasonal scaling, budget — **shall have a safe default** and shall not block commissioning. | **owner** |
| FR-12.6.3 | **Frequency is a property of the zone.** The day predicate of FR-5.1 (weekday mask, optional `every N days`) **moves from the program to the zone**; programs retain the start time, the enabled flag and the pass count. A zone runs in program *P* on day *D* iff *P* is enabled, the **zone's** predicate accepts *D*, and the zone's minutes in *P* are non-zero. *See FR-5.11 — this is a persisted-schema change.* | **owner** |
| FR-12.6.4 | **"Runs per day" means program starts, and the interface shall never conflate it with passes.** Two runs per day is two separate waterings hours apart; three passes is one watering delivered as three shorter ones to let clay infiltrate (FR-5.10). They have different purposes and opposite failure modes, and the words for them shall be distinct. | **owner** |
| FR-12.6.5 | Before Apply, the editor **shall show what the four fields actually imply**: minutes per day, minutes per week, litres per week at the zone's nominal flow, and the projected finish time of each cycle the zone joins. *Three innocuous numbers multiply into an overwatered garden, and nothing on the current screen says so.* | new |
| FR-12.6.6 | Validation **shall run before Apply and shall name the offending field**: per-day zone budget exceeded, duration above the cap (FR-5.3), start inside the DST band (FR-5.9), no day selected, schema mismatch. A rejected edit **shall leave the committed *and* the staged configuration untouched** (FR-2.2). | new |
| FR-12.6.7 | **"Delete" means retire, not erase** (**D19**). A retired slot is disabled, holds zero minutes, is excluded from the resolved plan and hidden from the status views — **and keeps its lifetime total and its run history intact.** | **D19** |
| FR-12.6.8 | **Resetting a zone's statistics shall be a separate, explicit, attributed action**, required only when a slot is re-used for a physically different bed. It is **the only sanctioned break in the monotonicity of a `total_increasing` counter**, and it shall be logged with who did it and when. | **D19** |
| FR-12.6.9 | **The editing surface shall cost no device entities** (**D18**). The form is built from HA helpers; one structured `api:` action carries a complete zone configuration to the device, which validates it and **stages it in RAM**. Staging stays on the device, so **FR-2.2 (atomic Apply) and FR-2.3 (config-pending) are unchanged and remain device-authoritative.** | **D18** |
| FR-12.6.10 | The form **shall be loadable from the device's committed configuration** in one action, and shall be loaded that way on first open. Home Assistant holds a *form*, never a second source of truth; an operator must always be able to discard local edits and see what the device actually believes. | **D18** |
| FR-12.6.11 | Zone **labels shall be persisted on the device** as part of the schedule blob, so a controller that has never met this Home Assistant still describes itself correctly — to the next dashboard, to the logs, and to whoever inherits it. | new |

### FR-12.7 — Budget, and what the interface may not do

| ID | Requirement | Origin |
|---|---|---|
| FR-12.7.1 | **A figure shall be published by the device only when the device is the only thing that can know it** — lifetime totals, run counts, projected next runs, outcomes. Everything derivable from those (states, averages, comparisons, period aggregates) **shall be derived in Home Assistant**, where it costs nothing and cannot fail to enumerate. | new |
| FR-12.7.2 | The v1 entity count **shall be tracked as a number in the repository**, not estimated. Budget: **91 today + 32 per-zone read-outs + 6 global − 3 v0 bench instruments** (*BENCH starve*, *BENCH hang*, *Simulation speed*, all removed before any valve is wired) **= 126**, and **128 once the D11 rain gauge lands**, against ~130. The working is in [§9](08-home-assistant.md#the-budget-and-the-fact-that-it-closes). | new |
| FR-12.7.3 | **No HA-derived value shall ever re-enter the control path.** Derivations are for reading. A template sensor may describe a zone's state; nothing may act on one. | P0 |

---
