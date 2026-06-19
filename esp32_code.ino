#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>

// =======================================================
// 🎚️ LOGGING SWITCH (ON/OFF)
// මේකෙන් තමයි Logs ඔක්කොම පාලනය කරන්නේ.
// මේ පේලිය කමෙන්ට් කලොත් (//#define ENABLE_MQTT_LOGS) 
// Logs යවන කෝඩ් එක bin එකට compile වෙන්නේම නෑ. 
// =======================================================
#define ENABLE_MQTT_LOGS 

#ifdef ENABLE_MQTT_LOGS
  // Switch එක ON නම්, මේකෙන් 'board/logs' topic එකට මැසේජ් එක යවනවා
  #define MQTT_LOG(msg) { if(client.connected()) { client.publish("board/logs", String(msg).c_str()); } }
#else
  // Switch එක OFF නම්, Compiler එක මේ කෝඩ් කෑල්ල සම්පූර්ණයෙන්ම හිස් කරනවා.
  #define MQTT_LOG(msg) 
#endif
// =======================================================

const char* ssid = "shan_dev_2";
const char* password = "888888889";

const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";

const char* firmware_url = "https://raw.githubusercontent.com/thilina32/esp32_code/main/code.bin"; 

// Update LED එක සඳහා Pin එක
const int UPDATE_LED = 19; 

WiFiClientSecure espClient;
PubSubClient client(espClient);

bool startOTA = false; 

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  // මෙතනදී තාම MQTT Connect වෙලා නැති නිසා මුකුත් ලොග් යවන්නේ නෑ
}

void performOTA() {
  MQTT_LOG("Starting OTA Update from GitHub...");
  
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      // Error එක String එකක් විදියට හදලා යවනවා
      MQTT_LOG(String("HTTP_UPDATE_FAILED Error: ") + httpUpdate.getLastErrorString());
      digitalWrite(UPDATE_LED, LOW); 
      break;
    case HTTP_UPDATE_NO_UPDATES:
      MQTT_LOG("HTTP_UPDATE_NO_UPDATES");
      digitalWrite(UPDATE_LED, LOW); 
      break;
    case HTTP_UPDATE_OK:
      MQTT_LOG("HTTP_UPDATE_OK - Update Successful! Restarting..."); 
      break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  MQTT_LOG(String("Message Arrived [") + topic + "]: " + message);

  if (String(topic) == "board/update" && message == "START_OTA") {
    startOTA = true;
  }
}

void reconnect() {
  while (!client.connected()) {
    
    if (client.connect("ESP32_Device_01", mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      
      // කනෙක්ට් වුන ගමන් ලොග් එකක් යවනවා
      MQTT_LOG("Connected to HiveMQ & Ready!");
      
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

void sensorTask(void * parameter) {
  for(;;) { 
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    MQTT_LOG("test..");
    
    vTaskDelay(pdMS_TO_TICKS(3000)); 
  }
}

void setup() {
  // Serial.begin(115200); එක සම්පූර්ණයෙන්ම අයින් කලා
  
  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  setup_wifi();
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

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
  if (startOTA) {
    startOTA = false;
    performOTA();
  }

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop(); 
}