#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <ESP32Servo.h>

// දැන් Logs යන්නේ Serial Monitor එකට පමණි
#define SERIAL_LOG(msg) { Serial.println(msg); }

// =======================================================
// 🔌 PINS SETUP
// =======================================================
const int SERVO1_PIN = 15;
const int SERVO2_PIN = 16;
const int UPDATE_LED = 19; 

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
// 📏 ULTRASONIC DISTANCE MEASUREMENT & LOGGING
// =======================================================
void checkDistanceAndLog(int servoID, int currentAngle, int trigPin, int echoPin) {
  // Ultrasonic Pulse යැවීම
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo එක ලබා ගැනීම (Timeout: 10000us ~ 1.7m පමණි, ප්‍රමාදයන් අවම කිරීමට)
  long duration = pulseIn(echoPin, HIGH, 10000); 
  
  if (duration == 0) return; // Signal එකක් නැත්නම් අත්හරින්න

  float distance = duration * 0.034 / 2;

  // දුර 20cm ට වඩා අඩු නම් පමණක් Serial Monitor එකට Log එකක් යවන්න
  if (distance > 0 && distance <= 20.0) {
    String logMsg = "🚨 ALERT! Servo: " + String(servoID) + " | Angle: " + String(currentAngle) + "° | Distance: " + String(distance, 1) + "cm";
    SERIAL_LOG(logMsg);
  }
}

// =======================================================
// 🔍 DUAL SCANNING FUNCTION (දෙපාරක් කැරකි හොයන කොටස)
// =======================================================
void scanTwice() {
  SERIAL_LOG("🔍 Starting Dual Radar Scan...");
  
  // වට 2ක් කැරකීමට (1 cycle = වමට + දකුණට)
  for (int cycle = 0; cycle < 1; cycle++) {
    
    // ➡️ Forward Sweep
    for (int step = 0; step <= 140; step++) {
      int s1_angle = 40 + step; // Servo 1: 40 to 180
      int s2_angle = 15 + (step * 135 / 140); // Servo 2: 15 to 150 (Mapping)

      servo1.write(s1_angle);
      servo2.write(s2_angle);

      checkDistanceAndLog(1, s1_angle, US1_TRIG, US1_ECHO);
      checkDistanceAndLog(2, s2_angle, US2_TRIG, US2_ECHO);

      client.loop(); // MQTT Connection එක Drop නොවී තබා ගැනීමට
      delay(25); // මෝටරය ස්මූත් ව ගමන් කිරීමට
    }

  }
  SERIAL_LOG("✅ Scan Complete!");
}

// =======================================================
// 🚀 ONLINE OTA UPDATE FUNCTION
// =======================================================
void performOTA() {
  isUpdating = true; 
  SERIAL_LOG("🚀 Starting OTA Update from GitHub...");
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      SERIAL_LOG(String("❌ HTTP_UPDATE_FAILED Error: ") + httpUpdate.getLastErrorString());
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false; 
      break;
    case HTTP_UPDATE_NO_UPDATES:
      SERIAL_LOG("⚠️ HTTP_UPDATE_NO_UPDATES: Current version is up to date.");
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false;
      break;
    case HTTP_UPDATE_OK:
      SERIAL_LOG("✅ HTTP_UPDATE_OK: Update Successful! Restarting now..."); 
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
    SERIAL_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
  
  if (strcmp(topic, "board/control") == 0) {
    if (message.equals("SCAN")) {
      scanTwice();
    }
    else if (message.startsWith("SERVO1:")) {
      int angle = message.substring(7).toInt();
      if (angle >= 40 && angle <= 180) { 
        servo1.write(angle);
        SERIAL_LOG("🔧 Test Mode: Servo 1 moved to " + String(angle) + "°");
      }
    } 
    else if (message.startsWith("SERVO2:")) {
      int angle = message.substring(7).toInt(); 
      if (angle >= 15 && angle <= 150) { 
        servo2.write(angle);
        SERIAL_LOG("🔧 Test Mode: Servo 2 moved to " + String(angle) + "°");
      }
    }
  }
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  SERIAL_LOG("\n✅ WiFi Connected!");
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "SentryGuard_Test_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      client.subscribe("board/control"); 
      SERIAL_LOG("🟢 SentryGuard [Radar Firmware] Online & Ready!");
    } else {
      delay(5000); 
    }
  }
}

// =======================================================
// ⚙️ SETUP & LOOP
// =======================================================
void setup() {
  Serial.begin(115200); // Serial Communication සක්‍රිය කර ඇත
  SERIAL_LOG("\n⚡ Booting ESP32 System...");

  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  // Ultrasonic Pins Setup
  pinMode(US1_TRIG, OUTPUT);
  pinMode(US1_ECHO, INPUT);
  pinMode(US2_TRIG, OUTPUT);
  pinMode(US2_ECHO, INPUT);
  
  // Servo Allocations
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  // Default Initial positions
  servo1.write(40); 
  servo2.write(15);
  
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
  delay(10); 
}