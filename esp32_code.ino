#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>

// =======================================================
// 🎚️ LOGGING SWITCH (ON/OFF)
// Logs ඕනේ නැත්නම් මේක කමෙන්ට් කරන්න (//#define ENABLE_MQTT_LOGS)
// =======================================================
#define ENABLE_MQTT_LOGS 

#ifdef ENABLE_MQTT_LOGS
  #define MQTT_LOG(msg) { if(client.connected()) { client.publish("board/logs", String(msg).c_str()); } }
#else
  #define MQTT_LOG(msg) 
#endif

// =======================================================
// 🛠️ CONFIGURATIONS
// =======================================================
const char* ssid = "shan_dev_2";
const char* password = "888888889";

const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";

// GitHub Raw URL (code.bin)
const char* firmware_url = "https://raw.githubusercontent.com/thilina32/esp32_code/main/code.bin"; 

const int UPDATE_LED = 19; 

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Flags
volatile bool startOTA = false; 
volatile bool isUpdating = false; 

// =======================================================
// 📡 FUNCTIONS
// =======================================================

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void performOTA() {
  isUpdating = true; // Sensor Data යවන එක නවත්තනවා
  
  MQTT_LOG("🚀 Starting OTA Update from GitHub...");
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); // GitHub HTTPS සඳහා SSL Bypass කිරීම

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      MQTT_LOG(String("❌ HTTP_UPDATE_FAILED Error: ") + httpUpdate.getLastErrorString());
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false; // ආයෙත් වැඩ පටන් ගන්න දෙනවා
      break;
    case HTTP_UPDATE_NO_UPDATES:
      MQTT_LOG("⚠️ HTTP_UPDATE_NO_UPDATES: Current version is up to date.");
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false;
      break;
    case HTTP_UPDATE_OK:
      MQTT_LOG("✅ HTTP_UPDATE_OK: Update Successful! Restarting now..."); 
      // Restart වෙන නිසා වෙන මොකුත් කරන්න ඕනේ නෑ
      break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim(); // අනවශ්‍ය හිස්තැන් අයින් කිරීම

  // අපි අල්ලගත්ත නියම C++ String Check ක්‍රමය
  bool topicMatch = (strcmp(topic, "board/update") == 0);
  bool msgMatch = message.equals("START_OTA");

  if (topicMatch && msgMatch) {
    MQTT_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
}

void reconnect() {
  while (!client.connected()) {
    // Unique Client ID එකක් හදනවා Connection කඩන එක නවත්තන්න
    String clientId = "ESP32_Board_" + String(random(0xffff), HEX);
    
    // LWT (Last Will) එක්ක Connect වෙනවා
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      
      MQTT_LOG("🟢 Connected to HiveMQ & Ready!");
      
      // Connect වුන බව පෙන්නන්න පොඩි blink එකක් දෙනවා
      digitalWrite(UPDATE_LED, HIGH); delay(200);
      digitalWrite(UPDATE_LED, LOW);
      
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

// =======================================================
// 🔄 FREERTOS TASKS
// =======================================================

void sensorTask(void * parameter) {
  for(;;) { 
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Update වෙන්නේ නැති වෙලාවට විතරක් ලොග් යවනවා
    if (!isUpdating) {
      MQTT_LOG("test..");
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000)); 
  }
}

// =======================================================
// ⚙️ SETUP & LOOP
// =======================================================

void setup() {
  // Serial.begin සම්පූර්ණයෙන්ම අයින් කරලා තියෙන්නේ (Hardware බරක් නෑ)
  
  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  setup_wifi();
  
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Sensor Task එක FreeRTOS හරහා දුවන්න දෙනවා
  xTaskCreate(
    sensorTask,   
    "SensorTask", 
    10000,        
    NULL,         
    1,            
    NULL          
  );
}

void loop() {
  // OTA Flag එක On වුනොත් Update එක පටන් ගන්නවා
  if (startOTA) {
    startOTA = false;
    performOTA();
  }

  // MQTT Connection එක පවත්වාගෙන යනවා
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop(); 
}