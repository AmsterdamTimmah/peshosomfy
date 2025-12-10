#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
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
  void set_ready_binary_sensor(binary_sensor::BinarySensor *sensor) { ready_binary_sensor_ = sensor; }

  void press_select_cover();
  void press_up();
  void press_down();
  void press_my();

  bool get_led3_state() const;
  bool get_led4_state() const;
  
  // Access binary sensor states
  bool get_led3_binary_sensor_state() const;
  bool get_led4_binary_sensor_state() const;
  
  // Cover selection tracking
  uint8_t get_current_cover_index() const { return current_cover_index_; }
  void calibrate_cover_index();
  void sync_cover_index_from_leds();  // Sync cover index based on LED states
  void select_cover(uint8_t target_cover_index);  // Select specific cover (0-4)
  
  // Cover control (selects cover then presses button)
  void cover_open(uint8_t cover_index);   // Select cover then press UP
  void cover_close(uint8_t cover_index);  // Select cover then press DOWN
  void cover_stop(uint8_t cover_index);   // Select cover then press MY
  
  // Operation state
  bool is_ready() const;  // Returns true if ready to accept new operations
  const char* get_busy_reason() const;  // Returns reason if busy, "Ready" if not

 protected:
  void press_button(InternalGPIOPin *pin, const char *button_name, bool skip_ready_check = false);
  void release_button_if_done();  // Non-blocking button release helper
  void handle_select_cover_state_machine();  // Handle select cover state machine

  InternalGPIOPin *select_cover_pin_;
  InternalGPIOPin *up_pin_;
  InternalGPIOPin *down_pin_;
  InternalGPIOPin *my_pin_;
  
  InternalGPIOPin *led3_pin_{nullptr};
  InternalGPIOPin *led4_pin_{nullptr};
  
  binary_sensor::BinarySensor *led3_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *led4_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *ready_binary_sensor_{nullptr};
  
  uint32_t button_press_duration_ms_{500};
  uint8_t current_cover_index_{3};  // Tracks currently selected cover (0-4), default 3
  
  // Development/debugging
  uint32_t last_cover_log_time_{0};  // Timestamp for periodic cover index logging
  bool last_ready_state_{true};  // Track previous ready state for change detection
  
  // Non-blocking button press state machine
  InternalGPIOPin *active_button_pin_{nullptr};  // Currently pressed button pin
  const char *active_button_name_{nullptr};       // Name of currently pressed button
  uint32_t button_press_start_time_{0};           // When button press started
  bool pending_cover_index_increment_{false};      // True if cover index should increment after release
  
  // LED sync tracking
  uint32_t last_led_sync_time_{0};                  // Timestamp of last LED sync
  uint32_t last_select_cover_complete_time_{0};     // Timestamp when select_cover last completed
  static constexpr uint32_t LED_SYNC_INTERVAL_MS = 2000;  // Sync every 2 seconds
  static constexpr uint32_t LED_SYNC_DELAY_AFTER_SELECT_MS = 2000;  // Wait 2s after select before syncing
  
  // Cover configuration
  static constexpr uint8_t NUM_COVERS = 5;  // Number of covers (0-4)
  
  // Development/debugging
  static constexpr uint32_t DEBUG_LOG_INTERVAL_MS = 5000;  // Debug log interval (5 seconds)
  
  // LED stability constants (for reset phase)
  static constexpr uint32_t LED_RESPONSE_TIME_MS = 100;  // LED physical response time
  static constexpr uint32_t FILTER_DELAY_MS = 100;       // delayed_on filter delay
  static constexpr uint32_t STABILITY_MARGIN_MS = 100;   // Safety margin
  static constexpr uint32_t LED_STABLE_DELAY_MS = LED_RESPONSE_TIME_MS + FILTER_DELAY_MS + STABILITY_MARGIN_MS;  // Total: 300ms
  static constexpr uint32_t MAX_RESET_PRESSES = 10;  // Maximum number of presses before giving up
  
  // Select cover state machine
  enum SelectCoverState {
    SELECT_COVER_IDLE,
    SELECT_COVER_RESETTING_TO_COVER3,      // Pressing until LED3 lights up
    SELECT_COVER_WAITING_FOR_LED3_STABLE,  // Waiting for LED3 to stabilize after press
    SELECT_COVER_CHECKING_LED3,             // Checking if LED3 is on
    SELECT_COVER_WAITING_FOR_BUTTON_RELEASE, // Waiting for button release
    SELECT_COVER_WAITING_FOR_NEXT_PRESS     // Waiting before next press in selection phase
  };
  SelectCoverState select_cover_state_{SELECT_COVER_IDLE};
  uint32_t select_cover_wait_start_time_{0};
  uint8_t select_cover_target_{0};
  uint8_t select_cover_presses_remaining_{0};
  uint8_t select_cover_press_count_{0};
  uint8_t select_cover_reset_press_count_{0};  // Press count during reset phase
  static constexpr uint32_t SELECT_COVER_PRESS_MARGIN_MS = 50;  // Small margin after button_press_duration
  
  // Pending action after select_cover completes
  enum PendingAction {
    PENDING_ACTION_NONE,
    PENDING_ACTION_PRESS_UP,
    PENDING_ACTION_PRESS_DOWN,
    PENDING_ACTION_PRESS_MY
  };
  PendingAction pending_action_{PENDING_ACTION_NONE};
};

}  // namespace pesho_somfy
}  // namespace esphome
