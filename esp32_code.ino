#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <ESP32Servo.h>

//#define ENABLE_MQTT_LOGS 
#ifdef ENABLE_MQTT_LOGS
  #define MQTT_LOG(msg) { if(client.connected()) { client.publish("board/logs", String(msg).c_str()); } }
#else
  #define MQTT_LOG(msg) 
#endif 

// =======================================================
// 🔌 PINS SETUP
// =======================================================
const int SERVO1_PIN = 15;
const int SERVO2_PIN = 16;
const int UPDATE_LED = 19; // OTA එක වෙනකොට පත්තුවන LED එක

const int US1_TRIG = 8;
const int US1_ECHO = 9;
const int US2_TRIG = 10;
const int US2_ECHO = 11;

Servo servo1;
Servo servo2;
WiFiClientSecure espClient;
PubSubClient client(espClient);

volatile bool startOTA = false; 
volatile bool isUpdating = false; 

// =======================================================
// 🌐 NETWORK CONFIGURATIONS
// =======================================================
const char* ssid = "Galaxy F05 9607";
const char* password = "303536&me";
const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";
const char* firmware_url = "https://raw.githubusercontent.com/thilina32/esp32_code/main/code.bin"; 

// අවසන් වරට RAM එක check කරපු වෙලාව
unsigned long lastMemCheck = 0;

// =======================================================
// 🛠️ DEBUGGING HELPER FUNCTION
// =======================================================
void printSystemStatus() {
  Serial.println("\n--- [SYSTEM STATUS] ---");
  Serial.printf("📶 WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("🌐 IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("📡 MQTT Status: %s\n", client.connected() ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("🧠 Free RAM (Heap): %d bytes\n", ESP.getFreeHeap());
  Serial.println("-----------------------\n");
}

// =======================================================
// 🚀 ONLINE OTA UPDATE FUNCTION
// =======================================================
void performOTA() {
  isUpdating = true; 
  Serial.println("[OTA] 🚀 Starting OTA Update from GitHub...");
  Serial.printf("[OTA] 🧠 RAM before killing MQTT: %d bytes\n", ESP.getFreeHeap());
  MQTT_LOG("🚀 Starting OTA Update from GitHub. Disconnecting MQTT to save RAM...");
  
  delay(500); // MQTT log එක යන්න වෙලාව දෙනවා
  
  // 1️⃣ FREE UP MEMORY
  client.disconnect();
  espClient.stop();
  delay(500); 

  Serial.printf("[OTA] 🧠 RAM after killing MQTT: %d bytes\n", ESP.getFreeHeap());
  Serial.println("[OTA] 🔌 Disconnected MQTT to save memory.");

  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  // 2️⃣ FOLLOW REDIRECTS (GitHub එකට අත්‍යවශ්‍යයි)
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.println("[OTA] ⏳ Downloading firmware...");
  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA_ERROR] ❌ HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false; 
      Serial.println("[OTA] 🔄 Restarting ESP32 to recover...");
      delay(1000);
      ESP.restart(); 
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] ⚠️ HTTP_UPDATE_NO_UPDATES: Current version is up to date.");
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false;
      Serial.println("[OTA] 🔄 Restarting ESP32...");
      delay(1000);
      ESP.restart();
      break;
      
    case HTTP_UPDATE_OK:
      Serial.println("[OTA] ✅ HTTP_UPDATE_OK: Update Successful!");
      Serial.println("[OTA] 🔄 Rebooting automatically...");
      break;
  }
}

// =======================================================
// 📡 MQTT CALLBACK (SERVO TESTING & OTA VERIFICATION)
// =======================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim(); 

  Serial.printf("[MQTT_RX] Message arrived on topic: %s | Payload: %s\n", topic, message.c_str());

  if (strcmp(topic, "board/update") == 0 && message.equals("START_OTA")) {
    Serial.println("[MQTT_CMD] 🔥 OTA Signal Verified! Setting flag to update...");
    MQTT_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
  
  if (strcmp(topic, "board/control") == 0) {
    if (message.startsWith("SERVO1:")) {
      int angle = message.substring(7).toInt();
      if (angle >= 0 && angle <= 180) {
        servo1.write(angle);
        Serial.printf("[SERVO] 🔧 Servo 1 moved to %d°\n", angle);
        MQTT_LOG("🔧 Test Mode: Servo 1 moved to " + String(angle) + "°");
      }
    } 
    else if (message.startsWith("SERVO2:")) {
      int angle = message.substring(7).toInt(); 
      if (angle >= 0 && angle <= 180) {
        servo2.write(angle);
        Serial.printf("[SERVO] 🔧 Servo 2 moved to %d°\n", angle);
        MQTT_LOG("🔧 Test Mode: Servo 2 moved to " + String(angle) + "°");
      }
    }
  }
}

// =======================================================
// 🌐 WIFI SETUP
// =======================================================
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.printf("[WIFI] 🔄 Connecting to %s ", ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if(attempts > 20) { // තත්පර 10කට පස්සෙත් connect වුණේ නැත්නම්
      Serial.println("\n[WIFI_ERROR] ❌ WiFi Connection Timeout!");
      attempts = 0;
    }
  }
  
  Serial.println("\n[WIFI] ✅ WiFi Connected!");
  Serial.printf("[WIFI] 🌐 IP Address: %s\n", WiFi.localIP().toString().c_str());
}

// =======================================================
// 🔌 MQTT RECONNECT
// =======================================================
void reconnect() {
  while (!client.connected()) {
    Serial.println("[MQTT] 🔄 Attempting MQTT connection...");
    
    // Check WiFi status before attempting MQTT connection
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("[MQTT_ERROR] ❌ WiFi disconnected. Reconnecting WiFi first...");
      setup_wifi();
    }
    
    String clientId = "SentryGuard_Test_" + String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      Serial.println("[MQTT] ✅ Connected to MQTT Broker!");
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      client.subscribe("board/control"); 
      MQTT_LOG("🟢 SentryGuard [Servo Test Firmware] Online & Ready!");
    } else {
      Serial.printf("[MQTT_ERROR] ❌ Failed to connect. State/Error Code: %d\n", client.state());
      Serial.println("[MQTT] ⏳ Trying again in 5 seconds...");
      delay(5000); 
    }
  }
}

// =======================================================
// ⚙️ SETUP & LOOP
// =======================================================
void setup() {
  Serial.begin(115200); // 🚨 Serial Monitor එකේ baud rate එක 115200 දාන්න අමතක කරන්න එපා
  delay(1000);
  Serial.println("\n\n=========================================");
  Serial.println("🤖 SentryGuard Firmware Booting Up...");
  Serial.println("=========================================");
  
  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  Serial.println("[SETUP] ⚙️ Allocating timers for Servos...");
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  setup_wifi(); 
  
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  printSystemStatus();
}

void loop() {
  if (startOTA) { 
    startOTA = false; 
    performOTA(); 
  }
  
  if (isUpdating) return; 

  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 
  
  // තත්පර 30කට සැරයක් System Status එක Serial Monitor එකේ පෙන්නන්න
  if (millis() - lastMemCheck > 30000) {
    printSystemStatus();
    lastMemCheck = millis();
  }
  
  delay(10); 
}