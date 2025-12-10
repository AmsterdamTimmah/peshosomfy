#include "pesho_somfy.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pesho_somfy {

static const char *const TAG = "pesho_somfy";

void PeshoSomfyComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Pesho Somfy Remote Control...");

  // Configure all button pins as INPUT initially (floating, high impedance)
  // This prevents current from flowing back into the circuit
  this->select_cover_pin_->pin_mode(gpio::FLAG_INPUT);
  this->up_pin_->pin_mode(gpio::FLAG_INPUT);
  this->down_pin_->pin_mode(gpio::FLAG_INPUT);
  this->my_pin_->pin_mode(gpio::FLAG_INPUT);
  ESP_LOGCONFIG(TAG, "  Select Cover Pin: GPIO%u", this->select_cover_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Up Pin: GPIO%u", this->up_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Down Pin: GPIO%u", this->down_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  My Pin: GPIO%u", this->my_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Button Press Duration: %u ms", this->button_press_duration_ms_);

  // Configure LED pins as INPUT if they are set
  if (this->led3_pin_ != nullptr) {
    this->led3_pin_->pin_mode(gpio::FLAG_INPUT);
    ESP_LOGCONFIG(TAG, "  LED3 Pin: GPIO%u", this->led3_pin_->get_pin());
  }
  if (this->led4_pin_ != nullptr) {
    this->led4_pin_->pin_mode(gpio::FLAG_INPUT);
    ESP_LOGCONFIG(TAG, "  LED4 Pin: GPIO%u", this->led4_pin_->get_pin());
  }

  ESP_LOGI(TAG, "Pesho Somfy Remote Control initialized!");
  ESP_LOGI(TAG, "All pins configured as INPUT (floating when off)");
}

void PeshoSomfyComponent::loop() {
  // Component loop - no periodic tasks needed
  // LED states are read on-demand via getter methods
}

void PeshoSomfyComponent::press_button(InternalGPIOPin *pin, const char *button_name) {
  if (pin == nullptr) {
    ESP_LOGW(TAG, "Attempted to press %s but pin is not configured", button_name);
    return;
  }

  ESP_LOGD(TAG, "Pressing %s button", button_name);

  // Safe button press sequence:
  // 1. Set LOW first (sets internal latch) to avoid any HIGH pulse
  // 2. Configure as OUTPUT (pin now sinks current to GND)
  // 3. Wait for press duration
  // 4. Configure back to INPUT (floating, high impedance)

  pin->digital_write(false);  // Set LOW first
  pin->pin_mode(gpio::FLAG_OUTPUT);  // Then configure as OUTPUT

  // Wait for button press duration
  delay(this->button_press_duration_ms_);

  // Release button: Set back to INPUT (floating, high impedance)
  pin->pin_mode(gpio::FLAG_INPUT);

  ESP_LOGD(TAG, "%s button released", button_name);
}

void PeshoSomfyComponent::press_select_cover() {
  this->press_button(this->select_cover_pin_, "Select Cover");
}

void PeshoSomfyComponent::press_up() {
  this->press_button(this->up_pin_, "Up");
}

void PeshoSomfyComponent::press_down() {
  this->press_button(this->down_pin_, "Down");
}

void PeshoSomfyComponent::press_my() {
  this->press_button(this->my_pin_, "My");
}

bool PeshoSomfyComponent::get_led3_state() const {
  if (this->led3_pin_ != nullptr) {
    // Inverted: HIGH reads as OFF (false), LOW reads as ON (true)
    return !this->led3_pin_->digital_read();
  }
  return false;
}

bool PeshoSomfyComponent::get_led4_state() const {
  if (this->led4_pin_ != nullptr) {
    // Inverted: HIGH reads as OFF (false), LOW reads as ON (true)
    return !this->led4_pin_->digital_read();
  }
  return false;
}

bool PeshoSomfyComponent::get_led3_binary_sensor_state() const {
  if (this->led3_binary_sensor_ != nullptr) {
    return this->led3_binary_sensor_->state;
  }
  return false;
}

bool PeshoSomfyComponent::get_led4_binary_sensor_state() const {
  if (this->led4_binary_sensor_ != nullptr) {
    return this->led4_binary_sensor_->state;
  }
  return false;
}

}  // namespace pesho_somfy
}  // namespace esphome
