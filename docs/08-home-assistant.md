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

> ⚠️ **~130 entities is a budget, not a free variable.** The binding constraint is **not RAM** (that is
> ~13–32 KB of a 200 KB+ heap) but the **initial ListEntities enumeration**: there are verified reports
> of ESP32 devices where *"not all entities are visible"* after connect — one with ~250 entities
> showing only **120–140** in HA **[V]** — and the documented crash class was a **task-watchdog reboot
> while the loop spun on a full TCP buffer**, fixed only in **ESPHome 2026.9** (#18577). The
> maintainers' own 148-entity reference device runs fine, so ~130 is demonstrably achievable — but
> **run 2026.9+**, and think twice before adding the 131st entity. Details in
> [§8](07-firmware.md#-the-real-entity-count-constraint-is-not-ram--it-is-the-listentities-dump).

## Dashboard

Clone the `dashboard-piscine` vocabulary rather than inventing one: conditional `markdown` banner cards
for fault / held / config-pending / offline; `heading` separators; `tile` cards with `grid_options`;
`custom:apexcharts-card` with `statistics:` series (`type: change` for counters, `type: max` for plan
levels) so charts read long-term statistics rather than the 10-day raw history; the
`input_select`-driven period switcher with `["1 jour","7 jours","30 jours","90 jours","1 an"]`; and your
existing colour vocabulary — `#3b82f6` actual, `#94a3b8` planned, `#06b6d4` water, `#f97316` temp,
`#ff5252` now-marker. Give the view an explicit `path:` so notification deep links survive adding a
second view.

---
