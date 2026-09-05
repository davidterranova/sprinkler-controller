[← Documentation index](../README.md)

# 15. Roadmap

**Guiding principle: test the riskiest assumptions with the cheapest artefacts, and let no water touch
the garden until the safety layer is proven on a bench.** The temptation is to plumb first because
plumbing is the visible, satisfying part. Resist it — plumbing is the *irreversible* part, and the
measurements you need in order to plumb correctly come from a bucket and a stopwatch, not from code.

## v0 — Bench prototype (no water, no garden)

ESP32 + relay board + **LEDs instead of valves**. Stock `sprinkler` for sequencing. RTC fitted and
proven from day one — set the clock, pull all power for 24 h, confirm time and DST. The scheduler as a
pure unit-tested function with a DST/leap-day/month-boundary test table. Independent over-run timer in
a separate code path, proven by killing the scheduler. Fail-closed *proven, not assumed*: reboot 20×
mid-run, hold the board in reset, deliberately hang a task. On-device manual-run enforcement. HA
integration with Noise encryption and the availability watchdog, proven by unplugging the board.
Hold/Pause/Stop as three verbs. **Dry-run mode with time acceleration.**

**Exit criteria:** every row of [§5](03-fault-policy.md) has a demonstrated, recorded behaviour;
unit tests green including DST; a simulated season runs unattended.

## v1 — Minimum viable garden

The ten requirements, safely, with the ambiguities resolved by explicit decision. 8 N/C zone valves + 1
N/C master; 30–50 VA transformer on a dedicated 30 mA RCD circuit; one volumetric flow meter positioned
to see **only** the irrigation circuit; IP66 vented enclosure; buried multicore with spare cores and a
draw string; backflow protection per D12. Weekday mask + optional interval + start time + duration in
minutes + priority + enabled. `max_concurrent_zones = 1`. Defer-not-truncate overflow. Device-side
volumes + HA `utility_meter` day/month/year + Water dashboard + a cost sensor. Materialised plan +
per-run records. Hold/Pause/Stop. Three-severity alarms + availability watchdog + weekly heartbeat.
Manual run with on-device enforcement. Freeze interlock. Restriction window. The laminated card and the
alarm runbook.

**Exit criteria:** one month unattended, ≥99 % of planned cycles completed or explicably skipped, water
attribution ≥99 % (NFR-R5).

**Explicitly OUT of scope for v1** — stated plainly, because scope creep here turns a two-month project
into a two-year one: weather intelligence of any kind (manual hold + manual seasonal % are the
substitutes and cover ~80 % of the benefit); soil moisture; litre or mm targets; concurrency > 1;
cycle-and-soak; rainwater tank / pump / source switching; a second independent motorised shutoff;
hydraulic profiling; automated VigiEau ingestion; physical button/display; fertigation; multi-year
reporting beyond what LTS gives free; a custom Lovelace card.

## v2 / v3

Per [§13](12-future-capabilities.md).

## If you want this working *this* season

A defensible minimum that still honours the spirit of all ten requirements: master valve + flow meter +
fail-closed hardware + over-run guard (safety is not cuttable); RTC + on-device sequencer with
minutes-based durations; weekday mask + start time + duration + priority, sequential only; HA `utility_meter` + Water dashboard + availability alert + hold with expiry +
manual run with an on-device timer; run records to JSONL. **Skip in the first cut:** rotation,
`satisfied_by_manual` credit, hydraulic profiling, the config hash/generation machinery, the weekly
digest.

---
