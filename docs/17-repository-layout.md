[← Documentation index](../README.md)

# 18. Repository layout

> **v0 built 2026-09-05.** The tree below is what exists now. The build toolchain mirrors the pool
> controller: **ESPHome in Docker**, flashed on the host with the ESP-IDF esptool, driven by a
> `Makefile`. Host unit tests need neither.

```
esp-sprinkler/
├── README.md                      documentation index
├── Makefile                       test / gates / validate / build / flash / ota / logs / secrets
├── sprinkler.yaml                 the v0 bench node -- 4 zones, LEDs, no water
├── secrets.yaml.template           (secrets.yaml is gitignored)
├── .github/workflows/ci.yml       host tests -> source gates -> validate + compile
├── docs/
│   ├── 21-bench-procedure.md      the v0 exit criteria as a checklist
│   ├── adr/                       one file per decision in §14 (NFR-D3)      [empty]
│   ├── alarm-runbook.md           NFR-D2                                     [v1]
│   ├── as-built.md                NFR-D4 -- trenches, zone map, colours      [v1]
│   └── enclosure-card.md          NFR-D1 -- the laminated one-pager          [v1]
├── components/
│   ├── irrigation_core/           pure C++17, ZERO ESPHome includes, host-tested
│   │   ├── types.h  tz.{h,cpp}  crc.h  guard.h
│   │   └── schedule.{h,cpp}  resolver.{h,cpp}  fsm.{h,cpp}
│   ├── irrigation_guard/          L1 -- deadman heartbeat + absolute deadline + raw GPIO close
│   ├── irrigation_scheduler/      L3 -- persisted schedule, due evaluation, the three verbs
│   └── water_meter/               L4 -- pulse capture, per-zone attribution [v1]
├── test/
│   ├── test_util.h                a ~60-line harness, so CI needs no dependency
│   └── {resolver,fsm,guard,season}_test.cpp
├── packages/                      base · safety · zones · scheduler · ui · rtc-ds3231
├── homeassistant/                 helper + automation + dashboard definitions [v1]
└── hardware/                      schematic, BOM, wiring diagram              [v1]
```

## Two deviations from the original sketch, both deliberate

**`components/irrigation_core/` rather than `lib/irrigation_core/`.** ESPHome only gathers source
files that belong to a *component package*, so a sibling `lib/` directory would never reach the
firmware build — the code would have to be duplicated or symlinked, and the tested logic would
eventually drift from the shipped logic. Making the core a config-less ESPHome component keeps **one
copy** compiled by both the host tests and the device build. Its `__init__.py` declares an empty
schema and generates nothing.

**Direct GPIO for the zones in v0, not the PCF8574.** The guard can only hard-close pins it writes
itself; behind an I²C expander its raw sweep would cover the master valve and nothing else. On a
four-zone bench that is the wrong trade for the layer whose entire job is proving fail-closed. The
expander returns with the eight-zone build, where the deadman does the safety work anyway — which is
what [§7](05-hardware.md#expander-or-direct-gpio--the-honest-answer) already concluded.

---
