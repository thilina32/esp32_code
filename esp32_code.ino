#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>


const char* ssid = "shan_dev_2";
const char* password = "888888889";

const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; // HiveMQ Cluster URL එක
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";


const int UPDATE_LED = 19;

WiFiClientSecure espClient;
PubSubClient client(espClient);


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


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Last Will (LWT) "Offline" sender
    if (client.connect("ESP32_Device_01", mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      Serial.println("Connected to HiveMQ!");
      
      // set "Online"
      client.publish("board/status", "Online", true); 
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

// FreeRTOS Task
void sensorTask(void * parameter) {
  for(;;) { // looping
    digitalWrite(UPDATE_LED, HIGH);
    Serial.println("Reading Ultrasonic Sensor...");
    // int distance = readUltrasonic(); 
    // client.publish("board/ultrasonic", String(distance).c_str());
    
    // delay for 2 seconds before next reading
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    Serial.println("Reading PIR Sensor...");
    // int motion = digitalRead(PIR_PIN);
    // client.publish("board/pir", String(motion).c_str());
    
    vTaskDelay(pdMS_TO_TICKS(3000)); 
  }
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(UPDATE_LED, OUTPUT);
  
  setup_wifi();
  
  // SSL Certificate verify remove
  espClient.setInsecure(); 
  
  // MQTT Server set
  client.setServer(mqtt_server, mqtt_port);

  //  FreeRTOS
  xTaskCreate(
    sensorTask,   // Task fun name
    "SensorTask", // name
    10000,        // Stack Size
    NULL,         // Parameters
    1,            // Priority 
    NULL          // Task Handle
  );
}

// Main Loop
void loop() {
  // only use this loop to maintain MQTT connection
  
  if (!client.connected()) {
    reconnect();
  }
  
  // Broker loop
  client.loop(); 
}