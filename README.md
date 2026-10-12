# Spotify Desktop Music Box

A standalone hardware music controller and display unit powered by an ESP32-S3, featuring dual screens for live album art and track metadata, physical playback controls, and Spotify Connect integration

---

### Key Features

* **Spotify Connect / API Integration:** Direct playback control, queue inspection, and live session syncing via the Spotify Web API and CSpot
* **Dual Display Output:** Dedicated screens for rendering high-res album artwork alongside real-time track title, artist, progress bar, and playback state
* **Physical Hardware Controls:** Rotary encoders and tactile switches for volume, track scrubbing, play/pause, and skip
* Connects directly to local network

---

### Tech Stack

* **Firmware:** C++ (ESP-IDF / Arduino core)
* **Libraries:** TFT_eSPI / LovyanGFX, ArduinoJson, CSpot
* **Networking:** ESP32 WiFiClientSecure, HTTP REST, OAuth 2.0 Web API
