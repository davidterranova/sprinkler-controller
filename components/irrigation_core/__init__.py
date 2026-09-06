"""irrigation_core -- the pure, host-testable core.

This component declares no configuration and generates no code. It exists so
that ESPHome copies `*.h` / `*.cpp` from this directory into the firmware
build: ESPHome only gathers source files that belong to a component package, so
a plain `lib/` directory (as docs/17-repository-layout.md sketched) would never
reach the compiler.

Making it a component instead means the host unit tests and the device build
compile *the same files*, with no symlink, no copy step, and no chance of the
tested logic drifting from the shipped logic.

Nothing here may include an ESPHome header. `make purity-gate` enforces it.
"""

import esphome.codegen as cg  # noqa: F401  (imported for symmetry with other components)
import esphome.config_validation as cv

CODEOWNERS = ["@davidterranova"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # Pure C++; nothing to instantiate.
    pass
