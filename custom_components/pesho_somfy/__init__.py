import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID
from esphome.components import binary_sensor

CODEOWNERS = ["@pesho"]
DEPENDENCIES = []
AUTO_LOAD = []

pesho_somfy_ns = cg.esphome_ns.namespace("pesho_somfy")
PeshoSomfyComponent = pesho_somfy_ns.class_("PeshoSomfyComponent", cg.Component)

CONF_PESHO_SOMFY = "pesho_somfy"
CONF_SELECT_COVER_PIN = "select_cover_pin"
CONF_UP_PIN = "up_pin"
CONF_DOWN_PIN = "down_pin"
CONF_MY_PIN = "my_pin"
CONF_LED3_PIN = "led3_pin"
CONF_LED4_PIN = "led4_pin"
CONF_LED3_BINARY_SENSOR = "led3_binary_sensor"
CONF_LED4_BINARY_SENSOR = "led4_binary_sensor"
CONF_BUTTON_PRESS_DURATION = "button_press_duration"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PeshoSomfyComponent),
        cv.Required(CONF_SELECT_COVER_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_UP_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DOWN_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MY_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_LED3_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_LED4_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_LED3_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_LED4_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_BUTTON_PRESS_DURATION, default="500ms"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set button pins
    select_cover_pin = await cg.gpio_pin_expression(config[CONF_SELECT_COVER_PIN])
    cg.add(var.set_select_cover_pin(select_cover_pin))
    
    up_pin = await cg.gpio_pin_expression(config[CONF_UP_PIN])
    cg.add(var.set_up_pin(up_pin))
    
    down_pin = await cg.gpio_pin_expression(config[CONF_DOWN_PIN])
    cg.add(var.set_down_pin(down_pin))
    
    my_pin = await cg.gpio_pin_expression(config[CONF_MY_PIN])
    cg.add(var.set_my_pin(my_pin))

    # Set LED pins (optional)
    if CONF_LED3_PIN in config:
        led3_pin = await cg.gpio_pin_expression(config[CONF_LED3_PIN])
        cg.add(var.set_led3_pin(led3_pin))
    
    if CONF_LED4_PIN in config:
        led4_pin = await cg.gpio_pin_expression(config[CONF_LED4_PIN])
        cg.add(var.set_led4_pin(led4_pin))

    # Set binary sensors (optional)
    if CONF_LED3_BINARY_SENSOR in config:
        led3_sensor = await cg.get_variable(config[CONF_LED3_BINARY_SENSOR])
        cg.add(var.set_led3_binary_sensor(led3_sensor))
    
    if CONF_LED4_BINARY_SENSOR in config:
        led4_sensor = await cg.get_variable(config[CONF_LED4_BINARY_SENSOR])
        cg.add(var.set_led4_binary_sensor(led4_sensor))

    # Set button press duration
    cg.add(var.set_button_press_duration(config[CONF_BUTTON_PRESS_DURATION]))
