# esp-sprinkler — Autonomous Garden Irrigation Controller

An ESP32-based irrigation controller driving up to **8 independent watering networks (zones)** through
electrically actuated valves, executing its schedule **autonomously**, monitored and configured from
**Home Assistant**, and **failing closed** on every fault class.

> **Status: v0 bench firmware built (2026-09-05). No hardware purchased, no water anywhere.**
> Four zones, LEDs where the solenoids will go. `make test` runs 61 host unit tests including a
> DST table and a full simulated season; `make build` compiles the firmware. What remains for v0 is
> the hardware demonstration checklist in [§21](docs/21-bench-procedure.md).
> The specification lives in [`docs/`](docs/). Items marked **[V]** were verified against source,
> datasheets, or the live Home Assistant instance; **[E]** are estimates; open questions are tracked in
> the [verification backlog](docs/16-verification-backlog.md).

---

## The one-paragraph version

Zones run **strictly sequentially** off a DN25 mains feed. The ESP32 owns the schedule, the clock and
the water counters, and keeps watering with **no Wi-Fi, no Home Assistant and no internet** — a network
fault must never stop a run or trigger a reboot. Home Assistant is where rules are *authored* and
results are *observed*; it is never in the control path. Above all, **no valve may ever remain open**:
that guarantee comes from normally-closed 24 VAC solenoids, a master valve in series, and a hardware
deadman — not from firmware.

## The paramount invariant

> **P0.** *"The system must never end up in a state with an electric valve remaining open — that would
> lead to flooding and uncontrolled costs."* — owner, 2026-09-05
>
> Every other requirement yields to this one. Where P0 conflicts with availability, watering quality,
> convenience, or elegance, **P0 wins**.

---

## Documentation

### Start here

| Document | What's in it |
|---|---|
| **[1. Design principles, overview & glossary](docs/01-overview.md)** | P0–P7, the system diagram, and the vocabulary the rest of the docs assume. **Read this first.** |
| **[14. Decisions — taken and open](docs/13-decisions.md)** | What has been settled (D1 flow rates, D2 buy-vs-build, D3 network autonomy, D4 no watering window) and what still needs an answer. |
| **[5. Fault-behaviour policy](docs/03-fault-policy.md)** | The 15-row table of what happens on every fault class. **The authoritative statement of intended behaviour** — it needs sign-off before code. |

### Requirements

| Document | What's in it |
|---|---|
| [4. Functional requirements](docs/02-functional-requirements.md) | FR-1…FR-10, traced back to the ten original requirements and disambiguated. |
| [6. Non-functional requirements](docs/04-non-functional-requirements.md) | Reliability, safety, security, maintainability, environmental, longevity, installability, and French regulation. |
| [12. Challenges to the stated requirements](docs/11-requirement-challenges.md) | Where the original ten were ambiguous or self-contradicting, and how each was resolved. |

### Design

| Document | What's in it |
|---|---|
| [7. Hardware architecture](docs/05-hardware.md) | Valves, relays, the deadman, the pin map, **the bench tests that gate the relay board**, fault coverage, budget. |
| [7b. Buy vs build (D2)](docs/06-buy-vs-build.md) | The weighted matrix, 10-year TCO, why every commercial controller was eliminated, and the reversibility argument. |
| [8. Firmware architecture](docs/07-firmware.md) | ESPHome + custom C++ components, the state machine, persistence, timekeeping, crash recovery, testing. |
| [9. Home Assistant integration](docs/08-home-assistant.md) | The live instance as it actually is, the entity model, where schedule truth lives, and the dashboard. |
| [10. Water metering](docs/09-water-metering.md) | Sensor choice, pulse counting, and what the meter is really for (hint: safety, not billing). |
| [11. Alarms](docs/10-alarms.md) | Severity taxonomy, what latches, and avoiding alarm fatigue. |

### Planning

| Document | What's in it |
|---|---|
| [13. Proposed new capabilities](docs/12-future-capabilities.md) | Rain skip, hydraulic profiling, cycle-and-soak, leak audit, freeze protection — by phase. |
| [15. Roadmap](docs/14-roadmap.md) | v0 bench → v1 garden → v2/v3, with what is **explicitly out of scope for v1**. |
| [16. Risk register](docs/15-risk-register.md) | R1–R23, ranked. The three that would keep me awake are called out. |
| [16. Verification backlog](docs/16-verification-backlog.md) | Every unverified claim, with the concrete check that resolves it. |
| [17. Repository layout](docs/17-repository-layout.md) | Where code, tests, YAML and docs will live. |
| [18. Changelog](docs/18-changelog.md) | What changed in the spec, and why. |
| **[21. v0 bench procedure](docs/21-bench-procedure.md)** | **The v0 exit criteria as a checklist** — every fault-policy row, the three go/no-go board tests, and what v0 deliberately does not cover. |
| **[20. Component summary](docs/20-component-summary.md)** | **Start here for shopping.** One line per component, mandatory first, optional grouped by purpose so each option's cost is visible. |
| [19. Bill of materials](docs/19-bill-of-materials.md) | The full working: per component mandatory/optional, role, minimal/recommended/robust and a pick, with 166 verified prices. |

---

## Decisions taken so far

| # | Decision | Answer |
|---|---|---|
| **D1** | Per-zone flow rates | **No zone below ~1 L/min** — city mains via DN25, multiple sprinklers per zone. Removes the metering-accuracy risk; leak detection still needs a volumetric meter. |
| **D2** | Buy vs build | **Use the ESP32 and relays already owned; build the software.** No custom PCB, no OpenSprinkler, no cloud controller. [Full analysis →](docs/06-buy-vs-build.md) |
| **D3** | Keep watering when Wi-Fi drops? | **Yes.** The controller must not depend on Wi-Fi, and a network fault must never cause a reboot. HA warns; it does not intervene. |
| **D4** | Watering window | **Fully delegated to the operator — no window, no cap.** Start time + duration; the cycle runs to completion. Removes the overrun policy entirely. |
| **D5** | Soil | **Heavy clay** → cycle-and-soak and multiple programs/day move into **v1**. |
| **D8** | Power cut mid-run | **Alert, bounded resume, progress %, actionable notification** with a safe default of abandon. |
| **D11** | Rain gauge | **Yes** — makes rain-skip autonomous rather than HA-dependent. |
| **D13** | Electrical work | **Owner is the electrician.** No contractor dependency, no labour in the BOM. |
| **D15** | Location | **Indoor technical room.** Retires the IP66 enclosure and risk R5 (condensation) outright. |
| **D14** | Budget | **≈€1 751 recommended** — see [§19](docs/19-bill-of-materials.md). |
| **D16** | Manifold location | **Recommend indoors** — cheaper *and* deletes risk R10. Conditional on a floor drain and on measuring the pipe distance. |

## Next steps

1. **Take the five measurements first — €12 and an afternoon.** They move ~€600 of decisions from
   guess to fact: **(a)** pipe distance *D* from the technical room to the zone fan-out (decides D16),
   **(b)** does the room have a floor drain (a hard go/no-go on D16), **(c)** static and dynamic
   pressure at the tie-in (decides valve brand and meter size), **(d)** per-zone flow with a bucket and
   stopwatch, **(e)** **bench tests T1 and T3 on the relay board you own** — ~20 min with a DMM and
   your ears, and **either can disqualify the board and change the whole 24 VAC topology.**
   Do (e) before ordering anything. Record results in the repo as commissioning evidence.
2. **Commit to the external safety layer** — NC master valve, a latching max-runtime backstop on the
   **zone-side** common, an NC kill switch in the overall 24 VAC common, and a manual bypass. It is controller-agnostic and **never wasted under any
   future decision**, so it needs no further analysis. ~€50–110.
3. **Buy a 2-channel relay module for the 9th channel** (the master valve). ~€3, and it buys failure
   diversity for the series element.
4. **Answer the remaining open decisions** — D5 (soil type and slope, which decides whether
   cycle-and-soak and multiple programs/day are v1 or v2), D6 (a pump, ever?), D11 (add a rain gauge
   while the board layout is open?), D12 (does the commune require a disconnecteur BA?).
5. **Then:** ADRs for each decision, a `.gitignore` with `secrets.yaml` in it (NFR-SEC5), and **v0 on a
   desk with LEDs** — proving every row of the [fault-behaviour policy](docs/03-fault-policy.md) before
   any water is involved.

---

## One honest question before starting

The build is worth roughly **165 extra hours over ten years** versus buying a controller, and
essentially all of that premium buys **FR-7, FR-8 and FR-9** — durable per-run records and
plan-vs-actual, three attributed pause verbs with a max-hold, and three-severity latching alarms. No
off-the-shelf controller provides them.

**If "it waters reliably and I can see litres in Home Assistant" would genuinely satisfy you, buy an
OpenSprinkler and stop here.** That is a legitimate answer and it is much cheaper. Everything in
`docs/` assumes you want the specification as written.
