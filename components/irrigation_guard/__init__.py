"""L1 -- the independent safety supervisor.

Deliberately knows nothing about schedules, programs or Home Assistant. It
holds pin numbers, a period and a deadline, because everything it does must
keep working when the layers above it have stopped.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import CONF_ID

AUTO_LOAD = ["irrigation_core"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@davidterranova"]

CONF_HEARTBEAT_PIN = "heartbeat_pin"
CONF_HEARTBEAT_PERIOD = "heartbeat_period"
CONF_LIVENESS_TIMEOUT = "liveness_timeout"
CONF_MASTER_PIN = "master_pin"
CONF_ZONE_PINS = "zone_pins"
CONF_ACTIVE_LOW = "active_low"
CONF_MAX_RUN_DURATION = "max_run_duration"
CONF_ON_TRIP = "on_trip"

irrigation_guard_ns = cg.esphome_ns.namespace("irrigation_guard")
IrrigationGuard = irrigation_guard_ns.class_("IrrigationGuard", cg.Component)
ArmAction = irrigation_guard_ns.class_("ArmAction", automation.Action)
DisarmAction = irrigation_guard_ns.class_("DisarmAction", automation.Action)
HardCloseAction = irrigation_guard_ns.class_("HardCloseAction", automation.Action)

# docs/07-firmware.md: the stock sprinkler component's own limits allow a
# 240-hour run to be expressed (multiplier 10 x run_duration 86400 s). This is
# the ceiling that is enforced independently of it, and it is also asserted at
# compile time in irrigation_core/guard.h -- clamping in one place only is how
# that 240-hour run reaches a valve.
MAX_RUN_SECONDS = 130 * 60


def _reject_strapping_pins(value):
    # GPIO12 (MTDI) is the one that must be respected absolutely: every relay
    # IN line carries a 1 kOhm pull-up, and holding MTDI high at reset selects a
    # 1.8 V VDD_SDIO, browns out the 3.3 V flash, and the board will not boot or
    # flash at all [V]. The others are boot-mode or flash pins.
    forbidden = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15}
    if value in forbidden:
        raise cv.Invalid(
            f"GPIO{value} is a strapping, boot-mode or SPI-flash pin and must never "
            f"drive a valve or the deadman. See the pin map in docs/05-hardware.md."
        )
    return value


def _output_pin(value):
    return _reject_strapping_pins(pins.internal_gpio_output_pin_number(value))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IrrigationGuard),
        cv.Required(CONF_HEARTBEAT_PIN): _output_pin,
        # 20 Hz. Fast enough that the retriggerable monostable (~1 s period)
        # never comes close to timing out in normal operation, slow enough to
        # cost nothing. The rate is NOT what makes the circuit work -- the
        # monostable removes the frequency sensitivity that sank the original
        # passive charge-pump design.
        cv.Optional(CONF_HEARTBEAT_PERIOD, default="25ms"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(min=cv.TimePeriod(microseconds=1000), max=cv.TimePeriod(milliseconds=200)),
        ),
        cv.Optional(CONF_LIVENESS_TIMEOUT, default="400ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=100), max=cv.TimePeriod(milliseconds=2000)),
        ),
        cv.Required(CONF_MASTER_PIN): _output_pin,
        cv.Required(CONF_ZONE_PINS): cv.All(
            cv.ensure_list(_output_pin), cv.Length(min=1, max=8)
        ),
        # An active-low opto board plus "all pins are output-disabled during
        # reset" (ESP32 datasheet v5.3, A.1 note 9) means booting, absent, in
        # reset or with the wire fallen off all resolve to relay OFF with no
        # external components. Bench test T3 is what confirms your board really
        # is active-low before this default is trusted.
        cv.Optional(CONF_ACTIVE_LOW, default=True): cv.boolean,
        cv.Optional(CONF_MAX_RUN_DURATION, default="60min"): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(
                min=cv.TimePeriod(seconds=10),
                max=cv.TimePeriod(seconds=MAX_RUN_SECONDS),
            ),
        ),
        cv.Optional(CONF_ON_TRIP): automation.validate_automation(),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_distinct_pins(config):
    used = [config[CONF_HEARTBEAT_PIN], config[CONF_MASTER_PIN], *config[CONF_ZONE_PINS]]
    if len(set(used)) != len(used):
        raise cv.Invalid(
            "the heartbeat, master and zone pins must all be different -- a shared pin "
            "means the deadman and a valve drive each other"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_distinct_pins


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_heartbeat_pin(config[CONF_HEARTBEAT_PIN]))
    cg.add(var.set_heartbeat_period_us(int(config[CONF_HEARTBEAT_PERIOD].total_microseconds)))
    cg.add(var.set_liveness_timeout_ms(int(config[CONF_LIVENESS_TIMEOUT].total_milliseconds)))
    cg.add(var.set_master_pin(config[CONF_MASTER_PIN]))
    for pin in config[CONF_ZONE_PINS]:
        cg.add(var.add_zone_pin(pin))
    cg.add(var.set_active_low(config[CONF_ACTIVE_LOW]))
    cg.add(var.set_max_run_seconds(int(config[CONF_MAX_RUN_DURATION].total_seconds)))

    # The pin table the safe-mode sweep uses. Generated from this same config,
    # so `on_safe_mode` can never close a different set of pins from the ones
    # the guard drives.
    panic_pins = [config[CONF_MASTER_PIN], *config[CONF_ZONE_PINS]]
    cg.add_global(
        cg.RawStatement(
            "namespace esphome { namespace irrigation_guard {\n"
            f"const uint8_t PANIC_PINS[] = {{{', '.join(str(p) for p in panic_pins)}}};\n"
            f"const uint8_t PANIC_PIN_COUNT = {len(panic_pins)};\n"
            f"const bool PANIC_ACTIVE_LOW = {'true' if config[CONF_ACTIVE_LOW] else 'false'};\n"
            "} }"
        )
    )

    for conf in config.get(CONF_ON_TRIP, []):
        await automation.build_automation(var.get_trip_trigger(), [(cg.uint8, "zone")], conf)


GUARD_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(IrrigationGuard)})


@automation.register_action(
    "irrigation_guard.arm",
    ArmAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(IrrigationGuard),
            cv.Required("zone"): cv.templatable(cv.int_range(min=0, max=7)),
            cv.Required("duration"): cv.templatable(cv.positive_time_period_seconds),
        }
    ),
)
async def arm_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_zone(await cg.templatable(config["zone"], args, cg.uint8)))
    duration = config["duration"]
    seconds = duration if isinstance(duration, cv.Lambda) else int(duration.total_seconds)
    cg.add(var.set_duration(await cg.templatable(seconds, args, cg.uint32)))
    return var


@automation.register_action("irrigation_guard.disarm", DisarmAction, GUARD_ACTION_SCHEMA)
async def disarm_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("irrigation_guard.hard_close", HardCloseAction, GUARD_ACTION_SCHEMA)
async def hard_close_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
