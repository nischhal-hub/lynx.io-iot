#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>

// =========================
// GPS and WiFi Config
// =========================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

const char* ssid = ".........._2.4";
const char* password = "135B@Santa$$246";

const char* SUPABASE_URL = "https://qmjnedgtawyiovixjwnz.supabase.co/rest/v1/gps_store?apikey=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InFtam5lZGd0YXd5aW92aXhqd256Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTAwODYxNjQsImV4cCI6MjA2NTY2MjE2NH0.kza3B5Oi2T_57cj8yBcYRi4zPt8fUrO_X7mRXnNvtUg";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InFtam5lZGd0YXd5aW92aXhqd256Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTAwODYxNjQsImV4cCI6MjA2NTY2MjE2NH0.kza3B5Oi2T_57cj8yBcYRi4zPt8fUrO_X7mRXnNvtUg";

// =========================
// Objects
// =========================
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Initializing...");

  // Start SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
  }

  connectToWifi();

  // Optional: Enable auto reconnect
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

// =========================
// Main Loop
// =========================
void loop() {
  checkWiFi();

  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());

      if (gps.location.isUpdated() && gps.location.isValid()) {
        float lat = gps.location.lat();
        float lng = gps.location.lng();
        float speed = gps.speed.kmph();

        

        String payload = "{\"lat\": " + String(lat, 6) +
                         ", \"lng\": " + String(lng, 6) +
                         ", \"speed\": " + String(speed, 2) + "}";

        Serial.println("Data: " + payload);

        if (WiFi.status() == WL_CONNECTED) {
          retryBufferedUploads(); // Try to send saved ones first
          sendPayloadToSupabase(payload);
        } else {
          saveToBuffer(payload);
        }
      }
    }
  }

  gpsSerial.flush();
  Serial.println("-------------------------------");
  delay(5000);
}

// =========================
// WiFi Connection Function
// =========================
void connectToWifi() {
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
      delay(1000);
      Serial.printf("Reconnecting (%d)...\n", retries + 1);
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected. IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("Reconnection failed.");
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
  file.println(payload);
  file.close();
  Serial.println("Saved to local buffer.");
}

// =========================
// Retry Uploads from Buffer
// =========================
void retryBufferedUploads() {
  File file = SPIFFS.open("/gps_buffer.txt", FILE_READ);
  if (!file || file.size() == 0) {
    file.close();
    return;
  }

  Serial.println("Re-uploading saved data...");

  bool allSent = true;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 5) {
      if (!sendPayloadToSupabase(line)) {
        Serial.println("Failed to resend: " + line);
        allSent = false;
        break;
      }
    }
  }

  file.close();

  if (allSent) {
    SPIFFS.remove("/gps_buffer.txt");
    Serial.println("All buffered data uploaded. File cleared.");
  } else {
    Serial.println("Upload interrupted. Will retry later.");
  }
}

// =========================
// Send to Supabase Function
// =========================
bool sendPayloadToSupabase(String payload) {
  HTTPClient http;
  http.begin(SUPABASE_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer", "return=representation");
  http.setTimeout(5000); // 5 seconds timeout


  int httpCode = http.POST(payload);
  String response = http.getString();

  Serial.printf("HTTP Code: %d\n", httpCode);
  Serial.println("Response: " + response);

  http.end();

  return httpCode >= 200 && httpCode < 300;
}
