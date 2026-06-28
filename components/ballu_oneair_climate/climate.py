import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor
from esphome.const import CONF_SENSOR

AUTO_LOAD = ["climate", "sensor"]

ballu_ns = cg.esphome_ns.namespace("ballu_oneair_climate")
BalluOneAirClimate = ballu_ns.class_("BalluOneAirClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate.climate_schema(BalluOneAirClimate).extend(
    {
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))
