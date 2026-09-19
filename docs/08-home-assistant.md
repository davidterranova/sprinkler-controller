[← Documentation index](../README.md)

# 9. Home Assistant integration

Everything here was read from your live instance on 2026-09-04. **[V]**

## Your instance, as it actually is

| Property | Value |
|---|---|
| Core | **2026.8.0**, **Home Assistant Container** (`hassio: false`) on a Raspberry Pi 5 |
| Timezone / language / units | `Europe/Paris` / `fr` / metric, litres |
| Recorder | SQLite, **262 MiB, ~10-day retention** |
| ESPHome | Installed. One device: **`Pool Controller`**, esp32dev, ESPHome **2026.7.2**, native API via zeroconf, 33 entities, `allow_service_calls: false` |
| MQTT | **None. No broker, and no add-on store to install one.** |
| HACS | 2.0.5, 8 repos. **apexcharts-card v2.2.3 is the only Lovelace resource.** No Mushroom, no card-mod. |
| Dashboards | 4, all **storage mode**, `type: sections` |
| Notifications | **`notify.mobile_app_dt_phone` is the only real channel.** No Alexa/Google/Telegram/email; `cloud.logged_in: false`. |
| Weather | **`weather.forecast_maison`** (met.no) only. **No rain gauge. No soil moisture. Anywhere.** |
| Areas | `local_technique` exists and is effectively empty — that is where the controller lives. **There is no `jardin` area yet.** |

## Decisions this drives

1. **Transport: ESPHome native API. Not close.** No broker exists and there is no clean way to install
   one (Container install, no Supervisor — `/addons` returns "Unknown command" **[V]**). Beyond
   convenience, the native API is *safer* here: commands are RPCs on a live connection, whereas
   **retained MQTT command messages are genuinely replayed on every reconnect and reboot — ESPHome
   cannot even see the retain flag [V]**, so a retained `valve/set → OPEN` reopens a valve unattended
   after a power cut. On reconnect the ESP pushes a **full state dump of every entity [V]**, so HA
   restarts cannot leave stale values.
2. **Use the `valve` domain, not `switch`.** All five `valve.*` services exist on 2026.8.0 and **zero
   `valve` entities exist yet [V]** — no collisions, no precedent to fight. Declare **`OPEN|CLOSE`
   only**; do not advertise `SET_POSITION`, which would invite calls the firmware cannot honour.
   `valve` also models the open/closed/opening/closing distinction honestly and maps to the HomeKit
   Valve service with an Irrigation subtype.
3. **Mirror your pool metering contract exactly:** `device_class: water`, `state_class:
   total_increasing`, **unit `L`** (litres, *not* m³), flow in `L/min` with no device class, then
   `utility_meter` clones of `Piscine - Volume aujourd'hui` (daily) and `... ce mois` (monthly) — plus
   the **yearly meter your pool is missing**. Set **`periodically_resetting: false`**, since unlike the
   pool's counter ours will be genuinely monotonic. *(Note the default is `true`, which silently
   discards water used during any HA gap.)* **[V]**
4. **Energy dashboard:** one `type: water` parent plus eight entries in the currently-**empty**
   `device_consumption_water`, each with `included_in_stat` so litres are not double-counted.
   ⚠️ `energy/save_prefs` is **full-replace per top-level key** — a careless write would silently
   delete the pool's source. Use the add-source/add-device paths only. **[V]**
5. **Adopt your own commit idiom.** Your pool controller already has
   `button.piscine_pool_controller_apply_filtration_config` +
   `binary_sensor.piscine_pool_controller_filtration_config_pending`. Reuse it verbatim for FR-2.2 —
   it solves the atomicity problem and you already know how it behaves.

## Three lessons your own config already encodes

Your `automation.piscine_alerte_manual_override_bloque` is unusually well-crafted and its `note:`
fields are, in effect, this project's architecture philosophy. Quoted verbatim **[V]**:

> "No `for:` here on purpose — the 30-minute delay lives in the ESP32 firmware (`millis()`-based), so
> it survives HA restarts and the controller's Wi-Fi blips, unlike a trigger `for:` clock."

> "`from`/`to` are explicit on both triggers so the controller dropping to 'unavailable' and back — it
> does flap — can't re-fire this."

So: **(1)** timing that must survive an HA restart belongs in firmware; **(2)** `from:`/`to:` are
always explicit because the controller flaps; **(3)** every alert has a matching `clear_notification`
with the same `tag`. All three carry straight into this project.

## Things in the live instance that complicate this

| # | Finding | Impact |
|---|---|---|
| 1 | **Container install, no Supervisor** | MQTT is off the table; firmware builds are CLI/Docker by hand. *(Something works today — the pool node was compiled 2026-09-03.)* |
| 2 | **No rain gauge, no soil moisture; met.no exposes precipitation only inside `weather.get_forecasts`, not as a state attribute** | Rain-skip is **structurally HA-dependent**. It must be a *pushed hint with a staleness timeout* (P6), never a precondition. **Raise a tipping-bucket gauge now, while the board layout is still open.** |
| 3 | **Your pool water counter is not reboot-persistent.** It reads **2.0 L** against a monthly **988 L**, with repeated `unavailable → 0.0` transitions in history — and **that raw sensor is the Energy dashboard's water source** | Do not copy it. Also: worth fixing separately; your pool's "Volume total remplissage" is not a total, and the Energy figure is only correct by accident of `total_increasing`'s reset handling. |
| 4 | Recorder is SQLite, 262 MiB, ~10-day retention; this project adds ~130 entities | Mandates `state_class` on everything you want to keep, a `recorder: exclude:` list for the pure config echoes, and rules out `history_stats` for anything seasonal. |
| 5 | **`utility_meter.reset` targets a tariff `select`** which tariff-less meters do not have | No service-callable "reset the season". Use `calibrate value: "0"`, or better, the LTS `statistic` card with a fixed period and no helper at all. |
| 6 | Entity IDs on the pool device are **already inconsistent** (`sensor.piscine_pool_controller_*` alongside `sensor.pool_controller_*`) because entities were added before and after the area assignment | **Create a `jardin` area on the `Exterieur` floor and assign the device *before* first adoption**, then pin every helper/dashboard/energy reference to the IDs actually generated. Renaming later silently breaks `utility_meter` sources and energy prefs. |
| 7 | **12 HomeKit bridges, one unpaired right now** — and **your pool outage was caused by an Apple Home scene** | **Explicitly exclude the hold switch and all buttons from HomeKit.** This is the single most likely repeat of your previous outage. Exposing the 8 `valve` entities is desirable, but decide it deliberately with `filter:`. |
| 8 | `notify.mobile_app_dt_phone` is a single point of failure | Add `persistent_notification` as a second sink on severe alerts so there is a UI record when a push is missed. ntfy/SMTP is a cheap second channel if wanted. |
| 9 | k3s + Docker bridges on the same Pi | mDNS discovery for a new ESPHome node can be flaky; add by IP with a UniFi static lease if needed. |

## Schedule source of truth — the decision

Three options were evaluated concretely:

- **(a) Schedules in HA, commanding the ESP.** Violates FR-1.1 outright, and violates it *silently* —
  the garden simply isn't watered and nothing reports it, because the thing that would report it is
  the thing that's down. **Rejected.**
- **(b) Schedules on the ESP in NVS, edited from HA via `time` / `number` / `select` / `switch`
  entities.** Every field is a first-class HA entity: inspectable, historisable, schema-validated by
  the selector, and renderable in the `entities` cards you already use. Costs ~40 config entities.
- **(c) HA authors a compact blob pushed to the ESP.** Decisive costs: a factory-reset device has **no
  schedule until HA connects** (an autonomy regression at exactly the moment autonomy matters); no
  validation (HA cannot tell you `0790` is not a time); not human-editable in your dashboard style; and
  it needs HA-resident staging state, i.e. two sources of truth wearing one hat.

**Recommendation: (b), with your pool's Apply/Pending commit protocol layered on top**, and a
structured `api: actions:` entry point kept in reserve for atomic bulk edits (this is where the
firmware analysis preferred to start, on the grounds that 8 zones × 4 schedules × 8 fields ≈ 250
entities is unmanageable). Reconciling the two: **ship one schedule per zone in v1** (~40 config
entities — comfortable), and introduce the structured action only when multi-schedule zones arrive.

⚠️ **Verify first:** whether HA→device user-defined actions (`api: actions:`) require
`allow_service_calls` to be enabled. The two analyses disagreed; `allow_service_calls` gates
*device→HA* calls, but the interaction needs a five-minute check. See [§17](16-verification-backlog.md).

## Entity model (sketch)

**Per zone (× 8)** — `valve` (the valve), `switch` (zone enabled), `time` (window start),
`number` (duration, nominal flow), `select` (day pattern), `sensor` (lifetime volume `total_increasing`;
planned litres today; planned/actual minutes today; next start; last end; last outcome enum),
`button` (water now), `binary_sensor` (zone fault, `device_class: problem`).

**Global** — connectivity `status`; hold switch + duration + remaining; master valve; instantaneous
flow; grand total; active zone; queue depth; next start; seasonal adjustment %; rain hint + its age;
apply-config button + config-pending; recompute button + last-recompute; emergency stop; fault rollup +
fault code; **clock-valid** binary sensor; and an `event` entity for cycle completion
(`termine` / `interrompu` / `echec` / `saute`).

Mark every knob `entity_category: config` and every diagnostic `entity_category: diagnostic` so the
device page groups them and they stay out of auto-generated dashboards.

> ℹ️ **This sketch predates FR-12 and is now the wider of two lists.** It names per-zone entities that the
> costed model below deliberately does **not** publish — planned litres today, planned/actual minutes today,
> last end, a per-zone fault flag — because each is derivable in Home Assistant from something the device
> already sends, and at 128 of ~130 there is no room for a figure that can be derived. Where the two disagree,
> **[the interface specification](#the-interface-specified) wins**; read this section for the *shape* of the
> model and that one for what is actually built.

> ⚠️ **~130 entities is a budget, not a free variable.** The binding constraint is **not RAM** (that is
> ~13–32 KB of a 200 KB+ heap) but the **initial ListEntities enumeration**: there are verified reports
> of ESP32 devices where *"not all entities are visible"* after connect — one with ~250 entities
> showing only **120–140** in HA **[V]** — and the documented crash class was a **task-watchdog reboot
> while the loop spun on a full TCP buffer**, fixed only in **ESPHome 2026.9** (#18577). The
> maintainers' own 148-entity reference device runs fine, so ~130 is demonstrably achievable — but
> **run 2026.9+**, and think twice before adding the 131st entity. Details in
> [§8](07-firmware.md#-the-real-entity-count-constraint-is-not-ram--it-is-the-listentities-dump).

## The interface, specified

[FR-12](02-functional-requirements.md#fr-12--the-home-assistant-interface-owner-request-2026-09-16) states what
the interface must **answer**. This section states how it is **built**, and — the part that actually
constrains it — what it **costs**.

### The rule that decides where everything lives

> **The device publishes only what the device is the only thing that can know. Everything derivable
> from that is derived in Home Assistant, where it costs nothing and cannot fail to enumerate.**

That is not a style preference. At **91 published entities today against a ~130 ceiling**, the
per-zone read-outs FR-12.2 asks for are affordable exactly once, and only if nothing derivable is
published alongside them.

**Published by the device — nothing else can supply these:**

| Entity | × | Why it cannot be derived in HA |
|---|---|---|
| `sensor` zone water total — L, `total_increasing`, `device_class: water` | 8 | Must be **monotonic across HA restarts and HA absence**. An HA-side integration of flow resets and misses every run that happened while HA was down — which is the run you most want counted. |
| `sensor` zone next run — **epoch seconds** | 8 | The day predicate is evaluated against `last_actual_day` (FR-5.2), which lives in device NVS. HA does not have the inputs. |
| `sensor` zone runs completed — count, `total_increasing` | 8 | The denominator of every average in FR-12.2.4. Only the device sees a run end, and only it can apply FR-12.1.4's "delivered water" test. |
| `text_sensor` zone last run — one digest string | 8 | Planned/actual/litres/outcome for the most recent run, including runs replayed from the ring buffer after an outage. |
| `sensor` flow rate — L/min | 1 | Pulse counting. |
| `sensor` water total — L, `total_increasing` | 1 | Same argument as the per-zone total. |
| `sensor` cycles completed — count | 1 | The denominator of FR-12.1.3. |
| `binary_sensor` master valve open | 1 | FR-12.1.1 wants it as a **distinct** item: master open with no zone open is a fault, and it is invisible if the two are merged. |
| `text_sensor` zone config read-back — numbers | 1 | FR-12.6.10. What the device actually believes, so HA's form can be reloaded from truth. |
| `text_sensor` zone labels read-back | 1 | Split from the line above for a hard reason — see the 255-byte note below. |
| | **32 + 6** | |

**Derived in Home Assistant — zero device entities:**

| Derived | From | Note |
|---|---|---|
| Per-zone state (`watering` / `next in queue` / `idle` / `disabled` / `retired` / `faulted`) | active zone + published plan + zone-enabled | FR-12.2.1. Eight template sensors, or none at all if the card computes it inline. |
| Average litres per run, per zone | zone total ÷ zone run count | FR-12.1.5. **Exact across any outage**, which an HA-accumulated mean is not. |
| Average litres per cycle, global | water total ÷ cycles completed | FR-12.1.3. |
| Day / month / year volumes | `utility_meter` on the totals | FR-6.3, `periodically_resetting: false` **[V]**. |
| Season volume | fixed-period `statistic` card | FR-6.5 — not a fourth helper. |
| Everything on the statistics view | long-term statistics | FR-12.5.3. |
| Next-run timestamps as "in 6 h" | epoch sensor → template sensor with `device_class: timestamp` | See below. |
| Staleness | `unavailable` + `last_changed` | FR-12.1.6. |

### The budget, and the fact that it closes

| | Entities |
|---|---|
| Published today (eight zones, v0) | **91** |
| \+ FR-12.2 per-zone read-outs (4 × 8) | +32 |
| \+ FR-12.1 / FR-12.6 globals | +6 |
| − v0 bench instruments, removed before any valve is wired (*BENCH starve*, *BENCH hang*, *Simulation speed*) | −3 |
| **v1 interface total** | **126** |
| \+ the D11 rain gauge, which has not spent its allowance yet (rain today, rain-skip active) | +2 |
| **v1 total** | **128 of ~130** |

**That is the whole budget, gone.** Two consequences worth stating plainly rather than discovering in
six months:

1. **The zone editor could not have been built from entities.** The mirror-entity idiom of decision
   (b) above — the right answer for the ~20 global knobs — would have cost **16–24 more** for
   per-zone labels and frequencies and put the device at ~150. That is what **D18** settles.
2. **The 131st entity is now a trade, not an addition.** [§8](07-firmware.md#-the-real-entity-count-constraint-is-not-ram--it-is-the-listentities-dump)
   explains why the failure mode is *silent* — entities missing after connect, not an error — which
   is the worst possible way for a budget to be exceeded.

### Three numbers that are not what they look like

**"Average water per run" has a denominator, and the denominator is the whole argument.** A skipped
run is not a zero-litre run. Counting `skipped_rain` in the denominator makes a well-behaved rainy
month look like a system failure; excluding `interrupted` hides real water. FR-12.1.4 settles it:
**a run counts iff it delivered water**, skips are counted separately and shown separately
(FR-12.5.2), and **the card states which definition it is using.**

**"Next run" is a schedule for the first zone and a projection for every other one.** Only the
cycle's start time is a commitment; zone 5 starts when zones 1–4 have finished, and one zone running
long moves it. FR-12.3.3 requires the projection be labelled as such. *An interface that presents a
computed start as a commitment is lying, and it will be believed.*

**"Runs per day" and "passes" are different quantities with opposite failure modes.** Two runs per
day is two waterings hours apart. Three passes is one watering split so heavy clay can absorb it
(FR-5.10, D5). Confusing them **triples the water** instead of improving infiltration, and both
words are short. FR-12.6.4 requires them kept distinct in the UI wording, not just in the model.

### Two implementation facts that shape the entity list

**Publish epoch seconds, render the timestamp in HA.** HA's `device_class: timestamp` requires an
ISO-8601 *string* state, which an ESPHome numeric `sensor` cannot carry. The known-good pattern is a
plain numeric sensor holding Unix epoch seconds, converted by a one-line HA template sensor that
*does* carry `device_class: timestamp` — which buys relative rendering ("in 6 h", "il y a 2 jours")
for free. Whether a `text_sensor` can carry the device class directly is **unverified**; the epoch
route does not depend on the answer.

**Digests are capped at 255 bytes over the native API.** That is why the zone configuration
read-back is **two** text sensors and not one: numbers only (`3:1:127:0:12:8` × 8 ≈ 112 bytes) fits
comfortably; adding eight 16-character labels (≈ 216 bytes) does not leave enough margin for a
long name to be safe. Splitting them costs one entity and removes a class of truncation bug that
would only appear once someone named a bed *"Massif de rosiers côté terrasse"*.

### The zone editor (D18)

Adding, configuring and retiring a zone happens through **Home Assistant helpers as a form** and
**one structured `api:` action as the transport** — and, critically, **the staging stays on the
device**:

```
input_text.zone_label ┐
input_number.days_per_week ├─ script.sprinkler_apply_zone ─→ esphome.sprinkler_set_zone
input_number.runs_per_day  │        (one call, whole zone)          │
input_number.minutes_run ──┘                                        ▼
                                                    device validates the whole zone
                                                    → stage_zone()  (RAM, FR-2.2)
                                                    → binary_sensor.config_pending = on (FR-2.3)
                                                                    │
      button.apply_schedule ────────────────────────────────────────┘
                                    → validate() the whole schedule → commit to NVS, or reject
```

```yaml
# Sketch. Field-by-field validation happens on the device, not in the selector.
api:
  actions:
    - action: set_zone
      variables:
        zone: int            # 1..8
        label: string
        enabled: bool
        weekday_mask: int    # bit 0 = Sunday .. bit 6 = Saturday
        interval_days: int   # 0 = filter off
        minutes: int[]       # one per program; 0 = this zone is not in that program
      then:
        - lambda: |-
            id(sched)->stage_zone(zone - 1, label, enabled, weekday_mask, interval_days, minutes);
    - action: retire_zone            # FR-12.6.7 — disable, zero, keep the history
      variables: { zone: int }
    - action: reset_zone_statistics  # FR-12.6.8 — separate, explicit, attributed
      variables: { zone: int, who: string }
```

**"Load from device" needs no action**: the read-back text sensors are always current, so the script
that populates the helpers simply parses them. That is also the answer to *"is HA now a second
source of truth?"* — **no**, and the reload button is what proves it.

> ### Why this is not the option (c) that was rejected above
>
> It looks like it at a glance, and the distinction is load-bearing:
>
> | | Rejected option (c) | D18 |
> |---|---|---|
> | Where the schedule lives | HA authors a blob; the device stores what it is given | **Device**, unchanged — schema-versioned, CRC'd, dual-bank (FR-5.8) |
> | Factory-reset device with no HA | **No schedule until HA connects** | Boots on committed config or factory defaults (FR-2.4) |
> | Validation | None — HA cannot tell you `0790` is not a time | **On the device, field by field**, rejecting before staging (FR-12.6.6) |
> | Staging / Apply | HA-resident staging state | **Device-resident** — `config_pending` stays authoritative (FR-2.3) |
> | What HA holds | The only copy | **A form**, reloadable from the device in one click |
>
> The global knobs — seasonal %, program start times, hold, winter, manual run — **stay as
> first-class entities**, because there are ~20 of them, they are touched often, and they are
> affordable. D18 moves only the **per-zone** surface, which is the part that scales with zone count
> and therefore the only part that could break the budget.

⚠️ **This design has a five-minute dependency that is now blocking.**
[Backlog item 1](16-verification-backlog.md) — *do HA→device `api: actions:` require
`allow_service_calls`?* — was a curiosity when it was written; the zone editor now rests on it. Two
things need the same check: that the action is callable at all with `allow_service_calls: false`,
and that **array variables** (`int[]`) are supported in user-defined actions.

**If it goes the wrong way, the fallback is one entity, not a redesign:** a single `text` entity on
the device carrying one zone's configuration as a compact string, written by the same HA script via
`text.set_value` and parsed by the same `stage_zone()`. Identical staging semantics, one entity
instead of zero, and uglier — but the shape of the design does not change.

### What this asks of the firmware

Three things in FR-12 are not read-outs of state the device already holds:

1. **Per-zone next-run projection.** `next_start_today()` looks only at today. FR-12.2.3 and FR-12.3.2
   need a forward projection across the 7-day horizon (up to 14 days, the `every N days` maximum) per
   zone. It is a pure function of (schedule, run state, now) — at most 14 × programs × 8 predicate
   evaluations, host-testable alongside the resolver, and it **must** live on the device because HA
   does not have `last_actual_day`.
2. **The run counter and the "delivered water" test** of FR-12.1.4, persisted alongside the volume
   totals.
3. **Per-zone day predicates and labels in the schedule blob** (FR-5.11, FR-12.6.11) — a
   `SCHEMA_VERSION` bump, with the old record **refused rather than reinterpreted** (FR-5.8).

---

## Dashboard

Clone the `dashboard-piscine` vocabulary rather than inventing one: conditional `markdown` banner cards
for fault / held / config-pending / offline; `heading` separators; `tile` cards with `grid_options`;
`custom:apexcharts-card` with `statistics:` series (`type: change` for counters, `type: max` for plan
levels) so charts read long-term statistics rather than the 10-day raw history; the
`input_select`-driven period switcher with `["1 jour","7 jours","30 jours","90 jours","1 an"]`; and your
existing colour vocabulary — `#3b82f6` actual, `#94a3b8` planned, `#06b6d4` water, `#f97316` temp,
`#ff5252` now-marker.

**Three views, one per question FR-12 asks.** Each gets an explicit `path:` so notification deep links
survive adding a fourth.

| View | Path | Answers | Principal cards |
|---|---|---|---|
| **Statut** | `/sprinkler/statut` | *What is it doing now?* | Banner row (fault / held / config-pending / offline / clock untrusted); a hero `tile` for **watering yes/no + active zone label + time remaining**, with the **master valve as its own tile**; global total, today, average per cycle; then one row per zone — state, last run, next run, total, average per run. |
| **Statistiques** | `/sprinkler/statistiques` | *What did it do?* | The period `input_select` at the top, driving **every** chart below it (FR-12.5.1); litres per zone as a comparison bar on one axis; litres over time; minutes and run count; skipped runs by reason; then the **run list** — most recent first, each row expanding to the single-run detail of FR-12.4.1. |
| **Zones** | `/sprinkler/zones` | *How do I change it?* | One section per slot, `in service` or `free`. The four fields of FR-12.6.2 as helpers, the implied totals of FR-12.6.5 recomputed live beneath them, then **Load from device** / **Stage** / **Apply**, with `config_pending` shown as a banner across the whole view rather than per zone. Retire and *Reset statistics* live here, the second one deliberately harder to reach. |

Two carry-overs that are easy to lose and expensive to rediscover:

- **Order the status view by what is scarce.** The thing a person opens this dashboard to learn is
  *"is water running right now, and did the garden get watered"*. Everything else is a second screen.
- **Exclude the new surface from HomeKit explicitly.** Finding #7 above — *your pool outage was caused
  by an Apple Home scene*. The zone editor's helpers, the Apply button, `retire_zone` and
  *Reset statistics* must all be outside the bridge `filter:`, and that is a decision to write down
  rather than a default to rely on.

---
