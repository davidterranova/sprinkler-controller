[← Documentation index](../README.md)

# 7. Hardware architecture

## Recommended stack

| Decision | Choice | Reason |
|---|---|---|
| **Valves** | **24 VAC normally-closed diaphragm solenoids** (Hunter PGV / Rain Bird DV / Toro), 8 zones + **1 identical master** | De-energised = closed, by physics. On power loss **line pressure itself forces the diaphragm onto the seat [V]** — no spring, no capacitor, no stored energy. Also SELV (gel splices in a flooded valve box are normal), universally stocked in France, and the body's manual bleed screw is a free mechanical override. |
| **Switching** | **Mechanical relays**, not triacs or SSRs | At ~730 ops/zone/year against a 100 k rating, wear is irrelevant (~135 years), so the choice reduces to failure mode: **a relay fails open (fail-closed); a triac fails shorted (fail-water-on).** |
| **Controller** | **The ESP32-WROOM DevKit already owned**, running ESPHome | D2 as revised. €0, and it is the *right* chip — the classic dual-core ESP32 has the PCNT hardware the flow meter needs, which the ESP32-C3 lacks entirely. |
| **Relay bank** | **The relays already owned**, subject to the bench checks below | 9 channels needed (8 zones + master). Relay contacts fail *open*, which is the correct failure mode. |
| **Relay drive** | **8 zones and the master valve, all on direct GPIO** | The guard can only hard-close pins it writes itself, so every pin that can reach a coil is one it owns. The deadman does the safety work. Master on its own module so no single board-level fault can weld a zone and the series element shut together. |
| ~~**Interlock**~~ | ~~74HC238 1-of-8 decoder~~ — **dropped with the custom PCB (D2)** | Topologically elegant, but with a master valve in series two zones opening at once is a *pressure* problem, not a *safety* problem. It was the PCB's only unique contribution and did not justify ~40 h plus a decade of sole maintainership. Sequential execution is now enforced in firmware (`sprinkler`, which guarantees one `active_req_`) and bounded by the master valve. |
| **Series element** | Master valve on a **separate GPIO and separate relay** | A hydraulic AND gate: watering requires two independent circuits to both work. Restores fail-closed even against a welded zone relay. |
| **Deadman** | **AC-coupled charge pump** gating a global enable line | The highest value-per-euro item in the build (~€2) — see below. |
| **Backstop** | A max-runtime timer on the **zone-side common** that **latches** — in practice a DIN timer relay (Finder 80-series or equivalent) **driving an impulse/bistable relay, plus a manual reset button**, since "latching" is not itself a purchasable timer function | The only layer that catches "firmware is running normally but forgot to close zone 4", and it is what converts *unbounded* damage to **~€3 bounded**. **It must latch**: a self-resetting timer facing a hung controller re-arms every 45 min and waters 24× a day. Require manual reset and wire the latch state to an HA binary sensor so the latch itself raises a CRITICAL. **See the placement warning below — putting it in the overall 24 VAC common is a latent bug.** |
| **Metering** | **One DN20 volumetric (piston) water meter with a 1 L/pulse emitter**, downstream of the master; optionally + a turbine for high-resolution rate | See [§10](09-water-metering.md). |
| **Power** | 230 V → **24 VAC 30–50 VA safety transformer** (valves) **+ a separate 230 V → 5 V DIN SMPS** (logic) | Valve inrush cannot brown out the MCU; logic ground stays isolated from the valve common. |
| **Time** | **DS3231 TCXO + coin cell**, storing **UTC** | **±2 ppm (0…+40 °C) ≈ 63 s/year [V]** — a garden box swings 50 K, which is exactly why you want a TCXO. The DS1307 that OpenSprinkler uses is ±20–50 ppm, i.e. 10–25 min/year. Same price. |
| **Enclosure** | IP66 polycarbonate, DIN rail, bottom entry, **membrane breather** | NFR-E2. |
| **Manual override** | Quarter-turn **bypass ball valve** around the whole manifold + an **external NC kill switch in the 24 VAC common** | The bypass makes the system socially acceptable — "the sprinkler computer is broken" never means "the garden dies". The kill switch breaks the common, so it is **downstream of every possible electronic fault including a welded relay**, which nothing else achieves. |

## The deadman — why ~€2 is the most important spend

The MCU must continuously emit a **square wave** on a dedicated GPIO. That AC signal is capacitively
coupled into a diode charge pump whose output holds the global valve-rail enable asserted; a bleed
resistor discharges it with a ~1 s time constant.

```
GPIO13 ──▶[74HCT14 buffer on 5 V]──[100 nF]──┬──|>|──(BAT54)──┬── 4.7 µF ──┬──▶ enable gate
                                             │                 │            │
                                             └──|<|── GND      └── 220 kΩ ──┘  (τ ≈ 1 s)
```

> **Correction to an earlier draft of this schematic.** As originally drawn — a 3.3 V GPIO through two
> **1N4148** silicon diodes — the charge pump yields roughly **3.3 − 2×0.7 ≈ 1.9 V**, which will not
> reliably turn on a MOSFET gate. Two changes fix it: **buffer the GPIO through a 74HCT14 running on
> 5 V** (a 74HC**T** part accepts 3.3 V logic levels from a 5 V rail), and use **BAT54/BAT85 Schottky**
> diodes (~0.3 V drop each). Convenient side effect: the same 74HCT14 package provides the Schmitt
> trigger the reed-switch flow meter needs ([§10](09-water-metering.md)).

**Why it is qualitatively better than a level-based enable:** it is sensitive to *activity*, not
*state*. A level-based enable **cannot distinguish "firmware fine, pin high" from "firmware dead, pin
high"**. Only a *changing* signal passes a capacitor. It therefore catches an MCU halted with the
enable pin high — **the dangerous case nothing else in this design catches** (see [§16 R1](15-risk-register.md)).

> ## ⚠️ CRITICAL — the deadman cannot be built as drawn above. Corrected 2026-09-05.
>
> Verified against ESPHome sources. **Two independent facts make the "LEDC square wave, re-armed from
> the supervisory loop" design non-functional**, and because this is the component that delivers P0,
> it matters more than anything else on this page.
>
> 1. **LEDC free-runs in hardware.** Once started it keeps producing the square wave *whether or not
>    any software is alive*. A hung main loop, a crash loop, safe mode — the wave continues, the charge
>    pump stays charged, and **the valve rail stays enabled**. The circuit would protect nothing. This
>    is exactly the "very elegant device that protects nothing" failure this section warned about, and
>    the design fell into it anyway.
> 2. **You cannot simply toggle the pin in `loop()` instead.** ESPHome's `loop_interval_` defaults to
>    16 ms, capping the achievable rate at **~31 Hz**. At 31 Hz the passive charge pump as specified
>    (100 nF / 4.7 µF / 220 kΩ) settles to about **1.75 V — below a logic-level MOSFET's V<sub>GS(th)</sub>**,
>    so the enable never asserts at all.
>
> **The fix has two halves, and both are needed:**
>
> - **Generate the heartbeat from an independent `esp_timer` callback, not from LEDC and not from
>   `loop()`.** The callback toggles the pin *and* checks a counter incremented by the main loop; if
>   the counter is stale it simply stops toggling. This makes **both** failure modes trip: the main
>   loop hanging (counter goes stale) *and* the whole runtime wedging (the callback stops running).
> - **Replace the passive charge pump with a retriggerable monostable** — a **74HC123** or **CD4538**
>   with a ~1 s period, retriggered by each edge. Output holds only while edges keep arriving, and it
>   drops cleanly ~1 s after they stop. This removes the V<sub>GS(th)</sub> threshold problem
>   entirely, tolerates a much lower heartbeat frequency, and does not care about diode drops.
>
> **Status: design corrected, not yet validated.** This must be proven on the bench before any valve
> is connected — verification item **E6**, now upgraded from "check the decay time" to "prove the
> circuit de-energises under a *deliberately hung* firmware". Test it by hanging a task on purpose,
> not by pulling power.

**Firmware discipline this imposes:** the heartbeat must depend on *evidence that the application is
alive*, never merely on hardware continuing to run. If you can stop the code and the wave continues,
the design is wrong. Test that specific case explicitly.

## Where the max-runtime backstop is wired — and why it matters

> ⚠️ **Correction (2026-09-05).** An earlier draft placed this timer "in the 24 VAC common". **That is
> a latent bug**, and it only shows up once cycle times are taken seriously.

A full cycle is the **sum** of all zone runs and can plausibly reach **4 h 30 or more** (see
[FR-4](02-functional-requirements.md)). The master valve should stay energised across the whole
cycle — dropping it between zones re-primes the mains and invites water hammer. So the overall
24 VAC common is **continuously live for the entire cycle**.

A 45-minute latching timer in that common therefore **trips mid-cycle and shuts the garden down** —
a safety device manufacturing the outage it exists to prevent, and one that would only appear in
August with everyone away.

**Correct placement: the zone-side common** — the shared return of the eight zone valves, downstream
of the zone relays and electrically independent of the master valve feed.

| | Overall 24 VAC common ❌ | Zone-side common ✅ |
|---|---|---|
| What it measures | Total cycle time | **A single zone's run** — which is what the requirement always meant |
| Long cycle | **Trips spuriously** | Irrelevant — cycle length no longer interacts with the backstop |
| Zone relay welded on | Catches it | Catches it |
| Master relay welded on | Catches it | Doesn't need to — no water flows without a zone open, and the flow meter covers it |
| Reset | Only between cycles | **The inter-zone settle gap resets it, automatically** |

### How to actually build the latch — two boxes, and use Set/Reset not a télérupteur

**Verified 2026-09-05: no single DIN product implements "latching max-runtime timer".** Finder's
Série 80 datasheet (S80FR, ed. X-2025) was read in full — its nine functions (AI, DI, SW, LI/LE, BE,
CE, DE, BI, SD) contain **no manual-reset, memory or self-hold behaviour anywhere in the series**.
So the backstop is genuinely a **timer + latch pair**.

**The trap in the near-miss options:**

- **Finder 80 in `DI` (interval) mode** looks like a one-shot: on power-up the contact goes to work
  position and returns to rest at the end of *T*, staying there. But **its reset is "remove the
  timer's supply", not "press a button"** — so a crash-looping ESP32 that re-pulses its enable output
  power-cycles the timer and buys itself **another full T minutes, indefinitely.** That is precisely
  the failure being defended against. `DI` alone is *not* a human-only latch.
- **Legrand 412401** (télérupteur silencieux et temporisé, 5–60 min, one module, ~€31 HT) is the only
  verified single-box product whose range reaches 60 min — but after timeout it returns to
  "waiting for an impulse", so the ESP32 can immediately re-trigger it. Bounds each run without
  requiring a human. A decent cheap belt-and-braces layer; **not** the latch.
- **Finder 13.61/13.62** is the closest to timer-plus-latch in one 17.5 mm box, but **its `IT` timer
  maxes out at 20 minutes**, so it cannot reach 60.

> ⚠️ **Use a Set/Reset bistable, not a plain impulse relay (télérupteur).** A télérupteur **toggles on
> every pulse** — so a stray pulse, contact bounce, or a second trigger from a looping controller
> flips it *back on*. A bistable with **separate SET and RESET inputs** cannot do that: per Finder's
> S13FR datasheet, *"only a pulse on the RESET command will open the relay's contacts."* The ESP32
> path can only ever SET; the timer can only ever RESET; nothing can accidentally un-latch. **This
> distinction is load-bearing for P0 and costs nothing to get right.**
> The **Finder 13.12.0.024.0000** is 12–24 V AC/DC, keeping the whole latch inside the 24 VAC domain.

Also note the **e-stop cheap-vs-proper gap has closed**: a genuine Schneider **XB5AS8445** is
€11.25–12.68 HT at three independent French distributors, and the complete IP66 yellow enclosure
**XALK178** (1 NC, **positive-opening contact**, 300 k mechanical / 10⁶ electrical cycles at AC-15) is
€13.75 HT **[V]**. There is no rational reason to fit a €5 no-name with no traceable part number for a
safety function — and the generic ones do not certify positive-opening.

**Consequent requirement:** the inter-zone settle gap (`valve_open_delay`) must be **comfortably longer
than the timer's reset time**. The 5–10 s gap already specified for diaphragm seating and pressure
recovery covers this, but it is now a **safety requirement rather than a hydraulic nicety** — set it,
and verify the timer actually resets within it on the bench (E6).

Set the period to ~1.5× the longest legitimate *single-zone* run — so if rotor zones need 35 min,
set 60 min, not 45.

## Existing hardware — assessment and the tests that gate it

The owner already has an **ESP32-WROOM DevKit** and an **8-channel relay board**. Verdict: **the chip
is right, the relay contacts are comfortably adequate, and three properties of the relay board must be
measured before it is trusted with water.**

**What is settled [V]:**

| Question | Answer |
|---|---|
| Right ESP32? | **Yes.** Classic dual-core, **8 PCNT units** for the flow meter (the C3 has none). |
| Do 9 relays + flow + I²C + deadman fit? | **Yes, with 11 spare pins**, even on the tighter 30-pin DevKit. |
| Is 24 VAC @ ~0.5 A within the contact rating? | **Yes, by ~6–14×.** SRD-05VDC-SL-C: 7 A/240 VAC resistive, **3 A/120 VAC inductive**, 250 VAC max, 10⁵ electrical ops ≈ 137 years at this duty. |
| 30-pin vs 38-pin DevKit? | **No practical difference** — the 38-pin's extra pins are GPIO6–11 (SPI flash, untouchable) and GPIO0 (strapping). Confirm your board exposes **VIN**, not only 3V3, so it can run from the DIN supply. |

**The decisive verified fact** — ESP32 Datasheet v5.3, Appendix A.1 note 9: *"During reset, all pins
are output-disabled."* Every pin is high-impedance at reset. On a standard **active-low** opto relay
board, whose `IN` pins are pulled *up* through the board's own 1 kΩ, that means **"ESP32 booting,
absent, in reset, or with the wire fallen off" resolves to relay OFF by construction**, with no
external components. That is a materially better position than this document assumed when it was
written against an active-high drive model — and it is why the widely-cited "direct GPIO opened the
garage door at 2 a.m." failure does not transfer here: that board was active-high.

### The three go/no-go bench tests

**T1 — Dry, independent contacts.** DMM continuity, **COM↔COM must be open**. If the COMs are bussed
the board cannot switch 24 VAC and is unusable — the same defect that disqualified the KinCony E8T.
Each channel must have a **3-way** screw terminal (NO/COM/NC), not 2-way.

**T3 — Polarity and floating-input behaviour. This is the single most important test.** Power the
board with **nothing connected to the IN pins** and listen. Relays must stay silent. The dangerous
variants that exist in the wild: **"high/low trigger selectable" boards** (a jumper or solder blob —
if yours is set to high-trigger, everything inverts and the board's own input pull-ups **energise every
relay the instant power comes up**, before a line of firmware runs);
Ten minutes with a 5 V supply and your ears.

**T4 — 3.3 V drive adequacy. Marginal by construction, and a well-documented real failure.** With
`VCC = 5 V` and a 3.3 V push-pull driver, the "off" state leaves **1.7 V across the opto LED** — near
the threshold, so the relay can sit half-on, chatter, or latch. The fix is to feed the **input-header
VCC from 3.3 V**, which gives correct polarity in both states but only **~2.05 mA** through the LED
against a 3.7 mA design point. Optocoupler CTR falls at low current *and* at low temperature, so a
relay that works on a warm September bench can fail to pull in on a February morning. **Test cold**
(a freezer bag) and sweep the logic rail down. If marginal, change R1 from 1 kΩ to **470 Ω** (≈4.4 mA)
— **not** the 220 Ω some vendor notes suggest: 470 Ω already restores the design margin, so the extra
drive buys nothing and only heats the LED.

Also: **remove the JD-VCC jumper.** With it fitted the board is *not* isolated despite the
optocouplers. Feed JD-VCC + GND from 5 V, feed the input-header VCC from the logic rail, and **do not
connect logic ground to the board's GND** — the input side needs only VCC and the IN pins.

### The 9th channel — and a free safety upgrade

You need **9 channels (8 zones + master)** and have 8. **Add a separate 2-channel opto module for the
master valve (~€3), and buy two so one is a hot spare.**

Not merely because it is cheapest. The master is the series element of the hydraulic AND gate, so
putting it on a **physically separate board with its own power feed and its own driver pin** means no
single board-level common-cause failure — a shorted rail, a flooded connector, a crack across the PCB
— can weld a zone relay *and* the master closed simultaneously. That is the "both welded (double
fault)" row this document currently marks *not covered*, **partially covered for €3**.

### Pin map

| Function | Pin |
|---|---|
| I²C SDA / SCL (DS3231) | **GPIO21 / GPIO22** |
| **Zones 1–8** | **GPIO 32, 33, 25, 26, 27, 23, 19, 18** — `inverted: true` (active-low board) |
| **Master valve** | **GPIO17** — on a *separate* relay module, not the 8-channel board |
| **Deadman heartbeat** | **GPIO13** |
| Flow meter pulse | **GPIO35** (input-only; needs an external 10 kΩ pull-up) |
| Latching-timer "tripped" sense | GPIO34 (input-only, external pull-up) → HA CRITICAL |
| Kill-switch sense / current sense (CT) | GPIO39 / GPIO36 (ADC1 — unaffected by Wi-Fi) |
| **Spare** | **4, 14, 16** — three output-capable pins, for the status LED, the physical button and the rain-gauge pulse input (D11). The four input-only pins are all allocated above. |

**Never use GPIO 0, 1, 2, 3, 5, 6–11, 12, 15.**

> ### ✅ As wired, 2026-09-06
>
> The eight relay inputs are on **32, 33, 25, 26, 27, 23, 19, 18**; master **17**; deadman **13**.
> The firmware matches (`sprinkler.yaml`), and this is the as-built record for NFR-D4.

> ⚠️ **GPIO12 is the one that must be respected absolutely.** Every relay `IN` line carries a 1 kΩ
> pull-up. On GPIO12 (MTDI) that holds the strapping pin high at reset, which selects a **1.8 V
> VDD_SDIO**, browns out the 3.3 V flash, and **the board will not boot or flash [V]**. Nine strong
> pull-ups on nine ESP32 pins is nine chances to make this mistake — including a future you rewiring
> the box with cold hands. The firmware cannot make it — `irrigation_guard`'s config schema rejects
> GPIO12 outright — so **it is purely a wiring hazard, and GPIO12 sits directly next to GPIO13, the
> deadman, on the 30-pin header.** Label that pin in the enclosure.

### Direct GPIO, and what actually delivers P0

**Every relay input is on a GPIO the firmware owns directly.** The reason is `irrigation_guard`: it
can only hard-close pins it writes itself, and its raw sweep is the layer whose entire job is proving
fail-closed. A pin the guard cannot reach is a coil the guard cannot open — precisely in the case the
sweep exists for, where the main loop is hung and nothing cooperative runs.

Be clear about what carries the guarantee, though: **the deadman delivers P0**, not the pin topology.
The active-low board plus the verified *"pins are output-disabled during reset"* gives relay-OFF at
cold boot, and the deadman gates the rail. Nothing in the output path has a reset pin, so **on a CPU
reset (R1) an output that is ON stays ON** until something asserts OFF. That something is the deadman.

**Keep the master on its own relay module** so the two halves of the hydraulic AND gate fail
independently — a shorted rail or a cracked PCB must not be able to weld a zone and the series element
shut at the same time. It also solves the 9th-channel problem for free.

**Do not drive the relays through a ULN2003** — that would invert the polarity into all-relays-on.

### What owning this hardware is actually worth

**~€25.** Electronics come to ≈€480 against a ~€1,200 project. The real value is that it is
**available today**, so v0 bench work starts this week instead of after a delivery — which is worth
more than the €25. **It is not, however, a reason to accept a board that fails T3.**

## Fault coverage

| Fault | Covered by |
|---|---|
| Firmware opens two zones | `sprinkler`'s single `active_req_`; damage bounded by the master valve — a pressure fault, not a safety one |
| Firmware forgets to close a zone | Latching max-runtime timer **on the zone-side common**; flow anomaly |
| **MCU hangs, output stuck high** | **Deadman (enable drops in ~1 s)** |
| Reboot / watchdog / brownout | Enable pulled to disabled by reset; relays default off |
| Boot GPIO glitch | Pull-downs + enable gating; <10 ms is hydraulically inert anyway |
| Power loss | Valve technology (N/C solenoid) |
| Wi-Fi / HA loss | Not a fault — the device schedules locally |
| Zone relay welded shut | Master valve still closes; current sense detects and alarms |
| Master relay welded shut | Decoder still limits to one zone; current sense detects |
| **Both welded (double fault)** | **Partially covered** — putting the master on a **physically separate relay module** with its own feed removes the board-level common-cause path (shorted rail, flooded connector, cracked PCB) for €3. The residual independent-double-failure case remains uncovered: manual kill switch / RCBO. Accepted and documented. |
| **Valve diaphragm weeping (grit)** | **Only the flow meter.** No electrical layer sees this. Mandatory 120-mesh filter upstream. |

## Indicative budget

| Variant | Total (excl. labour & trenching) |
|---|---|
| Minimal | ~€680 |
| **Recommended** | **~€1,200** |
| Robust (2 meters, pressure sensor, current sense, SPD) | ~€1,450 |

**~75 % of that is plumbing and trenching, not electronics.** So: **over-provision the buried
infrastructure and under-provision the electronics** — spare conduit, spare conductors, a spare valve
position on the manifold, a tee for a second flow meter. Do not compromise the electronics to save €30.
