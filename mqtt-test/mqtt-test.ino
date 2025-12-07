//migrate from http to mqtt
//test code for mqtt portocol
#include <WiFi.h>
#include <PubSubClient.h>


const char* ssid = ".........._2.4";
const char* password = "135B@Santa$$246";

const char *mqtt_broker = "broker.emqx.io";
const char *topic = "esp32/loc";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Initializing....");
  connectWifi();
  connectToMTQQ();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();
    client.publish(topic, "Hi, I'm ESP32 ^^",true);
  }
}

void connectWifi(){
    Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(1000);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void connectToMTQQ(){
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while(!client.connected()){
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("client connecting");
    if(client.connect(client_id.c_str(),mqtt_username,mqtt_password)){
      Serial.println("Connected");
    }else{
      Serial.print("Failed with state");
      Serial.print(client.state());
      delay(2000);
    }
  }

  client.publish(topic, "Hi, I'm ESP32 ^^");
  client.subscribe(topic);
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}
