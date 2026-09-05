[← Documentation index](../README.md)

# 10. Water metering

> **Updated by D1 (2026-09-05).** No zone runs below ~1 L/min; the supply is city mains via DN25
> driving multiple sprinklers, so real zone flows will be well into double-digit L/min. **This removes
> the run-metering accuracy problem entirely** — any of the meters below can measure a sprinkler run.
> **It does not change the leak-detection conclusion**, because a slow weep (single-digit L/h) is two
> orders of magnitude below any zone's running flow, and that is the case a turbine physically cannot
> see. The volumetric meter is therefore still recommended, but now for **safety and pool-comparable
> totals** rather than for basic accuracy.

## The sensor decision, which is not obvious

| Meter | Run-total resolution @ 10 L/min, 15 min | Detects a 6 L/h leak? |
|---|---|---|
| YF-B10 turbine (2.1 mL/pulse) | 0.001 % | **Never** — 6 L/h = 0.1 L/min, far below its **2 L/min floor [V]** |
| Single-jet meter, 1 L/pulse | 0.67 % | **Never** — starting flow ~15–30 L/h |
| **Volumetric (piston) meter, 1 L/pulse** | 0.67 % | **Yes — one pulse every 10 min, detected within ~1 h** |
| Volumetric, 10 L/pulse | 6.7 % | Yes but slowly |
| Volumetric, 100 L/pulse | **67 % — useless** | — |

**This table is the whole decision, and the conclusion is worth stating plainly: neither a cheap
turbine nor a cheap single-jet meter can detect a slow leak.** The turbine is physically blind below
its threshold; the single-jet's register does not turn. Only a **volumetric meter with a 1 L/pulse
emitter** gives leak sensitivity, pool-comparable totals, *and* adequate run resolution. Specify that,
or accept that "leak detection" means "burst detection" only.

> ⚠️ **Corrected 2026-09-05 — this instruction cannot be followed, and no longer needs to be.**
> An earlier draft said to buy the meter *with the emitter integral*, because "a retrofit emitter can
> cost more than the meter (€147)". Verified reality: **a 1 L/pulse emitter costs €32–59 HT** —
> Maddalena Reed MVM **€32 (0.5 L/pulse)**, Sensus HRI-B4 €44–59, Itron Cyble K1 €54, Diehl Izar
> Pulse I €52, all **[V]** — and **no European volumetric meter in this class has a genuinely integral
> emitter [V]**; they are all "pré-équipé" bodies with a clip-on head.
>
> **Buy body + emitter separately.** Verified best pick: **Maddalena MVM DN15 + Reed Switch MVM =
> €78 HT**, which gives **0.5 L/pulse — twice the resolution this section specified — for less money.**
>
> ⚠️ **Also corrected: four of the candidate meters previously named here are not volumetric at all**
> — Zenner MTKD is multi-jet, Zenner ETKD single-jet, B Meters GSD8-RFM single-jet, GMDM-I multi-jet
> **[V]**. They fail this section's own defining requirement. Baylan and Arad are not retail-sourceable
> in France (NFR-L4).
>
> **Minimum operating pressure is now datasheet-confirmed** (closing verification item 7):
> **Hunter PGV-101 = 1.5 bar, Rain Bird 100-DV = 1.0 bar [V]**. Rain Bird is the better choice if
> measured pressure is marginal — and this puts D7's gravity-feed conclusion on a datasheet footing.

**Optional second sensor (the "robust" variant):** a turbine in parallel for high-resolution *rate*
(burst detection in seconds rather than tens of seconds, and zone-signature matching). Cross-checking
the two also detects a failed meter — if one reads and the other doesn't, you know something is wrong,
which neither alone can tell you. *Kept for future reference only, since D1 rules it out today: if a
drip/micro-irrigation zone is ever added below the meter's floor, a **YF-S401** (0.3–6 L/min, 5880
pulses/L, ±2 % over 0.15–3 L/min **[V]**) on a filtered, pressure-regulated drip manifold is the known,
cheap fix.*

## Counting

- **Turbine → PCNT** (`pulse_counter`): counted in hardware, so pulses are never lost to Wi-Fi stack
  latency. 8 units available on the classic ESP32; you need 1–2.
- **Reed meter → `pulse_meter` (ISR), not PCNT.** A reed switch bounces for **1–5 ms**, two to three
  orders of magnitude longer than **PCNT's 13 µs filter ceiling [V]** — a raw reed on PCNT counts 3–10
  "litres" per litre. Fix with **all three**: a hardware RC + 74HC14 Schmitt trigger (10 k + 100 nF,
  τ = 1 ms), an opto-isolator (the reed is metres away on a long cable — you want isolation anyway),
  and a software plausibility gate rejecting anything under ~200 ms (at 1 L/pulse, a plumbing-limited
  60 L/min means the minimum legitimate interval is 1 s, so the margin is enormous). Provide the
  pull-up yourself and keep the contact current ≤1 mA to avoid erosion.

## Publishing to HA

Maintain **monotonic `total_increasing` counters restored from persistence before counting resumes**,
with a publish-boundary clamp: HA treats a *large* backward step as a reset but a **small backward step
inside a 10 % band is subtracted from long-term statistics [V]** — so losing 60 s of pulses on a power
cut would slowly corrupt history. Clamp at the publish boundary.

## What the meter is really for

| Detection | Condition |
|---|---|
| **Leak / stuck-open valve** | Flow > 0 with everything commanded closed → close master, latch. Run as a periodic night audit. |
| **Burst / sheared sprinkler** | Flow ≫ zone baseline → abort within seconds, close master. |
| **Blockage, clogged emitters, closed manual tap** | Flow ≪ baseline → don't keep running a zone that isn't watering. |
| **Broken zone wire vs no water** | Pair with current sensing: "commanded on / current flowing / no water" and "commanded on / no current / no water" look identical to you but are completely different repairs. |
| **Emitter drift** | Baseline trending down over weeks — a maintenance hint only continuous metering surfaces. |
| **Weeping diaphragm (grit)** | The *only* sensor that sees this. No electrical layer can. |

---
