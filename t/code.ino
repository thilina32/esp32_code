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
// 🔌 PINS SETUP (TESTING ONLY)
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



// =======================================================
// 🚀 ONLINE OTA UPDATE FUNCTION
// =======================================================
void performOTA() {
  isUpdating = true; 
  MQTT_LOG("🚀 Starting OTA Update from GitHub...");
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      MQTT_LOG(String("❌ HTTP_UPDATE_FAILED Error: ") + httpUpdate.getLastErrorString());
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false; 
      break;
    case HTTP_UPDATE_NO_UPDATES:
      MQTT_LOG("⚠️ HTTP_UPDATE_NO_UPDATES: Current version is up to date.");
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false;
      break;
    case HTTP_UPDATE_OK:
      MQTT_LOG("✅ HTTP_UPDATE_OK: Update Successful! Restarting now..."); 
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

  // GitHub එකෙන් Online Update වෙන්න සිග්නල් එක චෙක් කිරීම
  if (strcmp(topic, "board/update") == 0 && message.equals("START_OTA")) {
    MQTT_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
  
  // Servo මෝටර් දෙක ටෙස්ට් කිරීමේ කොටස
  if (strcmp(topic, "board/control") == 0) {
    // Servo 1 Control (උදා: SERVO1:90)
    if (message.startsWith("SERVO1:")) {
      int angle = message.substring(7).toInt();
      if (angle >= 0 && angle <= 180) {
        servo1.write(angle);
        MQTT_LOG("🔧 Test Mode: Servo 1 moved to " + String(angle) + "°");
      }
    } 
    // Servo 2 Control (උදා: SERVO2:45)
    else if (message.startsWith("SERVO2:")) {
      int angle = message.substring(7).toInt(); 
      if (angle >= 0 && angle <= 180) {
        servo2.write(angle);
        MQTT_LOG("🔧 Test Mode: Servo 2 moved to " + String(angle) + "°");
      }
    }
  }
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "SentryGuard_Test_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      client.subscribe("board/control"); 
      MQTT_LOG("🟢 SentryGuard [Servo Test Firmware] Online & Ready!");
    } else {
      delay(5000); 
    }
  }
}

// =======================================================
// ⚙️ SETUP & LOOP
// =======================================================
void setup() {
  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  // Servo Allocations
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  // Default position 90°
  //servo1.write(90); 
  //servo2.write(90);
  
  setup_wifi(); 
  
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (startOTA) { 
    startOTA = false; 
    performOTA(); 
  }
  
  if (isUpdating) return; // Update වෙන අතරතුර වෙනත් දේවල් ක්‍රියාත්මක වීම නැවතීම

  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 
  delay(10); // පොඩි ඩිලේ එකක් ස්ටේබල් වෙන්න
}