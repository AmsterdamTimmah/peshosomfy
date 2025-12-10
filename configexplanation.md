---
noteId: "93918d30d73a11f08aebbf4f45f130c4"
tags: []

---

### Basic Configuration

```yaml
external_components:
  - source:
      type: local
      path: custom_components

pesho_somfy:
  id: somfy_remote
  select_cover_pin: GPIO1
  up_pin: GPIO4
  down_pin: GPIO2
  my_pin: GPIO3
  led3_pin: GPIO6
  led4_pin: GPIO7
  led3_binary_sensor: esphome_LED3_State
  led4_binary_sensor: esphome_LED4_State
  button_press_duration: 200ms
```

### ESPHome Entities

**Buttons**:
```yaml
button:
  - platform: template
    name: "Somfy Select Cover"
    on_press:
      - lambda: |-
          id(somfy_remote)->press_select_cover();
  
  - platform: template
    name: "Somfy Up"
    on_press:
      - lambda: |-
          id(somfy_remote)->press_up();
  
  - platform: template
    name: "Somfy Down"
    on_press:
      - lambda: |-
          id(somfy_remote)->press_down();
  
  - platform: template
    name: "Somfy My"
    on_press:
      - lambda: |-
          id(somfy_remote)->press_my();
```

**Number Entity** (Cover Selection):
```yaml
number:
  - platform: template
    name: "Somfy Select Cover"
    id: somfy_select
    min_value: 1
    max_value: 5
    step: 1
    lambda: |-
      // Return current internal cover state as Remote Cover number (1-5)
      return (float)(id(somfy_remote)->get_current_cover_index() + 1);
    set_action:
      - lambda: |-
          // Convert from Remote Cover (1-5) to Index (0-4)
          uint8_t target_index = (uint8_t)(x - 1);
          id(somfy_remote)->select_cover(target_index);
    update_interval: 500ms
```

**How the Number Input Works:**

This is the easiest way to select covers from Home Assistant. Here's what happens:

1. **You pick a number** (1-5) in Home Assistant
2. **It converts** the Remote Cover number (1-5) to internal index (0-4) by subtracting 1
3. **It selects the cover** using the simple method:
   - **Reset Phase** (if needed): Checks if LED3 is on (Cover 3 selected)
     - If not, presses select_cover until LED3 lights up
     - This ensures we always start from a known position (Cover 3)
   - **Selection Phase**: From Cover 3, calculates how many presses are needed
     - Formula: `(target_index - 2 + 5) % 5` (Cover 3 = index 2)
     - Presses the button that many times
     - Handles wrapping (e.g., Cover 3 to Cover 1 = 4 presses)
4. **Tracks state**: Component keeps track internally and can sync from LEDs
5. **Stays busy**: Device is busy during the entire operation (reset + selection)

**Example**: You're on Cover 2 (index 1) and select Cover 5 (index 4):
- **Reset Phase**: LED3 is not on, so it presses select_cover until LED3 lights up (reaches Cover 3)
- **Selection Phase**: From Cover 3 (index 2), calculates: (4 - 2 + 5) % 5 = 2 presses needed
- First press: Cover 3 → Cover 4
- Second press: Cover 4 → Cover 5
- Component updates internal state to index 4 (Cover 5)

**Example**: LED3 is already on (Cover 3 selected) and you select Cover 1 (index 0):
- **Reset Phase**: Skipped (LED3 already on)
- **Selection Phase**: From Cover 3 (index 2), calculates: (0 - 2 + 5) % 5 = 3 presses needed
- First press: Cover 3 → Cover 4
- Second press: Cover 4 → Cover 5
- Third press: Cover 5 → Cover 1
- Component updates internal state to index 0 (Cover 1)

**Binary Sensors**:
```yaml
binary_sensor:
  - platform: template
    name: "Somfy LED3"
    id: esphome_LED3_State
    lambda: |-
      return id(somfy_remote)->get_led3_state();
    filters:
      - delayed_on: 100ms
      - delayed_off: 100ms
  
  - platform: template
    name: "Somfy LED4"
    id: esphome_LED4_State
    lambda: |-
      return id(somfy_remote)->get_led4_state();
    filters:
      - delayed_on: 100ms
      - delayed_off: 100ms

  - platform: template
    name: "Somfy Ready"
    id: somfy_ready
    # State is published directly by the component, no lambda needed
```

**Ready Binary Sensor**:
- Shows device ready/busy state in Home Assistant
- `ON` = Ready (send commands)
- `OFF` = Busy (operation in progress: select cover or button press)
- Updates in real-time (no polling)
- Must be linked to the component via `ready_binary_sensor` config option


### Lambda Usage

In ESPHome lambdas, you can access the component methods:

```yaml
on_press:
  - lambda: |-
      // Press a button
      id(somfy_remote)->press_up();
      
      // Get current cover
      uint8_t current_cover = id(somfy_remote)->get_current_cover_index();
      ESP_LOGI("custom", "Current cover: %u (Remote Cover %u)", 
               current_cover, current_cover + 1);
      
      // Select a specific cover (will reset to Cover 3 first if needed)
      id(somfy_remote)->select_cover(2);  // Select Remote Cover 3 (Index 2)
      
      // Check LED states
      bool led3_on = id(somfy_remote)->get_led3_binary_sensor_state();
      bool led4_on = id(somfy_remote)->get_led4_binary_sensor_state();
```