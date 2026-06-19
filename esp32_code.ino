#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>

//#define ENABLE_MQTT_LOGS 

#ifdef ENABLE_MQTT_LOGS
  #define MQTT_LOG(msg) { if(client.connected()) { client.publish("board/logs", String(msg).c_str()); } }
#else
  #define MQTT_LOG(msg) 
#endif

const char* ssid = "shan_dev_2";
const char* password = "888888889";

const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";

const char* firmware_url = "https://raw.githubusercontent.com/thilina32/esp32_code/main/code.bin"; 

const int UPDATE_LED = 19; 

WiFiClientSecure espClient;
PubSubClient client(espClient);

// 🛠️ වෙනස 1: 'volatile' එකතු කලා (Compiler එකෙන් මේක ignore කරන එක නවත්තන්න)
volatile bool startOTA = false; 
volatile bool isUpdating = false; // Update වෙද්දි අනිත් වැඩ නවත්තන්න

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void performOTA() {
  isUpdating = true; // 🛠️ වෙනස 2: Update වෙද්දි Sensor Task එක තාවකාලිකව නවත්තනවා
  
  MQTT_LOG("Starting OTA Update from GitHub...");
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      MQTT_LOG(String("HTTP_UPDATE_FAILED Error: ") + httpUpdate.getLastErrorString());
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false; // Fail වුනොත් ආයෙත් sensors වැඩ කරන්න දෙනවා
      break;
    case HTTP_UPDATE_NO_UPDATES:
      MQTT_LOG("HTTP_UPDATE_NO_UPDATES");
      digitalWrite(UPDATE_LED, LOW); 
      isUpdating = false;
      break;
    case HTTP_UPDATE_OK:
      MQTT_LOG("HTTP_UPDATE_OK - Update Successful! Restarting..."); 
      break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // 🛠️ වෙනස 3: අගට එකතු වෙලා තියෙන invisible spaces/newlines අයින් කරනවා
  message.trim(); 
  
  MQTT_LOG(String("Message Arrived [") + topic + "]: " + message);

  // 🛠️ වෙනස 4: String(topic) වෙනුවට කෙලින්ම strcmp පාවිච්චි කරනවා (සීයට සීයක් ශුවර් කරන්න)
  if (strcmp(topic, "board/update") == 0 && message == "START_OTA") {
    MQTT_LOG("⚡ OTA Signal Verified! Preparing to Update..."); // මේක ලොග් එකට ආවොත් වැඩේ 100% හරි
    startOTA = true;
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Device_01", mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      
      MQTT_LOG("Connected to HiveMQ & Ready!");
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

void sensorTask(void * parameter) {
  for(;;) { 
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 🛠️ වෙනස 5: Update වෙන වෙලාවට විතරක් මේක යවන්නේ නෑ 
    if (!isUpdating) {
      MQTT_LOG("test..");
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000)); 
  }
}

void setup() {
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