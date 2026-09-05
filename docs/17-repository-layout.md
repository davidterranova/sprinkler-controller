[← Documentation index](../README.md)

# 18. Repository layout

> **Bootstrapped 2026-09-05.** The files marked ✅ below now exist; the rest is the target shape.
> The build toolchain mirrors the pool controller: **ESPHome in Docker**, flashed on the host with
> the ESP-IDF esptool, driven by a `Makefile`.

```
esp-sprinkler/
├── README.md                     ✅ documentation index
├── Makefile                      ✅ validate / build / flash / ota / logs / logs-usb / secrets
├── sprinkler.yaml                ✅ bootstrap config -- deliberately drives no valve
├── secrets.yaml.template         ✅  (secrets.yaml is gitignored)
├── .github/workflows/validate.yml ✅ CI: placeholder secrets + `make validate`
├── docs/
│   ├── adr/                      # one file per decision in §14 (NFR-D3)
│   ├── alarm-runbook.md          # NFR-D2
│   ├── as-built.md               # NFR-D4 — trenches, zone map, conductor colours
│   └── enclosure-card.md         # NFR-D1 — the laminated one-pager
├── lib/irrigation_core/          # pure C++17, ZERO ESPHome includes, host-tested
│   ├── clock.h  store.h
│   └── schedule.{h,cpp}  resolver.{h,cpp}  fsm.{h,cpp}  meter.{h,cpp}  alarms.{h,cpp}
├── components/                   # ESPHome external components
│   ├── irrigation_scheduler/
│   ├── irrigation_guard/
│   └── water_meter/
├── test/
│   ├── fakes/{fake_clock.h,in_memory_store.h,hydraulic_model.h,fault_injector.h}
│   └── {resolver,fsm,meter,alarms,persistence,season}_test.cpp
├── packages/                     # shared YAML: base, safety, sprinkler, metering, ui
├── garden.yaml                   # production (esp32, esp-idf)
├── garden-bench.yaml             # 8 LEDs, simulation mode
├── garden-host.yaml              # platform: host, for CI
├── homeassistant/                # helper + automation + dashboard definitions, as code
├── hardware/                     # schematic, BOM, wiring diagram
├── secrets.example.yaml          # secrets.yaml is .gitignore'd from commit #1 (NFR-SEC5)
└── .github/workflows/ci.yaml     # host tests, esphome config validation, compile,
                                  # purity gate, flash-write grep gate
```

---
