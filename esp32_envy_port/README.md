# ESP-KNX-IP for ESP32

This library enables KNX/IP communication on ESP32 devices using UDP multicast on 224.0.23.12:3671. It is a port of the original [ESP8266 library by Nico Weichbrodt](https://github.com/envy/esp-knx-ip) with full API compatibility.

## Key Features

- Send and receive KNX messages via IP
- Web interface for configuration
- Persistent storage of configuration using Preferences
- Support for various KNX datatypes
- Callback-based message handling

## Installation

### PlatformIO

Add the library to your `platformio.ini`:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
  esp-knx-ip
```

### Manual Installation

1. Download the library
2. Extract to your Arduino/libraries folder or your PlatformIO lib directory
3. Restart your IDE

## ESP8266 to ESP32 Port Changes

- Replaced EEPROM storage with ESP32's Preferences library
- Updated UDP multicast implementation for ESP32
- Changed from `ESP8266WebServer` to ESP32's `WebServer` class
- Integrated ESP32's logging system
- Added defensive programming to prevent crashes

## Basic Usage

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include "esp-knx-ip.h"

const char* ssid = "your-ssid";
const char* password = "your-password";

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  
  // Set physical address (1.1.0)
  address_t pa;
  pa.bytes.high = (1 << 4) | 1;  // area=1, line=1
  pa.bytes.low = 0;              // member=0
  knx.physical_address_set(pa);
  
  // Start KNX
  knx.start();
  
  // Register callback for receiving messages
  knx.callback_register("Temperature", [](message_t const &msg, void *arg) {
    if (msg.ct == KNX_CT_WRITE) {
      float temp = knx.data_to_2byte_float(msg.data);
      Serial.printf("Received temperature: %.1f°C\n", temp);
    }
  });
  
  // Assign callback to a group address (1/2/3)
  knx.callback_assign(0, knx.GA_to_address(1, 2, 3));
}

void loop() {
  knx.loop();
}
```

## Sending Messages

```cpp
// Create a group address (3/2/1)
address_t dest = knx.GA_to_address(3, 2, 1);

// Send a boolean value (DPT 1.001)
knx.write_1bit(dest, true);  // Send ON command

// Send a temperature value (DPT 9.001)
knx.write_2byte_float(dest, 21.5);  // Send 21.5°C

// Send a dimming value (DPT 5.001)
knx.write_1byte_uint(dest, 128);  // Send 50% dimming value
```

## Receiving Messages

Register callbacks to process incoming messages:

```cpp
// Register a callback for processing messages
callback_id_t cb_id = knx.callback_register("Light Control", [](message_t const &msg, void *arg) {
  switch (msg.ct) {
    case KNX_CT_WRITE:
      bool value = knx.data_to_bool(msg.data);
      digitalWrite(LED_PIN, value ? HIGH : LOW);
      break;
    case KNX_CT_READ:
      // Answer with current state
      bool current_state = digitalRead(LED_PIN) == HIGH;
      knx.answer_1bit(msg.received_on, current_state);
      break;
  }
}, nullptr);

// Assign to a group address
knx.callback_assign(cb_id, knx.GA_to_address(1, 1, 1));
```

## Configuration

### Web Interface

The library provides a web interface for configuration. Access it by visiting the ESP32's IP address.

### Static Configuration

For static configuration without using the web interface:

```cpp
// Set physical address
knx.physical_address_set(knx.PA_to_address(1, 1, 1));

// Register callbacks
callback_id_t temp_cb_id = knx.callback_register("Temperature", temp_callback);
callback_id_t light_cb_id = knx.callback_register("Light", light_callback);

// Assign callbacks to group addresses
knx.callback_assign(temp_cb_id, knx.GA_to_address(1, 2, 3));
knx.callback_assign(light_cb_id, knx.GA_to_address(1, 2, 4));

// Start KNX with a custom WebServer instance (or pass nullptr for no web interface)
WebServer server(80);
knx.start(&server);
```

## Data Types

The library supports various KNX data types:

| Function | KNX DPT | Description |
|----------|---------|-------------|
| `send_1bit` | DPT 1.xxx | Boolean values |
| `send_2bit` | DPT 2.xxx | 2-bit control |
| `send_4bit` | DPT 3.xxx | Dimming, blinds |
| `send_1byte_int` | DPT 6.xxx | 8-bit signed value |
| `send_1byte_uint` | DPT 5.xxx | 8-bit unsigned value |
| `send_2byte_int` | DPT 8.xxx | 16-bit signed value |
| `send_2byte_uint` | DPT 7.xxx | 16-bit unsigned value |
| `send_2byte_float` | DPT 9.xxx | Float value with encoding |
| `send_3byte_time` | DPT 10.xxx | Time values |
| `send_3byte_date` | DPT 11.xxx | Date values |
| `send_3byte_color` | DPT 232.xxx | RGB color values |
| `send_4byte_int` | DPT 13.xxx | 32-bit signed value |
| `send_4byte_uint` | DPT 12.xxx | 32-bit unsigned value |
| `send_4byte_float` | DPT 14.xxx | IEEE float value |
| `send_14byte_string` | DPT 16.xxx | 14-byte string |

For each of these functions, there are corresponding `write_*` and `answer_*` variants for convenience.

## Advanced Usage

### Dynamic Configuration

Register configuration options for the web interface:

```cpp
// Register a string config
config_id_t hostname_id = knx.config_register_string("Hostname", 20, String("knx-device"));

// Register a boolean config
config_id_t enable_id = knx.config_register_bool("Enable", true);

// Register a numeric config 
config_id_t interval_id = knx.config_register_int("Update Interval", 60000);

// Register a group address config
config_id_t status_ga_id = knx.config_register_ga("Status Address");

// Get config values
String hostname = knx.config_get_string(hostname_id);
bool enabled = knx.config_get_bool(enable_id);
int interval = knx.config_get_int(interval_id);
address_t status_ga = knx.config_get_ga(status_ga_id);
```

### Feedback Display

Show feedback values in the web interface:

```cpp
float temperature = 0.0;
bool relay_state = false;

// Register feedback values
knx.feedback_register_float("Temperature", &temperature, 1);
knx.feedback_register_bool("Relay State", &relay_state);

// Register an action button
knx.feedback_register_action("Toggle Relay", toggle_relay_function);
```

## Troubleshooting

1. **UDP Multicast Issues**: Ensure your WiFi router allows multicast traffic. Some routers block it by default.

2. **Configuration Persistence**: Call `knx.save_to_preferences()` after changing configuration to persist it.

3. **WebServer Crashes**: Initialize the WebServer before passing it to KNX, and check for memory issues.

4. **Debugging**: Enable debugging by setting the log level for the KNX component:
   ```
   in main.cpp
   #include <esp_log.h>
   
   and
   esp_log_level_set("KNXIP", ESP_LOG_DEBUG);
   ```

## License

This library is licensed under the MIT License, as per the original library.

## Acknowledgments

- Original library by Nico Weichbrodt (envy)
- ESP32 port contributors, that would be me and Claude.ai...