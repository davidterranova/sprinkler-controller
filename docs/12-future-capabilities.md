[← Documentation index](../README.md)

# 13. Proposed new capabilities

## v1 (safety and correctness — not optional)

| Capability | Why |
|---|---|
| **Master valve + unexpected-flow shutoff** | Turns "stuck zone valve" from unbounded to bounded damage. The highest-value hardware addition in the project. |
| **Hardware deadman + max-runtime backstop** | ~€2. The only thing that closes a valve when the application is absent. |
| **Freeze interlock** (threshold **above** 0 °C) | Ice splits fittings, and a frozen path can leave a valve unable to seat. Cheap; prevents a real winter failure. |
| **Manual bypass valve + external NC kill switch** | The garden survives the electronics; anyone can hard-stop water with no tools and no app. |
| **Restriction window** (configurable forbidden hours) | France-specific and a hard constraint. Manual entry in v1. |
| **Global seasonal adjust %** | **The best value-for-effort feature in the whole document** — one dial captures most of ET's benefit for ~2 % of its cost, and ESPHome's `sprinkler` already has `multiplier_number`. |
| **Dry-run simulation with time acceleration** | Build it in v0. It pays for itself immediately. |
| **Commissioning "test all zones" routine** | Invaluable at install and after every winter. |

## v2 (intelligence and protection)

| Capability | Notes |
|---|---|
| ~~**Rain skip**~~ **→ moved to v1 by D11 (local tipping-bucket gauge)** | The highest-value/lowest-cost capability you didn't ask for. Needs either a **tipping-bucket gauge on the board** (truly autonomous — recommended, decide now while the layout is open) or a pushed HA hint with a staleness timeout. Consider also a **hardware rain-sensor NC contact in the 24 VAC common**, which works even if the ESP32 hangs. |
| **Forecast-based rain delay** | Requires `weather.get_forecasts` + a template; HA-dependent by construction. |
| **Per-zone hydraulic profiling** | Learn each zone's flow signature over a few weeks, alert on deviation (clogged emitter, burst line, partly-stuck valve). **The strongest payoff from a flow meter you've already fitted.** |
| **Night-time leak audit** | Close everything, watch the meter for 10 minutes. Cheap, and catches what nothing else does. |
| ~~**Cycle-and-soak**~~ **→ moved to v1 by D5 (heavy clay)** | Split a long run into pulses to avoid runoff on clay/slopes. **The best agronomic win available** — and on clay with sprinklers, the maximum *useful* cycle can be as low as 5 min needing 6 cycles/day, which would force this. |
| ~~**Resume-after-short-outage**~~ **→ moved to v1 by D8** | The refined version of FR-3.7, gated on outage < 10 min, trusted clock, still inside the window. |
| **Valve exercise / anti-stiction** | Off-season cycling so valves don't seize over winter. |
| **Volume-based targets (litres)** | Always bounded by a hard max duration. |
| **Actionable notifications** ("skip today / water anyway") **+ weekly digest** | Keeps your eye on the numbers and makes alarms useful rather than noise. |
| ~~**Physical button + status LED**~~ **→ v1 optional (FR-11), owner request** | "No phone needed" is a real quality bar in a garden — for guests, a house-sitter, or a gardener. |
| **Automated drought level from VigiEau** | Maps to the restriction window. Open dataset + API. |
| **Water budget / monthly quota** | Otherwise a quota is trivially bypassable and useless during a restriction. |

## v3 (agronomy and infrastructure)

ET0-based scheduling (FAO-56, targets in **mm**, per-zone crop coefficients and precipitation rates —
be honest that this is a large step for a modest gain over seasonal adjust); soil-moisture feedback,
first as advisory and a skip-veto, only later as a closed loop; **rainwater tank** with level sensing,
pump control with dry-run protection and source switching (**mandatory total air gap** for any mains
top-up — NFR-LEG1); mainline pressure sensing and pressure-decay leak tests (the most sensitive test
available, and the only one that catches leaks below the meter's starting flow); an independent
motorised main shutoff; per-zone cost and multi-year reporting; fertigation (**which forces the
disconnecteur BA decision**); greenhouse misting as a separate high-frequency program type.

---
