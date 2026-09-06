"""L3 -- the scheduler.

The configuration surface here is deliberately thin: pin the collaborators,
say how many zones there are, and set the two policy numbers that are not
safe to expose as runtime entities. Everything an operator edits day to day
(programs, durations, weekday masks) is staged through the C++ API from
template entities in `packages/ui.yaml`, so the entity model can change
without touching Python.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sprinkler, time as time_
from esphome.const import CONF_ID, CONF_TIME_ID

AUTO_LOAD = ["irrigation_core", "irrigation_guard"]
DEPENDENCIES = ["sprinkler", "time"]
CODEOWNERS = ["@davidterranova"]

CONF_SPRINKLER_ID = "sprinkler_id"
CONF_GUARD_ID = "guard_id"
CONF_ZONE_COUNT = "zone_count"
CONF_CATCH_UP_WINDOW = "catch_up_window"
CONF_DAILY_ZONE_BUDGET = "daily_zone_budget"

irrigation_scheduler_ns = cg.esphome_ns.namespace("irrigation_scheduler")
IrrigationScheduler = irrigation_scheduler_ns.class_("IrrigationScheduler", cg.Component)

irrigation_guard_ns = cg.esphome_ns.namespace("irrigation_guard")
IrrigationGuard = irrigation_guard_ns.class_("IrrigationGuard", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IrrigationScheduler),
        cv.Required(CONF_SPRINKLER_ID): cv.use_id(sprinkler.Sprinkler),
        cv.Required(CONF_GUARD_ID): cv.use_id(IrrigationGuard),
        cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        cv.Required(CONF_ZONE_COUNT): cv.int_range(min=1, max=8),
        # How long after a missed start a program may still begin. Bounded on
        # purpose: a controller that was off all day must NOT start the 05:00
        # program at 22:00. 0 means exact starts only.
        cv.Optional(CONF_CATCH_UP_WINDOW, default="60min"): cv.All(
            cv.positive_time_period_minutes,
            cv.Range(max=cv.TimePeriod(minutes=720)),
        ),
        # FR-3.7a. The per-day per-zone ceiling is what makes a resume bug
        # survivable: it is the budget, not the run counter, that is the real
        # stop condition. 0 disables it, which is not recommended.
        cv.Optional(CONF_DAILY_ZONE_BUDGET, default="3h"): cv.positive_time_period_seconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sprinkler(await cg.get_variable(config[CONF_SPRINKLER_ID])))
    cg.add(var.set_guard(await cg.get_variable(config[CONF_GUARD_ID])))
    cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    cg.add(var.set_zone_count(config[CONF_ZONE_COUNT]))
    cg.add(var.set_catch_up_minutes(int(config[CONF_CATCH_UP_WINDOW].total_minutes)))
    cg.add(var.set_daily_zone_budget(int(config[CONF_DAILY_ZONE_BUDGET].total_seconds)))
