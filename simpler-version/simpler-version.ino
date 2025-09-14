#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <PubSubClient.h>

// =========================
// GPS and WiFi Config
// =========================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

const char* ssid = ".........._2.4";
const char* password = "135B@Santa$$246";
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "emqx/esp32";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// =========================
// Objects
// =========================
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 3000; // 3 seconds

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Initializing...");

  connectToWifi();
  connectToMQTT();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

// =========================
// Main Loop
// =========================
void loop() {
  // Feed GPS parser
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Publish every 3 seconds if GPS has data
  if (millis() - lastPublish >= publishInterval) {
    if (gps.location.isValid()) {
      float lat = gps.location.lat();
      float lng = gps.location.lng();
      float speedKmh = gps.speed.kmph();
      float heading = gps.course.deg();

      String payload = String(lat, 6) + "," +
                       String(lng, 6) + "," +
                       String(speedKmh, 2) + "," +
                       String(heading, 2);

      if (client.connected()) {
        if (client.publish(topic, payload.c_str())) {
          Serial.println("Published: " + payload);
        } else {
          Serial.println("Publish failed!");
        }
      } else {
        attemptMQTTReconnect();
      }
    } else {
      Serial.println("No valid GPS data yet...");
    }

    lastPublish = millis();
  }

  // Keep MQTT connection alive
  if (!client.connected()) {
    attemptMQTTReconnect();
  }
  client.loop();
}

// =========================
// WiFi Functions
// =========================
void connectToWifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

// =========================
// MQTT Functions
// =========================
void connectToMQTT() {
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    Serial.println("Connecting to MQTT...");

    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("MQTT connected!");
      client.publish(topic, "ESP32 GPS Tracker connected");
    } else {
      Serial.printf("Failed, state: %d\n", client.state());
      delay(2000);
    }
  }
}

bool attemptMQTTReconnect() {
  String client_id = "esp32-client-" + String(WiFi.macAddress());
  if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
    Serial.println("MQTT reconnected!");
    return true;
  }
  return false;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
}
