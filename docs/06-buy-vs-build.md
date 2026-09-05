[← Documentation index](../README.md)

# 7b. Buy vs build — decision D2, resolved 2026-09-05

> ## Use the ESP32 and relays you already own. Build the software.
>
> **Revised 2026-09-05 (2):** the owner already has an **ESP32-WROOM DevKit and relays**. That makes
> **Custom-lite the outright answer**, not the fallback — the controller line item drops to **€0**, the
> KinCony B8's supply risk (R22) evaporates, and the one axis on which the B8 beat Custom-lite (0.6
> points, i.e. noise) is now irrelevant. **Do not buy a controller.**
>
> Still required, and unchanged: the **controller-agnostic external safety layer** (NC master valve,
> **latching** max-runtime timer, NC kill switch in the 24 VAC common, manual bypass), the **DS3231
> RTC**, and the **volumetric meter**. **No custom PCB. No OpenSprinkler. No cloud controller.**
>
> ⚠️ **Two things about the owned hardware must be established on a bench before it is trusted with
> water** — see [§7 · Existing hardware](05-hardware.md#existing-hardware--assessment-and-the-tests-that-gate-it).

**Weighted matrix** (8 axes; fail-closed 25, autonomy 20, HA depth 12, maintenance 12, effort 10,
flexibility 8, complexity 8, cost 5 — cost is weighted low because electronics are ~2.5 % of project
spend):

| Candidate | Score /100 |
|---|---|
| **KinCony B8 + ESPHome + external layer** | **86.6** 🥇 |
| Custom-lite (ESP32 + PCF8574 + Shelly relays, no PCB) | 86.0 |
| Full custom PCB | 77.4 |
| Rain Bird ESP-TM2 + LNK2 + layer | 69.6 |
| OpenSprinkler + expander + layer | 68.0 |
| OpenSprinkler stock | 61.6 |

**This reverses the earlier recommendation in this document**, for four reasons that all changed on
verification:

1. **OpenSprinkler is materially worse than its documentation implies.** Direct inspection of its
   schematics and firmware found **confirmed MAC97 triacs** driven from an **I²C expander whose output
   latch an MCU reset does not clear** **[V]**; **zero watchdog code in the entire firmware** (grepped)
   **[V]**; a **master valve that consumes one of only 8 station outputs**, so 8 zones + master needs a
   **€89 expander** ⇒ **€368 landed, not €259** **[V]**; an **ESP8266 that Espressif marks NRND**;
   **v4.0 still pre-order and not sold in the EU** **[V]**; **no per-zone volume in its HA
   integration** — flow *rate* only, controller-level **[V]**; and its own forum carries threads titled
   *"Valves not actually being stopped by the controller"* and *"Fuse Protection Issue, Burned Triac"*
   **[V]**.
2. **The MQTT blocker helps ESPHome more than it hurt OpenSprinkler** — ESPHome is first-party HA
   **Local Push**; OpenSprinkler is third-party HACS **local polling**.
3. **Most of the firmware effort OpenSprinkler would "save" is FR-7/8/9, which it does not provide.**
   The saving is roughly halved while the requirements go unmet.
4. **D3 neutralised its main claimed edge** — network-fault autonomy is two verified one-line ESPHome
   settings.

**Every commercial "smart" controller was eliminated on verified structure, not on price or taste:**
**Gardena** caps at **6 valves**, has **no local API**, and runs a **700-request-per-week** cloud quota
that a *5-valve* user was already exceeding in May 2026 **[V]** — plus its battery line is documented
**fail-open** (*"batteries must not be removed during irrigation (this results in continuous
irrigation)"* **[V]**), and it is the only **irreversible ecosystem** choice on the list.
**Hunter's WAND disables the X2's dial and its onboard schedule** **[V]** — you cannot have the cloud
*and* a local schedule. **Rachio** is 120 V/60 Hz with no EU model, and was **acquired by Rain Bird in
October 2025** **[V]**. **Netro's vendor documents no offline behaviour at all** **[V]**. **Solem** (a
Montpellier company, 20 km away) has **no HA integration for its 24 VAC line** **[V]**. **Rain Bird** is
much the best of them — a genuinely **Local Polling** core integration, no cloud account **[V]** — and
still fails: the **ESP-TM2 has no flow input at all**, and the Rain Bird 2.0 app migrates some
controllers onto an **IQ4 cloud platform where the LNK2 exposes no local API whatsoever** **[V]**.

## Why the ESPHome options win, and what that is actually worth

They separate from the field on exactly one axis — **fail-closed, weight 25** — and not because their
electronics are better. Everything else about a bought controller is recoverable with ~€50 of parts in
the 24 VAC common. **The one thing you cannot buy is the ~1-second deadman, because it requires an MCU
heartbeat from a live supervisory loop, and that means owning the firmware.**

But the honest calibration, at **€4.40/m³ TTC in Occitanie [V]** and ~15 L/min per zone:

| Stuck open for | Volume | Cost |
|---|--:|--:|
| 1 s (deadman) | 0.25 L | €0.001 |
| 45 min (latching max-runtime backstop) | 675 L | **€3** |
| 7 days (undetected) | 151,000 L | **€665** + erosion and saturation |

**The catastrophic case is unbounded duration, not some duration.** A latching max-runtime timer
converts unbounded to bounded just as a deadman does, and therefore **captures ~99.5 % of its value**.
That cuts against the build case and is stated plainly: the deadman is the highest value-per-euro item
*against nothing*, not against a €20 timer relay.

**Cut the PCB, not the firmware.** The custom board's sole unique contribution is the 74HC238
topological interlock — and with a master valve in series, two zones opening at once is a *pressure*
problem, not a *safety* problem. It costs ~40 hours and a decade of sole maintainership for
approximately nothing.

**One finding that reverses a hardware assumption:** the I²C expander is a **safety asset, not a
liability**. TI's PCF8574 powers up with all I/Os high → inverter low → ULN2003 off → **coil
de-energised [V]**, and ESPHome's driver transmits all-off in `setup()` before any pin is configured
**[V]**. A widely-cited HA thread documents a user abandoning **direct-GPIO** relays after one *"opened
the garage door at 2 in the morning"* — *a pull-down did not fix it* — and moving to a PCF8574 exactly
because its outputs stay inactive until the chip is initialised **[V]**. **So drive relays through an
expander, not straight off ESP32 pins**, even in Custom-lite. *(What the expander does **not** fix: it
has no reset pin, so on a watchdog reboot a relay that is ON stays ON until ESPHome asserts OFF. That
is R1 again — the deadman remains mandatory on every candidate.)*

## Two YAML lines that are safety requirements, not style

- **`inverted: true` on every relay.** A 2022 KinCony incident had *"all the relays toggle and make the
  relays on"* at boot; the vendor's fix changed exactly this one thing **[V]**. On an irrigation
  controller that opens every valve at boot.
- **An explicit `restore_mode: ALWAYS_OFF` on every valve switch.** **None of the 71 published KinCony
  ESPHome device pages sets it [V]** — every vendor example relies on a silent default for a
  safety-critical choice.

Plus `api: reboot_timeout: 0s` and `wifi: reboot_timeout: 0s` (P3b), and **wire the flow meter to a
direct GPIO** — KinCony's isolated digital inputs sit behind the I²C expander and are *polled*, so
**they cannot count pulses [V]**. That constraint is what selects the B8: it is the only KinCony board
that is a finished DIN-rail product, has a **DS3231 + battery** (FR-1.3), *and* exposes **7 free direct
GPIOs**. The **KC868-A8 has no RTC at all**, and the **KC868-E8T's relays 1–4 are mains-fed switched-live
outputs, not dry contacts** — 4 usable zones, not 8. **[V]**

## Reversibility — this is the part that actually matters

**The irreversible spend and the controller decision are almost perfectly disjoint. Once you see that,
most of the anxiety in this decision evaporates.**

| Irreversible (~€905 and nearly all the labour) | Reversible (~€350, ~4 hours) |
|---|---|
| Trenching, TPC conduit, draw string | The controller board |
| 12-core buried cable and its spare conductors | Its enclosure and power supplies |
| Valve boxes, manifold, DN25 tie-in | The firmware |
| **Valve technology — NC diaphragm vs latching (R3)** | The HA integration |
| Meter position and plumbing | The external safety layer *(portable too)* |
| Backflow protection, filter, bypass valve | |

Every surviving candidate — custom, KinCony, OpenSprinkler, Rain Bird, Hunter — presents the **identical
field interface: nine pairs of wires to 24 VAC NC solenoids on a shared common.** Swapping controllers
is a screwdriver job, not a plumbing job. The **external safety layer is controller-agnostic** (it lives
downstream of every possible electronic fault, including a welded relay or a shorted triac), so
**building it is never wasted under any future decision** — which makes it the one purchase to commit to
immediately, without further analysis. And the firmware is portable by construction: NFR-M3 already puts
the scheduler in `lib/irrigation_core/` with zero ESPHome includes, so moving from a B8 to a custom PCB
later is **a YAML pin-map change**. Even within the build branch, the PCB is a deferred decision.

**So reversibility does not pick a controller — it dissolves the question.** Commit hard and now to the
irreversible layer. Treat the controller as what it is: a €350 consumable at the end of nine wires,
revisable at any point in the next decade for the price of an afternoon.

## The 30-second decision rule

```
Are the valves NC 24 VAC diaphragm solenoids?
   NO ──▶ STOP. Fix this first. Everything else is reversible; this is not. (R3)

Is the external safety layer in the plan?
(NC master valve + LATCHING max-runtime timer + NC kill switch, in the 24 VAC common)
   NO ──▶ Add it now, before choosing anything else. ~€50–100, controller-agnostic,
          never wasted, and it is what actually delivers P0.

Hours you can give this in the next 12 months?
   >= 80 h ─▶ BUY THE BOARD, BUILD THE SOFTWARE. KinCony B8 + ESPHome.
              If B8 is sold out: ESP32 + PCF8574 + Shelly 1 Gen3. Do NOT design a PCB.
   30-80 h ─▶ BUY. OpenSprinkler AC + zone expander (you need it) + external layer
              + a separate ESPHome node for the meter.
   < 30 h ──▶ BUY SIMPLER. Rain Bird ESP-ME3 (NOT TM2 - no flow input) + LNK2 + layer.
              ASK THE SELLER: local API or IQ4/MQTT mode? If MQTT, walk away.

Tempted by Gardena / Rachio / Hydrawise / B-hyve / Netro / Solem?
   ──▶ NO. Cloud in the control path, or no HA integration, or (Gardena) 6 zones,
       no local API, a 700-req/week quota, and ecosystem lock-in.
```

**The switch conditions**, since this decision is time-driven rather than money-driven — the build is
**~308 h over 10 years vs ~145 h** for a bought option, a ~165 h gap worth ~€10,000 at €60/h, while on
*cash* the build wins by €150–460:

| If… | Then |
|---|---|
| Available time **< 80 h** in the next 12 months | OpenSprinkler + expander + layer. *A half-finished custom controller is strictly worse than a bought one — same triac-class risk profile **plus** untested code.* |
| Available time **< 30 h** | Rain Bird ESP-ME3 + LNK2 + layer + ESPHome meter node |
| ~~The B8 is out of stock~~ | ~~Custom-lite~~ — **moot: Custom-lite on owned hardware is now the plan.** |
| **The owned relay board fails bench test T1 or T3** | Replace **the relay board only** (~€10, or 9 × Shelly 1 Gen3 at €13.90 for a CE-marked EU-stocked option). Everything else stands. |
| You want the PCB exercise | Do it as **v2**, after a season. The firmware ports for free, so deferring costs nothing. |
| A pump / rainwater tank / second hydraulic group appears (D6, D7) | **Stay on the build** and rule out bought options harder — trivial in `irrigation_core`, impossible in a closed controller. |
| **You stop caring about FR-7 / FR-8 / FR-9 as specified** | **OpenSprinkler + layer.** *This is the real hinge:* those three are ~130 h and they are the entire reason to build. If "it waters and I can see litres" suffices, buy. |

---
