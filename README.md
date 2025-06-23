# 🚀 ESP32 GPS Tracker with Neo6M and Supabase Integration

This project is a GPS tracking system built using an ESP32 and a Neo6M GPS module. It reads real-time location and speed data, and sends it to a [Supabase](https://supabase.com/) backend. If WiFi is unavailable, the data is locally buffered using SPIFFS and retried upon reconnection.

## 🛠 Features

- 📡 Real-time GPS tracking (latitude, longitude, speed)
- ☁️ Upload data to Supabase REST API
- 📶 Automatic WiFi reconnection
- 💾 Local SPIFFS buffering and retry mechanism for offline data
- 🧠 Built with TinyGPS++, SPIFFS, and HTTPClient libraries

---

## 🔧 Hardware Requirements

- ESP32 development board  
- Neo6M GPS Module  
- Jumper wires  
- Internet-connected 2.4GHz WiFi network  

**Wiring:**
| Neo6M Pin | ESP32 Pin |
|-----------|-----------|
| TX        | GPIO 16   |
| RX        | GPIO 17   |
| VCC       | 3.3V      |
| GND       | GND       |

---

## 📦 Libraries Used

- [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus)
- WiFi.h
- HTTPClient.h
- SPIFFS.h
- FS.h

Install via Arduino Library Manager where needed.

---

## 🌐 Supabase Configuration

1. Create a table `gps_store` in your Supabase project with the following columns:
   - `id` (UUID, Primary Key)
   - `lat` (float8)
   - `lng` (float8)
   - `speed` (float8)
   - `timestamp` (optional, default: now())

2. Get your Supabase REST endpoint and API key, and replace them in:
   ```cpp
   const char* SUPABASE_URL = "YOUR_SUPABASE_REST_ENDPOINT";
   const char* SUPABASE_KEY = "YOUR_SUPABASE_API_KEY";

# 📄 Code Overview

## 🛠 Setup
- Initializes serial ports and SPIFFS  
- Connects to WiFi

## 🔁 Loop
- Reads GPS data every 5 seconds
- If valid location is found:
  - Sends to Supabase if WiFi is connected
  - Saves locally to `/gps_buffer.txt` if offline
- Tries resending buffered data on each loop when online

## 🧩 Key Functions
- `connectToWifi()` – Connects to specified WiFi
- `checkWiFi()` – Auto-reconnect logic
- `sendPayloadToSupabase()` – Sends JSON payload via POST
- `saveToBuffer()` – Stores data locally on failure
- `retryBufferedUploads()` – Sends saved data when reconnected

---

## 📝 Security Note

For production, avoid hardcoding sensitive values (like WiFi and API keys).  
Use secure key storage or environment configs.

---
## 📂 SPIFFS File System

Stored data is saved in a file called `gps_buffer.txt` located in the SPIFFS file system.  
It is automatically deleted upon successful upload.

---

## 📈 Future Improvements

- Add timestamp to GPS data  
- Add LED/buzzer for status feedback  
- Integrate with map visualization dashboard  
- OTA updates

---

## 📜 License

MIT – Feel free to use and modify this project for educational or personal use.

---

## 🤝 Author

Made with ❤️ using ESP32 and open-source tools by 🎴[nischhal-hub](https://github.com/nischhal-hub)  🥋[bisha21](https://github.com/bisha21) 🎰[basanta740255](https://github.com/basanta740255).  




