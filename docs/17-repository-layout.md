[← Documentation index](../README.md)

# 18. Repository layout

> **v0 built 2026-09-05.** The tree below is what exists now. The build toolchain mirrors the pool
> controller: **ESPHome in Docker**, flashed on the host with the ESP-IDF esptool, driven by a
> `Makefile`. Host unit tests need neither.

```
esp-sprinkler/
├── README.md                      documentation index
├── Makefile                       test / gates / validate / build / flash / ota / logs / secrets
├── sprinkler.yaml                 the v0 bench node -- 8 zones wired, LEDs, no water
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
│                                  + zone-ui, included once per zone with vars
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

**Direct GPIO for all eight zones.** The guard can only hard-close pins it writes itself, so every pin
that can reach a relay coil is one it owns — which is what makes its raw sweep worth having in the
case it exists for, where the main loop is hung. None of the eight wired pins is a strapping pin, so
the GPIO12 hazard does not arise in the firmware. See the as-wired note in
[§7](05-hardware.md#pin-map).

---
