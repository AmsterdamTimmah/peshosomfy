#include "pesho_somfy.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pesho_somfy {

static const char *const TAG = "pesho_somfy";

void PeshoSomfyComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Pesho Somfy Remote Control...");

  // Validate required pins
  if (this->select_cover_pin_ == nullptr || this->up_pin_ == nullptr || 
      this->down_pin_ == nullptr || this->my_pin_ == nullptr) {
    ESP_LOGE(TAG, "Required pins not configured!");
    this->mark_failed();
    return;
  }

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
  ESP_LOGCONFIG(TAG, "  Initial Cover Index: %u", this->current_cover_index_);
  
  // Publish initial ready state to binary sensor if linked
  if (this->ready_binary_sensor_ != nullptr) {
    this->ready_binary_sensor_->publish_state(this->is_ready());
  }
}

void PeshoSomfyComponent::loop() {
  uint32_t now = millis();
  
  // Development: Log active cover number periodically
  if (now - this->last_cover_log_time_ > DEBUG_LOG_INTERVAL_MS) {
    ESP_LOGD(TAG, "Active cover number: %u (Remote Cover %u)", 
             this->current_cover_index_, this->current_cover_index_ + 1);
    this->last_cover_log_time_ = now;
  }
  
  // Sync cover index from LEDs periodically (but not during select, and not immediately after select)
  if (this->select_cover_state_ == SELECT_COVER_IDLE &&
      now - this->last_led_sync_time_ > LED_SYNC_INTERVAL_MS &&
      now - this->last_select_cover_complete_time_ > LED_SYNC_DELAY_AFTER_SELECT_MS) {
    sync_cover_index_from_leds();
    this->last_led_sync_time_ = now;
  }
  
  // Handle select cover state machine
  if (this->select_cover_state_ != SELECT_COVER_IDLE) {
    handle_select_cover_state_machine();
  }
  
  // Check if button press duration has elapsed and release if needed
  release_button_if_done();
  
  // Track and log ready state changes
  bool current_ready_state = this->is_ready();
  if (current_ready_state != this->last_ready_state_) {
    ESP_LOGI(TAG, "Ready state changed: %s -> %s (Reason: %s)", 
             this->last_ready_state_ ? "READY" : "BUSY",
             current_ready_state ? "READY" : "BUSY",
             this->get_busy_reason());
    this->last_ready_state_ = current_ready_state;
    
    // Publish state change to binary sensor if linked
    if (this->ready_binary_sensor_ != nullptr) {
      this->ready_binary_sensor_->publish_state(current_ready_state);
    }
  }
}

void PeshoSomfyComponent::press_button(InternalGPIOPin *pin, const char *button_name, bool skip_ready_check) {
  if (pin == nullptr) {
    ESP_LOGW(TAG, "Attempted to press %s but pin is not configured", button_name);
    return;
  }
  
  // Check if ready to accept new operations (unless called internally)
  if (!skip_ready_check && !this->is_ready()) {
    ESP_LOGW(TAG, "Device busy (%s), ignoring %s button press", this->get_busy_reason(), button_name);
    return;
  }

  ESP_LOGD(TAG, "Pressing %s button", button_name);

  // If this is the select_cover_pin_ and we're in selection phase, increment cover index
  if (pin == this->select_cover_pin_ && 
      (this->select_cover_state_ == SELECT_COVER_WAITING_FOR_BUTTON_RELEASE || 
       this->select_cover_state_ == SELECT_COVER_WAITING_FOR_NEXT_PRESS)) {
    // This is a selection press - will increment cover index after release
    this->pending_cover_index_increment_ = true;
  } else {
    // Manual press or reset phase press - don't increment cover index
    this->pending_cover_index_increment_ = false;
  }

  // Safe button press sequence:
  // 1. Set LOW first (sets internal latch) to avoid any HIGH pulse
  // 2. Configure as OUTPUT (pin now sinks current to GND)
  // 3. Track state for non-blocking release
  // 4. Release will happen in loop() after duration

  pin->digital_write(false);  // Set LOW first
  pin->pin_mode(gpio::FLAG_OUTPUT);  // Then configure as OUTPUT

  // Store state for non-blocking release
  this->active_button_pin_ = pin;
  this->active_button_name_ = button_name;
  this->button_press_start_time_ = millis();
}

void PeshoSomfyComponent::release_button_if_done() {
  if (this->active_button_pin_ == nullptr) {
    return;  // No button being pressed
  }

  uint32_t now = millis();
  uint32_t elapsed = now - this->button_press_start_time_;

  if (elapsed >= this->button_press_duration_ms_) {
    // Release button: Set back to INPUT (floating, high impedance)
    this->active_button_pin_->pin_mode(gpio::FLAG_INPUT);
    
    ESP_LOGD(TAG, "%s button released", this->active_button_name_);
    
    // Handle cover index increment if needed
    if (this->pending_cover_index_increment_) {
      this->current_cover_index_ = (this->current_cover_index_ + 1) % NUM_COVERS;
      ESP_LOGD(TAG, "Cover index incremented to: %u", this->current_cover_index_);
      this->pending_cover_index_increment_ = false;
    }
    
    // Clear active button state
    this->active_button_pin_ = nullptr;
    this->active_button_name_ = nullptr;
  }
}

void PeshoSomfyComponent::press_select_cover() {
  // Manual press: Will be blocked if device is busy (checked in press_button)
  this->press_button(this->select_cover_pin_, "Select Cover", false);
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

void PeshoSomfyComponent::calibrate_cover_index() {
  this->current_cover_index_ = 3;
  ESP_LOGI(TAG, "Cover index calibrated to: %u (Remote Cover %u)", 
           this->current_cover_index_, this->current_cover_index_ + 1);
}

void PeshoSomfyComponent::sync_cover_index_from_leds() {
  // Use filtered binary sensor states to avoid erratic readings
  bool led3_on = get_led3_binary_sensor_state();
  bool led4_on = get_led4_binary_sensor_state();
  
  uint8_t detected_cover = this->current_cover_index_;  // Default to current
  
  // Determine cover based on LED states
  // LED3 ON = Remote Cover 3 = Index 2
  // LED4 ON = Remote Cover 4 = Index 3
  if (led3_on && !led4_on) {
    // LED3 ON, LED4 OFF = Remote Cover 3 = Index 2
    detected_cover = 2;
  } else if (!led3_on && led4_on) {
    // LED3 OFF, LED4 ON = Remote Cover 4 = Index 3
    detected_cover = 3;
  } else if (!led3_on && !led4_on) {
    // Both OFF = Could be Remote Covers 1, 2, or 5 (Indices 0, 1, or 4)
    // We can't determine exactly, so keep current index
    ESP_LOGD(TAG, "LEDs indicate covers 1, 2, or 5 selected (indices 0, 1, or 4), cannot determine exact cover");
    return;  // Don't change if we can't determine
  } else {
    // Both ON = Erratic state (as mentioned in README)
    ESP_LOGW(TAG, "Both LEDs ON - erratic reading, not syncing cover index");
    return;  // Don't sync during erratic state
  }
  
  // Only update if different from current
  if (detected_cover != this->current_cover_index_) {
    ESP_LOGI(TAG, "Syncing cover index from LEDs: %u -> %u (Remote Cover %u)", 
             this->current_cover_index_, detected_cover, detected_cover + 1);
    this->current_cover_index_ = detected_cover;
  }
}


void PeshoSomfyComponent::select_cover(uint8_t target_cover_index) {
  // Validate target cover index
  if (target_cover_index > 4) {
    ESP_LOGW(TAG, "Invalid target cover index: %u (must be 0-4)", target_cover_index);
    return;
  }
  
  // If a select cover operation is already in progress, cancel it first
  if (this->select_cover_state_ != SELECT_COVER_IDLE) {
    ESP_LOGI(TAG, "Cancelling previous select cover operation to start new one");
    this->select_cover_state_ = SELECT_COVER_IDLE;
    // Release any active button press for select_cover
    if (this->active_button_pin_ == this->select_cover_pin_) {
      this->active_button_pin_ = nullptr;
      this->active_button_name_ = nullptr;
    }
  }
  
  // Check if ready to accept new operations (should be ready now after cancellation)
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Device busy (%s), cannot start select cover", this->get_busy_reason());
    return;
  }
  
  // Check if already at target
  if (this->current_cover_index_ == target_cover_index) {
    ESP_LOGI(TAG, "Already at Remote Cover %u (Index %u), no selection needed", 
             target_cover_index + 1, target_cover_index);
    
    // Execute pending action if any (cover_open/cover_close/cover_stop may have set it)
    if (this->pending_action_ == PENDING_ACTION_PRESS_UP) {
      this->pending_action_ = PENDING_ACTION_NONE;
      ESP_LOGI(TAG, "Executing pending action: Press UP for Remote Cover %u", target_cover_index + 1);
      this->press_up();
    } else if (this->pending_action_ == PENDING_ACTION_PRESS_DOWN) {
      this->pending_action_ = PENDING_ACTION_NONE;
      ESP_LOGI(TAG, "Executing pending action: Press DOWN for Remote Cover %u", target_cover_index + 1);
      this->press_down();
    } else if (this->pending_action_ == PENDING_ACTION_PRESS_MY) {
      this->pending_action_ = PENDING_ACTION_NONE;
      ESP_LOGI(TAG, "Executing pending action: Press MY for Remote Cover %u", target_cover_index + 1);
      this->press_my();
    }
    return;
  }
  
  // Validate binary sensors are configured
  if (this->led3_binary_sensor_ == nullptr || this->led4_binary_sensor_ == nullptr) {
    ESP_LOGW(TAG, "Cannot select cover - binary sensors not configured");
    return;
  }
  
  // Check if LED3 is already on (we're at cover 3)
  bool led3_on = get_led3_binary_sensor_state();
  bool led4_on = get_led4_binary_sensor_state();
  
  // Calculate presses needed from cover 3 (index 2) to target
  uint8_t presses_needed = (target_cover_index - 2 + NUM_COVERS) % NUM_COVERS;
  
  this->select_cover_target_ = target_cover_index;
  this->select_cover_presses_remaining_ = presses_needed;
  this->select_cover_press_count_ = 0;
  
  if (led3_on && !led4_on) {
    // Already at cover 3, skip reset phase
    ESP_LOGI(TAG, "Already at Remote Cover 3 (Index 2), skipping reset phase");
    this->current_cover_index_ = 2;
    
    if (presses_needed == 0) {
      // Already at target
      ESP_LOGI(TAG, "Already at target Remote Cover %u (Index %u)", 
               target_cover_index + 1, target_cover_index);
      
      // Execute pending action if any
      if (this->pending_action_ == PENDING_ACTION_PRESS_UP) {
        this->pending_action_ = PENDING_ACTION_NONE;
        ESP_LOGI(TAG, "Executing pending action: Press UP for Remote Cover %u", target_cover_index + 1);
        this->press_up();
      } else if (this->pending_action_ == PENDING_ACTION_PRESS_DOWN) {
        this->pending_action_ = PENDING_ACTION_NONE;
        ESP_LOGI(TAG, "Executing pending action: Press DOWN for Remote Cover %u", target_cover_index + 1);
        this->press_down();
      } else if (this->pending_action_ == PENDING_ACTION_PRESS_MY) {
        this->pending_action_ = PENDING_ACTION_NONE;
        ESP_LOGI(TAG, "Executing pending action: Press MY for Remote Cover %u", target_cover_index + 1);
        this->press_my();
      }
      return;
    }
    
    // Start selection phase directly
    ESP_LOGI(TAG, "Selecting Remote Cover %u (Index %u) from Cover 3 - %u presses needed", 
             target_cover_index + 1, target_cover_index, presses_needed);
    this->select_cover_state_ = SELECT_COVER_WAITING_FOR_BUTTON_RELEASE;
    this->select_cover_wait_start_time_ = millis();
    this->press_button(this->select_cover_pin_, "Select Cover (Select)", true);
  } else {
    // Need to reset to cover 3 first
    ESP_LOGI(TAG, "Resetting to Remote Cover 3 (Index 2), then selecting Remote Cover %u (Index %u) - %u presses needed", 
             target_cover_index + 1, target_cover_index, presses_needed);
    this->select_cover_state_ = SELECT_COVER_RESETTING_TO_COVER3;
    this->select_cover_reset_press_count_ = 1;
    this->select_cover_wait_start_time_ = millis();
    this->press_button(this->select_cover_pin_, "Select Cover (Reset)", true);
  }
}

void PeshoSomfyComponent::cover_open(uint8_t cover_index) {
  if (cover_index > 4) {
    ESP_LOGW(TAG, "Invalid cover index for open: %u (must be 0-4)", cover_index);
    return;
  }
  
  // Check if already at target cover
  if (this->current_cover_index_ == cover_index && this->select_cover_state_ == SELECT_COVER_IDLE) {
    // Already at target, just press up
    ESP_LOGI(TAG, "Already at Remote Cover %u (Index %u), pressing UP", cover_index + 1, cover_index);
    this->press_up();
    return;
  }
  
  // Set pending action and select cover
  ESP_LOGI(TAG, "Opening Remote Cover %u (Index %u) - selecting cover first", cover_index + 1, cover_index);
  this->pending_action_ = PENDING_ACTION_PRESS_UP;
  this->select_cover(cover_index);
}

void PeshoSomfyComponent::cover_close(uint8_t cover_index) {
  if (cover_index > 4) {
    ESP_LOGW(TAG, "Invalid cover index for close: %u (must be 0-4)", cover_index);
    return;
  }
  
  // Check if already at target cover
  if (this->current_cover_index_ == cover_index && this->select_cover_state_ == SELECT_COVER_IDLE) {
    // Already at target, just press down
    ESP_LOGI(TAG, "Already at Remote Cover %u (Index %u), pressing DOWN", cover_index + 1, cover_index);
    this->press_down();
    return;
  }
  
  // Set pending action and select cover
  ESP_LOGI(TAG, "Closing Remote Cover %u (Index %u) - selecting cover first", cover_index + 1, cover_index);
  this->pending_action_ = PENDING_ACTION_PRESS_DOWN;
  this->select_cover(cover_index);
}

void PeshoSomfyComponent::cover_stop(uint8_t cover_index) {
  if (cover_index > 4) {
    ESP_LOGW(TAG, "Invalid cover index for stop: %u (must be 0-4)", cover_index);
    return;
  }
  
  // Check if already at target cover
  if (this->current_cover_index_ == cover_index && this->select_cover_state_ == SELECT_COVER_IDLE) {
    // Already at target, just press my
    ESP_LOGI(TAG, "Already at Remote Cover %u (Index %u), pressing MY", cover_index + 1, cover_index);
    this->press_my();
    return;
  }
  
  // Set pending action and select cover
  ESP_LOGI(TAG, "Stopping Remote Cover %u (Index %u) - selecting cover first", cover_index + 1, cover_index);
  this->pending_action_ = PENDING_ACTION_PRESS_MY;
  this->select_cover(cover_index);
}

void PeshoSomfyComponent::handle_select_cover_state_machine() {
  uint32_t now = millis();
  
  switch (this->select_cover_state_) {
    case SELECT_COVER_IDLE:
      // Should not happen, but handle gracefully
      break;
      
    case SELECT_COVER_RESETTING_TO_COVER3:
      // Wait for button to be released
      if (this->active_button_pin_ == nullptr) {
        // Button released, wait for LED to stabilize
        this->select_cover_state_ = SELECT_COVER_WAITING_FOR_LED3_STABLE;
        this->select_cover_wait_start_time_ = now;
        ESP_LOGD(TAG, "Reset phase: Button released, waiting for LED3 to stabilize (press #%u)", 
                 this->select_cover_reset_press_count_);
      }
      break;
      
    case SELECT_COVER_WAITING_FOR_LED3_STABLE:
      // Wait for LED3 to stabilize after button release
      if (now - this->select_cover_wait_start_time_ >= LED_STABLE_DELAY_MS) {
        this->select_cover_state_ = SELECT_COVER_CHECKING_LED3;
      }
      break;
      
    case SELECT_COVER_CHECKING_LED3: {
      // Check if LED3 is on (using filtered binary sensor state)
      bool led3_on = get_led3_binary_sensor_state();
      bool led4_on = get_led4_binary_sensor_state();
      
      if (led3_on && !led4_on) {
        // Success! LED3 is on, we're at Cover 3
        ESP_LOGI(TAG, "Reset phase complete! Remote Cover 3 (Index 2) reached after %u presses", 
                 this->select_cover_reset_press_count_);
        this->current_cover_index_ = 2;
        
        // Check if we need to do selection phase
        if (this->select_cover_presses_remaining_ == 0) {
          // Already at target
          ESP_LOGI(TAG, "Select cover complete! Already at target Remote Cover %u (Index %u)", 
                   this->select_cover_target_ + 1, this->select_cover_target_);
          this->select_cover_state_ = SELECT_COVER_IDLE;
          this->last_select_cover_complete_time_ = millis();
          
          // Execute pending action if any
          if (this->pending_action_ == PENDING_ACTION_PRESS_UP) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press UP for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_up();
          } else if (this->pending_action_ == PENDING_ACTION_PRESS_DOWN) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press DOWN for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_down();
          } else if (this->pending_action_ == PENDING_ACTION_PRESS_MY) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press MY for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_my();
          }
        } else {
          // Start selection phase
          ESP_LOGI(TAG, "Starting selection phase: %u presses needed to reach Remote Cover %u (Index %u)", 
                   this->select_cover_presses_remaining_, 
                   this->select_cover_target_ + 1, this->select_cover_target_);
          this->select_cover_state_ = SELECT_COVER_WAITING_FOR_BUTTON_RELEASE;
          this->select_cover_wait_start_time_ = now;
          this->press_button(this->select_cover_pin_, "Select Cover (Select)", true);
        }
      } else if (this->select_cover_reset_press_count_ >= MAX_RESET_PRESSES) {
        // Too many presses, give up
        ESP_LOGW(TAG, "Reset phase failed after %u presses - LED3 did not light up", 
                 this->select_cover_reset_press_count_);
        this->select_cover_state_ = SELECT_COVER_IDLE;
        
        // Clear pending action on failure
        if (this->pending_action_ != PENDING_ACTION_NONE) {
          ESP_LOGW(TAG, "Clearing pending action due to select_cover failure");
          this->pending_action_ = PENDING_ACTION_NONE;
        }
      } else {
        // LED3 not on yet, increment count and press select_cover again
        this->select_cover_reset_press_count_++;
        ESP_LOGD(TAG, "Reset phase: Pressing select_cover (press #%u)", 
                 this->select_cover_reset_press_count_);
        this->press_button(this->select_cover_pin_, "Select Cover (Reset)", true);
        this->select_cover_state_ = SELECT_COVER_RESETTING_TO_COVER3;
      }
      break;
    }
      
    case SELECT_COVER_WAITING_FOR_BUTTON_RELEASE:
      // Wait for button to be released
      if (this->active_button_pin_ == nullptr) {
        // Button released, decrement presses remaining
        this->select_cover_press_count_++;
        
        if (this->select_cover_presses_remaining_ > 0) {
          this->select_cover_presses_remaining_--;
        }
        
        ESP_LOGD(TAG, "Selection phase: Press completed, %u presses remaining", 
                 this->select_cover_presses_remaining_);
        
        if (this->select_cover_presses_remaining_ == 0) {
          // All presses complete
          ESP_LOGI(TAG, "Select cover complete! Remote Cover %u (Index %u) reached after %u selection presses", 
                   this->select_cover_target_ + 1, this->select_cover_target_, 
                   this->select_cover_press_count_);
          this->current_cover_index_ = this->select_cover_target_;
          this->select_cover_state_ = SELECT_COVER_IDLE;
          this->last_select_cover_complete_time_ = millis();
          
          // Execute pending action if any
          if (this->pending_action_ == PENDING_ACTION_PRESS_UP) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press UP for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_up();
          } else if (this->pending_action_ == PENDING_ACTION_PRESS_DOWN) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press DOWN for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_down();
          } else if (this->pending_action_ == PENDING_ACTION_PRESS_MY) {
            this->pending_action_ = PENDING_ACTION_NONE;
            ESP_LOGI(TAG, "Executing pending action: Press MY for Remote Cover %u", this->select_cover_target_ + 1);
            this->press_my();
          }
        } else {
          // Need more presses, wait a bit before next press
          this->select_cover_state_ = SELECT_COVER_WAITING_FOR_NEXT_PRESS;
          this->select_cover_wait_start_time_ = now;
        }
      }
      break;
      
    case SELECT_COVER_WAITING_FOR_NEXT_PRESS: {
      // Wait before next press
      uint32_t press_interval = this->button_press_duration_ms_ + SELECT_COVER_PRESS_MARGIN_MS;
      if (now - this->select_cover_wait_start_time_ >= press_interval) {
        // Press select_cover again
        this->press_button(this->select_cover_pin_, "Select Cover (Select)", true);
        this->select_cover_state_ = SELECT_COVER_WAITING_FOR_BUTTON_RELEASE;
      }
      break;
    }
  }
}

bool PeshoSomfyComponent::is_ready() const {
  // Not ready if select cover operation is in progress
  if (this->select_cover_state_ != SELECT_COVER_IDLE) {
    return false;
  }
  
  // Not ready if a button is currently being pressed
  if (this->active_button_pin_ != nullptr) {
    return false;
  }
  
  return true;
}

const char* PeshoSomfyComponent::get_busy_reason() const {
  if (this->select_cover_state_ != SELECT_COVER_IDLE) {
    return "Select cover in progress";
  }
  if (this->active_button_pin_ != nullptr) {
    return "Button press in progress";
  }
  return "Ready";
}

}  // namespace pesho_somfy
}  // namespace esphome
