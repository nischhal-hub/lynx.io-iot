//main code
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <PubSubClient.h>  // Fixed: Added missing include

// =========================
// GPS and WiFi Config
// =========================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

const char* ssid = ".........._2.4";
const char* password = "135B@Santa$$246";
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "vehicle/location";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

unsigned long lastPublish = 0;
unsigned long publishInterval = 4000; // Default 4 sec
double lastLat = 0.0, lastLng = 0.0;
double lastHeading = 0.0;
double distanceThreshold = 10;  // meters
double headingThreshold = 15;   // degrees

WiFiClient espClient;
PubSubClient client(espClient);

// =========================
// Objects
// =========================
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// =========================
// Helper Functions
// =========================
double radians(double degrees) {
  return degrees * PI / 180.0;
}

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000; // Radius of Earth in meters
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

double calculateHeadingChange(double h1, double h2) {
  double diff = abs(h2 - h1);
  if(diff > 180) diff = 360 - diff; // Wrap-around
  return diff;
}

void adjustInterval(double speedKmh) {
  if(speedKmh > 50) publishInterval = 2000;
  else if(speedKmh > 10) publishInterval = 4000;
  else if(speedKmh > 0) publishInterval = 8000;
  else publishInterval = 30000;
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Initializing...");

  // Start SPIFFS with proper error handling
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return; // Stop execution if SPIFFS fails
  }

  connectToWifi();
  connectToMQTT(); // Fixed: Corrected function name

  // Optional: Enable auto reconnect
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

// =========================
// Main Loop
// =========================
void loop() {
  checkWiFi();

  // Read GPS data
  while(gpsSerial.available() > 0){
    gps.encode(gpsSerial.read());
  }

  // Fixed: Moved GPS variable declarations outside the if block
  static float lat = 0, lng = 0, speedKmh = 0, heading = 0;
  bool gpsDataValid = false;

  if(gps.location.isUpdated() && gps.location.isValid() && gps.course.isUpdated()){
    lat = gps.location.lat();
    lng = gps.location.lng();
    speedKmh = gps.speed.kmph();
    heading = gps.course.deg();
    gpsDataValid = true;
    
    Serial.printf("GPS Data: Lat=%.6f, Lng=%.6f, Speed=%.2f km/h, Heading=%.2f°\n", 
                  lat, lng, speedKmh, heading);
  }

  // Only process if we have valid GPS data
  if(gpsDataValid) {
    // Adjust interval based on speed
    adjustInterval(speedKmh);

    // Calculate distance and heading change
    double dist = calculateDistance(lastLat, lastLng, lat, lng);
    double headingChange = calculateHeadingChange(lastHeading, heading);
    
    Serial.printf("Distance moved: %.2fm, Heading change: %.2f°\n", dist, headingChange);

    // Determine if we should publish
    bool shouldPublish = false;
    if(dist >= distanceThreshold) {
      shouldPublish = true;
      Serial.println("Publishing: Distance threshold reached");
    }
    else if(speedKmh > 0 && millis() - lastPublish >= publishInterval) {
      shouldPublish = true;
      Serial.println("Publishing: Time interval reached");
    }
    else if(headingChange >= headingThreshold) {
      shouldPublish = true;
      Serial.println("Publishing: Heading change threshold reached");
    }

    if(shouldPublish) {
      String payload = String(lat, 6) + "," + String(lng, 6) + "," + String(speedKmh, 2) + "," + String(heading, 2);
      
      // Fixed: Simplified MQTT publishing - removed duplicate code
      if(client.connected()){
        if(client.publish(topic, payload.c_str())){
          Serial.println("Published: " + payload);
        } else {
          Serial.println("Publish failed, saving to buffer");
          saveToBuffer(payload);
        }
      } else {
        Serial.println("MQTT disconnected, saving to buffer");
        saveToBuffer(payload);
        // Non-blocking reconnection attempt
        if(!attemptMQTTReconnect()) {
          Serial.println("MQTT reconnection failed");
        }
      }

      // Update last known values
      lastLat = lat;
      lastLng = lng;
      lastHeading = heading;
      lastPublish = millis();
    }
  }

  // Retry buffered uploads if connected
  if(WiFi.status() == WL_CONNECTED && client.connected()){
    retryBufferedUploads();
  }

  // Non-blocking MQTT connection maintenance
  if (!client.connected()) {
    attemptMQTTReconnect();
  }

  client.loop();
  delay(100); // Small delay to prevent overwhelming the loop
}

// =========================
// WiFi Connection Function
// =========================
void connectToWifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) { // Increased retries
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect to WiFi after 20 attempts");
  }
}

// =========================
// WiFi Reconnection Check
// =========================
void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
      delay(500);
      Serial.printf("Reconnecting (%d)...\n", retries + 1);
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected. IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("WiFi reconnection failed after 10 attempts");
    }
  }
}

// =========================
// Save to Local File
// =========================
void saveToBuffer(String payload) {
  File file = SPIFFS.open("/gps_buffer.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open buffer file for writing");
    return;
  }
  
  if(file.println(payload)) {
    Serial.println("Saved to local buffer: " + payload);
  } else {
    Serial.println("Failed to write to buffer file");
  }
  
  file.close();
}

// =========================
// Retry Uploads from Buffer
// =========================
void retryBufferedUploads() {
  if (!SPIFFS.exists("/gps_buffer.txt")) return;

  File file = SPIFFS.open("/gps_buffer.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open buffer file for reading");
    return;
  }
  
  if(file.size() == 0) {
    file.close();
    return;
  }

  Serial.println("Re-uploading buffered MQTT data...");

  // Temporary file to store unsent lines
  File tempFile = SPIFFS.open("/gps_buffer_tmp.txt", FILE_WRITE);
  if (!tempFile) {
    Serial.println("Failed to create temp file for buffering");
    file.close();
    return;
  }

  int successCount = 0, failCount = 0;
  
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 5) {
      // Publish via MQTT
      if (client.publish(topic, line.c_str())) {
        Serial.println("Successfully resent: " + line);
        successCount++;
      } else {
        // If publish fails, save line to temp file for next retry
        tempFile.println(line);
        Serial.println("Failed to resend, kept in buffer: " + line);
        failCount++;
      }
    }
  }

  file.close();
  tempFile.close();

  // Remove old buffer and rename temp file
  if(!SPIFFS.remove("/gps_buffer.txt")) {
    Serial.println("Warning: Failed to remove old buffer file");
  }
  
  if (SPIFFS.exists("/gps_buffer_tmp.txt")) {
    if(SPIFFS.rename("/gps_buffer_tmp.txt", "/gps_buffer.txt")) {
      Serial.printf("Buffer updated: %d sent, %d remaining\n", successCount, failCount);
    } else {
      Serial.println("Error: Failed to rename temp buffer file");
    }
  } else {
    Serial.printf("All buffered data uploaded successfully! (%d messages)\n", successCount);
  }
}

// =========================
// MQTT Connection Functions
// =========================
void connectToMQTT(){ // Fixed: Corrected function name
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  
  int attempts = 0;
  while(!client.connected() && attempts < 5){
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("MQTT client connecting (attempt %d)...\n", attempts + 1);
    
    if(client.connect(client_id.c_str(), mqtt_username, mqtt_password)){
      Serial.println("MQTT Connected successfully!");
      client.publish(topic, "Hi, I'm ESP32 GPS Tracker ^^", true);
      client.subscribe(topic);
    } else {
      Serial.printf("MQTT connection failed with state: %d\n", client.state());
      attempts++;
      if(attempts < 5) delay(2000);
    }
  }
  
  if(!client.connected()) {
    Serial.println("Failed to connect to MQTT after 5 attempts");
  }
}

// Fixed: Added non-blocking MQTT reconnection
bool attemptMQTTReconnect() {
  static unsigned long lastReconnectAttempt = 0;
  
  if (millis() - lastReconnectAttempt > 5000) { // Try every 5 seconds
    lastReconnectAttempt = millis();
    
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.println("Attempting MQTT reconnection...");
    
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("MQTT reconnected!");
      client.subscribe(topic);
      return true;
    } else {
      Serial.printf("MQTT reconnection failed, state: %d\n", client.state());
    }
  }
  return false;
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}
