[← Documentation index](../README.md)

# 20. Component summary — what to buy, in order of necessity

A one-line-per-component checklist derived from [§19, the full bill of materials](19-bill-of-materials.md),
reordered so the **mandatory spend is at the top** and the **optional spend is grouped by what it buys
you**. Prices are the **⭐ Recommended** option, TTC, and every figure traces to a component section
in §19.

**Tick the boxes as you acquire things.** `- [x]` marks it done.

**Assumptions:** manifold indoors (D16 / T-INDOOR, so buried cable is €0), *D* = 20 m, 11 valves
(8 + master + 2 spares), ESP32 + 8-channel relay board already owned (€0), **labour €0** (D13),
bench tooling excluded.

| | € TTC |
|---|---|
| **Mandatory total** | **1 535** |
| Optional total (everything below) | 286 |
| **Recommended build** | **1 821** |

---

## Before buying anything — €12

- [ ] **Measure *D*** — pipe distance, technical room to zone fan-out. Decides D16 and ~€620 of spend; break-even is *D* ≈ 58 m. *A tape measure.*
- [ ] **Check for a floor drain** in the technical room. A hard **go/no-go** on putting the manifold indoors.
- [ ] **Measure static and dynamic pressure** at the tie-in — **€12 gauge**. Decides valve brand (PGV needs 1.5 bar, Rain Bird DV 1.0 bar) and meter size.
- [ ] **Measure per-zone flow**, bucket and stopwatch, 60 s each. Confirms pipe sizing and the meter's range. *Free.*
- [ ] **Run bench tests T1 and T3** on the relay board you already own — ~20 min with a DMM and your ears. **Either can disqualify the board and change the whole 24 VAC topology.** Do this before ordering anything below.
- [ ] **Phone the commune** (D12) — does the *règlement de service* require a disconnecteur BA? An **€17 vs €388** swing.

---

# Mandatory — €1 535

## Hydraulics — €707 *(46 % of the mandatory spend)*

- [ ] **€178** · **9 × Hunter PGV 1" FF solenoid valves** *(A1/A2)* — Eight zone valves plus an identical master valve in series; normally-closed, so losing power closes them by physics rather than by code. **This is what actually delivers P0.**
- [ ] **€290** · **PE pipe, mixed Ø32/Ø25** *(A8)* — Carries water from the indoor manifold out to each zone. This quantity is the direct cost of moving the manifold indoors, and it is more than offset by €0 of buried cable.
- [ ] **€70** · **DN25 tie-in and fittings** *(A7)* — Taps the mains supply and puts an isolator at the tie-in, so the whole system can be worked on without shutting off the house.
- [ ] **€55** · **Ball valves + manual bypass loop** *(A5)* — Isolators either side of the manifold plus a bypass that lets you water by hand if every electronic thing in the box is dead. **This is what makes the project socially survivable.**
- [ ] **€50** · **120-mesh Y-strainer + isolators** *(A4)* — Keeps grit out of the diaphragms. **D5 upgraded this from advisory to mandatory** — cycle-and-soak admits grit on every valve opening, and a weeping diaphragm is the one fault no electrical layer can see.
- [ ] **€47** · **Manifold — 4 × 3-way + unions** *(A3)* — The valve assembly itself, built with unions everywhere and one spare position so a valve can be swapped or a zone added without cutting pipe.
- [ ] **€17** · **Backflow protection** *(A6)* — Stops garden water siphoning back into the potable supply. ⚠️ **Make the D12 phone call first** — this is €17 or €388 depending on the commune.

## The safety layer — €160 *(9 %, and non-negotiable)*

- [ ] **€105** · **Latching max-runtime backstop** — Finder 80.01 timer + 13.12 Set/Reset bistable + XALK178 reset station *(C2)* — Force-closes the valves if any single zone stays open past its limit, and **stays closed until a human resets it**. The only layer that catches "firmware is running normally but forgot to close zone 4". Wire it on the **zone-side common** so a long cycle can't trip it.
- [ ] **€38** · **Hardware deadman** — 74HCT14 + BAT54 charge pump *(C1)* — De-energises the valve rail within ~1 s if the ESP32 stops emitting its heartbeat. **The only thing that closes a valve when the application is absent** — crash loop, safe mode, hung OTA. ~€2 of parts; the rest is the DIN carrier and building it twice.
- [ ] **€17** · **NC kill switch** — Schneider XALK178 *(C3)* — Breaks the 24 VAC common, so it stops all water **downstream of every possible electronic fault**, including a welded relay. Anyone can hit it: no tools, no app.

## Power and enclosure — €352

- [ ] **€75** · **Hager GD313A DIN cabinet + door** *(G1)* — Houses everything on DIN rail. **D15 (indoor room) is why this is €75 and not an IP66 outdoor box** — and it retires risk R5, condensation, entirely.
- [ ] **€69** · **Hager ST314 40 VA safety transformer** *(D1)* — 230 V → 24 VAC for the valves, galvanically isolated (SELV). Sized on real inrush: one zone + master ≈ 17.8 VA worst case.
- [ ] **€64** · **Schneider Resi9 30 mA type-A ID + MCB** *(D3)* — Mains protection per NF C 15-100. Verified cheaper as an ID + MCB pair than a single RCBO.
- [ ] **€48** · **DIN rail, terminals, ferrules, markers** *(D5)* — Colour-coded screw terminals for 9 valve pairs plus signals, ferrules on every stranded conductor. Unglamorous, and it is what makes the box maintainable in five years.
- [ ] **€45** · **Indoor leak detection and containment** *(G4)* — Water sensor under the indoor manifold. **Mandatory *because* the manifold moved indoors** — a leak is now inside the building rather than in a garden box.
- [ ] **€35** · **Glands, labels, laminated card** *(G2)* — Cable entries, plus zone labels matching HA entity names exactly (NFR-I2), plus the A4 card inside the door telling anyone how to stop the water (NFR-D1).
- [ ] **€16** · **Mean Well HDR-15-5 5 V DIN supply** *(D2)* — Powers the ESP32 and nine relay coils. **Never power this from USB** — nine coils is most of an amp.

## Sensing and control — €171

- [ ] **€108** · **Maddalena MVM DN15 + reed emitter + turbine** *(E1)* — The volumetric water meter is a **safety instrument first and a metering one second**: it is the only way to detect flow when all valves are commanded closed. At 0.5 L/pulse it beats the spec, and the €14 turbine lets the safety instrument detect its own failure.
- [ ] **€16** · **2 × 2-channel relay module** *(B1)* — Your 8-channel board is one short: you need **9 channels (8 zones + master)**. Putting the master on physically separate hardware also removes the board-level common-cause failure. One is a spare.
- [ ] **€13** · **DS18B20 outdoors + AHT20 in cabinet** *(F2)* — Freeze interlock (no watering below ~+3 °C, since ice splits fittings) plus a cabinet-temperature diagnostic.
- [ ] **€12** · **PCF85063A + DS3231 RTC** *(B3)* — Battery-backed clock so the schedule survives a power cut with no network. **Buy both**: DS3231 is the better oscillator, PCF85063A is the only one whose ESPHome driver actually reports "I lost time".
- [ ] **€2** · **Reed-pulse conditioning** — RC + Schmitt + opto *(E2)* — Debounces the meter's reed contact. A raw reed on a hardware counter reads 3–10 "litres" per litre.

## Resilience — €145

- [ ] **€105** · **Spares held on site** — 2 valves, 1 × 24 VAC coil, flow sensor, connectors *(I)* — NFR-L2. The alternative is a dead garden waiting on an August delivery. ⚠️ **Check the coil part number** — Hunter 458200 is the *latching* solenoid the design prohibits.
- [ ] **€40** · **Bench rig** — LEDs, resistors, breadboard, USB logic analyser *(J2)* — Nine LEDs stand in for nine valves so the whole fault table can be proven on a desk before any water is involved. The €10 logic analyser is the right tool for verifying the deadman's decay.
- [ ] **€20** · **Spare ESP32 (3-pack)** *(B5)* — One in service, one flashed on the shelf, one for the bench.

---

# Optional — €286 total

Grouped by what each group actually buys, so you can price them independently.

## 1. Don't water when it just rained — €128

- [ ] **€128** · **Hydreon RG-15 optical rain sensor + shielded cable** *(F1)* — Skips watering after rain, **autonomously** — wired to a GPIO rather than pushed from HA, so it still works with the network down. Solid-state: **no funnel to block**, and a blocked funnel fails *silently* toward watering in the rain. Cheaper than a tipping bucket (€188–264) and lower maintenance.

> **The single largest optional line, and the one I'd take first.** Beyond the water saved, it removes
> the last structural HA dependency in the design. *Note it reverses your D11 wording — you asked for a
> tipping bucket; the RG-15 does the same job better and cheaper.*

## 2. Diagnose faults you otherwise can't see — €18

- [ ] **€16** · **Pressure gauge on a capped tee** *(F4)* — Supply pressure at a glance, and the capped port lets a transducer drop in later with no replumbing.
- [ ] **€2** · **Current-sense provision** *(F3)* — Reserve a GPIO and thread the common through a spare CT window, so a €13 sensor can be clipped on later to distinguish "broken valve wire" from "valve fine, no water" — identical symptoms, completely different repairs.

> Both are *provisions*, not instruments. Full current sensing later is ~€15, a pressure transducer ~€20.

## 3. Wiring quality and serviceability — €30

- [ ] **€20** · **Cable management** — trunking, spiral wrap *(G3)* — The difference between a box you're willing to open and one you dread.
- [ ] **€10** · **2 × PCF8574 I²C expander** *(B2)* — Drives the 8 zone relays over I²C instead of 8 GPIOs. **Be clear-eyed: its safety contribution here is near zero.** It buys elimination of the GPIO12 boot-brick hazard, half the wiring, and 11 spare pins.

## 4. Survive a component failure mid-season — €15

- [ ] **€15** · **Spare 8-channel relay board** *(B6)* — A dead channel in July becomes a five-minute swap. **Buy only after T1/T3/T4 pass on the board you own** — no point stocking a spare of a board that fails the tests.

## 5. Keep the future rainwater tank cheap — €25

- [ ] **€25** · **Tank/pump provisions** *(K)* — A tee, a spare manifold position and spare conductors so a future gravity tank plus pump (D6/D7) is a bolt-on rather than a re-plumb. ⚠️ A **gravity feed below ~0.5 bar cannot operate diaphragm solenoids** — that branch will need pressurising.

## 6. Let someone water the garden without a phone — €70

- [ ] **€70** · **8 × Ø22 momentary buttons + master switch + status LED** *(FR-11)* — Lets a house-sitter or family member run a zone or hold the system with no HA access. **Wired as inputs to the firmware, never straight to a valve** — so a press inherits the duration cap, the interlock and the whole safety layer. Uses the second PCF8574 the BOM already has you buying. Schneider XB5AA21 €6.92 each **[V]**, or Gotronic BP16039 16 mm at €4.50 for a ~€45 build.

> The *dead-controller* case is covered by the **manual bypass ball valve** already in the mandatory
> list, not by these switches — if the ESP32 is dead, no button helps.

## 7. Considered and deliberately not bought — €0

Nothing to acquire; one thing to verify.

- [ ] **Check the consumer unit for an existing type-2 SPD** *(D4)* — If fitted, €0. If not, €142 — and worth it in Hérault.

| § | Component | Verdict |
|---|---|---|
| B4 | Level shifter | **Not needed.** Run the whole I²C bus at 3.3 V — the PCF8574 is spec'd 2.5–6 V, and 5 V would put 5 V on SDA/SCL where **ESP32 pins are not 5 V tolerant**. Buying one would *add* a latent-damage path, not remove one. |
| B7 | Wired Ethernet | **Not in v1.** LAN8720 is **pin-incompatible** with the pin map (RMII takes GPIO21/22 = I²C, and the clock workaround takes GPIO17 = master valve). W5500-over-SPI works but buys reliability the design explicitly doesn't depend on (P3b). |
| H1/H2 | Buried valve multicore, conduit, gel splices | **€0 under T-INDOOR** — this is the ~€620 that moving the manifold indoors deletes outright, and with it risk R10. Under T-GARDEN it comes back. |

---

## What each decision is worth, in money

| Decision | Effect on the build |
|---|---|
| **D16 — manifold indoors** | **−€360 net** (+€290 pipe, −€625 cable/boxes/splices) **and deletes risk R10** |
| **D15 — indoor room** | **−€40 to −€90** on the enclosure, **and deletes risk R5** (condensation, previously ranked #4) |
| **D13 — you're the electrician** | **−€200-ish** of call-out, and the mains side gets done properly rather than avoided |
| **D2 — ESP32 + relays already owned** | **−€25.** Small. The real value is starting bench work this week instead of after a delivery. |
| **D11 — rain sensing** | **+€128** |
| **D12 — backflow (still open)** | **€17 or €388.** One phone call. |

---

## Buy in this order

- [ ] **1. The five measurements above** (~€12) — nothing else until these are done.
- [ ] **2. The v0 bench rig, ~€165** — bench kit, RTC, 9th-channel relays, deadman parts, spare ESP32. Everything needed to prove the fault table on LEDs.
- [ ] **3. v1 installation, ~€1 465** — once *D* is measured and D12 is answered.
- [ ] **4. Deferred with no penalty** — current sensing, pressure transducer, Ethernet.

> **Do not pre-buy cable.** The master valve, filter, meter and bypass go indoors regardless, so the
> zone-valve location can stay open until the trench is dug.
