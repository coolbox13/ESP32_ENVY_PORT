#include <WiFi.h>
#include <WebServer.h>
#include <esp_log.h>
#include "esp-knx-ip.h"
#include <Arduino.h>

// Replace with your Wi‑Fi network credentials
const char* ssid = "ssid";
const char* password = "password";

// Create a separate WebServer just for monitoring
WebServer monitorServer(80);

// Group address to send to
address_t groupAddr;

// Buffer for storing recent messages
const int MAX_MESSAGES = 10;
String recentMessages[MAX_MESSAGES];
int messageIndex = 0;

// Timer for periodic sending
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // 10 seconds

// Function declaration - needs to be before it's called
void addMessage(String message);

void handleRoot() {
  String html = "<html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 KNX Monitor</title>";
  html += "<style>body{font-family:Arial;margin:20px}";
  html += ".message{border:1px solid #ddd;padding:10px;margin:5px 0;border-radius:4px}";
  html += ".controls{margin:20px 0}</style>";
  html += "<script>function refreshPage(){window.location.reload()}</script>";
  html += "</head><body>";
  html += "<h1>ESP32 KNX Monitor</h1>";
  
  // Messages section
  html += "<h2>Recent KNX Messages</h2>";
  html += "<div class='messages'>";
  for (int i = 0; i < MAX_MESSAGES; i++) {
    int idx = (messageIndex - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
    if (recentMessages[idx].length() > 0) {
      html += "<div class='message'>" + recentMessages[idx] + "</div>";
    }
  }
  html += "</div>";
  
  // Controls section
  html += "<div class='controls'>";
  html += "<form action='/send' method='get'>";
  html += "Send temperature: <input type='number' name='temp' min='0' max='40' step='0.5' value='21.5'>";
  html += "<input type='submit' value='Send'>";
  html += "</form>";
  html += "<button onclick='refreshPage()'>Refresh</button>";
  html += "</div>";
  
  html += "</body></html>";
  
  monitorServer.send(200, "text/html", html);
}

void handleSend() {
  if (monitorServer.hasArg("temp")) {
    float temp = monitorServer.arg("temp").toFloat();
    knx.send_2byte_float(groupAddr, KNX_CT_WRITE, temp);
    
    String message = "Sent temperature: " + String(temp, 1) + "°C to 10/6/5";
    addMessage(message);
    Serial.println(message);
    
    monitorServer.sendHeader("Location", "/");
    monitorServer.send(302);
  } else {
    monitorServer.send(400, "text/plain", "Missing temperature parameter");
  }
}

// Function implementation
void addMessage(String message) {
  recentMessages[messageIndex] = message;
  messageIndex = (messageIndex + 1) % MAX_MESSAGES;
}

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

  // Set the originating physical address to 1.1.160
  address_t pa;
  pa.bytes.high = (1 << 4) | 1; // area=1, line=1 → 0x11
  pa.bytes.low = 160;           // member = 160
  knx.physical_address_set(pa);
  
  // Setup simple web monitor
  monitorServer.on("/", handleRoot);
  monitorServer.on("/send", handleSend);
  monitorServer.begin();
  Serial.println("Web monitor started");
  
  // Start KNX without built-in web interface
  knx.start(nullptr);

  // Register a callback to monitor messages on the bus
  knx.callback_register("Monitor", [](message_t const &msg, void *arg) {
    String message = "Received KNX message from ";
    message += String(msg.received_on.ga.area) + "/" + String(msg.received_on.ga.line) + "/" + String(msg.received_on.ga.member);
    message += ", CT=0x" + String(msg.ct, HEX) + ", Data:";
    
    for (uint8_t i = 0; i < msg.data_len; i++) {
      message += " 0x" + String(msg.data[i], HEX);
    }
    
    addMessage(message);
    Serial.println(message);
  }, nullptr);

  // Build the target group address "10/6/5"
  groupAddr.ga.line = 6;
  groupAddr.ga.area = 10;
  groupAddr.ga.member = 5;

  // Initial send
  knx.send_2byte_float(groupAddr, KNX_CT_WRITE, 21.5f);
  addMessage("Sent temperature command: 21.5°C to group 10/6/5");
  Serial.println("Sent temperature command: 21.5°C to group 10/6/5");
  lastSendTime = millis();
}

void loop() {
  // Process KNX messages
  knx.loop();
  
  // Handle web server requests
  monitorServer.handleClient();
  
  // Add a small delay
  delay(10);
}