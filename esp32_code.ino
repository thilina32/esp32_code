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

unsigned long lastMemCheck = 0;
bool wasConnected = false; // MQTT connection එකේ වෙනස අඳුරගන්න

// =======================================================
// 🛠️ MQTT ERROR TRANSLATOR
// =======================================================
// MQTT Error අංකය වචන බවට පත් කරන function එක
String getMqttErrorString(int state) {
  switch (state) {
    case -4: return "MQTT_CONNECTION_TIMEOUT (Broker ට කතා කරන්න බැරි වුණා)";
    case -3: return "MQTT_CONNECTION_LOST (මැදදි කනෙක්ෂන් එක කැඩිලා ගියා)";
    case -2: return "MQTT_CONNECT_FAILED (Network අවුලක් නිසා සම්බන්ධ වුණේ නෑ)";
    case -1: return "MQTT_DISCONNECTED (සාමාන්‍ය ලෙස විසන්ධි වුණා)";
    case 0:  return "MQTT_CONNECTED (සම්බන්ධයි)";
    case 1:  return "MQTT_CONNECT_BAD_PROTOCOL (Protocol එක වැරදියි)";
    case 2:  return "MQTT_CONNECT_BAD_CLIENT_ID (Client ID එක වෙන කෙනෙක් පාවිච්චි කරනවා)";
    case 3:  return "MQTT_CONNECT_UNAVAILABLE (Broker වැඩ කරන්නේ නෑ)";
    case 4:  return "MQTT_CONNECT_BAD_CREDENTIALS (Username/Password වැරදියි)";
    case 5:  return "MQTT_CONNECT_UNAUTHORIZED (ඇතුල්වෙන්න අවසර නෑ)";
    default: return "UNKNOWN ERROR (" + String(state) + ")";
  }
}

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

void performOTA() {
  isUpdating = true; 
  Serial.println("[OTA] 🚀 Starting OTA Update from GitHub...");
  
  delay(500); 
  client.disconnect();
  espClient.stop();
  delay(500); 

  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA_ERROR] ❌ HTTP_UPDATE_FAILED Error: %s\n", httpUpdate.getLastErrorString().c_str());
      delay(1000); ESP.restart(); break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] ⚠️ HTTP_UPDATE_NO_UPDATES");
      delay(1000); ESP.restart(); break;
    case HTTP_UPDATE_OK:
      Serial.println("[OTA] ✅ HTTP_UPDATE_OK!"); break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim(); 

  Serial.printf("[MQTT_RX] Topic: %s | Message: %s\n", topic, message.c_str());

  if (strcmp(topic, "board/update") == 0 && message.equals("START_OTA")) {
    startOTA = true;
  }
  else if (strcmp(topic, "board/control") == 0) {
    if (message.startsWith("SERVO1:")) {
      int angle = message.substring(7).toInt();
      if (angle >= 0 && angle <= 180) servo1.write(angle);
    } 
    else if (message.startsWith("SERVO2:")) {
      int angle = message.substring(7).toInt(); 
      if (angle >= 0 && angle <= 180) servo2.write(angle);
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.printf("\n[WIFI] 🔄 Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[WIFI] ✅ WiFi Connected!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("[MQTT] 🔄 Attempting connection...");
    
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("[MQTT_ERROR] ❌ WiFi disconnected!");
      setup_wifi();
    }
    
    // 🛑 අලුත් වෙනස්කම: Random වෙනුවට MAC Address එක පාවිච්චි කිරීම (Duplicate Client IDs නවත්වන්න)
    String clientId = "SentryGuard_" + WiFi.macAddress();
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      Serial.println("[MQTT] ✅ Connected!");
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      client.subscribe("board/control"); 
      wasConnected = true; // Status එක update කරනවා
    } else {
      // 🛑 අලුත් වෙනස්කම: Error එක හරියටම පෙන්නීම
      int errorState = client.state();
      Serial.printf("[MQTT_ERROR] ❌ Connection failed! Error: %s\n", getMqttErrorString(errorState).c_str());
      Serial.println("[MQTT] ⏳ Retrying in 5 seconds...");
      delay(5000); 
    }
  }
}

void setup() {
  Serial.begin(115200); 
  delay(1000);
  
  pinMode(UPDATE_LED, OUTPUT);
  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50); servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400); servo2.attach(SERVO2_PIN, 500, 2400);
  
  setup_wifi(); 
  
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // 🛑 අලුත් වෙනස්කම: Keep-Alive Time එක වැඩි කිරීම (තත්පර 60ක්)
  client.setKeepAlive(60); 
}

void loop() {
  if (startOTA) { startOTA = false; performOTA(); }
  if (isUpdating) return; 

  bool isConnected = client.connected();

  // 🛑 අලුත් වෙනස්කම: එකපාරටම disconnect වුණොත් හරියටම මොහොත අල්ලගෙන Error එක පෙන්නනවා
  if (wasConnected && !isConnected) {
    int errorState = client.state();
    Serial.printf("\n[MQTT_DROP] ⚠️ ALERT: Connection suddenly dropped!\n");
    Serial.printf("[MQTT_DROP] 🔍 Reason: %s\n\n", getMqttErrorString(errorState).c_str());
    wasConnected = false;
  }

  if (!isConnected) {
    reconnect();
  }
  
  client.loop(); 
  
  if (millis() - lastMemCheck > 30000) {
    printSystemStatus();
    lastMemCheck = millis();
  }
  
  delay(10); 
}