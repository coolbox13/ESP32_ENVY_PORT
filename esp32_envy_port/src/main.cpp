#include <WiFi.h>
#include <WebServer.h>
#include <esp_log.h>
#include "esp-knx-ip.h"
#include <Arduino.h>

// Replace with your Wi‑Fi network credentials
const char* ssid = "ssid";
const char* password = "password";

// Create WebServer outside of setup
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Enable KNX/IP library debugging
  esp_log_level_set("KNXIP", ESP_LOG_DEBUG);
  
  Serial.println("Starting KNX/IP Test");

  // Connect to Wi‑Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Set the originating (physical) address to 1.1.160.
  address_t pa;
  pa.bytes.high = (1 << 4) | 1; // area=1, line=1 → 0x11
  pa.bytes.low = 160;           // member = 160
  knx.physical_address_set(pa);
  
  // Make sure server is fully initialized before passing to KNX
  server.enableCORS(true);
  server.begin();
  delay(100);  // Give server a moment to initialize
  
  // Start the KNX/IP service - pass nullptr to disable web interface if it's causing issues
  // knx.start(nullptr);  // Option 1: Disable web interface
  knx.start(&server);   // Option 2: Use web interface with properly initialized server

  // Register a callback to monitor messages on the bus.
  knx.callback_register("Monitor", [](message_t const &msg, void *arg) {
    Serial.print("Received KNX message, CT=0x");
    Serial.print(msg.ct, HEX);
    Serial.print(" Data:");
    for (uint8_t i = 0; i < msg.data_len; i++) {
      Serial.print(" 0x");
      Serial.print(msg.data[i], HEX);
    }
    Serial.println();
  }, nullptr);

  // Build the target group address "10/6/5" (using group address format)
  address_t group;
  group.ga.line = 6;
  group.ga.area = 10;
  group.ga.member = 5;

  // Send a KNX message: using send_2byte_float to send 25.0°C.
  knx.send_2byte_float(group, KNX_CT_WRITE, 25.0f);
  Serial.println("Sent temperature command: 25°C to group 10/6/5");
}

void loop() {
  // Process KNX messages
  knx.loop();
  
  // Add a small delay to prevent watchdog timer issues
  delay(10);
}