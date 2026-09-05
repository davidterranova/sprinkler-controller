[← Documentation index](../README.md)

# 6. Non-functional requirements

## Reliability

| ID | Requirement | Verification |
|---|---|---|
| NFR-R1 | No single software fault results in water flowing more than **60 s** beyond its commanded duration. | Fault injection: kill the scheduler mid-run, assert the independent guard closes the valve. |
| NFR-R2 | ≥30 days fully offline while executing the last committed schedule. | Soak test with the AP powered down. |
| NFR-R3 | Unplanned restarts ≤1 per 30 days steady state. | Publish `reset_reason` + uptime; a rising restart count is itself an alarm. |
| NFR-R4 | Missed-cycle rate ≤1 % per season, excluding deliberate skips. | Derived from FR-7 records — the audit trail *is* the reliability instrument. |
| NFR-R5 | **Water-accounting completeness ≥99 %** of delivered litres attributed to a zone and a cause. | Lifetime meter total vs sum of per-run volumes. Residual = unexplained water. Excellent single-number health metric. |
| NFR-R6 | Power-cut recovery fully automatic, all valves closed. | Pull the plug 20× at random points, including mid-run and mid-flash-write. |
| NFR-R7 | Persistent state survives power loss with ≤5 min data loss. | Matches the flash write-interval design. |
| NFR-R8 | No unbounded heap growth over 30 days. | Publish free-heap as a diagnostic; alarm on a downward trend. |

## Safety

| ID | Requirement |
|---|---|
| NFR-S1 | Normally-closed valves; de-energised = no water. **The driver input must resolve to "coil off" whenever the MCU is absent, held in reset, or booting** — by a pull-*down* on an active-high driver, or by the board's own pull-*up* on an active-low opto board. **Which one applies is a bench measurement, not an assumption** (test T3). |
| NFR-S2 | **No latching/bistable solenoids and no position-holding motorised ball valves on zones.** Both fail *in place*. **[V]** |
| NFR-S3 | `restore_mode: ALWAYS_OFF` on every valve switch, never `RESTORE_*`. **[V]** |
| NFR-S4 | Every open-valve command carries an expiry, enforced at the driver layer (P4). |
| NFR-S5 | Independent watchdog + **hardware deadman on the valve rail** + a purely hardware NC interlock (kill switch / rain contact) in the 24 VAC common, so at least one shutdown path involves no software at all. |
| NFR-S6 | Normally-closed **master valve** upstream, closed whenever no zone is intentionally running. |
| NFR-S7 | Unexpected flow is detectable (P5). |
| NFR-S8 | Absolute per-zone max on-time **and** an absolute per-day per-zone volume budget, both independent of the scheduler's bookkeeping (this is what bounds the reboot-loop case). |
| NFR-S9 | Latched CRITICALs lock out irrigation; clearing requires explicit attributed acknowledgement. |
| NFR-S10 | **Freeze interlock** with the threshold set **above** 0 °C (default ~+3 °C) with hysteresis. |
| NFR-S11 | NF C 15-100: dedicated circuit, **30 mA type-A RCD**, earthed class-I equipment, valve wiring as **TBTS/SELV** from a safety isolating transformer, not sharing conduit with LV. **[V]** *D13: the owner is the electrician, so this is a specification checklist rather than a contractor dependency — and the BOM carries no labour allowance.* |
| NFR-S12 | No stored-energy hazard: any UPS must not be able to hold a valve open after the controller decided to close it. |

## Security

The ESPHome threat model explicitly assumes devices are not exposed to untrusted networks **[V]**, so
the network design *is* part of the security design.

| ID | Requirement |
|---|---|
| NFR-SEC1 | No cloud dependency for any core function. |
| NFR-SEC2 | **Noise API encryption with a unique 32-byte key.** `api: password:` was removed in ESPHome 2026.1.0 — encryption is the only supported auth. **[V]** |
| NFR-SEC3 | OTA auth with a unique per-device password, SHA256 (MD5 removed in 2026.1.0) **[V]**. OTA **shall refuse to run while a valve is open.** |
| NFR-SEC4 | If `web_server:` is enabled, username/password is mandatory — it has **no CSRF protection by design [V]**. Prefer enabling it only for commissioning. |
| NFR-SEC5 | Secrets in `secrets.yaml`, **`.gitignore`d in the very first commit.** Retrofitting a secrets leak out of git history is miserable. |
| NFR-SEC6 | Segmented IoT VLAN, no inbound internet exposure, no port forwarding. (You run UniFi — also set a static DHCP lease.) |
| NFR-SEC7 | Lockable enclosure; unpopulated debug header. State explicitly whether a physical attacker flooding the garden is in scope (probably not). |
| NFR-SEC8 | Flash encryption / secure boot: **deliberately out of scope** — poor effort/benefit for a private garden and it complicates OTA recovery. |
| NFR-SEC9 | Anyone on the trusted LAN may hold and run zones; actions are **attributed and logged**. |

## Maintainability

| ID | Requirement |
|---|---|
| NFR-M1 | Config as code in this repo; the device is reproducible from a clean checkout + `secrets.yaml`. Nothing exists only in a web UI. |
| NFR-M2 | **Build on ESPHome's `sprinkler` component rather than reinventing the sequencer.** Its three gaps *are* this project: no scheduler, no run-state persistence, no max-runtime watchdog. **[V]** |
| NFR-M3 | **The scheduler must be testable off-device** — a pure `next_actions(now, config, state) -> plan` function. Testing a scheduler by waiting until 05:00 is not viable. |
| NFR-M4 | Structured logging, retrievable without a laptop in the garden. |
| NFR-M5 | OTA without opening the enclosure, plus a documented recovery path for a bricked device. |
| NFR-M6 | Diagnostics as first-class entities: uptime, reset reason, free heap, RSSI, config generation + hash, clock-valid flag, active alarms, per-zone lifetime runtime and volume. |
| NFR-M7 | **A bench rig is not optional.** Changes are validated on LEDs before touching the garden. |
| NFR-M8 | SemVer firmware, version visible in HA, changelog in repo. **Minimum ESPHome version: 2026.9** — it carries the ListEntities loop-stall fix (#18577) and makes an API OOM drop the connection instead of rebooting (#18802/3), both of which matter at this project's ~130 entities (R24). Pin known-good ESPHome/HA versions — **two breaking changes landed in ESPHome 2026.1.0 alone [V]**. Budget a maintenance afternoon twice a year. |

## Environmental, power, longevity

| ID | Requirement |
|---|---|
| NFR-E1 | **Revised by D15 — the controller is indoors**, in the dry technical room with the pool pump. A **DIN-rail wall cabinet (IP30–IP54)** is sufficient; IP66, UV stabilisation and IK rating are **no longer required** for the controller. *They still apply to anything mounted outdoors — valve boxes, the rain gauge, junctions.* |
| NFR-E2 | **Do not seal the enclosure absolutely.** Use a breathable membrane vent, downward-facing glands, and a desiccant pack. **Condensation, not rain, is what kills garden electronics.** |
| NFR-E3 | **Controller (indoor, D15): 0 °C to +40 °C** is realistic for a technical room. **Field hardware (valves, gauge, buried cable) remains −15 °C to +55 °C.** Note the room shares its thermal environment with pool plant, so verify it does not run hot in August. |
| NFR-E4 | IP68 gel/resin splices for every buried joint. No bare crimps, no tape, no wire nuts. |
| NFR-E5 | Screened vents, sealed entries — ants and slugs colonise valve boxes; rodents chew low-voltage cable. |
| NFR-P1 | Mains powered. **`deep_sleep` explicitly excluded** — a sleeping device shows unavailable in HA and would poison the availability watchdog. **[V]** |
| NFR-P2 | Transformer sized on **50 Hz inrush**. ⚠️ **Corrected 2026-09-05:** an earlier draft claimed 475 mA / 11.4 VA inrush *"~28 % higher than the 60 Hz figures"*. Hunter's own spec sheet reads *"350 mA inrush, 190 mA holding, 60 Hz — **370 mA inrush, 210 mA holding, 50 Hz**"* **[V]** — i.e. **370 mA already *is* the 50 Hz figure, and the 28 % uplift was applied twice.** Correct values: **8.88 VA inrush / 5.04 VA holding per valve**; worst case (master + one zone both inrushing) **≈17.8 VA**, holding ≈10.1 VA. **A 30 VA transformer is comfortable; 24 VA is defensible.** The "30–50 VA" conclusion survives — it was right for the wrong reason — but do not be talked up to 60 VA on the strength of the 475 mA figure. Rain Bird 100-DV is lower still (0.30 A / 0.19 A) **[V]**. |
| NFR-P3 | ESP32 brownout detector enabled; a sagging supply must reset cleanly with valves closed, never behave erratically. |
| NFR-L1 | **Design life 10 years for the controller**; valves and flow sensors are **consumables**. |
| NFR-L2 | **Spares on site from day one:** 2 zone valves (same model), 1 spare coil, 1 flow sensor, 1 pre-flashed ESP32, spare connectors. Trivial cost; the alternative is a dead garden waiting for an August delivery. ⚠️ **When ordering the spare coil, check the part number.** **Hunter 458200 is the 9 V DC *latching* solenoid** for battery controllers **[V]** — ordering it would deliver exactly the technology NFR-S2 and R3 prohibit. The 24 VAC coil is **€14.14 [V]**. |
| NFR-L3 | A replacement device is commissionable in <30 min from a git checkout, with the lifetime total restorable. |
| NFR-L4 | **Avoid unobtainable parts.** Anything bought once from a single overseas seller is a 10-year liability. |
| NFR-L5 | **Spare conductors** in the buried cable (12 cores for 8 zones + master) and a **draw string** in the conduit. ⚠️ **Conditional on D16.** If the manifold moves into the technical room there is **no buried valve cable at all** — only pipe — and the advice inverts: the money that would have gone into cable, conduit, boxes and splices goes into the manifold, the pipe and the safety layer instead. Keep the draw string regardless. |

## Installability & documentation

| ID | Requirement |
|---|---|
| NFR-I1 | **Measure before you buy.** Per-zone flow (bucket + stopwatch, 60 s) and static/dynamic pressure first — the sensor choice, the hydraulic budget and the window arithmetic all depend on them. |
| NFR-I2 | Engraved/laminated zone labels at the manifold and in the enclosure, **matching HA entity names exactly**. Mismatched zone numbering is the #1 install-day error and produces baffling symptoms. |
| NFR-I3 | Documented commissioning routine, including **actually testing the safety features**: verify the master closes, verify the over-run guard fires, verify the availability watchdog fires by unplugging the device. Test them at install or you will never know if they work. |
| NFR-D1 | **A one-page laminated card inside the enclosure lid**: how to stop everything, how to run a zone, what the LEDs mean, where the bypass is, who to call. Highest documentation ROI in the project — it is for your household, a house-sitter, or you in five years. |
| NFR-D2 | An **alarm runbook**: per code, what it means, what to check physically, how to clear it. Without it, a latched alarm at 22:00 becomes "acknowledge everything". |
| NFR-D3 | An **ADR log** for every decision in [§14](13-decisions.md) — they will all be re-litigated. |
| NFR-D4 | An **as-built diagram**: trench routes, valve boxes, conductor colour → zone map, meter position. **Photograph the open trenches before backfilling.** In five years this is the most valuable document in the project. |

## Legal / regulatory (France)

| ID | Requirement |
|---|---|
| NFR-LEG1 | **No interconnection** between any private resource (well, rainwater tank) and the potable network — absolute. Valves and check valves are explicitly not accepted as separation. A mains top-up to a tank requires a **total air gap (AA/AB)**; a BA is *not* sufficient for fluid category 5. **[V]** |
| NFR-LEG2 | A **disconnecteur BA at the irrigation piquage is probably NOT legally mandatory here** — the arrêté du 10/09/2021 art. 6 exempts *maisons individuelles*. It remains strong professional practice, **the commune's *règlement de service* may impose one**, and **fertigation would make it non-optional**. **[V]** |
| NFR-LEG3 | Rainwater for garden use needs **no declaration** (arrêté du 12/07/2024 — note the 2008 arrêté was **abrogated in 2024**), but total separation, "eau non potable" labelling at every outlet, lockable/opaque tank with ≤1 mm mesh, and **a means of evaluating the volume used** are mandatory. *That last one makes metering a compliance feature.* **[V]** |
| NFR-LEG4 | The system **shall be able to enforce a local *arrêté sécheresse***: four levels, **hour-window based, set locally — do not hard-code hours. There is no odd/even day scheme in France.** Source is **VigiEau** (which replaced Propluvia). **[V]** |
| NFR-LEG5 | Buried cable: **50 cm** normal ground, **85 cm** under vehicle-accessible surface, in **gaine TPC rouge**, 20 cm clear of non-electrical pipework. **[V-ish — trade consensus; norm text paywalled]** |
| NFR-LEG6 | Investigate the **redevance assainissement exemption** for garden water (CGCT R. 2224-19-2) *before* finalising the metering layout — a codified right, conditional on a *branchement spécifique*, sous-compteur acceptance at the commune's discretion. **[V]** |

---
