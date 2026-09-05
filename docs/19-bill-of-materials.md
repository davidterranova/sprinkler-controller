[← Documentation index](../README.md) · [Quick component summary →](20-component-summary.md)

# 19. esp-sprinkler — Bill of Materials (decision-ready)

**Site:** Combaillaux, Hérault (34). **Date:** 2026-09-05. **Answers decision D14.**
**Supersedes** the indicative budget in [`docs/05-hardware.md`](../docs/05-hardware.md) §Indicative budget
and the enclosure/cable assumptions in `docs/04-non-functional-requirements.md` (NFR-E1/E2, NFR-L5).

---

## The answer, up front

| | |
|---|---|
| **Minimal** | **≈ €688** — works, but buys a meter that cannot detect a leak and a latch that clears on a power cut. Two of its lines are *dangerous*, not merely thin. |
| **⭐ Recommended** | **≈ €1 751** — the build I would put in. 40 % hydraulics, 21 % power+enclosure, 15 % sensors, **9 % safety layer**, 6 % spares, 0 % trenching. |
| **Robust** | **≈ €3 433** — and ~€690 of that is two components I actively recommend against. Substitute those and it is ~€2 870 for comfort, not for additional P0 coverage. |

**Top five recommendations, in order of how much they matter:**

1. **Move the manifold into the technical room (D16, §2).** Verified prices make this both **cheaper
   (≈ €360 at *D* = 20 m)** and dramatically more reliable: it **deletes risk R10** — *"buried splice
   failure, the leading cause of death for irrigation wiring"* — rather than mitigating it. Break-even
   is at *D* ≈ 58 m. **Conditional on the room having a floor drain.**
2. **Spend the €160 on the safety layer without arguing about it (§C).** Deadman €38, latching
   backstop €105, kill switch €17. It is **9 % of the build** and it is the only thing that delivers
   P0. Use a **Set/Reset bistable, not a télérupteur**, and put the timer on the **zone-side common**.
3. **Buy the water meter on *starting flow*, not on brand (§E1).** **Maddalena MVM DN15 + Reed Switch
   MVM = €78 HT** gives **0.5 L/pulse** — cheaper *and* twice the resolution the spec asked for — and
   **four of the spec's named candidate meters are not volumetric at all.** Add a **€14 turbine** so
   the safety instrument can detect its own failure.
4. **Do the five measurements before spending anything material (§17).** €12 and an afternoon moves
   ~€600 of decisions from guess to fact — and **T1/T3 can disqualify the relay board you already
   own**, which would change the whole 24 VAC topology.
5. **Take the Hydreon RG-15 (€99) over a tipping bucket (€188–264) (§F1).** Cheaper *and* lower
   maintenance: no funnel to block, and a blocked funnel fails *silently* toward watering in the rain.

**Two things that are cheaper than the spec assumed:** the pulse emitter (**€32–59, not €147**) and
the enclosure (**an indoor DIN cabinet, which deletes risk R5 outright**).
**One thing that is dearer:** valve boxes at **€100 each** — which is what tipped D16.

---

## 0. How to read this

Every component gets four things, per the owner's request:

1. **Mandatory / Optional** — and if optional, *exactly what you lose by skipping it*.
2. **Role** — one or two sentences.
3. **Minimal / Recommended / Robust** — real products, real part numbers, real prices.
4. **My pick, and why** — tied to criticality, failure impact, or maintenance cost.

**Price marks.** `[V]` = I fetched a live listing and read the price. `[E]` = estimated or inferred —
treat as a planning figure, not a quote. Prices are **EUR, TTC (incl. 20 % VAT)** unless marked HT.
Where a supplier quotes HT I say so, because the €/HT gap is 20 % and it distorts totals.

> ⚠️ **New this month, and it changes small-parts sourcing.** The EU's €150 customs-duty exemption
> ended **1 July 2026**. A **flat €3 customs duty now applies *per item*** (not per parcel) on
> non-EU consignments up to €150, until 1 July 2028, on top of VAT. **[V — European Commission,
> DG TAXUD guidance, 2026-06-08]** Practical effect: a €1.50 PCF8574 from AliExpress now lands at
> roughly €1.50 + €3 duty + VAT ≈ **€5.40**. **AliExpress is no longer the cheap option for
> individual cheap parts.** Buy small electronics as *one* multi-part order from an EU stockist
> (Mouser FR, Reichelt, TME, Amazon.fr) or accept the €3/item. This is not in the spec and it
> materially changes §B and §J.

---

## 1. What has changed since the spec was written — and three places the spec is wrong

### Changed by decision (2026-09-05)

| Decision | BOM consequence |
|---|---|
| **D15 — indoor technical room** (pool pump room; water, network, mains; dry, safe) | **The IP66 outdoor enclosure requirement collapses.** NFR-E1 (IP66/UV/IK), NFR-E2 (membrane breather, desiccant, downward glands), NFR-E3 (direct sun) and NFR-E5 (insects) are **void for the controller**. A plain indoor DIN wall cabinet is sufficient. Saves ~€40–90 and removes risk **R5 (condensation)** — which the risk register rates "high likelihood, high impact" — **entirely**. It also opens the far bigger question in §2. |
| **D13 — owner is a qualified electrician** | No contractor allowance anywhere. The RCBO/consumer-unit work is a €25 part and an hour, not a €200 call-out. It also means the mains side can be done *properly* rather than *avoided* — so I specify it fully per NF C 15-100 rather than hiding behind a plug-top transformer. |
| **D5 — heavy clay, cycle-and-soak in v1** | **Hydraulically: almost nothing changes.** Cycle-and-soak is N passes over the same zone list — same valves, same flow, same pipe. Two second-order effects: (a) valve operations per year rise from ~730/zone to ~2 200–4 400/zone with 3–6 passes/day, which is still **~1/45th** of the SRD relay's 10⁵ rating and ~5 % of a PGV solenoid's life — irrelevant; (b) **more valve cycles means more inrush events**, so do not undersize the transformer (§D1), and it makes the **120-mesh filter mandatory rather than advisory**, because grit is admitted on every open and a weeping diaphragm is the one fault no electrical layer sees. |
| **D11 — tipping-bucket rain gauge wanted** | New line item (§F1). Must be a **dry-contact reed pulse wired to a GPIO**, not a wireless HA integration — otherwise it fails P3b/FR-1.1 (autonomy) and P6 (HA-sourced inputs decay to permissive). |
| **D6/D7 — no pump now; future gravity rainwater tank** | Only cheap *provisions* are bought now (§K). The spec's finding stands and is now **confirmed against datasheets**: see §K2. |
| **D1 — no zone below ~1 L/min, DN25 mains, multiple heads/zone** | Fixes the meter choice (§E), allows one meter for all zones, and sets PE32 as the default lateral size. |
| **D2 — ESP32 DevKit + 8-ch relay board already owned (€0)** | 9th channel is a €4 module (§B1). |
| **D4 — no watering window** | Confirms the backstop belongs on the **zone-side common** (§C2), not the 24 VAC common. Nothing to buy differently, but it is *why* §C2 needs two devices rather than one. |

### Three corrections to the spec's own numbers

**1. The transformer sizing figure is wrong — it double-counts the 50 Hz derate.**
NFR-P2 and R18 state "Hunter PGV-101 at 24 VAC/50 Hz is **475 mA / 11.4 VA** inrush, 230 mA holding",
described as 28 % above the US 60 Hz figure. The Hunter PGV specification sheet gives, verbatim:

> *"24 VAC solenoid: – 350 mA inrush, 190 mA holding, 60 Hz — **370 mA inrush, 210 mA holding, 50 Hz**"*
> **[V — Hunter PGV Valve Spec sheet, "Solenoid Specifications"]**

**370 mA *is* the 50 Hz figure.** The spec appears to have taken 370 mA (already 50 Hz) and applied the
28 % uplift again, yielding 475 mA. Correct figures: **8.88 VA inrush / 5.04 VA holding per valve at
50 Hz**; worst case (master + one zone, both inrushing) **≈17.8 VA**, holding **≈10.1 VA**.
**Consequence: a 30 VA transformer is comfortable and a 24 VA unit is defensible.** The spec's
"30–50 VA" conclusion survives — it was right for the wrong reason — but do not let anyone talk you
up to 60 VA on the strength of the 475 mA number. (Rain Bird 100-DV is *lower* still: **0.30 A inrush
/ 0.19 A holding, 7.2 VA / 4.6 VA at 60 Hz** **[V — Rain Bird DV Series Tech Spec]**.)

**2. "A retrofit pulse emitter can cost more than the meter (€147)" is out of date, and it changes the
meter recommendation.** A **Sensus HRI-B4/D1** pulse sensor is listed at **€44.00 HT** by a French
distributor **[V — compteur-energie.com]**, against a Sensus 620 body of roughly €25–55 HT. So the
emitter costs *about the same as* the meter, not more. Buying integral is still simpler, but the
retrofit route is now a legitimate way to get a **genuinely volumetric, MID R400, ACS-certified**
meter for ~€100 HT — which is better than any integral-emitter meter I could verify in France at
that price. See §E1.

**3. "The ESP32-WROOM DevKit has no MAC/PHY" is half wrong, and the correct fact kills the LAN8720
option outright.** The classic ESP32 **does contain an Ethernet MAC (EMAC)**; it lacks only a PHY
**[V — ESP-IDF Ethernet programming guide]**. But the RMII interface to a LAN8720 consumes
**GPIO 0, 19, 21, 22, 25, 26, 27** **[V — esphome-docs `ethernet.rst`]** — which collides head-on
with this project's **I²C bus on GPIO21/22** (DS3231 + PCF8574), and the standard GPIO0-clock
workaround pushes the 50 MHz clock out on **GPIO17**, which is the **master valve** pin.
**LAN8720 is not merely awkward here, it is pin-incompatible with the published pin map.** A
**W5500 over SPI** is supported by ESPHome and needs ~5 pins from the 11 spares — that is the only
viable retrofit. See §B7 for the verdict (short version: don't, in v1).

---

## 2. D16 — THE decision this document exists to make: where does the valve manifold live?

D15 moved the controller indoors. The question nobody has asked is whether the **valves** should
follow it. They should — probably — and it is worth more than every other line in this BOM combined.

### The two topologies

| | **T-GARDEN** (spec as written) | **T-INDOOR** (proposed) |
|---|---|---|
| Water | DN25 tie-in → **one buried PE32 mainline**, length *D* → filter, master, 8 zone valves in 2 buried boxes → 8 short laterals | DN25 tie-in **inside the room** → filter, meter, bypass, master, 8 zone valves on a **wall board** → **8 buried laterals**, running together for *D* then fanning out |
| Electrics to the garden | **13-core SELV multicore**, length *D*, in TPC rouge, terminating in **18 gel splices in a flooded box** | **None. Zero buried conductors. Zero buried splices.** |
| Valve boxes | 2 × jumbo | **None** |
| Where you service a valve | Kneeling in a wet box, in the dark, in February | Standing up, dry, with a light and a bench |

### What it costs and what it saves

*Let **D** = pipe distance from the technical room to where the zones fan out. This is the one number
you must measure before ordering anything.*

**Deleted by T-INDOOR** *(the buried-electrical BOM, in full)*

| Item | Cost avoided |
|---|---|
| 13-core buried irrigation multicore, 75 m — extrapolated from a verified **7-core × 75 m at €189.00 TTC [V, arrosage-distribution.fr ref 02.51017]** (5-core €138.00, 3-core €82.00, same source **[V]**) | **~€330 [E]** |
| **2 × Regard Jumbo valve box** — Rain Bird VB-JMB **€106.00 TTC [V, arrosage-distribution.fr ref 15.VBJMB]** / Rain Bird HDPE **€100.32 TTC [V, rs-pompes.com]**. *(An Irritec/Carson Jumbo may exist at €47.53 [V — category-page range, specific SKU unconfirmed].)* | **€200.64 [V]** |
| Gaine TPC rouge DN40, 25–50 m + draw string | ~€40–70 [E] |
| 18–24 × IP68 gel splices — Rain Bird **KING €1.25 TTC each [V]**, DBM €1.45 **[V]** | **~€25–30 [V-derived]** |
| Grillage avertisseur rouge | ~€15 [E] |
| **Total deleted** | **≈ €610–645** |

> **The valve boxes are the surprise, and they change the arithmetic.** I had estimated €60 for two;
> the verified price is **€200**. A Jumbo box (70 × 53 × 31 cm) is what you need for four 1" valves —
> round 10" boxes hold **one** valve each, so nine of them would be **€342 [V-derived]** and far worse
> to service. **The valve boxes alone very nearly pay for the extra pipe.**

**Added by T-INDOOR**

| Item | Cost added |
|---|---|
| 7 extra pipe runs × *D* metres (the 8th run replaces the mainline you would have bought anyway). **PEHD PN10 Ø32 = €140.00 / 100 m coil = €1.40/m [V, arrosage-distribution.fr ref 09.1333]**; Ø25 = **€87.00 / 100 m = €0.87/m [V, ref 09.1323]** | *D*=20 m → **+€196**; *D*=40 m → **+€392** |
| Mixed sizing (Ø25 on the short/low-flow zones at €0.87/m) trims this | −~20 % [V-derived] |
| Wall sleeve + 8 penetrations, sealed | +€30 [E] |
| **Mandatory new item: indoor leak detection + tray / drain check** (§G4) | **+€30–55 [E]** |
| **Total added** | **≈ €260 at D=20 m; ≈ €460 at D=40 m** |

> ### Net cash: **T-INDOOR saves ≈ €360 at *D* = 20 m.**
> Break-even is at **≈ €625 ÷ (7 × €1.40/m) ≈ D = 58 m** — far further out than I expected before the
> valve-box and cable prices came back verified. Below ~55 m, **moving the manifold indoors is both
> cheaper and more reliable, and it is not a close call.**

The trench is the *same length* either way — only its first *D* metres gets wider — so if you are
digging it yourself the labour delta is small. And note what the cash figure hides: **the €625 you
avoid is spent almost entirely on things whose only purpose is to survive being buried** (a 13-core
cable, gel splices, conduit, boxes), whereas the €260 you spend is on **pipe, which is the most
reliable component in the entire system.** You are converting fragile spend into durable spend.

### The hydraulic objection, quantified — and it does not survive

The professional design criterion for buried irrigation mains is **≤ 3 m/100 m of head loss
(≈0.3 bar/100 m)**, with velocity **1–1.5 m/s optimal, 2 m/s absolute max**, and singular losses
taken at **~10 % of linear losses** **[V — ARDEPI, *Les conduites et les pertes de charge*, 2013]**.
PE32 PN10 is rated to **3 m³/h at 1.5 m/s** **[V — mon-irrigation.com sizing table]**.

A zone at **1.5 m³/h (25 L/min)** in PE32 (ID ≈ 28 mm) runs at **v = 0.68 m/s** — half the optimum,
so we are nowhere near the limit. Darcy–Weisbach with Blasius friction (Re ≈ 1.9 × 10⁴,
f ≈ 0.027) gives **hf ≈ 0.45 m per 20 m ⇒ ~0.045 bar** *[E — calculated; formula and inputs given so
you can audit it, and it agrees with the ≤3 m/100 m criterion]*.

Put that next to the components you are *already* accepting:

| Element | Loss at ~1.5–2.5 m³/h |
|---|---|
| **20 m extra PE32** | **0.045 bar** *[E, calculated]* |
| **40 m extra PE32** | **0.09 bar** *[E, calculated]* |
| Hunter PGV-101 1" globe, at 2.5 m³/h | **0.1 bar** **[V — Hunter PGV spec, pressure-loss table]** |
| Hunter PGV-101 1" globe, at 7.0 m³/h | 0.4 bar **[V]** |
| Rain Bird 100-DV 1", at 2.0 m³/h | **0.19 bar** **[V — Rain Bird DV Tech Spec]** |
| Rain Bird 100-DV 1", at 5.0 m³/h | 0.31 bar **[V]** |
| 120-mesh disc filter, clean | ~0.15–0.3 bar [E] |

> **The finding: 20–40 m of extra PE32 costs less pressure than the master valve you are already
> fitting.** On a ~3 bar mains that is 1.5–3 %. **Pressure drop is not an argument against moving the
> manifold indoors.** If you have been assuming otherwise — as the framing of the question implies —
> that assumption is wrong by roughly an order of magnitude.
>
> Two footnotes that *are* real, both cheap to handle:
> - **Choose the pipe on velocity, not on loss.** Keep every lateral ≤ 3 m³/h in PE32 (≤ 1.5 m³/h in
>   PE25) and hammer stays mild. This matters more with long laterals than short ones.
> - Hunter's own table shows the PGV-101 going **non-linear above 7 m³/h (0.4 bar → 1.0 bar at 8 m³/h)
>   [V]**. Nothing here goes near that, but it is the reason not to consolidate zones later.

### What T-INDOOR actually buys — the part that isn't money

1. **Risk R10 is deleted, not mitigated.** The risk register calls buried splice failure *"the
   leading cause of death for irrigation wiring"* and can only offer "gel splices; spare cores;
   current sensing detects the open circuit". T-INDOOR has **no buried conductors at all**. This is
   the single largest reliability gain available anywhere in the project, and it costs roughly €0 net.
2. **The valves become maintainable.** A PGV diaphragm rebuild is a 5-minute job on a bench and a
   40-minute job in a wet box. Over a 10-year life with valves explicitly classed as **consumables**
   (NFR-L1), you will do this repeatedly. This is the *maintenance-cost* argument the owner asked to
   be weighed, and it dominates.
3. **NFR-E5 (ants, slugs, rodents in valve boxes) and NFR-E4 (buried joints) become non-requirements.**
4. **Coils and bleed screws stay dry** — the two parts that actually fail.
5. **Everything instrumented is in one place**: tie-in, filter, meter, bypass, master, kill switch,
   labels, laminated card (NFR-D1). Commissioning (NFR-I3) becomes a single-position job.
6. **Buried pipe is pressurised only during its own zone's run** (~20–35 min) instead of for the whole
   cycle (up to 4 h 30). An 8× reduction in exposure window for a buried burst. *Honest caveat: put
   the master valve indoors in T-GARDEN too and the mainline is also depressurised between cycles, so
   this advantage shrinks to the within-cycle window. It is real but it is not the deciding factor.*

### What it costs you — stated plainly

1. **A leak is now indoors.** This is the one serious objection. A weeping diaphragm or a failed union
   wets the technical room instead of the lawn. **Mitigation is mandatory, not optional:** the manifold
   sits over a floor drain or in a bunded tray, and a **wired water sensor on the floor becomes an
   ESP32 input that latches CRITICAL and closes the master** (§G4, ~€45). The room already houses
   a pool pump — a machine with wet seals — so it is probably already tolerant. **Verify: does it have
   a floor drain? If it has no drain and something water-intolerant in it, T-INDOOR is off.**
2. **~86 L of extra standing water** in the ground at *D*=20 m (7 × 20 m × 0.616 L/m). Two firmware
   consequences, both benign and both worth writing down now: each run begins with a **~11 L / ~26 s
   fill transient**, so the *excess-flow* threshold must not trip on it (the 60 s no-flow window in
   `docs/10-alarms.md` is already long enough); and per-run volume carries a **constant ~11 L offset
   per zone** that never reaches a plant. Hydraulic profiling learns it; NFR-R5's 99 % attribution
   target must account for it.
3. **You can no longer isolate one zone from the garden.** Walking back to the room to shut zone 4 is
   mildly annoying. Not worth fitting 8 garden ball valves to fix.
4. **8 wall penetrations** instead of one, or one sleeve carrying eight pipes.

### Verdict

> ### ✅ Move the master valve, filter, meter and bypass indoors **unconditionally**.
> ### ✅ Move the 8 zone valves indoors too, **if** *D* ≤ ~58 m **and** the room has a drain.
> ### 🟡 Between ~58 and ~90 m it still wins on reliability, at €50–300 more.
> ### ⛔ Do not buy the 13-core cable, the TPC conduit, the valve boxes or the gel splices this week.

The first line is free and you should do it whatever else you decide: the filter, meter, bypass and
master valve are the *serviceable, instrumented* items, and putting them in the room costs nothing
extra because the tie-in is there anyway.

The second line is the ≈€360-and-a-lot-of-reliability decision, and it hinges on one measurement.
**Decision rule:**

| *D* (room → fan-out) | Do this |
|---|---|
| **≤ 40 m** | **T-INDOOR.** Cheaper **and** better. No further analysis needed. |
| **40–58 m** | **T-INDOOR.** Still cheaper on cash, and decisively better on R10 and maintenance. |
| **58–90 m** | **T-INDOOR still, on reliability** — it now costs €50–300 more, which is a fair price for deleting every buried joint. But this is where a judgement call appears. |
| **> 90 m** | **Hybrid**: filter, meter, bypass and master indoors; zone manifold in the garden. You buy the multicore, but the parts you service stay dry and you halve the splice count. |
| **No floor drain / water-intolerant room** | ⛔ **T-GARDEN**, with the hybrid indoor head end. This overrides *D* entirely. |

Because the hybrid keeps the expensive indoor items either way, **choosing T-INDOOR is a decision you
can defer until the trench is open** — as long as you do not pre-buy the cable. That is the
buy-first / buy-later split in §15.

---

## 3. §A — Valves and hydraulics

*Quantities assume **T-INDOOR**. Under T-GARDEN, add §H in full and add 2 valve boxes.*

### A1. Zone valves — 8 off — **MANDATORY**

**Role.** One normally-closed 24 VAC diaphragm solenoid per zone. **De-energised = closed by physics:
on power loss line pressure itself forces the diaphragm onto the seat** — no spring, no capacitor, no
stored energy. This is where P0 actually lives; everything electrical is a second line of defence.

**Optional? No.** And note **NFR-S2 / R3**: latching/bistable solenoids and position-holding motorised
ball valves are **prohibited** on zones because they fail *in place*. That is a purchasing decision
made before any code exists and it is irreversible without re-plumbing. Do not let a cheaper
"latching" valve into this list.

| | Product | Unit price | 8 off |
|---|---|---|---|
| **Minimal** | Generic Chinese 1" 24 VAC NC solenoid, Amazon.fr/AliExpress | ~€10–14 [E] | ~€90 [E] |
| **Recommended** | **Hunter PGV 1" FF 24 V**, ref **02.PGV1FF**, arrosage-distribution.fr, in stock, 3–5 working days | **€19.80 TTC [V]** | **€158.40 [V]** |
| **Robust** | Hunter **PGV-101G with flow control** (per-zone throttling) or Rain Bird **100-DVF** | ~€24–30 [E] | ~€200–240 [E] |

**Verified specifications that matter (both from manufacturer spec sheets):**

| | Hunter PGV-101 | Rain Bird 100-DV |
|---|---|---|
| Pressure range | **1.5–10 bar** (20–150 psi) **[V]** | **1.0–10.4 bar** (15–150 psi) **[V]** |
| Flow range | 0.05–9 m³/h **[V]** | 0.05–9.08 m³/h **[V]** |
| Solenoid, **50 Hz** | **370 mA inrush / 210 mA holding [V]** | 0.30 A / 0.19 A at 60 Hz; coil 42–55 Ω **[V]** |
| Loss at 2.5 m³/h | **0.1 bar [V]** | 0.19 bar at 2.0 m³/h **[V]** |
| Pilot filter | — | 90-mesh (200 µm) on diaphragm **and** solenoid **[V]** |

> **My pick: Recommended — 8 × Hunter PGV 1" FF at €19.80 TTC.**
>
> This closes **verification-backlog item 7** ("diaphragm solenoid minimum operating pressure for the
> exact model, ~1–1.5 bar assumed"): **PGV = 1.5 bar, Rain Bird DV = 1.0 bar, both verified.** The
> assumption was right.
>
> Why not Minimal: at €90 vs €158 you save €68 and lose the one thing that matters over ten years —
> **a spare-parts channel**. A PGV diaphragm, solenoid and jar-top bonnet are stocked by every
> irrigation supplier in France for a decade (NFR-L4), and **valves are explicitly consumables**
> (NFR-L1). A generic valve is a whole-unit replacement of an unobtainable part. €68 across 10 years
> against "the garden is down in August" is not a real saving.
>
> Why not Robust: **flow control is not worth €50 here.** It throttles a zone at the valve, but the
> right place to balance a zone is the nozzles, and per-zone flow is measured by the meter anyway.
> **One exception worth taking:** if any zone turns out to be significantly over-pressured at
> commissioning, buy *one* PGV-101 with flow control then. Do not buy eight speculatively.
>
> **Rain Bird 100-DV is an equally good answer** and its **1.0 bar minimum** makes it the better choice
> if your static pressure measures low. Its lower coil current (4.6 VA holding) also shrinks the
> transformer. Choose on price and local stock; do not mix models, because a mixed set defeats the
> spares plan.

### A2. Master valve — 1 off — **MANDATORY**

**Role.** A **hydraulic AND gate**: watering requires two independent circuits to both work. It
restores fail-closed even against a welded zone relay, and it is what bounds the damage in
fault-policy row 8 — *the one genuinely dangerous case*. It is also what makes the flow meter
meaningful: with the master closed, **any** flow is a fault.

**Optional? Absolutely not.** `docs/12-future-capabilities.md` calls it *"the highest-value hardware
addition in the project"*, and it converts unbounded damage to bounded.

> **My pick: an identical Hunter PGV 1" FF, €19.80 TTC [V].** Identical deliberately — it makes the
> spares pool fungible (a spare valve covers 9 positions, not 8) and it means one set of pressure-loss
> and current figures. **Keep the master's relay on a separate module and separate GPIO** (§B1).
> Under T-INDOOR, mount it upstream of the filter's outlet and upstream of the meter, so the meter
> sees only water that got past the master (FR-6.7).
>
> **Do not drop the master between zones** (FR-4.3): it re-primes the mains and invites water hammer,
> and that matters more with long T-INDOOR laterals.

### A3. Manifold — **MANDATORY**

**Role.** The mechanical assembly carrying the 9 valves off one feed. Its topology is also the
project's main reliability control: **"manifold topology concentrates splices in one box"** (R10) —
and under T-INDOOR it concentrates them in *no* box.

| | Approach | Price |
|---|---|---|
| **Minimal** | Build from PVC pressure fittings: PVC-U 40 mm pressure pipe as the header, 9 × threaded tees/saddles, 9 × 1" M threaded adapters, unions at each end. Glued. | **~€60–90 [E]** |
| **Recommended** | **Build it from PVC-U 40 mm with a *union* (raccord démontable) either side of every valve** so any valve comes out without cutting pipe, **plus one spare capped valve position (10 in total)** and a **1/4" gauge port** | **~€90–130 [E]** |
| **Robust** | A pre-assembled commercial 8-way PE/PVC manifold, or two 4-way manifolds | ~€120–200 [E] |

> **My pick: Recommended — build it, with unions everywhere and a spare position.** Two reasons, both
> maintenance-cost:
>
> - **Unions are the difference between a 10-minute valve swap and a pipe-cutting job.** Valves are
>   consumables (NFR-L1). Spending €30 on unions now is repaid on the second valve failure.
> - **The 10th (spare, capped) position is the cheapest future-proofing in the document** and it is
>   exactly what the spec asks for: *"over-provision the buried infrastructure and under-provision the
>   electronics — spare conduit, spare conductors, a spare valve position on the manifold, a tee for a
>   second flow meter."* It costs ~€8 and it covers a 9th zone, a garden tap, a future rainwater
>   branch (§K), or a hose bib for washing filters.
>
> **Pre-assembled manifolds are a false economy under T-INDOOR** — they are designed to sit in a
> valve box, with fixed spacing and no unions, and you will be mounting to a wall board instead. Build
> it. Mount it on a **plywood or PVC backing board screwed to the wall**, valves in a row at
> ~1.0–1.4 m so you can work standing up, **labelled with engraved or laminated labels matching the HA
> entity names exactly** (NFR-I2 — *"mismatched zone numbering is the #1 install-day error and produces
> baffling symptoms"*).

### A4. Filter, 120-mesh — **MANDATORY, and D5 upgraded it from advisory**

**Role.** Keeps grit out of the pilot passages. **The fault-coverage table is blunt: a weeping
diaphragm from grit is covered by "only the flow meter — no electrical layer sees this", and the
mitigation is "mandatory 120-mesh filter upstream".**

**Optional? No — and D5 makes it more important, not less.** Cycle-and-soak means 3–6× more valve
open/close events per day, and each one is an opportunity to admit and trap a particle. Note both
Rain Bird DV and Hunter PGV carry their own **90-mesh (200 µm)** pilot filters **[V]** — the upstream
120-mesh exists to stop those blinding.

| | Product | Price |
|---|---|---|
| **Minimal** | 1" brass or plastic Y-strainer with a **stainless 120-mesh screen** | **~€15–25 [E]** |
| **Recommended** | **1" disc or screen filter, 120-mesh (~125 µm), with a flushing/drain plug**, plus **an isolating ball valve either side** so it can be cleaned without draining the manifold | **~€30–50 [E]** |
| **Robust** | A self-cleaning / semi-automatic flush filter, or two filters in parallel with a changeover | ~€90–160 [E] |

> **My pick: Recommended.** The two isolating valves are the point, and they are ~€20 of the price: a
> filter you must drain the system to clean is a filter you will not clean, and an unblinded filter is
> the *only* thing standing between grit and the one failure mode with no electrical detector. This is
> a pure maintenance-cost argument. **Skip Robust:** self-cleaning filters solve a canal/borehole
> problem; you are on treated city mains where the load is occasional main-flushing debris.
>
> **Under T-INDOOR this gets much better:** the filter is at eye level next to a drain, so cleaning it
> is a two-minute job rather than a trip to the garden with a bucket. Put it **upstream of the meter**
> so debris never reaches the piston chamber.

### A5. Bypass + isolation ball valves — **MANDATORY**

**Role.** A quarter-turn bypass **around the whole manifold** so the garden can be watered with the
controller dead or removed (§C4), plus an inlet isolator so you can work on any of it without turning
off the house.

| | Product | Price |
|---|---|---|
| **Minimal** | 2 × 1" brass PN16 lever ball valve + a tee | **~€25–35 [E]** |
| **Recommended** | **3 × 1" brass PN16 full-bore lever ball valves** (inlet isolator, bypass, bypass outlet) **+ 2 × 1"** for the filter (§A4) **+ 1 × hose-bib takeoff on the bypass** so a hose can be run directly | **~€55–80 [E]** |
| **Robust** | Add a lockable/padlockable handle on the inlet isolator (NFR-SEC7-adjacent) | +€15 [E] |

> **My pick: Recommended.** This is cheap brass and it is what makes the project socially survivable —
> *"the sprinkler computer is broken" must never mean "the garden dies"*. The hose bib on the bypass is
> a €10 addition that turns the bypass from a theoretical feature into the thing you actually use, and
> gives you the **bucket-and-stopwatch flow measurement point NFR-I1 demands** before you can size
> anything.

### A6. Backflow protection — **CONDITIONALLY MANDATORY — make the phone call (D12) before buying**

**Role.** Prevents irrigation water re-entering the potable network.

**The legal position, per the spec and worth restating because it is a €150–400 decision:**
**NFR-LEG2 — a disconnecteur BA at the irrigation piquage is probably NOT legally mandatory here,
because the arrêté du 10/09/2021 art. 6 exempts *maisons individuelles*. [V, per spec]** It remains
strong professional practice; **the commune's *règlement de service* may impose one**; and
**fertigation would make it non-optional**. Separately and absolutely: **NFR-LEG1 — no interconnection
between any private resource (well, rainwater tank) and the potable network. A BA is *not* sufficient
for fluid category 5; a mains top-up to a tank requires a total air gap (AA/AB). [V]**

| | Product | Price |
|---|---|---|
| **Minimal** | A **clapet anti-pollution / disconnecteur CA (HA)** DN20–25 — a simple check valve with a vent | **~€20–45 [E]** |
| **Recommended** | **Make the call to the commune first (D12).** If not required: a CA-type anti-pollution check valve plus, at each valve, the valve's own downstream position. If required: a **disconnecteur BA (Socla / Watts / Honeywell) DN20 or DN25** | **~€25 or ~€150–400 [E]** |
| **Robust** | A **BA regardless**, with the mandated annual inspection accepted | ~€150–400 + annual check [E] |

> **My pick: one phone call, then the cheap option unless told otherwise.** This is the single
> highest-leverage phone call in the project — **the same call also answers whether the commune
> accepts a sous-compteur for the *redevance assainissement* deduction (NFR-LEG6, CGCT R. 2224-19-2)**,
> which could pay back a meaningful fraction of this whole BOM every year. Do not buy a €300
> disconnecteur speculatively, and do not skip the €25 check valve — it is cheap insurance and it is
> what makes the installation defensible if anyone ever asks.
>
> **Hard rule, no exceptions:** if the rainwater tank ever arrives (D6/D7), **its mains top-up must be
> a total air gap** — a tundish discharging above the tank's flood level, not a valve, not a check
> valve, not a BA. Design the tank's inlet that way from day one (§K).

### A7. DN25 tie-in and fittings — **MANDATORY**

**Role.** Connects the manifold to the DN25 city mains, and the manifold outlets to the buried
laterals.

| | Approach | Price |
|---|---|---|
| **Minimal** | A tee cut into the existing DN25 line + PE compression fittings, no isolator | **~€30–50 [E]** |
| **Recommended** | Tee + **isolating ball valve at the tie-in**, PE compression fittings (Plasson/Cepex) at every manifold outlet, PTFE/loctite, plus **8–9 × 1" M-to-32 mm PE compression elbows** | **~€70–120 [E]** |
| **Robust** | Add a **pressure reducer** if static pressure measures >5 bar, and a second isolator | +€40–70 [E] |

> **My pick: Recommended.** The isolator at the tie-in is what lets you work on the whole system
> without shutting off the house, and it is €15. **Measure static and dynamic pressure before buying
> a reducer** (NFR-I1): above ~5 bar a reducer protects the diaphragms and reduces misting losses;
> below 4 bar it is pure loss. **Do not buy one blind.**

### A8. Pipe — **MANDATORY**, quantity depends entirely on §2

**Role.** Carries water from the manifold to the heads.

| | Approach | Price |
|---|---|---|
| **Minimal** | PE25 PN10 throughout | ~€1.00/m [E] |
| **Recommended** | **PE32 PN10 for every run over ~15 m and any zone over 1.5 m³/h; PE25 PN10 for short/low-flow zones.** Sized on **velocity ≤1.5 m/s** — i.e. **≤3 m³/h in PE32, ≤1.75 m³/h in PE25 [V]** | **~€1.30–1.90/m [E]** |
| **Robust** | PE40 PN10 on the two longest runs | ~€2.60/m [E] |

> **My pick: Recommended, sized on velocity not on loss.** Under T-INDOOR at *D*=20 m, budget roughly
> **8 × (20 m + zone length)**; the incremental pipe over T-GARDEN is **7 × D ≈ 140 m ≈ €225 [E]**.
> **Do not buy pipe until you have measured *D* and each zone's flow with a bucket and a stopwatch
> (NFR-I1).** Pipe is the one item where guessing costs real money and a re-dig.
---

## 4. §B — Control electronics

**Already owned, €0 (D2):** ESP32-WROOM DevKit, 8-channel opto relay board.
**Worth remembering what that is actually worth: ~€25.** The real value is that it is *available today*,
so v0 bench work starts this week. **It is not a reason to accept a board that fails test T3.**

### B1. The 9th relay channel (master valve) — **MANDATORY**

**Role.** You need 9 channels (8 zones + master) and own 8. But the reason to use a *separate module*
rather than a 9-channel board is safety, not arithmetic: putting the master — the series element of
the hydraulic AND gate — on a **physically separate board with its own power feed and its own driver
pin** means no single board-level common-cause failure (a shorted rail, a flooded connector, a crack
across the PCB) can weld a zone relay *and* the master closed at the same time. That converts the
"both welded (double fault)" row of the fault-coverage table from *not covered* to *partially covered*
for about €4.

| | Product | Price |
|---|---|---|
| **Minimal** | 1 × 2-channel 5 V opto-isolated relay module, active-low, 3-way NO/COM/NC terminals — AliExpress | **~€2.50 + €3 duty ≈ €5.50 [E]** |
| **Recommended** | **2 ×** the same module from **Amazon.fr** (one fitted, one hot spare in the box) | **~€14–18 for two [E]** |
| **Robust** | A **DIN-mount interface relay with a socket** (Finder 34.51 + 95.05, or Phoenix PLC-RSC) driven from the GPIO through a transistor — no opto board at all | ~€12–18 [E] |

> **My pick: Recommended — buy two, from Amazon.fr not AliExpress.** Two reasons. (1) It is the
> **series element of the safety AND gate**; a spare on the shelf converts "the garden is down until
> a parcel arrives" into a five-minute swap, and NFR-L2 already demands spares on site from day one.
> (2) With the new **€3-per-item EU duty**, the AliExpress saving on a €2 part has largely evaporated,
> and Amazon.fr gives you returns and next-day replacement — which for a P0-adjacent part is worth
> more than €4. **The Robust option is actually tempting** (a socketed DIN relay is more serviceable
> than a Chinese module) but it loses the opto isolation and needs a driver transistor, so it trades a
> verified-by-bench-test property for a tidier rail. Not worth it.
>
> **Wiring discipline that matters more than the part:** run this module's **JD-VCC/power from a
> separate feed** and its input from **GPIO17 direct**. If it shares a 5 V wire with the 8-channel
> board you have paid €4 for nothing.

### B2. PCF8574 I²C I/O expander — **OPTIONAL, and the honest reason to buy it is not safety**

**Role.** Drives the 8 zone relay inputs over I²C instead of 8 GPIOs.

**Optional — what you lose by skipping it:** nothing safety-relevant. **Say this plainly, because the
spec is unusually candid about it and it is worth preserving:** on this hardware the expander's
contribution to P0 is close to zero. The active-low relay board plus the verified fact that the
ESP32's *"pins are output-disabled during reset"* already gives relay-OFF at cold boot, and the
expander **does not help in the case that actually matters** — on a CPU reset (R1) its output latch
survives exactly as a GPIO does, because it has no reset pin either. **The deadman is what delivers
P0.** Skipping the expander is defensible; skipping the deadman is not.

**The real reason to fit it** is that **every relay `IN` line carries a 1 kΩ pull-up, and on GPIO12
(MTDI) that holds a strapping pin high at reset, selects a 1.8 V VDD_SDIO, browns out the 3.3 V flash,
and the board will not boot or flash.** Nine strong pull-ups on nine ESP32 pins is nine chances to
make that mistake — including a future you rewiring the box with cold hands. The expander reduces
that to one pin-map decision made once. It also halves the wiring.

| | Product | Price |
|---|---|---|
| **Minimal** | Skip it — zones direct on GPIO **32, 33, 25, 26, 27, 23, 19, 18**, master 17, deadman 13. **Never use GPIO 0, 1, 2, 3, 5, 6–11, 12, 15.** | **€0** |
| **Recommended** | **1 × PCF8574 breakout module** with address jumpers (set 0x20), `inverted: true`, **at 3.3 V** — plus a **second as a spare** | **~€8–12 for two [E]** |
| **Robust** | A **PCF8574AN DIP-16 IC** on the same perfboard as the deadman, socketed, with proper 4.7 kΩ bus pull-ups — est. **€1.20–2.00 excl. VAT at Mouser.fr / Reichelt [E]** | ~€5 for two + socket [E] |

> **My pick: Recommended.** Take it for ~€5, for the GPIO12 hazard and the wiring, and be clear-eyed
> that it is an ergonomics purchase, not a safety one. **Keep the master on a direct GPIO** so the two
> halves of the AND gate fail independently: an I²C lock-up — a slave holding SDA low, a real event in
> a box that also contains a 24 VAC inductive load — still leaves the firmware able to close the
> master and stop all water.
>
> **Two hard constraints on the drive current, both from the spec and both correct:** if bench test T4
> shows the 3.3 V drive is marginal, change the board's R1 from 1 kΩ to **470 Ω (≈4.4 mA)** — **not**
> the 220 Ω some vendor notes suggest, because nine channels at 9.8 mA would push **88 mA through the
> PCF8574's ground pin against a ±100 mA absolute maximum**. And **do not drive the relays through a
> ULN2003** — it inverts the polarity into all-relays-on.

### B3. Real-time clock — **MANDATORY**, and **the spec's part choice should change**

**Role.** FR-1.3: wall-clock time from a battery-backed RTC, storing **UTC**, with SNTP as an
opportunistic correction only. A power cut of any length must not lose the date, because fault-policy
row 6 says an untrusted clock **refuses all scheduled runs** — so the RTC is what stands between a
30-day offline period and a dead garden.

**Optional? No.** Without it, FR-1.1/1.5 (30 days offline) are unimplementable.

> ### ⚠️ Two traps here, and I found a third that inverts the spec's mitigation for R15.
>
> **Trap 1 — the ZS-042 coin-cell charging circuit (verification item 11), now fully characterised.**
> The **DS3231 chip has no trickle charger**; the charger is a board-level addition on ZS-042 clones —
> a **200 Ω resistor + a 1N4148 diode** from VCC to VBAT. Charge current is only ~0.1 mA, but as the
> cell voltage rises the diode drop falls, so **from a 5 V rail the cell is driven to ~4.5–4.7 V** —
> above the 4.2 V ceiling that even a *rechargeable* LIR2032 tolerates, and in a **primary CR2032 that
> is an overcharge / venting risk** as well as ruining the cell. **[V]**
> **The cheap fix nobody mentions: at 3.3 V VCC the charging circuit does not function at all**
> (insufficient headroom over the cell). The DS3231 is happy at 3.3 V and so is the ESP32. **Run the
> whole I²C bus at 3.3 V and the trap disappears with no desoldering.** If you must run 5 V, lift the
> 200 Ω resistor — and do *not* cut the track from the battery to pin 14, which runs very close to the
> board edge.
> **Sourcing consequence:** LIR2032 is **not stocked by Reichelt or TME** — it is an Amazon/AliExpress
> part, which is exactly the NFR-L4 "10-year liability" pattern. A **CR2032 is €0.44 at Reichelt [V]**
> and will be available forever. **Use a CR2032, at 3.3 V.**
>
> **Trap 2 — R15, and the spec's proposed fix does not work.** R15 correctly says the DS3231 on
> ESPHome's `ds1307` platform cannot report "I lost time" (it reads only the CH bit, never the
> DS3231's OSF flag in register 0x0F bit 7), so a dead coin cell yields a **plausible wrong time** —
> and the mitigation offered is *"or use PCF8563"*. **Checked against the ESPHome source: the
> `pcf8563` driver reads only the stop bit and ignores the VL (voltage-low / clock-integrity) bit, so
> it does not help either.** **[V]**
> **The platform that actually does what R15 wants is `pcf85063`**, whose driver reads the OS bit,
> logs *"RTC halted, not syncing to system clock"* and **refuses to sync** — i.e. it hands the
> firmware exactly the `clock_valid: off` signal fault-policy row 6 needs. **[V]**

| | Product | Price |
|---|---|---|
| **Minimal** | AliExpress **DS3231 + AT24C32 ZS-042** — €1.16 pre-VAT [V-snippet], ships with an LIR2032 and the charger fitted; +€3 duty ⇒ **~€5 landed [E]**. Run at 3.3 V. | **~€5 [E]** |
| **Recommended** | **Gotronic ref 39224 — "Module RTC I²C PCF85063A 333051", €3.95 TTC [V]** (the only cheap, EU-stocked module whose ESPHome driver refuses to sync a lost clock) **+ Gotronic ref 38024 "Module RTC I²C DS3231 GT584", €7.90 TTC, CR2032 supplied [V]** as the accuracy/spare unit | **€11.85 [V]** |
| **Robust** | **Adafruit DS3231 Precision RTC STEMMA QT #5188 — €14.50 incl. VAT, BerryBase.de, 34 in stock [V]** (CR1220, **no charger circuit at all**) or Gotronic ref 34360 ADA3013 at €21.95 TTC [V] | **€14.50–21.95 [V]** |

> **My pick: Recommended — buy both, for €11.85 total.**
>
> This is not indecision, it is the correct answer to a genuine conflict. The **DS3231 has the
> accuracy** you want (**±2 ppm over 0–40 °C ≈ 63 s/year**, against the DS1307's ±20–50 ppm =
> 10–25 min/year), but **its ESPHome driver cannot tell you it lost time.** The **PCF85063A has the
> integrity flag** you need for fault-policy row 6, but is a plain ±20 ppm part. €11.85 gets you both
> on the same bus at different addresses: **run the PCF85063A as the "is my clock trustworthy?"
> oracle and the DS3231 as the accurate source**, and cross-check them — a disagreement is itself a
> CRITICAL. That is a genuinely better answer than either part alone, and it costs €4 more than the
> spec's plan.
>
> If you want one part only: **take the €3.95 PCF85063A.** D15 removed the reason the TCXO mattered
> — the spec's whole case for the DS3231 was *"a garden box swings 50 K, which is exactly why you want
> a TCXO"*, and **a dry indoor technical room does not swing 50 K.** At a stable ~15–25 °C a ±20 ppm
> part drifts ~10 min/year, and it re-syncs from SNTP whenever Wi-Fi is up, which is nearly always.
> **The indoor move materially weakens the DS3231 argument, and that is worth noticing.**
>
> **Do not buy the €14.50 Adafruit board.** It is a nicer board and it solves the charging trap in
> hardware, but running the bus at 3.3 V solves it for free, and the money is better spent on §C.
>
> **Also buy: 5 × CR2032, €0.44 each at Reichelt [V].** And **store UTC**, not local time.

### B4. Level shifting — **NOT NEEDED. Do not buy one.**

**Role it would play:** none, if you design correctly.

A PCF8574 at 5 V has V_IH ≈ 0.7 × V_CC = **3.5 V**, which is *above* a 3.3 V ESP32's output high —
marginal and out of spec. **But the PCF8574, the DS3231 and the PCF85063 all run happily at 3.3 V**,
so **powering the entire I²C bus at 3.3 V removes the boundary entirely** and simultaneously disarms
the ZS-042 charging trap (§B3). That is the design.

> **My pick: buy nothing.** The genuine 3.3/5 V boundary in this project is **the relay opto inputs,
> and that is not an I²C problem and a level shifter does not fix it** — it is bench test **T4**, and
> the fix if it fails is a resistor change (1 kΩ → 470 Ω). **Anyone who reaches for a TXS0108E here
> has misdiagnosed the problem.** *If you want one on the shelf anyway: TXS0108E module ~€6–9 Amazon.fr
> [E].*

### B5. Spare ESP32 board — **MANDATORY (NFR-L2)**

**Role.** A **pre-flashed** replacement. NFR-L3 requires a replacement device commissionable in
<30 min from a git checkout with the lifetime total restorable; that is not achievable if the board
has to be delivered first.

| | Product | Price |
|---|---|---|
| **Minimal** | 1 × ESP32-WROOM-32 DevKit, AliExpress — est. €4–5.50 + €3 duty ⇒ **~€9 [E]** | ~€9 [E] |
| **Recommended** | **2 ×** ESP32-WROOM-32 38-pin DevKit from Amazon.fr (3-packs are common) — est. **€18–25 for three [E]**; or Mouser.fr **ESP32-DevKitC-32E** ~€12 excl. VAT [E] | **~€20 [E]** |
| **Robust** | 1 spare DevKit **+ 1 WT32-ETH01** so the spare can also test the Ethernet path | ~€35 [E] |

> **My pick: Recommended — a 3-pack.** €20 for three boards means one in service, one flashed on the
> shelf, one to destroy on the bench. Given a **10-year design life** and that DevKits are the single
> most likely thing to be discontinued, buying three now is the cheapest possible answer to NFR-L4.
>
> **This closes verification item E4** ("does the DevKit expose VIN?"). On the **official Espressif
> ESP32-DevKitC the power pad is labelled `5V`, not `VIN`** — 30-pin clones commonly label the same
> pad `VIN`. Either way it is the pre-regulator input and it is what you feed from the DIN 5 V supply.
> **The important part is the warning that goes with it: the USB, `5V` and `3V3` inputs are mutually
> exclusive — powering two at once can damage the board.** So once the DIN supply is wired to `5V`,
> **do not plug in USB to reflash without disconnecting it.** Put that on the laminated card
> (NFR-D1); it is exactly the mistake a future you makes at 22:00, and it destroys the controller.

### B6. Spare 8-channel relay board — **OPTIONAL, and I recommend buying one**

**Role.** Replaces the board that all eight zones depend on.

**Optional — what you lose:** on a board-level failure (and the whole reason the master is on a
separate module is that board-level failures are the credible common-cause), the garden is down until
a parcel arrives. In August. **Est. €12–18 Amazon.fr / €4–7 AliExpress [E].**

> **My pick: buy one spare, Amazon.fr, ~€15.** But **buy it only after T1/T3/T4 pass on the board you
> own**, and then **buy the identical model** — the tests characterise a *specific* board, and
> "high/low trigger selectable" variants exist in the wild where a jumper inverts everything and a
> PCF8574's power-on-high state **energises all eight relays**. A spare of a different variant is not
> a spare, it is a trap.

### B7. Wired Ethernet (D15 asks) — **OPTIONAL. Verdict: not in v1.**

**Role it would play.** Replaces Wi-Fi for the HA link.

**The assessment you asked for, honestly.**

1. **It cannot improve any safety or autonomy property, by construction.** P3b and FR-1.1 require the
   controller to run its full schedule **with no Wi-Fi, no HA and no internet, indefinitely**, and
   fault-policy rows 1–3 say a network fault neither stops a run nor reboots the device. A more
   reliable network therefore buys **zero** P0, zero autonomy, and zero availability of *watering*.
   It buys better *observation*. That is a real but second-order benefit.
2. **The spec's technical premise is wrong and the correct fact makes the cheap route impossible.**
   The ESP32 *does* have an EMAC. But **RMII to a LAN8720 consumes GPIO 0, 19, 21, 22, 25, 26, 27
   [V — esphome-docs]**, which takes **GPIO21/22 — this project's I²C bus** — and the usual GPIO0
   workaround emits the 50 MHz clock on **GPIO17, which is the master valve**. There is no pin map in
   which a LAN8720 and this design coexist.
3. **W5500 over SPI is the only viable route**: ESPHome supports `ethernet: type: W5500`, it needs
   ~5 pins from the 11 spares, est. **€8–14 Amazon.fr [E]**.
4. **If you actually want Ethernet, the honest way is a different board**, not a PHY bolted to a
   DevKit: **WT32-ETH01 est. €12–18** or **Olimex ESP32-POE-ISO est. €30–40 [E]**, both ESPHome-
   supported with published pin maps. But the WT32-ETH01 has fewer free GPIOs and a different pin map,
   so it costs a firmware port.

> **My pick: ship v1 on Wi-Fi. Buy nothing now.** Then apply a **decision rule instead of a purchase:**
> **measure RSSI in the technical room for a week.** If it is consistently better than **−70 dBm**,
> Ethernet is solving a problem you do not have. If it is worse than **−80 dBm** or flapping — quite
> possible in a pump room with masonry and a metal-cased pump — spend **€10 on a W5500** and use five
> spare GPIOs. Do not consider the LAN8720. Do not re-board unless you are already rewriting the
> firmware for another reason.
>
> The instinct behind D15's question is right — a wired link in a room that has a network port is
> obviously nicer than Wi-Fi. But this system is *specified* to not care, and spending design effort
> on the network is spending it in the one place the requirements explicitly de-risked.
---

## 5. §C — The safety layer (P0, non-negotiable)

This is the section where "minimal" is sometimes **dangerous**, and I say so.

### C1. Hardware deadman (AC-coupled charge pump) — **MANDATORY**

**Role.** The MCU must continuously emit a square wave; that AC signal is capacitively coupled into a
diode charge pump whose DC output holds the valve-rail enable asserted. Stop the wave and the enable
collapses in ~1 s. It is the **only** layer that catches **R1** — an ESP32 CPU reset leaves GPIO
output-enable *and level* intact, so a crash loop can leave a valve **open indefinitely while the
device looks healthy online**. A level-based enable cannot distinguish "firmware fine, pin high" from
"firmware dead, pin high"; only a *changing* signal passes a capacitor.

**Optional? No.** Skipping it means accepting unbounded, invisible flooding as a live failure mode.
The spec is right that this is the highest value-per-euro item in the build.

**All discrete prices below are DigiKey France, EUR excl. VAT, verified on a live listing today [V].
DigiKey is free over €75 and €25 below it, ~3 days to France — so this order must be consolidated
with §E2 and §J or the shipping dominates.**

| Part | MPN | Unit | |
|---|---|---|---|
| Hex Schmitt inverter, **14-PDIP** | **SN74HCT14N** (TI) | **€0.80** | [V] |
| Schottky diode, **DO-35 through-hole**, 30 V / 200 mA, Vf 800 mV | **BAT85S-TAP** (Vishay) | **€0.43** | [V] |
| Logic-level N-FET, TO-92 | **2N7000** (ST) | **€0.047** | [V] |
| Logic-level N-FET, TO-220 *(overkill — see pick)* | IRLZ44NPBF (Infineon) | €1.54 | [V] |
| **Enable relay**, SPDT, 5 V coil (80 mA / 62.5 Ω), contacts **16 A @ 440 VAC** | **G2RL-1-E DC5** (Omron) | **€3.64** (€3.13 @10) | [V] |
| DIN-socketable relay, DPDT, 5 V coil, 8 A | Finder 40.52.9.005.0000 | €7.25 — **non-stock, 6-week lead** | [V] |
| Perfboard 100 × 160 mm, FR2 | **RE200-HP** (Roth) | **€4.00** | [V] |
| DIN-rail PCB carrier, 17.5 × 90 × 58 mm, cover incl. | **CNMB/1/KIT** (CamdenBoss) | **€7.39** | [V] |
| Resistor + capacitor assortment kits (100 nF, 4.7 µF, 220 k, 10 k, 1 k, 470 R …) | — | ~€18–30 TTC | [E] |

| | Build | Cost |
|---|---|---|
| **Minimal** | SN74HCT14N + 2 × BAT85 + passives + a **2N7000** gating the **G2RL-1-E DC5**, on cut-down perfboard, contact in the 24 VAC common | **≈ €10 excl. VAT + kits [V-derived]** |
| **Recommended** | The same circuit, but **built twice** (one fitted, one spare board), mounted in the **CNMB/1/KIT DIN carrier**, with the **74HCT14 in a DIP socket**, a **test point on the enable node**, and a **front-panel LED on the enable rail** so "enable asserted" is visible without a meter | **≈ €30–40 incl. kits [V/E]** |
| **Robust** | Add a **second, independent** charge pump on a second GPIO gating a *series* second enable relay (two-out-of-two) | **≈ €50–65 [E]** |

> **My pick: Recommended — and note two corrections to the parts list.**
>
> **Use the 2N7000 at €0.047, not an IRLZ44N at €1.54.** The G2RL-1-E's coil draws **80 mA at 5 V
> (400 mW) [V]** — which is ~20× what a 74HCT14 output can source, so **the MOSFET stage is mandatory,
> not optional**. But an IRLZ44N is a 47 A part switching 80 mA: 30× the price for nothing. The
> 2N7000 (200–350 mA, logic-level Vgs(th)) is the correct part.
>
> **The DIN carrier does not fit the perfboard, and you should notice before ordering.**
> CNMB/1/KIT is **17.5 mm = one module wide [V]**; a 100 × 160 mm RE200-HP will not go in it. Either
> buy the wider CamdenBoss CNMB/**4** or /**6** (price not verified, [E]) **or cut the perfboard down
> to ~85 × 55 mm** — which is ample for one 74HCT14, one FET, one relay and a dozen passives. **Cut
> the board; it is free.**
>
> **Build it twice.** The €10 delta is the best insurance in the document: this is a hand-built
> P0-critical circuit, and a bad solder joint on the only one you have stops the project. The second
> board also lets you *destructively* test the first (short the enable, stop the heartbeat mid-run,
> freeze it) which is exactly what bench item **E6** requires.
>
> **Do not build the Robust version.** A second charge pump doubles the number of things that can
> fail *open* — nuisance shutdowns — to reduce a risk that is already covered. That is gold-plating,
> and worse, it trades availability for a safety property you already have.

**Two build details that are easy to get wrong and both are in the spec for good reason:**

- **The 74HCT14 buffer on 5 V is not optional.** A raw 3.3 V GPIO through two silicon diodes yields
  ~1.9 V, which will not reliably turn on a MOSFET gate. The "T" in HC**T** is what accepts 3.3 V
  logic on a 5 V rail. Use **Schottky** (BAT54/BAT85, ~0.3 V) not 1N4148.
- **Generate the square wave from a hardware timer but re-arm it from the supervisory loop.** A purely
  hardware-generated wave keeps running after the firmware dies — a beautiful circuit protecting
  nothing. This is the classic way to get this wrong, and it is a firmware discipline, not a purchase.
- The same 74HCT14 package supplies the Schmitt trigger the reed water meter needs (§E2). One €0.60
  chip, two jobs. Buy **five**.

### C2. Latching max-runtime backstop — **MANDATORY**

**Role.** A hardware timer on the **zone-side common** that trips at ~1.5× the longest legitimate
single-zone run and **latches until manually reset**. It is the only layer that catches "firmware is
running normally but forgot to close zone 4" — converting *unbounded* damage into **bounded** damage.

**It must latch (R23).** A self-resetting timer facing a hung controller re-arms every 60 min and
waters **24× a day** — a safety device manufacturing a flood.

**It must be on the zone-side common, not the 24 VAC common.** With no watering window (D4) a cycle
can legitimately run 4 h 30 with the master continuously energised; a 60-min timer in the overall
common would **trip mid-cycle every time**. On the zone-side common it bounds a *single zone run*,
and the 5–10 s inter-zone settle gap resets it for free — which is why FR-4.3 promotes that gap from
a hydraulic nicety to a **safety requirement**, and why bench test E6 must confirm the timer resets
inside it.

**"Latching timer" is not a purchasable function — confirmed independently.** I had my researcher
attack this from the supplier side while `docs/05-hardware.md` attacked it from the datasheet side,
and both landed in the same place:

- **Finder Série 80's complete function set, read off the live elecdirect product page — AI, DI, SW,
  BE, CE, DE across six ranges from 100 ms to 24 h — contains no latch, no memory and no manual
  reset. [V]** (The spec's own reading of datasheet S80FR ed. X-2025 adds LI/LE, BI and SD and reaches
  the same conclusion.) **60 min sits comfortably inside the range [V]**; the latch does not exist.
- **I could not, and will not, claim that *nothing* on the market latches.** Schneider RE17/RE22,
  Omron H3DK, Crouzet, Carlo Gavazzi, Tele Haase and Broyce Control were all unreachable (RS, Farnell,
  Mouser and Phoenix blocked every request). The honest statement is: **of everything I could inspect,
  none latches.**

> ### ⚠️ Do not use a télérupteur (impulse relay) for the latch. Use a Set/Reset bistable.
>
> `docs/05-hardware.md` now makes this point and it is correct and load-bearing: **a télérupteur
> toggles on every pulse**, so a stray pulse, contact bounce, or a second trigger from a
> crash-looping controller flips it **back on**. A bistable with **separate SET and RESET inputs**
> cannot: *"only a pulse on the RESET command will open the relay's contacts."* The ESP32 path can
> only ever SET; the timer can only ever RESET; nothing can accidentally un-latch.
>
> **My researcher hit the same conclusion from a completely different direction, which corroborates
> it:** every télérupteur that could be priced — **Legrand CX³ 412400 at €19.58 HT / €23.50 TTC [V]**,
> **Schneider Acti9 iTL A9C15032 at €33.60 HT [V]** — has a **230 V coil**, while the entire deadman
> and timer chain lives in the 24 V domain. Driving a 230 V coil from 24 V logic means inserting an
> interposing relay **into the very safety chain you are trying to keep simple**. The
> **Finder 13.12.0.024.0000 (12–24 V AC/DC) keeps the whole latch inside the 24 VAC domain** and is
> Set/Reset. **Two independent reasons to reject the télérupteur; take the bistable.**

**Also flagged, because it is the trap most likely to catch a careful person:** **Finder 80 in `DI`
(interval) mode looks like a one-shot latch and is not one.** Its reset is *"remove the timer's
supply"*, not *"press a button"* — so a **crash-looping ESP32 that re-pulses its enable output
power-cycles the timer and buys itself another full T minutes, indefinitely.** That is precisely the
failure being defended against. And **Legrand 412401** (télérupteur temporisé, 5–60 min, ~€31 HT [V])
bounds each run without needing a human, which makes it a decent cheap **extra** layer — but it
returns to "waiting for an impulse", so **it is not the latch either**.

| | Parts | Cost |
|---|---|---|
| **Minimal** | Timer wired to an ordinary interface relay in a **seal-in (self-hold)** circuit with an NC reset button. Textbook industrial latch, cheapest parts — **but the latch clears on power loss**, so a flicker re-arms it and the forensic record is gone | **~€60–70 [E]** |
| **⭐ Recommended** | **Finder 80.01.0.240.0000** multifunction timer, 6 functions / 6 ranges 100 ms–24 h, 12–240 V AC/DC, 1 CO 16 A, 17.5 mm — **€40.75 HT / €48.90 TTC, in stock, elecdirect.fr [V]** ⟶ **Finder 13.12.0.024.0000 Set/Reset bistable, 12–24 V AC/DC** (~€25–35 HT [E], not verified) ⟶ **Schneider XALK178** boxed reset/kill station **€13.75 HT / €16.50 TTC [V]** ⟶ **an auxiliary contact wired to GPIO34** so the latch raises a CRITICAL in HA | **≈ €100–115 TTC [V/E mix]** |
| **Robust** | A **safety relay with manual reset** (Schneider Preventa XPS-AC / Pilz PNOZ) doing timer + latch + reset + monitored contacts in one certified box | **~€180–320 [E]** |

> **My pick: Recommended — Finder 80.01 + Finder 13.12 Set/Reset bistable + XALK178, ≈ €105 TTC.**
>
> **Buy the Finder 80.01 from elecdirect, not DigiKey.** Same part number, **€48.90 TTC vs €72.64
> excl. VAT — DigiKey is 78 % dearer [V both]**. This is the single largest avoidable overpayment I
> found while pricing this BOM.
>
> **Why not Minimal:** a seal-in latch **clears on power loss**, and the fault this device exists to
> catch is a *hung controller* — a hung controller plus a brownout is not an exotic coincidence, it is
> the same afternoon. The bistable's mechanical latch survives power cycling, which is the whole
> point.
>
> **Why not Robust:** a Preventa monitors an e-stop loop for dual-channel faults. That is not this
> problem, and €250 buys nothing this design needs.
>
> **Wire the latch state to GPIO34.** It is one wire and it is the difference between "the garden
> stopped in August and nobody knows why" and a CRITICAL in HA (R23 requires exactly this).
>
> **Set the period to ~1.5× the longest legitimate single-zone run** — if rotors need 35 min, set
> **60 min, not 45** — and **verify on the bench that the timer resets inside the inter-zone settle
> gap (E6)**. Note that **verification item 12 (CD4541B RC values and drift) is now moot**: a €49
> industrial timer with a calibrated range selector beats an RC oscillator you have to characterise
> over temperature, and it costs about the same once you count your afternoon.

### C3. NC kill switch in the 24 VAC common — **MANDATORY**

**Role.** A hard, normally-closed contact in the overall 24 VAC common. It is **downstream of every
possible electronic fault**, including a welded relay, a shorted rail, and a firmware that has lost
its mind — which nothing else in the design achieves. It is also the "anyone can stop the water with
no tools and no app" affordance that makes the system socially acceptable (NFR-D1).

**Optional? No**, and it is ~€8. NFR-S5 requires at least one shutdown path with no software in it.

| | Product | Cost |
|---|---|---|
| **Minimal** | A generic 22 mm panel-mount NC pushbutton, Amazon/AliExpress | ~€4–8 [E] |
| **⭐ Recommended** | **Schneider XALK178** — complete **IP66 yellow e-stop station**, Ø22 mushroom head, turn-to-release, **1 NC positive-opening contact**, 300 k mechanical / 10⁶ electrical cycles at AC-15 — **€13.75 HT / €16.50 TTC [V, elecdirect.fr]**. Mounted **outside** the cabinet, labelled *ARRÊT ARROSAGE*, **with its state sensed on GPIO39** | **€16.50 [V]** |
| *(alternatives verified)* | Schneider **XB4BS8445** head+contact only, €13.50 HT / €16.20 TTC [V] · XALK178E (1 NO + 1 NC) €18.60 TTC [V] · XALK178F (2 NC) €24.42 TTC [V] · Legrand Plexo L 069549L €54.50 TTC [V] | |
| **Robust** | A Preventa-grade e-stop with a monitored dual-channel contact | ~€60–90 [E] |

> **My pick: Recommended — Schneider XALK178 at €16.50 TTC, and the cheap-vs-proper argument has
> simply evaporated.** I went looking for the €5-button-versus-€60-e-stop trade-off the brief implies
> and **it does not exist**: a genuine, boxed, IP66, certified station with a **positive-opening**
> contact is €16.50 TTC. For €10 over a no-name you get certification, an IP rating, a traceable part
> number, and the ready-made enclosure. **Generic buttons do not certify positive-opening** — meaning
> a welded contact can leave them looking pressed while the circuit stays closed, which is the one
> failure mode a kill switch may not have. **There is no rational case for the generic part in a
> safety function.**
>
> **The GPIO39 sense line is the part people skip and shouldn't.** Without it, "someone pressed the
> big red button three weeks ago" is indistinguishable from a dead controller, and you will spend an
> afternoon debugging firmware that is working perfectly. One wire, pure maintenance cost.
>
> **Skip Robust:** monitored dual-channel contacts solve a machine-guarding problem, not this one.

### C4. Manual bypass ball valve — **MANDATORY (hydraulic, not electrical)**

**Role.** A quarter-turn ball valve plumbed **around the whole manifold** so the garden can be watered
with the electronics completely dead or removed. "The sprinkler computer is broken" must never mean
"the garden dies", and it is what lets you take the controller to the bench for a weekend.

**Optional? Technically yes; practically no.** ~€25 of brass buys the political licence to take the
system offline. Skipping it means every firmware experiment is a hostage negotiation.

See §A5 for products and pricing (it is just two ball valves and a tee).

> **My pick: fit it, and fit it where a non-technical person can find it, with a laminated label.**
> With T-INDOOR this becomes trivially easy — it is 40 cm of pipe on the same wall board.

---

## 6. §D — Power

> ### Note on price sources — read once, applies to the whole document
> **Blocked / JS-rendered, so nothing from them is verified: Amazon.fr, AliExpress, ManoMano, Conrad,
> RS France (and Distrelec, which now redirects to RS), Farnell FR, Mouser.fr, Cdiscount, Leroy
> Merlin, Castorama, Phoenix Contact, Kubii.** Every price attributed to those is **[E]**, without
> exception. *(One partial exception: the sensor researcher reached Amazon.fr and AliExpress.fr with a
> real browser, so §F's Amazon prices are marked [V] and are TTC.)*
>
> **Suppliers that fetch cleanly and gave citable prices — prefer these:**
> **elecdirect.fr** (shows HT *and* TTC plus stock — the best single source for §C/§D/§G),
> **arrosage-distribution.fr** and **rs-pompes.com** (§A), **compteur-energie.com** (§E),
> **Gotronic.fr**, **Reichelt**, **BerryBase.de**, **DigiKey.fr**, **bricozor.com**,
> **Météo-Shopping**, **Antratek**, **Comptoir du Câble**, Semageek, Lextronic, Eleshop, Welectron.
>
> **Shipping dominates small orders and should drive how you batch.**
> **Reichelt: flat €7.90.** **DigiKey France: free ≥ €75, otherwise €25, ~3 days.**
> **compteur-energie: free > €200 HT.** So consolidate **§B + §C-discretes + §E2 + §J** into *one*
> DigiKey basket and *one* Gotronic/Reichelt basket. On a €40 parts order, getting this wrong costs
> more than any per-item saving in this document.

### D1. 24 VAC safety isolating transformer — **MANDATORY**

**Role.** SELV/TBTS supply for the valve rail. **NFR-S11 requires valve wiring to be TBTS from a
*safety isolating transformer*** — not any 24 V source. Keeping it separate from the 5 V logic supply
means **valve inrush cannot brown out the MCU** and the logic ground stays isolated from the valve
common.

**Sizing, corrected (see §1):** the Hunter PGV is **370 mA / 8.88 VA inrush and 210 mA / 5.04 VA
holding at 50 Hz [V — Hunter spec sheet]**, not the 475 mA / 11.4 VA in NFR-P2. Worst case is master
+ one zone: **~17.8 VA inrush, ~10.1 VA holding.**

| | Product | Price |
|---|---|---|
| **Minimal** | **Plug-in 24 V irrigation transformer, RAIN ref S-RI-1700010001** — **€15.75 HT / €18.90 TTC [V, cour-et-jardin.fr]**, 24 h. ⚠️ *The listing does not state the VA rating* **[V that it is absent]**; this class is typically **24 VAC 750 mA = 18 VA**, which sits **right on** the 17.8 VA two-valve inrush with no margin. | **€18.90 [V]** |
| **⭐ Recommended** | **Hager ST314** — safety transformer **40 VA**, 230 V → 12/24 V, toroidal, 4 DIN modules — **€69.14 TTC (≈€57.62 HT) [V, blanc-habitat.com]**, delivered 48 h | **€69.14 [V]** |
| *(best VA per €)* | **Legrand 044213** — control + safety transformer **100 VA**, 230/400 V → 24 V, screw, DIN-mountable — **€43.96 HT / €52.75 TTC, in stock [V, elecdirect.fr]** | €52.75 [V] |
| *(other verified)* | Legrand **413098** 63 VA, 5 mod, €69.79 HT / €83.75 TTC — *derniers articles* [V] · Legrand **413096** 25 VA, €42.17 HT / €50.60 TTC — **rupture** [V] · Legrand **413097** 40 VA, €76.00 HT / €91.20 TTC — backorder [V] | |
| **Robust** | 63 VA DIN (Legrand 413098, €83.75 TTC [V]) with a thermal cut-out and fused secondary | €83.75 [V] |

> **My pick: Recommended — the Hager ST314, 40 VA, €69.14 TTC.**
>
> **⚠️ Correction to the spec's candidate list: Legrand 042761 is a 12 V, 100 VA, IP55 unit — not
> 24 V — and it is delisted at Rexel. Do not order it. [V]**
>
> **Do not fit 150 VA.** The spec's reasoning is right: *"a modest transformer limits fault current
> and degrades gracefully"*. A large transformer turns a shorted valve conductor into a fire-class
> event rather than a sagging rail — and under T-GARDEN that conductor is buried, while under
> T-INDOOR it passes through a wall. **40 VA is 2.2× the corrected worst-case inrush (17.8 VA) and
> ~4× the holding load. That is correct engineering margin and no more.**
>
> **The Legrand 044213 at €52.75 TTC for 100 VA is the tempting one, and I am deliberately not
> recommending it.** It is €16 cheaper *and* 2.5× the capacity — which is exactly why to refuse it.
> Cheaper and bigger is the wrong trade here, because capacity you do not need is fault current you
> do not want on a SELV circuit. **This is the one line in the BOM where I am recommending you pay
> more for less.**
>
> **Why not Minimal:** at €18.90 the plug-top is electrically *just barely* adequate — and "just
> barely" is the problem. **17.8 VA inrush against a nominal 18 VA is zero margin**, the listing will
> not even confirm the rating, and it puts the SELV source on a wall socket: no DIN mounting, no
> proper primary protection, and a cord anyone can unplug. **Because D13 makes the labour free, the
> only reason people buy plug-tops — avoiding an electrician — does not apply here.** €50 more and
> one hour.
>
> **D5 consequence:** cycle-and-soak multiplies open/close events by 3–6×, so the transformer sees far
> more inrush cycles per season. That argues for a **comfortable 40 VA rather than a marginal 18–25
> VA**, and for not skimping on the primary fuse. It is *not* an argument for 63 or 100 VA.

### D2. 5 V DC logic supply — **MANDATORY. Never USB.**

**Role.** Powers the ESP32, the relay coils, the expander, the RTC and the deadman.

**Budget:** 9 × SRD-05VDC-SL-**C** relay coils at 71.4 mA = 643 mA (or the -**D** variant at 89.3 mA =
804 mA — **verification item E5, read the relay can**), plus the ESP32 at ~250 mA peak on Wi-Fi TX,
plus logic. **~1.0–1.3 A worst case.**

| | Product | Price |
|---|---|---|
| **Minimal** | A 5 V 3 A wall-wart into a screw-terminal breakout | ~€10–14 [E] |
| **⭐ Recommended** | **Mean Well HDR-15-5** (5 V / 2.4 A / 12 W, DIN) — **€10.70 excl. VAT @1, €10.19 @5, DigiKey.fr, 1 603 in stock [V]**, or **€13.25 HT / €15.90 TTC at Gotronic.fr ref 14905 [V]** | **€10.70–15.90 [V]** |
| *(headroom)* | **Mean Well HDR-30-5** (5 V / 3 A) — **€17.42 HT / €20.90 TTC, Gotronic ref 14908 [V]** | €20.90 [V] |
| **Robust** | **Mean Well MDR-20-5** (metal case, 5 V / 3 A) — **€12.33 excl. VAT, DigiKey [V]**; or HDR-60-5 (6.5 A) €25.79 HT / €30.95 TTC, Gotronic ref 14912 [V] | €12.33–30.95 [V] |

> **My pick: Recommended — Mean Well HDR-15-5 (2.4 A), or HDR-30-5 if you want the headroom.**
>
> **The "never USB" rule is not fussiness and it deserves stating.** A USB supply gives you: a
> connector that vibrates loose, no DIN mounting, no defined earthing, a cable that invites someone to
> unplug it, and — worst — **a second power path into the DevKit's `5V`/USB pins, which are mutually
> exclusive** (§B5). It also makes NFR-P3 (clean brownout reset with valves closed) untestable,
> because you cannot cleanly sag a wall-wart.
>
> 2.4 A against a 1.3 A worst case is ~1.8× margin, which is right. **The one thing to check first is
> whether your relay board is the -C or the -D coil variant (verification item E5)** — at 89.3 mA × 9
> = 804 mA, the -D variant plus a transmitting ESP32 gets close enough to 1.3 A that 2.4 A is only
> 1.5× margin. **Read the relay can before ordering; if it is -D, take the HDR-30-5 at €20.90.**
>
> **Buy it from Gotronic at €15.90 TTC unless you are already batching ≥€75 at DigiKey.** DigiKey's
> €10.70 excl. VAT looks cheaper but becomes €12.84 with VAT plus **€25 shipping under €75** — so on
> a standalone order Gotronic wins by €22. If you are buying the §C discretes from DigiKey anyway,
> add it there and take the €10.70.

### D3. RCBO / differential protection — **MANDATORY (NFR-S11, NF C 15-100)**

**Role.** 30 mA type-A residual-current protection on the circuit feeding the controller, plus
overcurrent protection. **D13 makes this labour-free**, so specify it properly.

| | Product | Price |
|---|---|---|
| **Minimal** | Feed the cabinet from an existing circuit **already behind a 30 mA type-A interrupteur différentiel**, fitting only **Schneider Resi9 XP R9PFC616** 1P+N 16 A curve C — **€8.04 HT / €9.65 TTC, in stock [V, elecdirect.fr]** | **€9.65 [V]** |
| **⭐ Recommended** | **Schneider Resi9 XP R9PRA240** interrupteur différentiel 2P **40 A 30 mA type A**, 2 modules — **€45.42 HT / €54.50 TTC, in stock [V]** **+ R9PFC616** MCB €9.65 TTC [V] | **€64.15 [V]** |
| *(single-device route)* | **Schneider Acti9 iDD40T A9DB2616** RCBO 1P+N 16 A **30 mA type A-si** — **€106.08 HT / €127.30 TTC [V]**. Or **Resi9 XP R9PDCF16** 1P+N 16 A 30 mA **type F(si)** — **€89.08 HT / €106.90 TTC, in stock [V]**; **type F is a superset of type A per IEC 62423**, so it satisfies NFR-S11. | €106.90–127.30 [V] |
| **Robust** | Recommended **+ a type-2 SPD** (§D4) | ~€150–210 [V-derived] |

> **My pick: Recommended — the ID + MCB pair at €64.15 TTC, not a single RCBO.** Verified pricing
> makes this easy: **€64.15 for a 40 A type-A differential plus a 16 A MCB, against €106.90–127.30 for
> a single 16 A type-A/F RCBO [V both]** — half the price, two modules more, identical protection, and
> the 40 A differential leaves headroom to put a future circuit behind it. **Note also that no 10 A
> type-A RCBO exists at any supplier I could reach [V — absence confirmed on elecdirect]**, so 16 A is
> the practical minimum either way; on 1.5 mm² that is fine.
>
> Two reasons for a dedicated circuit that are not about compliance:
>
> - **A dedicated circuit means testing the system does not involve the rest of the house.** NFR-I3
>   requires you to actually *test* the safety features, including pulling power; NFR-R6 requires
>   pulling the plug **20× at random points including mid-run and mid-flash-write**. Doing that on a
>   shared circuit that also feeds the pool pump is not viable.
> - **Type-A specifically, not type-AC.** A switched-mode 5 V supply produces pulsating DC residual
>   currents that a type-AC device can fail to detect. NFR-S11 already says type-A; it is the right
>   call and it is ~€10 more.
>
> **The Minimal option is genuinely acceptable** if the technical room already has a 30 mA type-A
> differential covering it — verify the *type* (A vs AC) on the label, not just the 30 mA. But given
> D13 and a €50 part, take the dedicated circuit; it makes everything downstream easier.

### D4. Surge protection (parafoudre) — **OPTIONAL. Recommended here, unusually.**

**Role.** Clamps mains transients.

**Optional — what you lose:** a lightning-adjacent surge takes out the SMPS, and possibly the ESP32
and the relay board with it. Everything fails *closed* (NC valves), so this is a **cost and
availability** risk, not a P0 risk. Say that plainly — nobody floods because they skipped an SPD.

| | Product | Price |
|---|---|---|
| **Minimal** | **None** — if the main consumer unit already has a type-2 SPD, you do **not** need a second one in a sub-panel | **€0** |
| **Recommended** | **Legrand 412244** parafoudre T2, Imax 40 kA/pole, In 20 kA, 1P+N, 2 mod — **€70.75 HT / €84.90 TTC [V, elecdirect.fr — currently *rupture de stock*]**. ⚠️ Per its own product page it **requires a mandatory upstream 25 A 1P+N disjoncteur** under NF C 15-100 [V] | **€84.90 + MCB [V]** |
| *(self-protected, no extra MCB)* | **Schneider Resi9 XP R9PLC** combi type-2, 1P+N, Imax 10 kA / In 5 kA, 2 mod, **self-protected**, supplied with comb, terminal block and 16 mm² lead — **€118.25 HT / €141.90 TTC, in stock [V]** | €141.90 [V] |
| **Robust** | Either of the above **+ gas-discharge / 24 V field-side surge arresters on the buried valve conductors** (T-GARDEN only; Hunter and Rain Bird both sell these — **price not verified, irrigation sites blocked [E]**) | ~€170–230 [E] |

> **My pick: check the consumer unit first — and if it already has a type-2 SPD, fit nothing and save
> €85–142.** That is the honest answer and it is the most likely outcome in a house with a pool
> installation. NF C 15-100 §534 makes a type-2 SPD mandatory where AQ2 applies and/or the supply is
> overhead-fed, and **the Hérault sits in France's high-keraunic zone**, so it is quite likely already
> there. **A second SPD in a sub-panel downstream of an existing one buys essentially nothing.**
>
> **If there is none, fit the Schneider R9PLC at €141.90 rather than the Legrand at €84.90** —
> counter-intuitively, because the Legrand **requires a mandatory upstream 25 A disjoncteur [V]**,
> which erases most of the €57 gap and adds a module and a failure point. The R9PLC is
> self-protected and arrives with its comb and 16 mm² lead. *(It is also the one in stock.)*
>
> **⚠️ And a limitation worth stating plainly, because it is easy to feel protected and not be: a
> 230 V SPD protects the transformer primary and nothing else.** Under T-GARDEN your real exposure is
> **eight buried 24 VAC valve runs acting as an induction loop**, and the mains SPD does nothing for
> them — you would need separate 24 V field-side arresters (Robust tier).
>
> **T-INDOOR deletes that entire coupling path.** No buried conductors means nothing for a nearby
> strike to induce into. So **§2's recommendation collapses the SPD question from a two-sided problem
> to a mains-only one**, and lets you skip the Robust tier entirely. **That is a €90–170 saving that
> nobody counted in §2, and another independent confirmation of the manifold verdict.**

### D5. DIN rail, terminals, fuses, wiring accessories — **MANDATORY**

**Role.** Mechanical and electrical hygiene. Terminals are what make the box maintainable in five
years.

| | Contents | Price |
|---|---|---|
| **Minimal** | 1 m DIN rail (**IMO ref 12019, €4.13 [V, lelectricien.net]**) + ~20 screw terminals (**IMO ER 2.5 mm², €0.37 ea [V]**) + 4 end stops (**€0.37 ea [V]**) | **~€16–20 [V-derived]** |
| **⭐ Recommended** | Rail as above (or the **2 m IDE TS35, €8.40 [V]**) + **~40 × Legrand Viking 3 screw terminals 2.5 mm² (ref 037160), €0.50 HT / €0.60 TTC each, in stock [V, elecdirect.fr]**, **colour-coded** — Viking 3 orange (037120, €1.04 TTC [V]) and green/yellow earth (037170, €1.82 TTC [V]) + **Schneider NSYTRAAB35 markable end stops, €0.62 TTC [V]** + **bridging combs (€0.78–1.74 [V])** + 2 fuse terminals + **ferrules on every stranded conductor** + numbered markers | **~€40–55 [V-derived]** |
| *(push-in alternative)* | **IMO ZK2.5 push-in terminals, €0.43 each [V]** — spring-cage at screw-terminal price | ~€35–50 [V-derived] |
| **Robust** | Add a **knife-disconnect / isolating terminal block for the 8 zone returns** so any zone can be lifted for a resistance measurement without unscrewing anything | ~€75–100 [E] |

> **My pick: Recommended, and consider the knife-disconnect upgrade for the zone commons.**
>
> **Take the push-in/spring-cage terminals — they are the same price.** Screw terminals on stranded
> wire loosen with thermal cycling and are the classic cause of "zone 5 works intermittently". Spring
> cages do not. **IMO ZK2.5 push-in is €0.43 against €0.37–0.50 for screw [V]** — so the usual
> cost objection to spring-cage simply does not apply at this scale. Over a 10-year life with a
> twice-yearly maintenance afternoon (NFR-M8), this is free reliability.
>
> **The colour coding is not cosmetic — it is a safety requirement in disguise.** The max-runtime
> backstop goes on the **zone-side common**, which is a *different* conductor from the overall 24 VAC
> common (§C2), and the whole latent-bug story in the spec exists because those two were conflated.
> **Give them different colours and different terminal blocks so they cannot be confused during a
> rewire.** Fifteen euros of coloured terminals is the cheapest possible defence against re-creating
> that bug in five years.
>
> **The Robust knife-disconnect option is worth the €40** if you can find it: it lets you isolate one
> zone for a resistance measurement without disturbing anything else — which is exactly the T1/T4
> style of test you will repeat after every winter (NFR-I3).
---

## 7. §E — Metering

### E1. The water meter — **MANDATORY**

**Role.** It is the only sensor that can see a weeping diaphragm, a burst lateral, a blocked zone, or
flow with everything commanded closed (fault-policy row 8, *"the one genuinely dangerous case"*).
**P5 says "closed" must be observable, not merely commanded** — this is the instrument that makes P5
true. Totals for the HA Water dashboard are a by-product, not the purpose.

**Optional? No.** Skip it and you delete FR-6, fault rows 7/8/9, the night leak audit, hydraulic
profiling, NFR-R5's water-attribution metric, and the *only* coverage for a grit-weeping diaphragm.
You also lose NFR-LEG3's mandatory *"means of evaluating the volume used"*, the day a rainwater tank
appears.

**The spec's conclusion is right, but the wording needs tightening — and the reason is not MID Q1, it
is *starting flow*.** Verified from datasheets:

| Meter | Type | **Starting flow** | Q1 (MID) | Min. register step |
|---|---|---|---|---|
| **Diehl ALTAIR V4 DN15** | volumetric piston | **0.4 L/h [V]** | R160 | — |
| Diehl ALTAIR V4 DN20 | volumetric piston | **0.7 L/h [V]** | R160 | — |
| **Itron Aquadis+ DN15** | volumetric piston | **1 L/h [V]** (±5 % @ 3 L/h) | 15.6 L/h @R160 | 0.02 L [V] |
| Itron Aquadis+ DN20 | volumetric piston | 2 L/h [V] | 25 L/h @R160 | 0.02 L |
| **Sensus 620 DN15** | volumetric piston | *"extreme low"*, no figure [V] | **6.25 L/h @R400 [V]** | 0.05 L [V] |
| Sensus 620 DN20 | volumetric piston | not stated | 10.00 L/h @R400 [V] | 0.05 L |
| B Meters GMDM-I DN20 | **multi-jet** | not stated | 25 L/h @R160H [V] | 0.05 L |
| YF-S401 turbine | turbine | **18 L/h [V]** | — | — |
| YF-S201 turbine | turbine | **60 L/h [V]** | — | — |
| **YF-B10 turbine** | turbine | **120 L/h (2 L/min) [V]** | — | — |

> **Restate the finding precisely, because the precision matters:** a turbine floors at **18–120 L/h**
> and **cannot see a 5 L/h leak, at all, ever**. A jet meter's impeller *does* eventually turn at
> 5 L/h, but it is out of tolerance below 10–25 L/h and — the part that actually kills it — is
> normally sold with a **10 L/pulse** emitter, giving **one pulse every two hours**. Only a
> **volumetric piston meter with a 0.5–1 L/pulse emitter** gives a pulse every 6–12 minutes at 5 L/h.
> **That is the whole design.**

> ### ⚠️ Four corrections to the spec's candidate list, all verified from datasheets
> - **Zenner MTKD is multi-jet and ETKD is single-jet — neither is volumetric. [V]**
> - **B Meters GSD8-RFM is single-jet and GMDM-I is multi-jet — neither is volumetric. [V]**
> - **Baylan and Arad are not retail-sourceable in France** (no distributor, no public price) — which
>   makes them **NFR-L4 violations** regardless of their specifications.
> - **No European volumetric meter in this price class has a *truly integral* pulse output.** They are
>   all *"pré-équipé"* (an inductive or reed target on the pointer) plus a clip-on module. The only
>   genuinely integral 1 L/pulse unit found (Maddalena CD ONE) is **single-jet [V]**. So the spec's
>   instruction to *"buy the meter with the emitter integral"* **cannot be followed**, and the correct
>   framing is instead: *pick the cheapest good emitter.*

**And the €147 figure that drove that instruction is real but avoidable.** Verified emitter prices at
compteur-energie.com, all **HT**: **Maddalena Reed Switch MVM (ref 2749032) €32.00 — 2 imp/L = 0.5
L/pulse [V]**; **Diehl Izar Pulse I (3080041) €52.00, k=1 → 1 L/pulse, dry contact, 5 m cable [V]**;
**Sensus HRI-B4 €44.00–59.00 [V, two listings on the same site]**; **Itron Cyble Sensor V2 2-wire K1
€54.00 [V]**. **A 1 L/pulse emitter costs €32–59 HT, not €147.**

| | Product | Price |
|---|---|---|
| **Minimal (does not meet the requirement)** | **YF-B10** 1" brass turbine — €6.15 + €0.61 Cdiscount [E], €13.95 for a YF-B5 at **Gotronic ref 37579 [V]**. **120 L/h floor: blind to the failure the meter exists for.** | **~€7–14** |
| **Minimal-that-works** | **Cartelectronic USL-20** (GIOANOLA), **€71.00 TTC [V]**, 4 imp/L = 0.25 L/pulse, reed — *but single-jet*. Or **TOODO AJN20TI €64.90 TTC [V]**, 1 L/pulse — page claims "volumétrique" but a 130 mm DN20 body rated to 90 °C is a **jet-meter signature**; piston meters are cold-water ≤30–50 °C. **Ask for the type-approval number before believing it.** | **~€65–71** |
| **⭐ Recommended** | **Maddalena MVM DN15, ref 2775115, €46.00 HT [V]** (volumetric piston, **MID R400**, listing claims sensitivity <1 L/h) **+ Maddalena Reed Switch MVM, ref 2749032, €32.00 HT [V]** (**2 imp/L = 0.5 L/pulse**, 2-wire dry contact, 1 m lead). In stock 24–48 h. **DN20 variant: ref 2775120N €58.00 HT ⇒ €90.00 HT total.** | **€78.00 HT ≈ €93.60 TTC [V]** |
| **Robust** | **Diehl ALTAIR V4 DN15, ref 3036510, €48.00 HT [V]** (**0.4 L/h starting flow — the most sensitive part available**) **+ Izar Pulse I dry-contact €52.00 HT [V]** ⇒ ~€100–106 HT. *Lead time 2–4 weeks; the emitter is battery-powered.* Plus **a YF-B5 turbine (€13.95 TTC [V]) in parallel on a tee.** | **~€120–140 TTC** |

> **My pick: Recommended — Maddalena MVM DN15 + Reed Switch MVM, €78.00 HT / ~€93.60 TTC, plus a
> €14 turbine on a tee.**
>
> **This changes the spec's recommendation, and for a good reason.** The spec named the Sensus 620 at
> 1 L/pulse. The Maddalena is **cheaper (€78 vs €103 HT [V])** *and* gives **0.5 L/pulse — twice the
> resolution** — from a **passive dry-contact reed with no battery to die**, in stock in France. At a
> 5 L/h weep that is **a pulse every 6 minutes** instead of 12. It is better on every axis that
> matters, including NFR-L4 sourceability.
>
> **DN15, not DN20 — and here I am departing from the spec.** DN15 Q3 = 2.5 m³/h (42 L/min) with
> Q4 = 3.125 m³/h (52 L/min) **[V]**, against sequential zones that never exceed ~2 m³/h — so ~2×
> headroom. In exchange, DN15 has **6.25 L/h Q1 versus 10.00 L/h** on the Sensus family and Diehl's
> DN15 starting flow is **0.4 L/h against 0.7 L/h [V]**. **Leak sensitivity is the entire purpose of
> this instrument, so buy the size that maximises it.** *The one caveat: if static pressure measures
> below ~2.5 bar, take DN20 (ref 2775120N, €58 HT) and accept the coarser floor — pressure you do not
> have is worth more than sensitivity you will not use.*
>
> **Do add the €14 turbine — this is the one place I will deliberately gold-plate.** The volumetric
> meter is a **single point of failure for four separate CRITICAL alarms**, and a stuck meter reads
> exactly like "no leak" — a **silent failure of a safety instrument**. R7 (an unbounded volume-mode
> run chasing a target from a dead meter) is the same failure wearing a different hat. Two dissimilar
> sensors on the same water is the only way to detect it: if one reads and the other does not, you
> know something is wrong, **which neither alone can tell you**. **€14 to make a safety instrument
> self-checking is the best value in this BOM after the deadman.** Use the same tee the spec already
> wanted for a second meter, and put the turbine on **PCNT** (where its 13 µs filter is appropriate)
> while the reed goes on `pulse_meter`.
>
> **Skip the Diehl ALTAIR Robust option** despite it being the better instrument: 0.4 L/h versus
> ~1 L/h buys nothing you will use (you are detecting a weeping diaphragm, which is single-digit L/h,
> not a pinhole), it costs €25 more, the emitter has a **battery that dies in ~15 years**, and it has
> a **2–4 week lead time** that would delay v1. Passive reed beats powered emitter for a 10-year
> unattended install.
>
> **Plumbing (FR-6.7):** downstream of the master valve, downstream of the filter, seeing **only** the
> irrigation circuit — so a hosepipe wash does not raise an unexpected-flow CRITICAL. Under T-INDOOR
> this is trivial. **And make the D12 phone call about NFR-LEG6 (the *redevance assainissement*
> deduction, CGCT R. 2224-19-2) before you fix the meter position** — a *branchement spécifique* could
> repay a large fraction of this BOM annually, and the position is decided by the plumbing.

### E2. Reed-pulse signal conditioning — **MANDATORY, and nearly free**

**Role.** A reed switch bounces for **1–5 ms**, two to three orders of magnitude longer than **PCNT's
13 µs filter ceiling**, so a raw reed on hardware pulse counting registers **3–10 "litres" per litre**.
Since these pulses feed leak detection and the daily volume cap, an untrustworthy pulse is a **safety**
problem, not a metrology one.

**Optional? No.** The spec is right that you need **all three** layers: RC + Schmitt (10 kΩ + 100 nF,
τ = 1 ms), an **opto-isolator**, and a **software plausibility gate rejecting intervals under
~200 ms** (at 0.5 L/pulse and a plumbing-limited 60 L/min, the shortest legitimate interval is 0.5 s
— the margin is still large).

| | Parts | Cost |
|---|---|---|
| **Minimal** | 10 kΩ + 100 nF + one gate of the **74HCT14 already bought for the deadman** + `pulse_meter` (ISR) in firmware | **~€0.50 [E]** |
| **Recommended** | The above **+ a PC817 or 4N35 opto**, **+ a 10 kΩ pull-up on GPIO35** (input-only, no internal pull-up), **+ reed contact current kept ≤1 mA** to avoid contact erosion | **~€2 [E]** |
| **Robust** | A 6N137 high-speed opto + an LM393 comparator on a small PCB with a test point | ~€6 [E] |

> **My pick: Recommended.** €2, and the isolation is the point: a multi-metre cable entering a box
> that also contains 230 V and a 24 VAC inductive load. **Use `pulse_meter` for the reed, not
> `pulse_counter`/PCNT** — a firmware decision no purchase can rescue. **Skip Robust:** a 6N137 solves
> a megabaud problem and you have a ~0.03 Hz signal. Note the **0.5 L/pulse Maddalena doubles the
> pulse rate**, which is still nothing — at 25 L/min you get 0.8 Hz.
---

## 8. §F — Sensors

### F1. Rain gauge (D11) — **OPTIONAL, but buy it now** — and my pick is not a tipping bucket

**Role.** A dry-contact pulse into a spare GPIO gives **autonomous rain-skip**: the device itself knows
it rained and can suppress a cycle with Wi-Fi down, HA down and no internet. That is the difference
between rain-skip that satisfies FR-1.1 and rain-skip that is an HA automation wearing a costume.

**Optional — what you lose by skipping it:** rain-skip becomes an HA-pushed hint which, under **P6**,
must carry a TTL and **decay to the permissive default** — so when the HA integration breaks (and it
will), **you water in the rain**. You also lose the rainfall series that makes seasonal adjust honest
in the FR-7 records. The live HA instance has **no rain sensor at all**, so today the fallback does
not even exist.

> **The hard requirement, which eliminates most of the market:** a **potential-free contact or
> open-collector pulse on wires**. Not 433 MHz, not Wi-Fi, not a proprietary console. That rules out
> every consumer weather station and all Ecowitt/Netatmo/Bresser display kits — they are HA
> integrations wearing a rain-gauge costume and they fail the requirement by construction.

| | Product | Resolution | Price |
|---|---|---|---|
| **Minimal** | **Misol WH-SP-RG / WH-SP-RG-2** spare-part bucket, reed on 2 wires. *Amazon.fr ASIN B00QDMBXUA exists but was bot-blocked — no price seen.* AliExpress snippets $9.70–18.05. **Resolution conflicts across sources (0.2794 vs 0.3 mm/tip) — calibrate it yourself with a measuring cylinder.** | ~0.28 mm/tip [E] | **~€15–30 [E]** |
| **⭐ Recommended** | **Hydreon RG-15** solid-state optical — **€99.00 TTC, in stock, Antratek (NL) [V]**; **€99.75 TTC / €82.44 HT at LunaticoAstro EU, ~2 days [V]** (was €142). Output is **NPN open-collector, 500 mA / 80 V, 5–35 VDC, 0.2 mm resolution [V — rainsensors.com]** — it **electrically emulates a tipping bucket**, so the ESP32 pulse-counting code is *identical* | 0.2 mm (or 0.02) | **€99.00 [V]** |
| *(the tipping-bucket route)* | **Davis 6464M AeroCone rain collector**, flat base (*replaces the 6463 / old 7852*) — **€149.00 TTC / €124.17 HT, 13 in stock, 24–72 h, Météo-Shopping [V]**; 0.2 mm/tip reed (ILS), 12 m RJ12 lead. **+ mount WSF-10150 €39.00 TTC [V]** ⇒ **€188** | 0.2 mm/tip | €188 [V] |
| *(other verified)* | Davis 6466M with integrated mount €169.00 TTC [V] · Davis 6463M €198.00 TTC [V, CIMA, official FR importer] · **Pronamic Rain-O-Matic Pro** (P/N 300.021 single reed / 300.023 dual NO+NC), 0.1 / 0.2 / 0.25 / 0.5 mm selectable at the same price — **€220.00 HT ≈ €264 TTC, made to order, 3–6 weeks, Alpha Omega Electronics [V]** · SparkFun **SEN-15901** weather-meter kit $79.95 excl. VAT/duty [V] · Hydreon **RG-11** €69.00 / **RG-9** €59.00 TTC [V] | | |
| **Robust** | R.M. Young **52202H** (230 V heated — the plain 52202 is 120 V, get this one for France) — **US-only listings, ~$742 [E], no EUR price found**. Lambrecht and Delta-T are **quote-only B2B, no public pricing** | 0.1 mm | ~€700+ [E] |

> ### My pick: **the Hydreon RG-15 at €99 TTC — and this reverses my own first instinct.**
>
> I started this section intending to recommend a 0.2 mm/tip Davis or Pronamic bucket, because that is
> the conventional answer and the spec frames D11 as *"a tipping-bucket rain gauge"*. **The verified
> prices invert it:** the RG-15 is **€99 against €188 for a Davis-plus-mount and €264 for a Pronamic**,
> and it is **the lower-maintenance instrument**, which is precisely the criterion I was asked to weigh.
>
> - **No bucket to stick, no funnel to block, no bearing to wear.** The failure mode of a tipping
>   bucket in the Hérault is a funnel filling with pine debris and dust, after which **it silently
>   reads zero rain** — and under **P6 that decays to *permissive*, so it makes you water in the
>   rain**, which is the exact failure the gauge exists to prevent. It is a *silent* failure of a
>   sensor, and this document has already flagged that pattern twice (the stuck meter, §E1).
> - **Zero firmware cost.** The RG-15 emulates a tipping bucket electrically, so the ESPHome config
>   is the same `pulse_meter`/`pulse_counter` you would write anyway.
> - **NFR-L4 is satisfied**: it is a current catalogue product from a US manufacturer with two EU
>   stockists holding stock, not a spare part from an overseas marketplace.
>
> **What you give up, stated honestly:** it is optical, so **it cannot be gravimetrically calibrated**
> against a measuring cylinder — you trust the factory. For meteorology that would disqualify it; for
> a **rain-*skip* decision** ("did it rain more than 5 mm in the last 24 h?") calibration accuracy is
> irrelevant. It also needs **5 V up the cable, so 4 conductors instead of 2**, which is free.
>
> **If you want a real gravimetric instrument, buy the Davis 6464M at €149 + the €39 mount.** That is
> a defensible €89 premium for an instrument you can check with a jug, and I would not argue with it.
> **Do not buy the Pronamic** — €264 and a 3–6 week lead time for the same 0.2 mm resolution.
> **Do not buy the Misol** unless €80 genuinely matters: it is the NFR-L4 "bought once from an
> overseas seller" pattern, it lives outdoors in Hérault sun on a plastic bearing, and its resolution
> is not consistently documented.
>
> **Buy it now even though rain-skip is v2**, for exactly the reason D11 gives: the board layout is
> open once. Reserve **GPIO32** (spare, interrupt-capable, and ADC2 so useless for analogue with Wi-Fi
> anyway), fit a **10 kΩ pull-up and 100 nF** now, and land 4 cores in the cabinet.
>
> **Mounting and cabling.** ⚠️ **Mount it clear of the roof drip line and above sprinkler throw.** A
> gauge inside a sprinkler arc reads "rain" every time you water — genuinely funny, and it will
> suppress the entire schedule. Cable: **LIYCY 2 × 0.34 mm² shielded at €0.97/m TTC, in stock, 10 m
> minimum, Comptoir du Câble [V]** ⇒ **30 m = €29.10 [V]** (use the 4-core equivalent for the RG-15);
> alternatives verified: LIYCY 2 × 0.25 €0.76/m [V], Gotronic CBR222 €1.35/m [V]. **Tie the shield to
> ground at the indoor end only**, and debounce 50–100 ms in software.
> Mount: **WSF-10150 pro gauge mount, €39.00 TTC / €32.50 HT, in stock [V]**; Davis 6670 universal
> €62.00 TTC [V]; Davis 7717 95 cm extension €84.00 TTC [V].

### F2. Freeze / ambient temperature sensor — **MANDATORY**

**Role.** Feeds the **NFR-S10 freeze interlock** with the threshold set **above** zero (default ~+3 °C)
with hysteresis. Ice splits fittings, and a partly-frozen path can leave a diaphragm **unable to
seat** — which is a P0 failure, not merely a plumbing one.

**Optional? No, and it costs €8.** Skip it and fault-policy row 12 has no trigger.

| | Product | Price |
|---|---|---|
| **Minimal** | **DS18B20 bare TO-92, genuine — €3.90 TTC, Gotronic ref 42356 [V]**, potted yourself | **€3.90 [V]** |
| **⭐ Recommended** | **DS18B20 waterproof probe, 3 m lead — €7.99 TTC, Amazon.fr B016H1CCXI [V]**, or **Gotronic ref 31695 DFR0198 (Ø6 × 35, 90 cm) €7.95 TTC / €6.62 HT [V]** — one **outdoors in shade** as the interlock input — **plus an AHT20 (SEN0527) €4.70 TTC, Gotronic ref 37920 [V]** inside the cabinet as a diagnostic | **~€13 [V]** |
| *(other verified)* | DS18B20 1 m ×5 €10.45 (€2.09/pc) Amazon.fr [V] · 2 m ×5 €11.79 [V] · Velleman WPSE324 screw-terminal €12.90 Gotronic [V] · Shelly 3 m €12.98 [V] · SHT30 SEN0330 €8.20 Gotronic [V] · cabled waterproof SHT30 1 m €8.26 Amazon.fr [V] · DHT22 €10.90 Gotronic [V] | |
| **Robust** | An **SHT31/SHT30 waterproof probe** outdoors (±0.2 °C 0–65 °C, ±0.3 °C over −40…+90 °C) — best accuracy, worst wiring | ~€20 [V] |

> **My pick: Recommended — one DS18B20 outdoors + one AHT20 in the cabinet, ~€13 total.**
>
> **DS18B20 for the interlock, and the reasoning is about wiring, not accuracy.** It is ±0.5 °C spec
> and ~±0.2 °C typical at 0 °C, epoxy-potted in a 316 tube, −55 °C rated, and **1-Wire is happy over
> 3–10 m with a 4.7 kΩ pull-up**. The SHT30 is the more accurate part but **I²C over 3 m outdoors is
> marginal** — and a sensor that drops off the bus during a February frost is worse than a sensor
> 0.3 °C out. **DHT22 is disqualified**, not merely inferior: its curve degrades to ±2 °C below 0 °C
> and its open grille dies to frost and condensation — exactly the conditions the interlock is for.
>
> ⚠️ **Clone warning, and it matters because this is a safety interlock.** Sub-€1 marketplace
> "DS18B20" probes frequently contain **clone dies with 2 °C offsets, non-conforming scratchpad
> registers and broken parasitic power**. A 2 °C offset on a +3 °C threshold is the whole margin.
> **Pay €7.95 at Gotronic, or pot the genuine €3.90 TO-92 yourself.** Do not buy the 10-for-€9.69
> AliExpress pack for this job — buy those for the bench.
>
> **Set the cutoff at +2 to +3 °C, not 0 °C**, to absorb siting error, and **do not put the freeze
> sensor inside the cabinet** — that is the mistake that makes the interlock never fire, because the
> cabinet is warm. The €4.70 AHT20 answers "is the cabinet cooking?", without which **NFR-E3's +55 °C
> limit is unverifiable** — and a technical room containing a pool pump does get warm.

### F3. Valve-coil current sensing (CT) — **OPTIONAL. Defer in v1, but buy the €8 provision.**

**Role.** Distinguishes *"commanded on / current flowing / no water"* (hydraulic: closed tap,
blockage, stuck diaphragm) from *"commanded on / no current / no water"* (electrical: broken
conductor, dead coil, failed relay). The spec is right that **these look identical to you and are
completely different repairs.** It also detects a welded relay and an open coil (fault row 11).

**Optional — what you lose:** fault-policy row 11 has no detector, and diagnosing a dead zone becomes
"go and put a meter on it".

> ### ⚠️ Three of the spec's candidate parts are quantitatively unusable. This is worth reading before you buy any of them.
>
> The load is **210 mA holding at 24 VAC ≈ 5 VA**. At that level:
>
> - **SCT-013-000 (100 A : 50 mA, 2000:1) — reject, ~400× too large.** At 0.25 A primary the secondary
>   delivers **0.125 mA**, which across the standard 33 Ω burden is **4.1 mV RMS (11.7 mV pk-pk)**.
>   The ESP32 SAR ADC noise floor is ±10–20 mV — **the signal is entirely inside the noise.** Reaching
>   1 V RMS would need an 8 kΩ burden, which saturates the core. And 0.25 A is **0.25 % of rating**
>   (YHDC specify ±3 % over 10–120 A), so ratio and phase error explode. *(Salvageable by threading
>   20 primary turns through the jaws → 2.5 mA → 400 Ω → 1 V RMS. It works, and it is silly next to a
>   €1.57 part.)*
> - **ACS712-5A — reject on drift, not sensitivity.** At 185 mV/A, 0.25 A RMS gives **46 mV RMS** on a
>   2.5 V pedestal, against **21 mV RMS datasheet output noise** unfiltered (~11 mV with C_F) — an SNR
>   of 6–12 dB. The killer is the **zero-current offset drift of ±1.5 % of Vcc = ±75 mV ≈ ±0.4 A
>   equivalent: the drift alone exceeds the entire signal.** Total error ≈ **±30 % at 0.25 A**.
> - **Murata 56050C — reject on frequency.** Farnell lists its range as **20 kHz–200 kHz [V]**. It is
>   an SMPS/ballast part; at 50 Hz its 9.3 mH secondary is ~2.9 Ω, so nearly all primary current goes
>   into magnetising. ⚠️ **The same caution applies to the Pulse FIS121NL (€3.21 HT) and PE-63587NL
>   (€6.62 HT) at Farnell [V prices] — verify the frequency spec before ordering either.**
> - **Talema AS-103 — I could not confirm this part number exists in Talema's catalogue and will not
>   assert it.** Talema **AC1010** is real (10 A, 1000:1, 9.5 mm window) but is **0 in warehouse at
>   TME [V]** and no EUR price was obtainable.

| | Approach | Price |
|---|---|---|
| **Minimal** | **Skip it.** The flow meter already catches every case where no water arrives, whatever the cause. You lose the *diagnosis*, not the *detection*. | **€0** |
| **⭐ Recommended (v1)** | **Defer the sensor; buy the provision.** Reserve **GPIO36 (ADC1 — unaffected by Wi-Fi)**, leave two terminals free in the 24 VAC common, and thread the common through a spare CT window so one can be clipped on later with no rewiring | **~€2 [E]** |
| **Robust** | **ZMCT103C** (5 A : 5 mA, 1000:1, **50/60 Hz rated**, 4500 V isolation) — **€7.84 for 5 (€1.57 ea), Amazon.fr B0DCVLQVKV [V]**; bare CT ×1 €3.23 [V]; ×10 AliExpress €5.09 [V] — read via an **ADS1115 16-bit I²C ADC, €6.99, Amazon.fr B07RGHK4PT [V]** rather than the ESP32's noisy SAR | **~€15 [V]** |
| *(best engineering answer, if sourceable)* | A **Talema AC1005/AC1010-class 1000:1 CT with a 9.5 mm window**, valve wire threaded **10 turns**, **390 Ω burden**, unity buffer biased to 1.65 V ⇒ **1 V RMS at 0.25 A with no amplifier at all**, fully isolated, genuinely 50 Hz-rated. ⚠️ **out of stock, no price [V stock, no price]** | — |

> **My pick: defer, with the €2 provision — and this is the clearest "minimal is genuinely fine" case
> in the document.** The flow meter already detects every no-water condition; the CT only tells you
> *which kind*. That is a **maintenance-convenience** feature.
>
> **And its value collapses under T-INDOOR, which is a nice second-order confirmation of §2.** The
> failure the CT most helps diagnose is a **broken buried conductor** — and T-INDOOR has no buried
> conductors. Every zone's wiring becomes a 2 m run you can see and probe in 10 seconds. **If you move
> the manifold indoors, the CT drops from "worth €15" to "almost pointless".**
>
> **If you do fit it: put ONE sensor on the shared 24 VAC common, not eight.** Only 1–2 valves are
> ever energised and the firmware already knows which channel it fired. And **use the ZMCT103C, not
> the SCT-013 or the ACS712** — at 0.25 A it yields **50 mV RMS / 141 mV pk-pk** across the module's
> 200 Ω sampler, which its on-board LM358 gain trimmer lifts to **0.5–1 V RMS on a 1.65 V pedestal**
> ⇒ 9–10 effective ADC bits. Add the **€6.99 ADS1115**; the ESP32's own ADC is the weak link.
>
> *(A €0.30 alternative exists — a plain 1 Ω / 1 W low-side shunt in the shared common gives
> **250 mV RMS at 0.25 A** — but it bonds ESP32 ground to the 24 VAC common. That is essentially what
> OpenSprinkler does and it is acceptable with a class-2 transformer, but it is a deliberate
> isolation/EMC decision and it contradicts this design's "logic ground stays isolated from the valve
> common" premise. **Don't.**)*

### F4. Mainline pressure sensor — **OPTIONAL. Fit the gauge, defer the transducer.**

**Role.** Enables **pressure-decay leak testing** — close everything and watch the pressure fall —
the most sensitive leak test available and **the only one that catches leaks below the meter's
starting flow**. Also supplies real data for the FR-4.2 hydraulic budget.

**Optional — what you lose:** the sub-1 L/h leak band (which the Maddalena already largely covers),
and NFR-I1's pressure measurement has to be taken with a dial gauge instead of logged continuously.

| | Product | Price |
|---|---|---|
| **⭐ Minimal-plus (my pick)** | **Manomètre glycérine 0–10 bar, radial Ø63, 1/4" male — €12.05 TTC, rs-pompes.com ref 2812-1100 [V]** *(last units)*, on a **1/4" tee with a capped port** so a transducer drops in later | **~€16 [V]** |
| **Recommended** | The above **+ a 0–10 bar G1/4 0.5–4.5 V ratiometric transducer, IP67 steel body — €12.48 TTC, Amazon.fr B0H88XX1D3 [V]** (exact spec match) read via the **ADS1115 €6.99 [V]** | **~€36 [V]** |
| *(other verified)* | I²C variant B0H897SVPG €12.54 [V] · AliExpress G1/4 0–1.2 MPa €11.69–13.79 [V] · TECNOIOT €18.46 [V] · Ultisolar SUS304 €42.06 [V] · **AFRISO Ø63 inox glycerine gauge €75.60 TTC, bricozor [V] — a 6× premium for no benefit here** | |
| **Robust** | Industrial: **Keller PA-21Y** (0–10 bar, **0.5–4.5 V ratiometric available**, G1/4) is technically the ideal part; Huba 520 and Danfoss MBS 3000 (4–20 mA, not ratiometric) are the alternatives. **All are quote-only or configured-order in France for a single unit — I obtained ZERO live prices and will not invent them. Estimates ~€120–300 HT [E], low confidence.** | ~€150–300 [E] |

> **My pick: Minimal-plus — the €12 dial gauge on a capped 1/4" tee, ~€16.**
>
> **Fit the gauge, unconditionally.** You cannot size a zone, choose a pipe diameter, decide about a
> pressure reducer, or diagnose "the far heads are weak" without it, and **NFR-I1 makes the
> measurement a prerequisite to buying anything.** It is also the instrument you will actually glance
> at.
>
> **Do not fit the transducer in v1**, even at €12.48. Pressure-decay testing is a v3 capability, the
> cheap transducers drift, and **the capped port costs €4 to leave open.** This is exactly where the
> spec's advice applies — *over-provision the plumbing, under-provision the electronics*.
>
> ⚠️ **One thing to get right if you ever do fit it: none of the cheap transducers has any
> drinking-water certification** — no **ACS** (the mandatory French attestation for materials in
> contact with water for human consumption), no NSF/61, WRAS or KTW **[V — none of the listings claim
> any]**, and the "carbon-steel alloy" variants will simply rust. **Fit it downstream of the backflow
> device (§A6) and the question is legally moot.** Tee it into the potable feed *upstream* and you
> legally need an ACS-attested component, which no €12 part is. *(Keller PA-21Y and Huba 520 do offer
> genuinely drinking-water-approved variants — [E], not verified this session.)*
>
> Also note: many "0–1.2 MPa" listings top out at **12 bar**, so a 3–4 bar mainline uses only ~30–40 %
> of the ADC span. **A 0–6 bar variant would use the ADC far better** if you can source one.

---

## 9. §G — Enclosure and installation (re-specified for D15)

### G1. Cabinet — **MANDATORY**, and this is where D15 saves the most money

**Role.** Houses the DIN rail, the SMPS, the transformer, the timer/impulse relay, the terminals, the
ESP32 and the relay boards.

> **What D15 deleted.** NFR-E1 (IP66, UV-stabilised ASA/PC, specified IK), NFR-E2 (membrane breather,
> desiccant, downward-facing glands), NFR-E3 (not in direct sun), NFR-E5 (screened vents against ants
> and slugs) — **all void for the controller**. So is **risk R5 (condensation), rated "high likelihood,
> high impact" and described as "the leading cause of death for garden electronics"**. That risk is
> not mitigated; it is **gone**. This is the largest single reliability improvement in the D15 decision
> and it costs nothing.

**Module count first, because it decides the size.** Transformer 4–5 M + SMPS 1–3 M + Finder 80.01
timer 1 M + Finder 13.12 bistable 1 M + differential 2 M + MCB 1 M + SPD 2 M + deadman DIN carrier
1 M + terminals + the ESP/relay assembly ⇒ **~30–35 modules. A 2 × 13 M cabinet will be tight; buy
3 rows.**

| | Product | Price |
|---|---|---|
| **Minimal** | A bare DIN rail on a plywood backboard, no enclosure | ~€10 [V-derived] |
| *(cheapest enclosed)* | **Schneider Resi9 R9H13402**, 2 × 13 M, 252 × 375 × 108, IP30 / **IP40 with door**, IK08 — **€28.17 HT / €33.80 TTC, in stock [V]**, supplied with rails, terminal blocks, blanks and a labelling kit. **+ door (R9H13422) as LOT1481: €43.99 HT / €52.79 TTC [V]** | €33.80–52.79 [V] |
| **⭐ Recommended** | **Hager Gamma GD313A**, **3 × 13 M = 39 modules** — **€45.83 HT / €55.00 TTC [V, elecdirect.fr]** + opaque or transparent door (~€14–27 TTC [V range]) | **~€70–82 [V]** |
| *(other verified)* | Hager GD213A 2×13 M €37.97 TTC [V] · GD218A 2×18 M €62.50 TTC [V] · GD318A 3×18 M €94.90 TTC [V] · Legrand Drivia 1×18 M + transparent door €59.00 TTC [V] · Resi9 transparent door R9H13426 [V, listed] | |
| **Robust — lockable** | **Legrand Drivia white door 2×13 M + lock N°850 (401332 + 401391) — €28.22 HT / €33.86 TTC, in stock [V]** (door + lock only; body separate). Or **Legrand Plexo³ 2 × 12 M, 432 × 340 × 161, IP65 / IK09, with lock (001922 + 001966) — €104.57 HT / €125.48 TTC, in stock [V]** | €125.48 [V] |

> **My pick: Hager GD313A (3 × 13 M) with a transparent door, ~€75 TTC.**
>
> - **Three rows, not two.** At ~30–35 modules, a 2 × 13 M box means you will be fighting for space
>   the day you add anything — and §D4 (SPD, 2 M) and §F (a rain-gauge conditioning board) are both
>   "later" items that need room. €17 for 13 spare modules is the cheapest decision here.
> - **A transparent door** means the status LED, the deadman enable LED (§C1) and the latch state are
>   visible **without opening anything** — which is what makes NFR-D1's laminated card usable by a
>   house-sitter, and what makes the kill switch's effect legible.
> - **I considered the €125 IP65 Plexo³ and am not recommending it.** The instinct is right — the room
>   contains a pool pump, a machine with wet seals, and under T-INDOOR your own manifold — but the
>   correct answer to that hazard is **positional, not ingress-rated**: mount the cabinet **high on the
>   wall and offset from the manifold**, put the wet plumbing below and to one side, and no splash
>   reaches it. €50 of IP rating cannot fix a cabinet mounted underneath a leaking union, and good
>   siting makes IP40 sufficient. Spend the €50 on §C instead.
> - **Lockable is optional here.** NFR-SEC7 asks for it, but the room is inside the house. If you want
>   it, **Legrand's door + lock at €33.86 [V]** is the cheap route.
>
> **However you specify it, this is €40–90 *less* than the IP66 / UV-stabilised / membrane-breather /
> desiccant build NFR-E1 and NFR-E2 required — and it deletes risk R5 outright.** That is the largest
> single saving D15 produced.

### G2. Glands, labelling, and the laminated card — **MANDATORY**

| | Contents | Price |
|---|---|---|
| **Minimal** | **Digital Electric IP67 M20 glands (7–12 mm), €0.33 HT / €0.40 TTC each [V, elecdirect.fr]** — lot of 20 **€6.60 HT / €7.92 TTC [V]** — plus a marker pen and a printed A4 sheet | **~€10 [V-derived]** |
| **⭐ Recommended** | The gland lot above + **Legrand ISO 16 IP68 glands (098001) €1.05 TTC ea / lot of 10 €10.56 TTC [V]** where cables are larger + **Legrand punch kit (001955) €8.17 HT / €9.80 TTC [V]** + **laminated printed labels** at the manifold and terminals matching HA entity names exactly (NFR-I2) + **the laminated A4 card inside the door** (NFR-D1) | **~€30–40 [V-derived]** |
| **Robust** | **Dymo Rhino continuous vinyl tape, €21.58 HT / €25.90 TTC [V, bricozor.com]** — *only worth it if you already own the printer; the Rhino 4200 unit itself is ~€100+ [E]* — or engraved traffolyte throughout | ~€50–110 [E] |

> **My pick: Recommended, ~€35 — and do not buy a label printer.** For a home build, a **laminated,
> printed A4 schematic taped inside the door costs ~€2 and is genuinely better documentation than
> per-wire markers**, because it shows relationships rather than identifiers. The Dymo route only pays
> off if the printer is already on your shelf.
>
> The spec calls the laminated card *"the highest documentation ROI in the project"* and it is right:
> how to stop everything, how to run a zone, what the LEDs mean, where the bypass is, who to call.
> **It costs €2 and it is for your household, a house-sitter, or you in five years.** Add two lines to
> it from this document: *"do not plug in USB while the 5 V rail is connected"* (§B5) and *"the red
> button outside the box stops all water"* (§C3).
>
> **NFR-I2 is worth one sentence of emphasis**: mismatched zone numbering is *the #1 install-day
> error* and produces genuinely baffling symptoms. Label the manifold, the terminals, and the pipe
> ends, and make the labels read exactly what HA reads.

### G3. Cable management inside the cabinet — **OPTIONAL, take it**

Slotted trunking (goulotte), spiral wrap, and cable ties: **~€15–25 [E]**.
> **My pick: buy it.** It is €20 and it is the difference between a box you are willing to open and a
> box you dread. Given NFR-M8's twice-yearly maintenance afternoon, that is a real cost.

### G4. **NEW — Indoor leak detection and containment — MANDATORY *if* you adopt T-INDOOR**

**Role.** T-INDOOR is the one decision in this document that **introduces** a hazard: a weeping
diaphragm or a failed union now wets the technical room instead of the lawn. This item is the
mitigation, and it is what makes §2's recommendation responsible rather than reckless.

**Optional? Not if the manifold is indoors.** Skip it and you have traded a bounded outdoor nuisance
for an unbounded indoor one — which would invert P2 (availability traded for safety), not honour it.

| | Contents | Price |
|---|---|---|
| **Minimal** | **Confirm the room has a floor drain**, and site the manifold over it | **€0** |
| **Recommended** | Floor drain confirmed **+ a wired water-detection probe on the floor under the manifold, wired to a spare GPIO**, latching a CRITICAL and **closing the master valve**; **+ a shallow bunded tray under the manifold** | **~€30–55 [E]** |
| **Robust** | Two probes (under the manifold and by the door) + a motorised main shutoff upstream of the whole house | ~€150–300 [E] |

> **My pick: Recommended, and treat it as non-negotiable if the manifold moves indoors.**
>
> A wired probe — not a Zigbee/Wi-Fi leak puck. Same reasoning as the rain gauge (§F1): it must work
> with HA down, because a leak while HA is down is exactly the scenario. It is a dry contact into a
> spare GPIO with a pull-up; the firmware action is *close the master, latch CRITICAL, refuse all
> irrigation*. **This closes the loop on the one real objection to §2, and it costs €40.**
>
> **And the precondition, stated as a go/no-go:** ⛔ **if the room has no floor drain and contains
> something water-intolerant, do not move the manifold indoors.** Verify this before you buy pipe.
---

## 10. §H — Cable and trenching

> ### ⛔ **Buy nothing in this section until D16 (§2) is decided.**
> **Under T-INDOOR this entire section is €0.** It exists to cost the T-GARDEN alternative honestly,
> and it is the reason §2 matters: **this is the most expensive and least reliable ~€600 in the
> project, and it can be deleted.**

### H1. Buried valve multicore — **MANDATORY under T-GARDEN, deleted under T-INDOOR**

**Role.** Carries 24 VAC SELV from the cabinet to the valve manifold. **NFR-L5 requires 12 cores for
8 zones + master** (9 working + 3 spare) — *"the cheapest future-proofing in the project, because
digging the garden up again is the expensive part."*

| | Product | Price |
|---|---|---|
| **Minimal** | **Câble multiconducteur 0.8 mm × 7 conducteurs, 75 m — €189.00 TTC (was €243, −22 %), ref 02.51017, arrosage-distribution.fr [V]**. ⚠️ **7 cores serves 6 zones + master with zero spares — it violates NFR-L5.** | **€189.00 [V]** |
| **Recommended** | A **13-core** equivalent — **extrapolated from the verified 3-core €82.00 / 5-core €138.00 / 7-core €189.00 series at 75 m [V]** ⇒ **~€330 [E]**. Or **U-1000 R2V 12G1.5** from an electrical wholesaler, ~€2.50–4.50/m ⇒ **€190–340 for 75 m [E]** | **~€330 [E]** |
| **Robust** | 13-core **plus a spare 7-core** pulled in the same conduit for a future sub-manifold | ~€520 [E] |

> **My pick: if T-GARDEN, buy 13 cores and do not economise — but the real pick is to make this line
> €0 by adopting T-INDOOR.**
>
> ⚠️ **Get the cable *type* right, because two of the obvious choices are wrong:**
> - **U-1000 R2V is the French direct-burial standard.** If the cable is not in conduit for its whole
>   length, this is what it must be.
> - **H07RN-F is a rubber *flexible*, not a direct-burial cable.** It is commonly mis-sold for this
>   job. Do not bury it unprotected.
> - **LiYY / LiYCY must be in conduit — never direct-buried.**
>
> **NFR-L5's spare cores are not optional and they are why "Minimal" is wrong here.** A 7-core coil
> saves €140 and leaves you with **zero spare conductors on a 10-year buried asset**. R10 says buried
> splice failure is *"the leading cause of death for irrigation wiring"*, and the only cheap repair
> for a failed core is **to move to a spare**. Buying a cable with no spares converts a 10-minute
> re-termination into a re-dig. **If you are buying buried cable at all, buy the version you cannot
> regret.**

### H2. Conduit, draw string, splices, warning tape — **MANDATORY under T-GARDEN**

| | Contents | Price |
|---|---|---|
| **Minimal** | Direct-buried R2V, no conduit, 20 × **Rain Bird KING gel splices at €1.25 TTC ea (ref 02.59020) [V]** = €25 | **~€25 [V]** |
| **Recommended** | **Gaine TPC rouge DN40, 50 m, with tire-fil (draw string)** ~€40–70 [E] + **24 × Rain Bird DBM gel splices €1.45 TTC ea (ref 02.59030) [V]** = €35 + **grillage avertisseur rouge** ~€15 [E] | **~€90–120 [V/E]** |
| **Robust** | DN50 or DN63 TPC (room for a future second cable) + resin splices + full-length grillage | ~€150–190 [E] |

> **My pick: Recommended — TPC rouge with a draw string, always.** **NFR-L5 names the draw string
> specifically** and it is the cheapest item in this document: it converts "re-dig the garden" into
> "pull a new cable in an afternoon". Size the conduit **one step larger than you need** for the same
> reason.
>
> **Depth, per NFR-LEG5 [V-ish, trade consensus]: 50 cm in normal ground, 85 cm under
> vehicle-accessible surface, in gaine TPC rouge, 20 cm clear of non-electrical pipework.** ⚠️ That
> last clause is a real constraint under T-GARDEN — **the valve cable and the irrigation pipe cannot
> share a trench at the same level**, so you dig wider or deeper than you expected. **Under T-INDOOR
> the clause vanishes with the cable, and the pipe depth becomes a purely mechanical question
> (40–50 cm), free of the 85 cm vehicle rule.** That is a real trenching saving §2 did not count.
>
> **Gel splices, never bare crimps, tape or wire nuts (NFR-E4).** And **photograph every open trench
> before backfilling (NFR-D4)** — in five years that is the most valuable document in the project.

### H3. Signal cable for the rain gauge and (T-GARDEN) the meter — **MANDATORY**

**Role.** Runs the reed/open-collector signals into the cabinet.

| | Product | Price |
|---|---|---|
| **Minimal** | **LIYCY 2 × 0.25 mm² shielded, €0.76/m TTC, in stock, 10 m min, Comptoir du Câble [V]** ⇒ 30 m = **€22.80** | €22.80 [V] |
| **⭐ Recommended** | **LIYCY 2 × 0.34 mm² shielded, €0.97/m TTC [V]** ⇒ 30 m = **€29.10**; use the 4-core equivalent for the RG-15's 5 V feed | **€29.10 [V]** |
| *(other verified)* | Gotronic CBR222 2 × 0.22 shielded €1.35/m TTC ⇒ 30 m ≈ €40.50 [V] · Achat-Électrique LIYCY 2 × 0.34 at **€0.45/m HT — cheapest per metre but 0 in stock and a 373 m minimum [V]** | |
| **Robust** | Shielded cable **in its own TPC conduit**, separated from the 24 VAC runs | +€30 [E] |

> **My pick: Recommended — LIYCY 2 × 0.34, ~€29 for 30 m, shield grounded at the cabinet end only.**
> Under T-INDOOR this is the *only* cable leaving the building, which is a pleasing simplification: one
> shielded pair (or quad) to the rain gauge, and nothing else.

---

## 11. §I — Spares held on site from day one (NFR-L2)

**Role.** NFR-L2 is explicit and it is one of the highest-value requirements in the spec:
*"2 zone valves (same model), 1 spare coil, 1 flow sensor, 1 pre-flashed ESP32, spare IP68 connectors.
Trivial cost; the alternative is a dead garden waiting for an August delivery."*

**Mandatory? Yes — treat this as a single line item, not eight optional ones.** NFR-L3 requires a
replacement device commissionable in **under 30 minutes from a git checkout**, and that is arithmetic
about what is on your shelf, not about your firmware.

| Item | Product | Price |
|---|---|---|
| **2 × spare zone valve** (same model — a spare of a different model is not a spare) | Hunter PGV 1" FF, **€19.80 TTC ea [V]** | **€39.60 [V]** |
| **1 × spare 24 VAC solenoid coil** | **Solénoïde Hunter 24 V AC — €14.14 TTC, rs-pompes.com [V]** *(or ref 02.SOL24HUNT €18.20 TTC, arrosage-distribution [V])* | **€14.14 [V]** |
| **1 × spare flow-meter emitter** | **2nd Maddalena Reed Switch MVM ref 2749032 — €32.00 HT ≈ €38.40 TTC [V]** | **€38.40 [V]** |
| **1 × pre-flashed ESP32** | Included in the §B5 3-pack | *(€0 extra)* |
| **1 × spare 2-ch relay module** | Included in §B1 | *(€0 extra)* |
| **1 × spare deadman board** | Included in §C1 | *(€0 extra)* |
| **Spare connectors / gel splices** | 10 × Rain Bird KING **€1.25 ea [V]** | **€12.50 [V]** |
| *Optional: spare diaphragm* | Rain Bird 100DV membrane ref 02.100MB €29.50 TTC [V] — *Hunter equivalent not priced* | ~€30 [V] |
| **Total** | | **≈ €105 [V]** |

> **My pick: buy all of it, and note the two corrections.**
>
> ⚠️ **Correction to the spec's part number: Hunter 458200 is the 9 V DC *latching* solenoid** (6–9 VDC,
> for battery-powered controllers) — **not the 24 VAC coil [V]**. Ordering it would get you exactly the
> latching technology **NFR-S2 and R3 prohibit**. The part you want is the plain **24 VAC coil at
> €14.14**.
>
> ⚠️ **Stock the emitter, not a second meter.** The Maddalena's brass piston body is a ~15-year
> mechanical part; what fails is the **electronics/reed module**. A €38 spare emitter is the correct
> spare — a second €46 meter is not. *(Emergency stopgap if the emitter dies mid-August: a
> **YFS201 turbine at €10.90 TTC, Gotronic ref 31320 [V]** keeps volume totalising while you lose leak
> detection for a week.)*
>
> **€105 is about 8 % of the recommended build and it is the difference between a two-hour repair and
> a three-week dead garden in August.** There is no version of this BOM where skipping it is correct.

---

## 12. §J — Bench and commissioning kit — **mostly already owned; excluded from all totals**

> **D13 says the owner is a qualified electrician.** A working electrician owns a multimeter, crimpers,
> a ferrule kit, strippers and VDE screwdrivers as a matter of course. **Listing them as project cost
> would materially overstate this build**, so the table below is a **checklist with
> replacement-cost references only — none of it is in the three totals in §14.**

### J1. Probably already owned — verify, don't buy

| Tool | Why this build needs it | Replacement cost |
|---|---|---|
| **Digital multimeter** (continuity + AC volts) | **Test T1** (COM↔COM must read open — the go/no-go that can disqualify the relay board outright) and **T4** (sweep the logic rail). Also measuring 24 VAC and coil resistance (42–55 Ω on a Rain Bird DV [V]) | €20–35 budget / €110–160 Brymen BM235 [E] |
| **Crimping tool + ferrule (embouts) assortment** | §D5 requires a ferrule on every stranded conductor into a terminal | €25–45 [E] |
| **Wire strippers** | — | €12–50 [E] |
| **VDE screwdrivers** | The 230 V side (§D1, §D3) | €40–70 [E] |
| **Precision screwdrivers** | Setting the Finder 80.01's range selector, the ZMCT103C trimmer | €15–25 [E] |
| **Min/max thermometer** | Verifying **T4's cold test** and NFR-E3's cabinet temperature | €8–12 [E] |

### J2. Genuinely needed and plausibly not owned — **this is the only part that costs money**

| | Item | Price |
|---|---|---|
| **⭐ Mandatory** | **LEDs + resistors + breadboard + Dupont jumpers** — the **v0 bench rig**, where LEDs stand in for the 9 valves. **NFR-M7: "a bench rig is not optional. Changes are validated on LEDs before touching the garden."** A starter kit covers it | **~€18–30 [E]** |
| **⭐ Strongly recommended** | **8-channel USB logic analyser (24 MHz, Saleae-clone)** — est. **€8–15 Amazon.fr / €2–4 AliExpress [E]**, or a **SparkFun unit at ~€29.90 [E]** | **~€10–30 [E]** |
| **Optional** | **Bench PSU** — Korad KA3005D ~€90–130 [E] / RD RD6006 ~€70–110 [E] | €70–130 [E] |
| *(covers the same job for €8)* | A **5 V 3 A USB supply + a screw-terminal breakout** | ~€10–15 [E] |
| **Optional** | **Oscilloscope** — FNIRSI 1013D / 2C23T ~€90–150 [E], DSO138 ~€15–25 [E] | €15–150 [E] |
| **Free** | A freezer bag — for **T4's cold soak**, the cheapest test in the plan | €0 |

> **My pick: buy the LED kit and the €10 logic analyser. That is it — ~€30–45 total.**
>
> **The logic analyser is the right tool for the deadman, and better than a scope for it.** What you
> need to verify (bench item **E6**) is *"does the enable line drop within ~1 s of the heartbeat
> stopping"* — a timing question about a slow digital signal, which is exactly what a logic analyser
> answers, with a capture you can save as commissioning evidence. **A €10 analyser is overwhelmingly
> sufficient**; a scope is only needed if you suspect *analogue* edge or ringing problems, which on a
> few-hundred-hertz square wave you do not.
>
> **A bench PSU is optional here, and I would skip it.** What it buys is the ability to *sweep* the
> logic rail for **test T4** ("does 3.3 V drive the opto reliably, cold?") and to sag the supply for
> **NFR-P3**. Those are real tests — but you can do T4 crudely with a resistive divider or a couple
> of series diodes on a 5 V supply, and NFR-P6's *"pull the plug 20× at random points"* wants an
> actual mains interruption, not a PSU. **If you already own one, use it; do not buy one for this.**
>
> **An oscilloscope is optional and I would skip it too.** Note that buying the DIN timer (§C2)
> **retires verification item 12** (characterising CD4541B RC drift), which was the main thing that
> would have demanded a scope.
---

## 13. §K — Provisions for the future rainwater tank and pump (D6/D7)

**Role.** D6/D7 say there is no pump today, but a **gravity-fed rainwater tank with an optional pump**
is plausible later. The spec's guidance is exactly right and worth quoting: *"over-provision the
buried infrastructure and under-provision the electronics."* **Buy the cheap provisions now; do not
price or buy the tank.**

**Optional? The provisions are effectively mandatory because they are nearly free.** Skipping them
means re-plumbing the manifold and re-pulling cable later — which is the expensive part.

| # | Provision | What it buys | Cost |
|---|---|---|---|
| K1 | **A spare (10th) capped valve position on the manifold** | A tank branch, a 9th zone, or a filter-flushing tap, with no re-plumbing. Under approach A the **Rain 3-way PVC collector (€11.69 TTC ea [V])** gives you this **for free** — 3 × 3-way = 9 ports for 9 valves, so buy a 4th unit or simply leave one port capped | **€0–12 [V]** |
| K2 | **A tee + capped branch upstream** for the future tank feed | The physical tie-in point. **Té Plasson 32 × 32 × 32 €11.60 TTC (ref 11.70430) + Bouchon Plasson 32 €3.85 TTC (ref 11.71230) [V]** = **€15.45**. **Better: Té taraudé Plasson 32 × 1" × 32 at €8.70 TTC (ref 11.71430) [V]** + a 1" brass plug (~€2 [E]) — the **threaded** branch later takes a 1" ball valve or valve directly | **€11–15 [V]** |
| K3 | **A spare relay channel** | The 2-channel module (§B1) has **one channel spare by construction** — the master uses one. **Zero extra cost, and it is why a 2-channel module beats a 1-channel one.** | **€0** |
| K4 | **Spare GPIOs** | The pin map already leaves **11 spare** (4, 14, 16, 18, 19, 23, 25, 26, 27, 32, 33). After the rain gauge (32), leak sensor, CT provision (36) and pressure (39) there are still ~8. **No purchase; just do not spend them carelessly** — and note §B7: **do not spend GPIO21/22 or 17 on Ethernet.** | **€0** |
| K5 | **Spare conductors** — *T-GARDEN only* | NFR-L5's 12–13 cores. **Deleted under T-INDOOR** — the equivalent provision becomes *"leave the wall sleeve oversized"* | €0 (in §H1) |
| K6 | **A tee + capped port for a second flow meter** | Already bought as the turbine tee in §E1 | **€0** |
| K7 | **A spare 1/4" pressure port** | Already bought in §F4 | **€0** |
| **Total provisions** | | | **≈ €12–27 [V]** |

> **My pick: buy K1 and K2 — about €25 all in. Everything else is already provisioned by choices made
> elsewhere in this document, which is the pleasing part.**

### The finding that governs any future tank branch — now confirmed against datasheets

The spec (D7, verification item 7) assumed diaphragm solenoids need ~1–1.5 bar and that
**gravity feed below ~0.5 bar rules them out entirely**. **Both halves are now verified:**

| Valve | Minimum operating pressure |
|---|---|
| **Hunter PGV-101** | **1.5 bar** (20 psi) **[V — Hunter PGV spec sheet]** |
| **Rain Bird 100-DV / DVF** | **1.0 bar** (15 psi) **[V — Rain Bird DV Series Tech Spec]** |
| Irritrol 2400 | **conflicting: 0.7 bar** (arrosage-distribution) **vs 1.4 bar** (rs-pompes), same family **[V both, contradictory]** — get the datasheet |
| Toro EZ-FLO Plus | ~0.7 bar (10 psi) **[E — price verified, pressure spec not]** |

**1.0 bar = 10 m of head.** A gravity tank on a garden slope delivers a fraction of that. **So the
spec's conclusion stands and is now backed by manufacturer data: you cannot put a diaphragm solenoid
on a gravity-fed branch.** Two options, and you should note which you are choosing *before* you buy
the tank, not after:

1. **Pressurise the tank branch with a pump.** Then the branch uses the same PGV valves, the same
   spares, the same everything. This is much the simpler answer, and it is why K3's spare relay
   channel matters — **the pump needs a channel, and you already have one.** It also brings dry-run
   protection into scope.
2. **Use spring-return (fail-closed) motorised ball valves on that branch only.** Verified options:
   **U.S. Solid `USS-MSV00018`, 1" brass, standard port, 9–24 V AC/DC, 2-wire auto-return, Normally
   Closed — $38.76 USD, 877 in stock [V]**; stainless full-port with manual override, NSF-61,
   `USS-MSV00040` — **$64.33 USD [V]**. ⚠️ **Prices are USD ex-works US and VAT-exclusive; landed cost
   ≈ €60–75 and €95–115 respectively [E].** ⚠️ **Also note `USS-MSV00466` is the *Normally Open*
   variant — the wrong failure mode; do not order it.** Belimo/Bonomi spring-return actuators on a 1"
   body are HVAC-channel, quote-only, **no verified price**.

> **My recommendation for the future branch: plan for option 1 (a pump), and buy nothing for it now
> beyond K2 and K3.**
>
> ⚠️ **But be aware of a subtlety in the motorised-valve option that the spec does not mention.**
> U.S. Solid's *"2-wire auto-return"* is a **motor-plus-return-spring** design: **the motor is
> energised the whole time the valve is open**, so it draws continuous current rather than a pulse.
> That means (a) it must be budgeted into the transformer, and (b) it is *not* directly comparable to
> a solenoid on the "de-energised = closed" test — it *is* fail-closed on loss of power, which
> satisfies FR-3.1, but it holds open by continuous motor torque, which is a different and less
> elegant guarantee than *"line pressure forces the diaphragm onto the seat"*. **NFR-S2 prohibits
> *position-holding* motorised ball valves; spring-return ones are permitted, but verify the specific
> model returns on loss of power and does not merely stop.**
>
> ⚠️ **And the absolute rule, restated because it is the one with legal teeth (NFR-LEG1):** any mains
> top-up to the tank requires a **total air gap (AA/AB)** — a tundish discharging above the tank's
> flood level. **A valve, a check valve, and even a disconnecteur BA are all explicitly insufficient
> for fluid category 5. [V]** Design the tank inlet that way from day one; retrofitting an air gap
> into a plumbed-in top-up is a rebuild. NFR-LEG3 also requires "eau non potable" labelling at every
> outlet, a lockable/opaque tank with ≤1 mm mesh, and **a means of evaluating the volume used — which
> makes the §E1 meter a compliance feature, not just a safety one.**

---

## 14. Totals

### The three builds

**Assumptions:** T-INDOOR adopted (so §H = €0); 11 valves (8 + master + 2 spares); D = 20 m;
existing ESP32 + 8-ch relay board at €0 (D2); **all labour at €0 (D13)**; **bench tooling excluded**
(§J1). Prices TTC. HT-quoted items converted at ×1.20.

| § | Item | Minimal | Recommended | Robust |
|---|---|---|---|---|
| **A1/A2** | 9 × zone + master valve | 90 *[E, generic]* | **178** *(9 × €19.80)* [V] | 230 *(Rain Bird 100-DV)* [V] |
| A3 | Manifold | 35 *(3 × Rain 3-way)* [V] | **47** *(4 × Rain 3-way + unions)* [V/E] | 90 *(Plasson PE build)* [V] |
| A4 | 120-mesh filter | 15 *[E, generic Y-strainer]* | **50** *(Y-strainer + 2 isolators)* [V/E] | 105 *(ARKAL 130 µ)* [V] |
| A5 | Ball valves + bypass | 25 [V] | **55** *(3 × Sferaco €8.40 + fittings)* [V] | 70 [V] |
| A6 | Backflow (subject to D12) | 8 *(Socla clapet)* [V] | **17** *(GRANDSIRE 1")* [V] | 388 *(Socla BA2860 DN20)* [V] |
| A7 | DN25 tie-in + fittings | 30 [V] | **70** [V] | 170 *(+ RBM reducer €99.80)* [V] |
| A8 | PE pipe *(T-INDOOR, D=20 m)* | 200 [V] | **290** *(mixed Ø32/Ø25)* [V] | 390 [V] |
| **B1** | 9th-channel relay module | 6 [E] | **16** *(2, Amazon.fr)* [E] | 16 [E] |
| B2 | PCF8574 | 0 *(direct GPIO)* | **10** *(×2)* [E] | 10 [E] |
| B3 | RTC | 5 *(ZS-042)* [E] | **12** *(PCF85063A €3.95 + DS3231 €7.90)* [V] | 15 *(Adafruit #5188)* [V] |
| B5 | Spare ESP32 | 9 [E] | **20** *(3-pack)* [E] | 20 [E] |
| B6 | Spare 8-ch relay board | 0 | **15** [E] | 15 [E] |
| B7 | Ethernet | 0 | **0** | 12 *(W5500)* [E] |
| **C1** | Deadman | 12 [V/E] | **38** *(built twice, DIN carrier)* [V/E] | 60 [E] |
| C2 | Latching backstop | 65 [E] | **105** *(Finder 80.01 €48.90 + 13.12 + XALK178)* [V/E] | 300 *(Preventa)* [E] |
| C3 | Kill switch | 6 [E] | **17** *(XALK178)* [V] | 80 [E] |
| **D1** | 24 VAC transformer | 19 *(plug-in)* [V] | **69** *(Hager ST314 40 VA)* [V] | 84 *(Legrand 63 VA)* [V] |
| D2 | 5 V DIN SMPS | 12 [E] | **16** *(HDR-15-5, Gotronic)* [V] | 21 *(HDR-30-5)* [V] |
| D3 | RCBO / differential | 10 *(MCB only)* [V] | **64** *(R9PRA240 + R9PFC616)* [V] | 127 *(A9DB2616)* [V] |
| D4 | SPD | 0 | **0** *(assume existing — else €142)* | 142 *(R9PLC)* [V] |
| D5 | DIN rail + terminals | 18 [V] | **48** [V] | 100 [E] |
| **E1** | Water meter | 14 *(YF-B5 — **fails the requirement**)* [V] | **108** *(Maddalena MVM DN15 + reed + turbine)* [V] | 140 *(Diehl ALTAIR + Izar)* [V] |
| E2 | Pulse conditioning | 1 [E] | **2** [E] | 6 [E] |
| **F1** | Rain gauge + mount + cable | 0 *(skip)* | **128** *(RG-15 €99 + LIYCY €29)* [V] | 216 *(Davis 6464M + mount + cable)* [V] |
| F2 | Freeze + cabinet temp | 4 [V] | **13** [V] | 20 [V] |
| F3 | Current sensing | 0 | **2** *(provision only)* [E] | 15 *(ZMCT103C + ADS1115)* [V] |
| F4 | Pressure | 12 *(gauge)* [V] | **16** *(gauge + capped port)* [V] | 36 *(+ transducer + ADS1115)* [V] |
| **G1** | Cabinet | 10 *(bare rail)* [V] | **75** *(Hager GD313A + door)* [V] | 125 *(Plexo³ IP65 lockable)* [V] |
| G2 | Glands + labels + card | 10 [V] | **35** [V] | 60 [E] |
| G3 | Cable management | 0 | **20** [E] | 20 [E] |
| **G4** | **Indoor leak detection** *(T-INDOOR)* | 0 *(drain only)* | **45** [E] | 150 [E] |
| **H** | **Cable + trenching** | **0** | **0** *(T-INDOOR)* | **0** |
| **I** | Spares (NFR-L2) | 40 [V] | **105** [V] | 135 [V] |
| **K** | Future-tank provisions | 12 [V] | **25** [V] | 25 [V] |
| **J2** | Bench rig (LEDs + analyser) | 20 [E] | **40** [E] | 40 [E] |
| | **TOTAL** | **≈ €688** | **≈ €1 751** | **≈ €3 433** |

### Where the money actually goes in the recommended build

| Block | € | Share |
|---|---|---|
| **Hydraulics (§A) — valves, manifold, filter, pipe, fittings, backflow** | **707** | **40 %** |
| Power + enclosure + terminals + labels (§D, §G) | 372 | 21 % |
| Sensors (§E, §F) — meter, rain gauge, temp, pressure | 269 | 15 % |
| **Safety layer (§C) — deadman, latching backstop, kill switch** | **160** | **9 %** |
| Spares held on site (§I) | 105 | 6 % |
| Control electronics (§B) | 73 | 4 % |
| Bench rig (§J2) | 40 | 2 % |
| Provisions (§K) | 25 | 1 % |
| **Cable and trenching (§H)** | **0** | **0 %** |
| **Total** | **1 751** | |

> ### The three things this table says
>
> 1. **The recommended build is ≈ €1 751 TTC, against the spec's indicative ~€1 200** — but that
>    figure *excluded* trenching and labour, and never actually costed the cable, the valve boxes or
>    the safety layer. The ~€550 gap is almost entirely four lines that were absent rather than
>    underestimated: **§A8 pipe €290** (a T-INDOOR consequence, offset by €0 in §H), **§F1 rain gauge
>    €128** (a D11 addition), **§I spares €105** (NFR-L2, never priced) and **§C2 €105** (priced
>    properly for the first time). **Adjusting for those, the spec's estimate was good.**
> 2. **The spec's headline claim — "~75 % of that is plumbing and trenching, not electronics" — is no
>    longer true, and D15/D16 are why.** Hydraulics is **39 %** and trenching is **0 %**. The advice
>    that followed from it (*"over-provision the buried infrastructure and under-provision the
>    electronics"*) needs inverting: **under T-INDOOR there is almost no buried infrastructure to
>    over-provision.** What remains is *"over-provision the manifold and the pipe, and spend freely on
>    the safety layer, because it is only 9 % of the build."*
> 3. **The safety layer that delivers P0 costs €160 — 9 % of the build.** The deadman is €38 and the
>    latching backstop €105. **There is no defensible version of this project that economises here**,
>    and at 9 % there is no pressure to.

**Robust is deliberately absurd in two places and you should not read the €3 433 as a real option:**
€388 of it is a Socla BA2860 disconnecteur that **D12 may well say you do not need**, and €300 is a
Preventa safety relay that solves the wrong problem. **Substitute the recommended items for those two
and "Robust" falls to ≈ €2 870** — which is still more than I would spend, and the delta over
Recommended buys mostly *comfort* (a bigger transformer, an IP65 lockable cabinet, an ARKAL filter)
rather than any additional P0 coverage.

---

## 15. Buy first / buy later

### This week — the v0 bench rig (~€165)

**Everything needed to run tests T1, T3, T4 and E6 and to prove every row of the fault-behaviour
policy on LEDs, before any water exists.**

| Item | € |
|---|---|
| **LED + resistor + breadboard + jumper kit** (§J2) | 20 |
| **8-ch USB logic analyser** (§J2) — for E6, the deadman decay | 10 |
| **Deadman discretes ×2 builds** (§C1) — SN74HCT14N ×5, BAT85 ×6, 2N7000, G2RL-1-E DC5 ×2, perfboard, CNMB carrier, R/C kits | 38 |
| **RTC: PCF85063A €3.95 + DS3231 GT584 €7.90 + 5 × CR2032** (§B3) | 14 |
| **PCF8574 ×2** (§B2) | 10 |
| **2-channel relay module ×2** (§B1) | 16 |
| **Spare ESP32 3-pack** (§B5) | 20 |
| **5 V DIN SMPS — HDR-15-5** (§D2) | 16 |
| **DS18B20 + AHT20** (§F2) | 13 |
| Optocouplers, pull-ups, misc (§E2) | 3 |
| **≈ Total** | **≈ €160** |

> **Do this first, and consolidate it into one Gotronic/Reichelt order and one DigiKey order** — the
> §J1 note about €25 DigiKey shipping under €75 is the difference between €160 and €185.
>
> **Two of these are gating, not merely useful.** **T3** (*do the relays stay off with the inputs
> floating?*) is described in the spec as *"the single most important test in the project"* and it can
> **disqualify the relay board you already own**. **T1** (COM↔COM must read open) can disqualify it
> outright. **Do not buy anything in §A or §D1 until both pass**, because a bussed-COM board means the
> whole 24 VAC topology changes.

### Before the trench — the measurements, which cost €12

| Item | Why |
|---|---|
| **Manomètre 0–10 bar €12.05** (§F4) | **NFR-I1: measure before you buy.** Static and dynamic pressure decide the valve brand (PGV 1.5 bar vs Rain Bird 1.0 bar), the meter size (DN15 vs DN20), and whether §A7's pressure reducer is needed at all |
| A bucket and a stopwatch | Per-zone L/min. Decides PE32 vs PE25 per run, and the §E1 meter size |
| A tape measure | ***D*** — the room-to-fan-out distance that decides D16 (§2) |
| One phone call to the commune | **D12**: does the *règlement de service* require a disconnecteur BA (an €8-vs-€388 swing **[V both]**)? And **NFR-LEG6** — will they accept a sous-compteur for the *redevance assainissement* deduction? |

> **That is €12 and an afternoon, and it moves four purchasing decisions worth ~€600 from guess to
> fact.** It is the highest-leverage work in the whole plan.

### v1 installation (≈ €1 465)

Everything in §A, §C2, §C3, §D1, §D3, §D5, §E1, §G, §I, §K. **Order §A and §E1 together** —
arrosage-distribution is 3–5 working days and compteur-energie is 24–48 h with free shipping over
€200 HT.

### Can wait, with no penalty

| Item | € | Why it waits |
|---|---|---|
| **F1 rain gauge** *(€128)* | 128 | Rain-skip is v2. **But buy the €4 of pull-up/debounce parts and reserve GPIO32 now** — that is D11's actual point. |
| F3 current sensing | 15 | Deferred to a provision; and T-INDOOR makes it near-pointless |
| F4 pressure transducer | 20 | v3 capability; the capped port costs €4 |
| D4 SPD | 142 | **Check the consumer unit first** — probably already there |
| B7 Ethernet | 12 | **Measure RSSI for a week first** (§B7) |
| A6 disconnecteur BA | 388 | **Blocked on the D12 phone call** — do not buy speculatively |
| A7 pressure reducer | 100 | **Blocked on measuring static pressure** — pure loss below 4 bar |
| §H cable and trenching | 0–620 | **Blocked on D16.** This is the big one — do not pre-buy |
| Bench PSU, oscilloscope | 70–150 | Optional; the €10 analyser covers E6 |
---

## 16. Everywhere the spec's earlier estimate was wrong

Consolidated, because several of these change a purchase.

| # | The spec says | Verified reality | Consequence |
|---|---|---|---|
| 1 | NFR-P2 / R18: Hunter PGV at 50 Hz is **475 mA / 11.4 VA** inrush, *"~28 % higher than the 60 Hz figures"* **[V]** | Hunter's spec sheet: *"350 mA inrush, 190 mA holding, 60 Hz — **370 mA inrush, 210 mA holding, 50 Hz**"* **[V]**. **370 mA already *is* the 50 Hz figure**; the 28 % uplift was applied twice | Worst case is **17.8 VA, not 22.8 VA**. 30–40 VA is still right, but the €19 plug-in transformer moves from "clearly inadequate" to "adequate with zero margin" — still reject it, on protection grounds not capacity |
| 2 | §10: *"a retrofit pulse emitter can cost more than the meter (one French supplier lists an add-on emitter at €147)"* **[V]** ⇒ *"buy the meter with the emitter integral"* | **A 1 L/pulse emitter costs €32–59 HT**: Maddalena Reed MVM **€32 (0.5 L/pulse)**, Sensus HRI-B4 **€44–59**, Itron Cyble K1 **€54**, Diehl Izar Pulse I **€52** — all **[V]**. And **no European volumetric meter in this class has a truly integral emitter [V]** — they are all "pré-équipé" + clip-on | **The instruction cannot be followed and no longer needs to be.** Buy meter + emitter separately for €78 HT and get **twice** the specified resolution |
| 3 | §7: *"the ESP32-WROOM DevKit has no MAC/PHY"* (paraphrased in D15's framing) | The classic ESP32 **has an integrated EMAC**; it lacks only a PHY **[V]**. But RMII to a LAN8720 takes **GPIO 0, 19, 21, 22, 25, 26, 27 [V]** — colliding with **I²C on 21/22**, and the GPIO0 workaround puts the clock on **GPIO17 = the master valve** | **LAN8720 is pin-incompatible with the published pin map, not merely awkward.** W5500-over-SPI is the only route, and §B7 says don't bother in v1 |
| 4 | R15: the DS3231 on ESPHome's `ds1307` platform cannot report "I lost time" — *"or use PCF8563"* | Confirmed for `ds1307` (reads CH only, never OSF at 0x0F bit 7). **But `pcf8563`'s ESPHome driver also ignores the VL bit, so the proposed fix does not work.** The platform that *does* read the oscillator-stop bit, log *"RTC halted, not syncing to system clock"* and refuse to sync is **`pcf85063`** — all **[V], read from the ESPHome sources** | **R15's mitigation must change to PCF85063.** A module is **€3.95 TTC at Gotronic [V]** |
| 5 | Verification item 11: the ZS-042 coin-cell charging trap | Fully characterised: the **DS3231 chip has no charger**; ZS-042 clones add a **200 Ω + 1N4148** from VCC to VBAT, ~0.1 mA, which **drives the cell to ~4.5–4.7 V from a 5 V rail** — above even a LIR2032's 4.2 V ceiling **[V]**. **At 3.3 V VCC the circuit does not function at all** | **Item 11 is closed, and the fix is free: run the whole I²C bus at 3.3 V.** No desoldering. Also: **LIR2032 is stocked by neither Reichelt nor TME [V]** — an NFR-L4 liability — while a **CR2032 is €0.44 [V]** |
| 6 | §I / NFR-L2 spare: *"1 spare coil"*, elsewhere identified as Hunter 458200 | **Hunter 458200 is the 9 V DC *latching* solenoid** for battery controllers **[V]** | Ordering it would deliver **exactly the latching technology NFR-S2 and R3 prohibit.** The 24 VAC coil is **€14.14 [V]** |
| 7 | Enclosure: *"IP66 polycarbonate, DIN rail, bottom entry, membrane breather"* + NFR-E1/E2/E3/E5 + **R5 (condensation), "high likelihood, high impact", "the leading cause of death for garden electronics"** | **D15 voids all of it.** An indoor DIN cabinet is **€33–75 TTC [V]** | **Saves €40–90 and deletes R5 entirely.** The largest single reliability gain in D15, at negative cost |
| 8 | Indicative budget: *"~75 % of that is plumbing and trenching, not electronics"* ⇒ *"over-provision the buried infrastructure"* | Under T-INDOOR, hydraulics is **39 %** and trenching is **0 %** | **The advice needs inverting.** There is almost no buried infrastructure left to over-provision; the money that was going into cable, conduit, boxes and splices should go into the manifold, the pipe and §C |
| 9 | Valve boxes assumed cheap (never costed) | **Rain Bird Jumbo VB-JMB €106.00 TTC [V] / HDPE €100.32 [V]**; nine round 10" boxes would be **€342 [V-derived]** | **Two boxes are €200**, which is most of the extra pipe T-INDOOR needs. This is what tipped D16 |
| 10 | Backstop: *"a DIN timer relay driving an impulse/bistable relay"* (~€3 bounded damage, cost unpriced) | Verified twice over that **no DIN product latches** (Finder Série 80's nine functions contain no memory or manual reset **[V]**). And **every priceable télérupteur has a 230 V coil [V]** while the chain is 24 V | The doc's own 2026-09-05 update is right: **use a Set/Reset bistable, not a télérupteur** — a télérupteur *toggles*, so a looping controller can flip it back on. Real cost of the layer: **≈€105 TTC** |
| 11 | Verification item 12: characterise CD4541B RC values for ~60 min | **Moot.** A Finder 80.01 covers **100 ms–24 h** on a calibrated selector for **€48.90 TTC [V]** | Item 12 is retired. It also removes the main reason to buy an oscilloscope (§J2) |
| 12 | Verification item E4: *"does the DevKit expose VIN?"* | On the **official Espressif DevKitC the pad is labelled `5V`, not `VIN`** (30-pin clones say `VIN`). **And the USB, `5V` and `3V3` inputs are mutually exclusive — powering two at once can damage the board** | **E4 closed.** Add to the laminated card: *do not plug in USB while the 5 V rail is connected* |
| 13 | Not in the spec at all | **The EU's €150 customs-duty exemption ended 1 July 2026; a flat €3 duty now applies *per item* on non-EU consignments up to €150, until 1 July 2028 [V — DG TAXUD]** | **AliExpress is no longer the cheap route for individual cheap parts.** A €1.50 IC lands at ~€5.40. Consolidate, or buy in the EU |
| 14 | §10 candidate meters: Zenner MTKD/ETKD, B Meters GSD8-RFM/GMDM-I, Baylan, Arad | **Zenner MTKD is multi-jet, ETKD is single-jet; B Meters GSD8-RFM is single-jet, GMDM-I is multi-jet — none is volumetric [V]. Baylan and Arad are not retail-sourceable in France** | Four of the named candidates **fail the section's own defining requirement**; two more fail NFR-L4 |
| 15 | Verification item 7: *"diaphragm solenoid minimum operating pressure ~1–1.5 bar assumed"* | **PGV-101 = 1.5 bar; Rain Bird 100-DV = 1.0 bar [V both]** | **Item 7 closed, assumption confirmed.** Rain Bird is the better choice if pressure measures low. D7's gravity-feed conclusion is now datasheet-backed |
| 16 | Kill switch: implied a €5-vs-€60 trade-off | A boxed, IP66, certified, **positive-opening** Schneider **XALK178** is **€16.50 TTC [V]** | **The trade-off does not exist.** Generic buttons do not certify positive-opening; there is no rational case for one in a safety function |

### Verification-backlog items this BOM closes

**Closed:** **7** (min operating pressure — 1.5 / 1.0 bar), **8** (meter starting flow and pulse
weight — and the integral-emitter premise is void), **11** (ZS-042 charging — run at 3.3 V), **12**
(CD4541B — retired, buy a DIN timer), **19** (Finder price and latching behaviour — €48.90, and it
**does not** latch), **E4** (DevKit pad is `5V`, and inputs are mutually exclusive).
**Newly answered beyond the backlog:** **R15's mitigation** (PCF85063, not PCF8563).
**Still open and still gating:** **E1/E2/E3** (T1/T3/T4 — bench, and T3/T1 can disqualify the relay
board), **E5** (SRD -C or -D coil, sets the 5 V supply), **E6** (deadman decay), **D12** (the commune
phone call, an €8-vs-€388 swing), **D16** (§2, and it turns on measuring *D*).

---

## 17. The five measurements to take before spending anything material

1. ***D*** — pipe distance, technical room to zone fan-out. **Decides D16 (§2), and with it ~€620 of
   §H and ~€290 of §A8.** A tape measure.
2. **Does the technical room have a floor drain?** **A hard go/no-go on T-INDOOR** (§G4). Look.
3. **Static and dynamic pressure at the tie-in.** Decides valve brand (PGV 1.5 bar vs DV 1.0 bar),
   meter size (DN15 vs DN20), and whether the €100 reducer is needed. **€12 gauge.**
4. **Per-zone flow, bucket and stopwatch, 60 s.** Decides Ø32 vs Ø25 per lateral and confirms the
   meter's Q3. Free.
5. **T1 and T3 on the relay board you already own.** ~20 minutes with a DMM and your ears, and
   **either can disqualify the board and change the whole 24 VAC topology.** Do this before ordering
   anything in §A or §D.

Plus one phone call — **D12** — which resolves an **€8-vs-€388** component decision **[V both]** and a
possibly recurring *redevance assainissement* saving (NFR-LEG6).
