[← Documentation index](../README.md)

# Design principles, system overview & glossary

## 1. Design principles

These resolve dozens of small arguments later, so they are stated first.

> **P0 — THE PARAMOUNT INVARIANT.** *"The system must never end up in a state with an electric valve
> remaining open — that would lead to flooding and uncontrolled costs."* — owner, 2026-09-05.
>
> Every other requirement yields to this one. Where P0 conflicts with availability, watering quality,
> convenience, or elegance, **P0 wins**. This is the single sentence to re-read whenever a design
> question feels balanced.

- **P1 — Fail-closed is a hardware property, not a software behaviour.** Software can only ever
  *command* a valve closed. The guarantee that no water flows when things go wrong must come from
  the valve mechanism and the power topology. P0 is not achievable in firmware alone — this is why
  the deadman circuit ([§7](05-hardware.md)) is mandatory rather than a nicety.
- **P2 — Availability is traded for safety at every ambiguous decision.** A garden tolerates a missed
  day easily and a missed week in spring. It does not tolerate a flood. This is the opposite of the
  instinct most software engineers bring, so it is written down.
- **P3 — The ESP32 is the execution authority; Home Assistant is the editing and observation
  authority.** HA is never in the control path. If HA is down, watering proceeds exactly as planned.
- **P3b — A network fault is not a water fault.** Loss of Wi-Fi, HA, or internet **shall not stop a
  run, shall not suspend the schedule, and shall not cause a reboot.** HA's job is to *report* the
  loss of connectivity, not to react to it. *(Owner, 2026-09-05: "the ESP32 should not rely on Wi-Fi
  to be able to operate… HA should warn if connection is lost but that should not prevent the system
  from functioning.")* Note this makes `api: reboot_timeout: 0s` and `wifi: reboot_timeout: 0s` a
  **stated requirement**, not a tuning choice — see [§16 R2](15-risk-register.md).
- **P4 — Every open-valve command carries its own expiry.** No API, service call, button, or UI action
  in this system may open a valve without also saying *for how long*. Enforced at the lowest driver
  layer, not in the scheduler.
- **P5 — "Closed" must be observable, not merely commanded.** Hence a flow meter and a master valve
  are **v1 safety components**, not v2 metering niceties.
- **P6 — Anything HA-sourced (rain, forecast, seasonal hints) carries a TTL and decays to the
  *permissive* default.** A broken HA integration must never be able to silently kill the garden.
- **P7 — The logic that decides whether water flows is unit-tested off-device.** If it lives in
  ESPHome YAML lambdas, it is untestable, and untestable is unacceptable for this class of fault.

---

## 2. System overview

```
                            ┌────────────────────────────────────────┐
   230 V AC ──[RCBO 30mA]──▶│ IP66 enclosure, "Local technique"      │
                            │                                        │
                            │  ┌──────────┐   ┌──────────────────┐   │
                            │  │ 5 V SMPS │   │ 24 VAC 40 VA txf │   │
                            │  └────┬─────┘   └────────┬─────────┘   │
                            │       │                  │             │
                            │  ┌────▼──────────┐       │ valve rail  │
        Wi-Fi ◀── HA ──────▶│  │  ESP32 (IDF)  │       │             │
      (native API,          │  │  + DS3231 RTC │       │             │
       observation only)    │  └──┬─────────┬──┘       │             │
                            │     │         │          │             │
                            │  ┌──▼──────┐  │   ┌──────▼──────────┐  │
                            │  │ 9 relay │  └──▶│ DEADMAN gate    │  │
                            │  │ channels│      │ + LATCHING      │  │
                            │  │ direct  │      │   max-run timer │  │
                            │  └──┬──────┘      │ + NC kill loop  │  │
                            │     │  dry ctcts  └──────┬──────────┘  │
                            │     └──────────────◀─────┘             │
                            └─────┼────────────────────────────────────┘
                                  │ 24 VAC SELV, buried multicore
                    ┌─────────────┴──────────────┐
                    │                            │
              ┌─────▼─────┐              ┌───────▼────────┐
              │  MASTER    │  ───────▶   │ Zone 1 .. 8    │  (N/C solenoids)
              │  valve NC  │             │ manifold       │
              └─────┬──────┘             └────────────────┘
                    │
              ┌─────▼──────────────┐
              │ DN20 volumetric    │  1 L/pulse — one meter serves all 8 zones
              │ water meter        │  *because* execution is sequential
              └────────────────────┘
```

**The single most important architectural consequence of sequential execution:** one flow meter, one
current sensor and one pressure sensor on the shared main give complete per-zone data. Eight of
anything is wasted money.

---

## 3. Glossary

| Term | Meaning |
|---|---|
| **Network / Zone** | One independently valved irrigation circuit. 8 of them. Used interchangeably; "zone" is used in code and entity IDs. |
| **Program** | A `(zone set, window, day predicate)` tuple. Multiple programs allow >1 cycle/day. |
| **Cycle** | One pass through the enabled zones of a program, executed sequentially. |
| **Run** | One zone opening once, with a duration and/or volume target. |
| ~~**Window**~~ | **Withdrawn by D4.** There is no window and no latest-finish — a program has a **start time**, and each zone a **duration**. The cycle runs to completion. |
| **Master valve** | A normally-closed solenoid upstream of the whole manifold; hydraulically in series with every zone. |
| **Deadman** | A charge-pump circuit that de-energises the valve rail unless the MCU keeps emitting a square wave. |
| **Fail closed** | On loss of drive energy, valves reach/remain closed without any MCU action. |
| **Hold / Pause / Stop** | Three distinct verbs — see [FR-8](02-functional-requirements.md#fr-8--pause--hold--stop). |

---
