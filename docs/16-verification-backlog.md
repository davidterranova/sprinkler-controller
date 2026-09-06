[← Documentation index](../README.md)

# 17. Unverified items / verification backlog

Resolve these before freezing the design. Each is a short, concrete check.

| # | Item | How to check |
|---|---|---|
| **E7** | **Does the technical room have a floor drain?** A hard **go/no-go** on siting the manifold indoors (D16). | Look. Free. |
| **E8** | **Pipe distance *D*, technical room to zone fan-out.** Decides D16 and ~€620 of cable/conduit/boxes. Break-even is **D ≈ 58 m**. | A tape measure. |
| **E9** | **Static and dynamic pressure at the tie-in.** Decides valve brand (PGV 1.5 bar vs DV 1.0 bar), meter size, and whether a reducer is needed. | €12 gauge. |
| 1 | **Do `api: actions:` (HA→device) require `allow_service_calls`?** The two analyses disagreed; that option gates *device→HA* calls. This decides whether the structured-action config path is available at all. | Declare one action on the pool node, look for `esphome.*` in `ha_list_services`. |
| 2 | **ESPHome `valve` component + `template valve` platform**, and that the HA integration maps it to the `valve` domain. HA-side `valve.*` services are confirmed present; the ESPHome side is not. | ESPHome docs for 2026.7.x; or flash one template valve and look. |
| 3 | **`on_safe_mode:` runs early enough, and `gpio_reset_pin`/`gpio_set_level` are callable from an ESPHome lambda** on your version. **A five-minute test that matters a lot.** | Bench: force safe mode with a pin held high, scope the pin. |
| 4 | **`periodically_resetting: false` behaviour** against a monotonic NVS-backed source. | Build Zone 1 with `false` and Zone 2 with `true`, compare `last_period` after one daily rollover. |
| 5 | **`esp32: preferences: flash_write_interval:`** exact key name and semantics. | ESPHome docs / source. |
| 6 | **ESP32-S3 PCNT unit count** (the classic ESP32's 8 units and the C3's total absence are confirmed). | ESP32-S3 TRM. |
| ~~7~~ | **CLOSED** — **PGV-101 = 1.5 bar, Rain Bird 100-DV = 1.0 bar [V]**. Assumption confirmed; Rain Bird is the better pick if measured pressure is marginal. | — |
| ~~8~~ | **CLOSED** — Emitters cost **€32–59 HT, not €147**, and **no volumetric meter in this class has an integral emitter** — buy body + head separately. **Maddalena MVM DN15 + Reed MVM = €78 HT at 0.5 L/pulse.** | — |
| ~~9~~ | **CLOSED** — the pool controller is built with **ESPHome in Docker** (`ghcr.io/esphome/esphome`), and **flashed on the host** with the esptool bundled in `~/.espressif` (Docker Desktop on macOS cannot pass a USB-serial device into a container). `esp-sprinkler` now mirrors this exactly. | — |
| 10 | **What exactly your pool meter is** (make, model, pulse or manual read) — "comparable to the pool" is easiest to satisfy by specifying the same class of instrument. | Look at it. |
| ~~11~~ | **CLOSED** — **Run the whole I²C bus at 3.3 V** and the ZS-042 charging circuit does not function — no desoldering, and a plain CR2032 (€0.44) is correct. LIR2032 is stocked by neither Reichelt nor TME **[V]**. | — |
| ~~12~~ | **CLOSED** — **Retired.** A Finder 80.01 covers 100 ms–24 h on a calibrated selector for €48.90 **[V]** — no CD4541B, no RC characterisation, and it removes the main reason to buy an oscilloscope. | — |
| 13 | **Boot-time relay behaviour of whichever off-the-shelf board is chosen.** Many opto-isolated boards have pull-ups that **energise relays when the input floats** — exactly the wrong default. | Bench-test before installing. |
| 14 | **VigiEau API endpoints** (the open dataset exists; specific endpoints unconfirmed). | VigiEau docs, if v2 pursues this. |
| ~~15–17~~ | ~~KinCony B8 relay part number / drive polarity / stock~~ — **retired: no controller is being bought.** | — |
| **E1** | **Are the relay outputs dry, independent contacts?** COM↔COM must read open; each channel must have a 3-way NO/COM/NC terminal. | **Test T1.** DMM continuity, 5 min. **GO/NO-GO — the design fails outright if the COMs are bussed.** |
| **E2** | **Is the board active-low, and are the relays off with the inputs floating?** Beware "high/low trigger selectable" variants — set to high-trigger, everything inverts and the board's own input pull-ups **energise every relay the instant power comes up**, before a line of firmware runs. | **Test T3.** Power the board with nothing on the IN pins and listen. 10 min. **The single most important test in the project.** |
| **E3** | **Does 3.3 V logic drive the opto reliably, cold?** ~2.05 mA vs a 3.7 mA design point; optocoupler CTR falls at both low current and low temperature. | **Test T4.** Sweep the logic rail, then repeat in a freezer bag. If marginal, change R1 → **470 Ω** (not 220 Ω — 470 Ω already restores the design margin). |
| ~~E4~~ | **CLOSED** — On the **official Espressif DevKitC the pad is labelled `5V`, not `VIN`** (30-pin clones say `VIN`), and **USB / 5V / 3V3 are mutually exclusive — powering two at once can damage the board.** Add to the laminated card: *never plug in USB while the 5 V rail is live.* | — |
| ~~E5~~ | **Partly closed by direct read of the board** (2026-09-05): **ESP32-D0WD-V3 rev v3.1**, dual core, 40 MHz crystal, **4 MB flash**, MAC `10:06:1c:83:b9:94`, **flash voltage 3.3 V set by strapping pin**. Confirms the classic ESP32 (8 PCNT units) and the 1.75 MB app-partition arithmetic. *The relay-coil variant (-C 71 mA vs -D 89 mA) is still unread — that needs the relay board in hand.* | Read the relay can. |
| **E5** | **Relay coil variant** — SRD-05VDC-SL-**C** (71.4 mA) vs -**D** (89.3 mA), which sets the 5 V supply budget. | Read the relay can. |
| **E6** ⬆️🔧 | **Prove the deadman de-energises the rail under a *deliberately hung* firmware** — not by pulling power, which any design passes. The original LEDC + charge-pump design **would have failed this test**, which is why it is now an `esp_timer` heartbeat driving a retriggerable monostable. Also measure the decay time (~1 s). | Bench, before any valve is connected. **The single most important circuit validation in the project.** 🔧 **The firmware side now exists** (2026-09-05): the guard's heartbeat is an `esp_timer` callback gated on a counter only the main loop increments, and `sprinkler.yaml` carries two buttons — *BENCH starve the deadman* (stops feeding without hanging the CPU) and *BENCH hang the main loop* (does not return). **The circuit has not been built**, so E6 remains open. Procedure: [§21](21-bench-procedure.md). |
| 18 | **Which firmware/config puts a Rain Bird LNK2 into IQ4/MQTT mode** rather than leaving the local API. | Only matters if Rain Bird re-enters scope via the switch conditions — but then it is **the single highest-value question to ask the seller**. |
| ~~19~~ | **CLOSED** — **€48.90 TTC at elecdirect vs €72.64 at DigiKey [V]** — and verified that Finder Série 80 **does not latch**: its nine functions contain no memory or manual reset. Hence the Set/Reset bistable pairing. | — |
| 20 | **Toro (Evolution/Tempus) and Zigbee/Matter multi-zone 24 VAC controllers** were not researched — search budget exhausted. **No claim is made either way.** | Only if you want the candidate set widened. |

---
