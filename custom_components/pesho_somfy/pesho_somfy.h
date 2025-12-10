#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace pesho_somfy {

class PeshoSomfyComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_select_cover_pin(InternalGPIOPin *pin) { select_cover_pin_ = pin; }
  void set_up_pin(InternalGPIOPin *pin) { up_pin_ = pin; }
  void set_down_pin(InternalGPIOPin *pin) { down_pin_ = pin; }
  void set_my_pin(InternalGPIOPin *pin) { my_pin_ = pin; }
  void set_led3_pin(InternalGPIOPin *pin) { led3_pin_ = pin; }
  void set_led4_pin(InternalGPIOPin *pin) { led4_pin_ = pin; }
  
  void set_button_press_duration(uint32_t duration_ms) { button_press_duration_ms_ = duration_ms; }

  // Binary sensor setters
  void set_led3_binary_sensor(binary_sensor::BinarySensor *sensor) { led3_binary_sensor_ = sensor; }
  void set_led4_binary_sensor(binary_sensor::BinarySensor *sensor) { led4_binary_sensor_ = sensor; }

  void press_select_cover();
  void press_up();
  void press_down();
  void press_my();

  bool get_led3_state() const;
  bool get_led4_state() const;
  
  // Access binary sensor states
  bool get_led3_binary_sensor_state() const;
  bool get_led4_binary_sensor_state() const;

 protected:
  void press_button(InternalGPIOPin *pin, const char *button_name);

  InternalGPIOPin *select_cover_pin_;
  InternalGPIOPin *up_pin_;
  InternalGPIOPin *down_pin_;
  InternalGPIOPin *my_pin_;
  
  InternalGPIOPin *led3_pin_{nullptr};
  InternalGPIOPin *led4_pin_{nullptr};
  
  binary_sensor::BinarySensor *led3_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *led4_binary_sensor_{nullptr};
  
  uint32_t button_press_duration_ms_{500};
};

}  // namespace pesho_somfy
}  // namespace esphome
