#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* ssid = "shan_dev_2";
const char* password = "888888889";

const char* mqtt_server = "65f4ab6222f64614b909988b240a72c7.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "esp32"; 
const char* mqtt_password = "Thilinakavishan32@gmail.com";

const int UPDATE_LED = 19; 

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Test කිරීම සඳහා ලොග් යවන Function එක
void MQTT_LOG(String msg) {
  if (client.connected()) {
    client.publish("board/logs", msg.c_str());
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // "==" වෙනුවට නියම C++ ක්‍රම පාවිච්චි කරමු
  // 1. Topic එක Check කිරීම (strcmp පාවිච්චි කරලා)
  bool topicMatch = (strcmp(topic, "board/update") == 0);
  
  // 2. Message එක Check කිරීම (.equals() පාවිච්චි කරලා)
  bool msgMatch = message.equals("START_OTA");

  // වෙන වෙනම ලොග් කරලා බලමු කොතනද ලෙඩේ කියලා
  MQTT_LOG("🕵️ Topic Check: " + String(topicMatch ? "PASS ✅" : "FAIL ❌"));
  MQTT_LOG("🕵️ Msg Check: " + String(msgMatch ? "PASS ✅" : "FAIL ❌"));

  if (topicMatch && msgMatch) {
    MQTT_LOG("🔥 SUCCESS: Trigger eka wada Yako!");
    digitalWrite(UPDATE_LED, HIGH);
  } else {
    MQTT_LOG("❌ FAILED: Magulak wela thiyenawa!");
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Test_01", mqtt_user, mqtt_password)) {
      client.subscribe("board/update");
      MQTT_LOG("🟢 TEST BOARD ONLINE & WAITING FOR TRIGGER...");
      
      // Connect වුන ගමන් LED එක 3 පාරක් Blink කරලා නිවනවා
      for(int i=0; i<3; i++) {
        digitalWrite(UPDATE_LED, HIGH); delay(150);
        digitalWrite(UPDATE_LED, LOW); delay(150);
      }
    } else {
      delay(5000); 
    }
  }
}

void setup() {
  pinMode(UPDATE_LED, OUTPUT);
  digitalWrite(UPDATE_LED, LOW);
  
  setup_wifi();
  espClient.setInsecure(); 
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 
}