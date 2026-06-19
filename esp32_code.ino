#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <ESP32Servo.h>

#define ENABLE_MQTT_LOGS 
#ifdef ENABLE_MQTT_LOGS
  #define MQTT_LOG(msg) { if(client.connected()) { client.publish("board/logs", String(msg).c_str()); } }
#else
  #define MQTT_LOG(msg) 
#endif

// =======================================================
// 🔌 PINS & HARDWARE SETUP
// =======================================================
const int PIR_PINS[4] = {4, 5, 6, 7}; // PIR 1, 2, 3, 4

const int SERVO1_PIN = 15;
const int SERVO2_PIN = 16;

const int US1_TRIG = 8;
const int US1_ECHO = 9;
const int US2_TRIG = 10;
const int US2_ECHO = 11;

const int RED_LED = 12;
const int BUZZER = 13;
const int UPDATE_LED = 19; 

Servo servo1;
Servo servo2;
WiFiClientSecure espClient;
PubSubClient client(espClient);

QueueHandle_t pirQueue;

volatile bool startOTA = false; 
volatile bool isUpdating = false; 
volatile bool alarmActive = false; 

// =======================================================
// 🌐 NETWORK CONFIGURATIONS
// =======================================================
const char* ssid = "shan_dev_2";
const char* password = "888888889";
const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";
const char* firmware_url = "https://raw.githubusercontent.com/thilina32/esp32_code/main/code.bin"; 

// =======================================================
// ⚡ HARDWARE INTERRUPTS (ISRs)
// =======================================================
void IRAM_ATTR isr_pir1() { int id = 1; xQueueSendFromISR(pirQueue, &id, NULL); }
void IRAM_ATTR isr_pir2() { int id = 2; xQueueSendFromISR(pirQueue, &id, NULL); }
void IRAM_ATTR isr_pir3() { int id = 3; xQueueSendFromISR(pirQueue, &id, NULL); }
void IRAM_ATTR isr_pir4() { int id = 4; xQueueSendFromISR(pirQueue, &id, NULL); }

// =======================================================
// 📏 ULTRASONIC READ FUNCTION
// =======================================================
int readUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 3000); 
  if (duration == 0) return 999; 
  
  int distance = duration * 0.034 / 2;
  return distance;
}

// =======================================================
// 🚨 TRIGGER ALARM FUNCTION
// =======================================================
void triggerAlarm(int distance, int angle) {
  alarmActive = true;
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER, HIGH);
  
  String payload = "{\"distance\":" + String(distance) + ",\"angle\":" + String(angle) + "}";
  MQTT_LOG("🐘 ELEPHANT DETECTED! " + payload);
  
  if(client.connected()) {
    client.publish("board/alert", payload.c_str());
  }
}

// =======================================================
// 📡 MQTT CALLBACK & CONNECTION FUNCTIONS
// =======================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim(); 

  if (strcmp(topic, "board/update") == 0 && message.equals("START_OTA")) {
    MQTT_LOG("🔥 OTA Signal Verified! Preparing to Update...");
    startOTA = true;
  }
  
  if (strcmp(topic, "board/control") == 0 && message.equals("ALARM_OFF")) {
    alarmActive = false;
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    xQueueReset(pirQueue); 
    
    servo1.write(90);
    servo2.write(90);
    MQTT_LOG("🛡️ Alarm Reset. Resuming Sensor Monitoring...");
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
    String clientId = "TuskerGuard_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, "board/status", 1, true, "Offline")) {
      client.publish("board/status", "Online", true); 
      client.subscribe("board/update");
      client.subscribe("board/control"); 
      MQTT_LOG("🟢 TuskerGuard Online & Ready!");
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}

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
// 🎯 RADAR SWEEP FUNCTION
// =======================================================
bool sweepAndSearch(Servo &servo, int targetAngle, int trigPin, int echoPin) {
  int startAngle = 90; 
  int step = (targetAngle > startAngle) ? 1 : -1; 
  
  for (int currentAngle = startAngle; currentAngle != targetAngle + step; currentAngle += step) {
    if (isUpdating || alarmActive) return false; 

    servo.write(currentAngle);
    vTaskDelay(pdMS_TO_TICKS(30)); // හැරෙන වේගය
    
    int distance = readUltrasonic(trigPin, echoPin);
    
    if (distance <= 20) {
      triggerAlarm(distance, currentAngle); 
      return true; 
    }
  }
  return false; 
}

// =======================================================
// 🔄 MAIN SENSOR TASK
// =======================================================
void sensorTask(void * parameter) {
  int triggeredPirId; 
  
  for(;;) { 
    if (isUpdating || alarmActive) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue; 
    }

    if (xQueueReceive(pirQueue, &triggeredPirId, 0) == pdTRUE) {
      
      switch(triggeredPirId) {
        case 1:
          MQTT_LOG("👀 PIR 1 Triggered! Radar sweeping to 50°...");
          sweepAndSearch(servo1, 50, US1_TRIG, US1_ECHO);
          break;
          
        case 2:
          MQTT_LOG("👀 PIR 2 Triggered! Radar sweeping to 180°...");
          sweepAndSearch(servo1, 180, US1_TRIG, US1_ECHO);
          break;
          
        case 3:
          MQTT_LOG("👀 PIR 3 Triggered! Radar sweeping to 50°...");
          sweepAndSearch(servo2, 50, US2_TRIG, US2_ECHO);
          break;
          
        case 4:
          MQTT_LOG("👀 PIR 4 Triggered! Radar sweeping to 180°...");
          sweepAndSearch(servo2, 180, US2_TRIG, US2_ECHO);
          break;
      }
      
      if (!alarmActive) {
        servo1.write(90);
        servo2.write(90);
        vTaskDelay(pdMS_TO_TICKS(500)); 
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50)); 
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
  
  pinMode(US1_TRIG, OUTPUT); pinMode(US1_ECHO, INPUT);
  pinMode(US2_TRIG, OUTPUT); pinMode(US2_ECHO, INPUT);
  
  for(int i=0; i<4; i++) pinMode(PIR_PINS[i], INPUT);

  pirQueue = xQueueCreate(10, sizeof(int));

  attachInterrupt(digitalPinToInterrupt(PIR_PINS[0]), isr_pir1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIR_PINS[1]), isr_pir2, RISING);
  attachInterrupt(digitalPinToInterrupt(PIR_PINS[2]), isr_pir3, RISING);
  attachInterrupt(digitalPinToInterrupt(PIR_PINS[3]), isr_pir4, RISING);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  servo1.write(90); servo2.write(90);
  
  setup_wifi(); 
  
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  xTaskCreate(sensorTask, "SensorTask", 10000, NULL, 1, NULL);
}

void loop() {
  if (startOTA) { startOTA = false; performOTA(); }
  if (!client.connected()) reconnect();
  client.loop(); 
}