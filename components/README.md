---
noteId: "93918d31d73a11f08aebbf4f45f130c4"
tags: []

---

This file explains the workings of the custom pesho_somfy component. It handles button presses, and reads LEDs.

## Hardware Integration

We're using an **ESP32C3 Supermini** connected to the high side of the remote's buttons. The GPIO pins sink current to emulate button presses.

The LEDs are connected directly to the remote MCU's input pins, after the current limiting resistor. LED1&2's input pins got damaged during development, so only LED3&4 are readable now.

When multiple LEDs are lit, the readings get erratic, so we filter those out in ESPHome (not in the custom component itself).

The image below depicts the pins, wires, and their colors:

![Pesho Somfy](include/../../include/Pesho%20Somfy.png)

### Button Press Simulation

I simulate button presses using a safe, non-blocking GPIO sequence:

1. **Start**: All button pins are INPUT (floating, high impedance) - basically disconnected
2. **Press** (non-blocking):
   - Set pin to LOW first (important!)
   - Switch pin to OUTPUT (now it sinks current to ground)
   - Return immediately (doesn't block!)
   - The release happens automatically in `loop()` after the configured duration
   - Pin goes back to INPUT (floating again)

Why this sequence? Accidentally sending HIGH pulses can damage the remote. This way we're safe, and everything is non-blocking so ESPHome can continue to work.

### LED Status Reading

We first read the LEDs in ESP32 (raw), then pass along the value to ESPhome. ESPhome filters out LED3&4 erratics behavior, and we then use these states internally in the somfy component:

**Raw GPIO Reading** (inside ESP32):
- `get_led3_state()` and `get_led4_state()` read pins directly
- Logic is inverted: HIGH = OFF, LOW = ON (because of how the circuit works)
- No filtering, just raw state

**Filtered Reading** (through ESPHome):
- `get_led3_binary_sensor_state()` and `get_led4_binary_sensor_state()` use ESPHome's filtered binary sensors
- These have debouncing and filtering (100ms delays)
- Much more reliable when both LEDs light up and readings get erratic
- This is used for cover detection

### Cover Selection Tracking

The component keeps track of which cover is currently selected (0-4, which corresponds to Remote Covers 1-5):

- **Current Cover Index**: Internally tracks which cover is selected
- **LED Sync**: Every 2 seconds, it checks the LEDs and syncs the cover index
  - LED3 ON = Remote Cover 3 = Index 2
  - LED4 ON = Remote Cover 4 = Index 3
  - Both OFF = Could be Covers 1, 2, or 5 (indices 0, 1, or 4)

### The Simple Select Method

When you select a cover, it always starts from a known position (Cover 3) for reliability:

1. Check if LED3 is already on (Cover 3 is selected)
2. If not, keep pressing select_cover until LED3 lights up (reset phase)
3. From Cover 3, calculate how many presses are needed to reach the target
4. Press the button that many times
5. Device stays busy during the entire operation

### Operation State Management

The component tracks whether it's ready or busy to prevent conflicts:

- **Ready**: Device is idle and can accept new commands
  - No select cover operation running
  - No button press active

- **Busy**: Device is doing something
  - Select cover operation in progress (might be resetting to Cover 3)
  - Button press in progress

**Protection**:
- External calls (from Home Assistant) are blocked when busy
- Internal calls (from state machines) bypass the ready check so operations can continue
- State changes are published immediately to Home Assistant via the "Somfy Ready" binary sensor

The "Somfy Ready" binary sensor shows:
- `ON` = Ready (go ahead, send commands)
- `OFF` = Busy (wait, I'm doing something)
- Updates in real-time (no polling needed)

### Pin Configuration

**Button Pins** (required):
- `select_cover_pin`: GPIO pin for selecting/cycling through covers
- `up_pin`: GPIO pin for moving cover up
- `down_pin`: GPIO pin for moving cover down
- `my_pin`: GPIO pin for "My" position (preset)

**LED Pins** (optional, but recommended):
- `led3_pin`: GPIO pin for reading LED3 status
- `led4_pin`: GPIO pin for reading LED4 status

**Binary Sensors** (optional, but recommended):
- `led3_binary_sensor`: Reference to ESPHome binary sensor for LED3
- `led4_binary_sensor`: Reference to ESPHome binary sensor for LED4
- `ready_binary_sensor`: Reference to ESPHome binary sensor for ready state (shows busy/ready in Home Assistant)

**Configuration**:
- `button_press_duration`: How long to hold the button (default: 500ms)

## API Reference

### C++ Methods

#### Button Control
- `void press_select_cover()` - Simulate Select Cover button press
- `void press_up()` - Simulate Up button press
- `void press_down()` - Simulate Down button press
- `void press_my()` - Simulate My button press

#### Cover Control
- `void cover_open(uint8_t cover_index)` - Select cover then press UP button
- `void cover_close(uint8_t cover_index)` - Select cover then press DOWN button
- `void cover_stop(uint8_t cover_index)` - Select cover then press MY button

These methods are used by the cover control buttons. They automatically:
- Select the correct cover first (if not already selected)
- Then press the appropriate command button
- Handle all the state machine logic internally

#### Cover Selection
- `uint8_t get_current_cover_index() const` - Get currently selected cover index (0-4)
- `void select_cover(uint8_t target_cover_index)` - Select specific cover (0-4) using simple method:
  - Checks if LED3 is on (Cover 3 selected)
  - If not, resets to Cover 3 by pressing select_cover until LED3 lights up
  - From Cover 3, calculates and presses the correct number of times to reach target
  - Device stays busy during entire operation (reset + selection phases)
- `void calibrate_cover_index()` - Manually set cover index to 3 (Remote Cover 4)
- `void sync_cover_index_from_leds()` - Sync cover index based on LED states (LED3 = Cover 3, LED4 = Cover 4)

#### LED State Reading
- `bool get_led3_state() const` - Read LED3 GPIO pin directly (raw state)
- `bool get_led4_state() const` - Read LED4 GPIO pin directly (raw state)
- `bool get_led3_binary_sensor_state() const` - Get LED3 binary sensor state (filtered, recommended)
- `bool get_led4_binary_sensor_state() const` - Get LED4 binary sensor state (filtered, recommended)

#### Operation State
- `bool is_ready() const` - Returns true if device is ready to accept new operations
- `const char* get_busy_reason() const` - Returns reason if busy ("Select cover in progress", "Button press in progress", or "Ready")


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

The MIT License allows you to:
- ✅ Use the code commercially
- ✅ Modify the code
- ✅ Distribute the code
- ✅ Sublicense the code
- ✅ Use privately

**Requirements:**
- Include the original copyright notice and license text
- State any significant changes made to the code

**No warranty or liability:** The software is provided "as is" without warranty of any kind.

## Credits

Created by Tim