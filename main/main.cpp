#include "secrets.h"
#include <string>
#include "ArduinoJson.h"
#include "Recommender.h"

std::string accessToken = "";
std::string currentTrackId = "";
SpotifyRecommender recommender;

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h> 
#include <ESP32Encoder.h>
#include <TJpg_Decoder.h>

WiFiClientSecure secureClient;
void refreshSpotifyToken(); // fwd decl so fetchJson can call it on 401

bool fetchJson(std::string url, std::string method, std::string body, JsonDocument& doc, JsonDocument* filter) {
    if (accessToken == "" && url.find("token") == std::string::npos) return false;

    HTTPClient http;
    secureClient.setInsecure();
    http.begin(secureClient, String(url.c_str()));
    http.setConnectTimeout(3000); // ms - stop TLS/connect stalls from hanging loop()
    http.setTimeout(5000);        // ms - stop a slow response body from hanging loop()

    int httpCode = -1;
    if (method == "POST") {
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        httpCode = http.POST(String(body.c_str()));
    } else if (method == "PUT") {
        http.addHeader("Authorization", "Bearer " + String(accessToken.c_str()));
        http.addHeader("Content-Type", "application/json");
        httpCode = http.PUT(String(body.c_str()));
    } else {
        http.addHeader("Authorization", "Bearer " + String(accessToken.c_str()));
        httpCode = http.GET();
    }

    bool success = false;
    if (httpCode == 200) {
        DeserializationError err = filter ?
            deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter)) :
            deserializeJson(doc, http.getStream());
        success = (err == DeserializationError::Ok);
        if (!success) {
            Serial.printf("[HTTP] JSON parse error: %s for %s\n", err.c_str(), url.c_str());
        }
    } else if (httpCode == 204) {
        success = true; // e.g. PUT play with no content
    } else if (httpCode == 401) {
        Serial.println("[HTTP] 401 Unauthorized - refreshing token");
        refreshSpotifyToken();
    } else if (httpCode == 429) {
        Serial.println("[HTTP] 429 Rate limited - backing off");
    } else {
        Serial.printf("[HTTP] Request failed, code=%d for %s\n", httpCode, url.c_str());
    }

    http.end();
    return success;
}

// =================================================================
//                  ESP32-S3 HARDWARE ENVIRONMENT
// =================================================================
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCLK 12
#define TFT_DC   9
#define TFT_RST  8
#define TFT1_CS  10 
#define TFT2_CS  14 
#define ENC_CLK  4
#define ENC_DT   5
#define ENC_SW   6 

Adafruit_ILI9341 tft1 = Adafruit_ILI9341(TFT1_CS, TFT_DC, TFT_RST);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(TFT2_CS, TFT_DC, TFT_RST);
ESP32Encoder encoder;

unsigned long tokenStartTime = 0;
unsigned long lastSpotifyUpdate = 0;
unsigned long lastInputTime = 0;

int lastEncoderCount = 0;
bool pendingPlay = false;

bool tft1_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft1.height() || x >= tft1.width()) return 0;
  tft1.drawRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

void playCurrentAlbum() {
  AlbumData current = recommender.getCurrentAlbum();
  if (current.uri == "") return;
  
  // Format payload based on URI type
  std::string body;
  if (current.uri.find("spotify:track:") != std::string::npos) {
      body = "{\"uris\":[\"" + current.uri + "\"]}";
  } else {
      body = "{\"context_uri\":\"" + current.uri + "\"}";
  }
  
  JsonDocument dummy;
  fetchJson("https://api.spotify.com/v1/me/player/play", "PUT", body, dummy, nullptr);
}

void refreshSpotifyToken() {
  std::string postData = "grant_type=refresh_token&refresh_token=" + std::string(SPOTIFY_REFRESH_TOKEN) + 
                         "&client_id=" + std::string(SPOTIFY_CLIENT_ID) + 
                         "&client_secret=" + std::string(SPOTIFY_CLIENT_SECRET);
  JsonDocument doc; 
  if (fetchJson("https://accounts.spotify.com/api/token", "POST", postData, doc, nullptr)) {
      accessToken = doc["access_token"].as<std::string>();
      tokenStartTime = millis();
  } else {
      Serial.println("[ERROR] Token refresh failed - will retry next cycle");
  }
}

void updateUI(std::string title, std::string artist, std::string album, int trackNum, int totalTracks, long progress, long duration) {
  tft2.fillRect(0, 0, 284, 76, ILI9341_BLACK);
  
  tft2.setCursor(5, 5);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.print(String(title.substr(0, 16).c_str()));
  
  if (totalTracks > 0) {
      tft2.setCursor(210, 5);
      tft2.setTextColor(ILI9341_YELLOW);
      tft2.setTextSize(2);
      tft2.printf("%d/%d", trackNum, totalTracks);
  }
  
  tft2.setCursor(5, 30);
  tft2.setTextColor(ILI9341_LIGHTGREY);
  tft2.setTextSize(1);
  tft2.print(String(artist.substr(0, 20).c_str()) + " - " + String(album.substr(0, 20).c_str()));

  std::string modeLabel = recommender.isDiscoveryTurn() ? "Discover: " + recommender.topGenre : "My Library";
  tft2.setCursor(5, 45);
  tft2.setTextColor(ILI9341_MAGENTA);
  tft2.print(String(modeLabel.c_str()));

  int barWidth = 274;
  int barHeight = 10;
  float progressPercent = (duration > 0) ? (float)progress / (float)duration : 0.0f;
  
  tft2.drawRect(5, 60, barWidth, barHeight, ILI9341_WHITE);
  tft2.fillRect(5, 60, progressPercent * barWidth, barHeight, ILI9341_GREEN);
}

void drawAlbumArt(std::string url) {
  HTTPClient http;
  http.begin(secureClient, String(url.c_str()));
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  if (http.GET() == 200) {
    int len = http.getSize();
    uint8_t* buffer = (uint8_t*)malloc(len);
    if (buffer) {
      http.getStreamPtr()->readBytes(buffer, len);
      tft1.fillScreen(ILI9341_BLACK);
      TJpgDec.drawJpg(0, 0, buffer, len);
      free(buffer);
    }
  }
  http.end();
}

void updateCurrentlyPlaying() {
  if (accessToken == "") return;
  
  JsonDocument doc;
  if (fetchJson("https://api.spotify.com/v1/me/player/currently-playing", "GET", "", doc, nullptr)) {
      if (doc["item"]) {
        std::string trackId = doc["item"]["id"] | "";
        std::string imageUrl = doc["item"]["album"]["images"][1]["url"] | ""; 

        updateUI(
          doc["item"]["name"] | "Unknown", 
          doc["item"]["artists"][0]["name"] | "Unknown", 
          doc["item"]["album"]["name"] | "Unknown", 
          doc["item"]["track_number"] | 1, 
          doc["item"]["album"]["total_tracks"] | 1, 
          doc["progress_ms"] | 0, 
          doc["item"]["duration_ms"] | 1
        );

        if (trackId != currentTrackId) {
          currentTrackId = trackId;
          drawAlbumArt(imageUrl);
        }
      }
      // NOTE: a successful response with no "item" (nothing playing) is valid -
      // we simply skip the UI update rather than treating it as an error.
  }
}

void setup() {
  Serial.begin(115200);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  tft1.begin(); tft2.begin();
  tft1.setRotation(1); tft2.setRotation(1);
  tft1.fillScreen(ILI9341_BLACK); tft2.fillScreen(ILI9341_BLACK);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENC_DT, ENC_CLK);
  encoder.setCount(0);
  pinMode(ENC_SW, INPUT_PULLUP);

  TJpgDec.setJpgScale(1); 
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft1_output);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  tft2.setTextColor(ILI9341_WHITE);
  tft2.setTextSize(2);
  tft2.setCursor(10, 10);
  tft2.println("Connecting WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  refreshSpotifyToken();
  recommender.init();
}

void loop() {
  if (millis() - tokenStartTime > 3500000) refreshSpotifyToken();

  int currentEncoderCount = encoder.getCount();
  
  // 1. Debounce encoder turns to preview the screen first
  if (abs(currentEncoderCount - lastEncoderCount) >= 1) {
    bool forward = currentEncoderCount > lastEncoderCount;
    recommender.cycleNext(forward);
    
    AlbumData preview = recommender.getCurrentAlbum();
    updateUI(">>> Queuing Album <<<", preview.name, recommender.isDiscoveryTurn() ? "Discovery" : "Library", 0, 0, 0, 1);
    
    pendingPlay = true;
    lastInputTime = millis();
    lastEncoderCount = currentEncoderCount;
  }

  // 2. Play only after encoder has stopped moving for 1.5s
  if (pendingPlay && (millis() - lastInputTime > 100)) {
    playCurrentAlbum();
    pendingPlay = false;
    lastSpotifyUpdate = millis() - 3000; // Force immediate fetch
  }

  // 3. Normal polling
  if (!pendingPlay && (millis() - lastSpotifyUpdate > 3000)) {
    updateCurrentlyPlaying();
    lastSpotifyUpdate = millis();
  }
}

extern "C" void app_main() {
    initArduino();
    setup();
    while (true) {
        loop();
        vTaskDelay(1); // Yield to prevent the FreeRTOS watchdog from triggering
    }
}