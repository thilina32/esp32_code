#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>

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
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

void performOTA() {
  Serial.println("Starting OTA Update from GitHub...");
  
  // Update එක පටන් ගන්නකොටම LED එක පත්තු කරනවා
  digitalWrite(UPDATE_LED, HIGH); 
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure(); 

  t_httpUpdate_return ret = httpUpdate.update(otaClient, firmware_url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      // Update එක Fail වුනොත් LED එක නිවනවා
      digitalWrite(UPDATE_LED, LOW); 
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      // අලුත් Update එකක් නැත්නම් LED එක නිවනවා
      digitalWrite(UPDATE_LED, LOW); 
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK - Update Successful! Restarting..."); 
      // Update එක හරි ගියොත් ESP එක Restart වෙනවා, එතකොට LED එක Auto OFF වෙනවා
      break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("MQTT Message Arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "board/update" && message == "START_OTA") {
    startOTA = true;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect("ESP32_Device_01", mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      Serial.println("Connected to HiveMQ!");
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

void sensorTask(void * parameter) {
  for(;;) { 
    //Serial.println("Reading Ultrasonic Sensor...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    Serial.println("test..");
    vTaskDelay(pdMS_TO_TICKS(3000)); 
  }
}

void setup() {
  Serial.begin(115200);
  
  // LED Pin එක Output එකක් විදියට හදලා මුලින්ම නිවලා තියනවා
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