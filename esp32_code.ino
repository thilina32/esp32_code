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
const int UPDATE_LED = 19; 
const int RED_LED = 12;
const int BUZZER = 13;

Servo servo1;
Servo servo2;
WiFiClientSecure espClient;
PubSubClient client(espClient);

volatile bool startOTA = false; 
volatile bool isUpdating = false; 

// =======================================================
// 🎯 SERVO & ALARM VARIABLES
// =======================================================
int currentAngle1 = 90; 
int targetAngle1 = 90;  
int currentAngle2 = 90; 
int targetAngle2 = 90;  
unsigned long lastMoveTime = 0;
const int SERVO_SPEED_DELAY = 15; // සර්වෝ වේගය

// Alarm Variables
bool isAlarmActive = false;
unsigned long lastAlarmTime = 0;
bool alarmState = false;
const int ALARM_INTERVAL = 200; // තත්පර 0.2 (200ms)

// =======================================================
// 🌐 NETWORK CONFIGURATIONS
// =======================================================
const char* ssid = "Redmi 14C";
const char* password = "12345678";
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
// 📡 MQTT CALLBACK 
// =======================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim(); 

  if (strcmp(topic, "board/update") == 0 && message.equals("START_OTA")) {
    MQTT_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
  
  if (strcmp(topic, "board/control") == 0) {
    // 🛑 Emergency Stop Command
    if (message.equals("STOP")) {
      targetAngle1 = currentAngle1; // මෝටර් 1 එතනම නතර කිරීම
      targetAngle2 = currentAngle2; // මෝටර් 2 එතනම නතර කිරීම
      isAlarmActive = true;         // Alarm එක On කිරීම
      MQTT_LOG("🛑 Emergency STOP! Motors halted and Alarm Activated.");
    }
    // 🟢 Reset Alarm Command
    else if (message.equals("RESET")) {
      isAlarmActive = false;
      digitalWrite(RED_LED, LOW);
      digitalWrite(BUZZER, LOW);
      MQTT_LOG("✅ Alarm Reset. System Normal.");
    }
    // 🔧 Servo Controls
    else if (message.startsWith("SERVO1:")) {
      int angle = message.substring(7).toInt();
      if (angle >= 0 && angle <= 180) {
        targetAngle1 = angle; 
        MQTT_LOG("🔧 Test Mode: Servo 1 Target set to " + String(angle) + "°");
      }
    } 
    else if (message.startsWith("SERVO2:")) {
      int angle = message.substring(7).toInt(); 
      if (angle >= 0 && angle <= 180) {
        targetAngle2 = angle;
        MQTT_LOG("🔧 Test Mode: Servo 2 Target set to " + String(angle) + "°");
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
      MQTT_LOG("🟢 SentryGuard Online & Ready!");
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
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  digitalWrite(UPDATE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  servo1.write(currentAngle1); 
  servo2.write(currentAngle2);
  
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
  
  if (isUpdating) return; 

  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long currentMillis = millis();

  // =======================================================
  // ⚙️ SERVO SLOW MOVEMENT LOGIC (NON-BLOCKING)
  // =======================================================
  if (currentMillis - lastMoveTime >= SERVO_SPEED_DELAY) {
    lastMoveTime = currentMillis;
    
    // Servo 1 Update
    if (currentAngle1 < targetAngle1) {
      currentAngle1++;
      servo1.write(currentAngle1);
    } else if (currentAngle1 > targetAngle1) {
      currentAngle1--;
      servo1.write(currentAngle1);
    }

    // Servo 2 Update
    if (currentAngle2 < targetAngle2) {
      currentAngle2++;
      servo2.write(currentAngle2);
    } else if (currentAngle2 > targetAngle2) {
      currentAngle2--;
      servo2.write(currentAngle2);
    }
  }

  // =======================================================
  // 🚨 ALARM LOGIC (NON-BLOCKING 0.2s BLINK)
  // =======================================================
  if (isAlarmActive) {
    if (currentMillis - lastAlarmTime >= ALARM_INTERVAL) {
      lastAlarmTime = currentMillis;
      alarmState = !alarmState; // State එක මාරු කිරීම (High/Low)
      
      digitalWrite(RED_LED, alarmState ? HIGH : LOW);
      digitalWrite(BUZZER, alarmState ? HIGH : LOW);
    }
  }
}