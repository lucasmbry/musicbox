// Set to 1 to compile for your PC. Set to 0 to flash to the ESP32-S3.
#define RUN_ON_PC 1 

#include "secrets.h"
#include <string>
#include "ArduinoJson.h"
#include "Recommender.h"

std::string accessToken = "";
std::string currentTrackId = "";
SpotifyRecommender recommender;

// =================================================================
//                 STREAM-PARSING HTTP ABSTRACTION
// =================================================================
#if RUN_ON_PC == 1
    #include <iostream>
    #include <fstream>
    #include <cstdlib>
    #include <chrono>
    #include <opencv2/opencv.hpp>

    unsigned long lastTokenRefresh = 0; // fwd decl of usage, defined in PC section below
    void refreshSpotifyTokenPC(); // fwd decl so fetchJson can call it on 401

    unsigned long sysTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // Returns true only on a successful (200) response with parseable JSON.
    // On 401 it triggers a token refresh; on 429 it just fails (caller can retry later).
    bool fetchJson(std::string url, std::string method, std::string body, JsonDocument& doc, JsonDocument* filter) {
        if (accessToken == "" && url.find("token") == std::string::npos) return false;

        std::string cmd;
        // -s silent, -S show errors, --max-time / --connect-timeout stop curl from hanging forever,
        // -w writes the HTTP status code on its own line at the end of the response file.
        std::string common = " -s -S --max-time 5 --connect-timeout 3 -w \"\\n%{http_code}\" ";

        if (method == "POST") {
            cmd = "curl" + common + "-X POST -H \"Content-Type: application/x-www-form-urlencoded\" -d \"" + body + "\" \"" + url + "\" > response.json 2>curl_err.txt";
        } else if (method == "PUT") {
            cmd = "curl" + common + "-X PUT -H \"Content-Type: application/json\" -H \"Authorization: Bearer " + accessToken + "\" -d '" + body + "' \"" + url + "\" > response.json 2>curl_err.txt";
        } else {
            cmd = "curl" + common + "-H \"Authorization: Bearer " + accessToken + "\" \"" + url + "\" > response.json 2>curl_err.txt";
        }

        int ret = system(cmd.c_str());
        if (ret != 0) {
            std::cerr << "[HTTP] curl failed to execute (ret=" << ret << ") for " << url << std::endl;
            return false;
        }

        // Read the whole file, split off the trailing status code we appended with -w.
        std::ifstream file("response.json");
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        size_t lastNewline = content.find_last_of('\n');
        if (lastNewline == std::string::npos) {
            std::cerr << "[HTTP] Malformed response (no status line) for " << url << std::endl;
            return false;
        }

        std::string jsonPart = content.substr(0, lastNewline);
        int httpCode = 0;
        try {
            httpCode = std::stoi(content.substr(lastNewline + 1));
        } catch (...) {
            httpCode = 0;
        }

        if (httpCode == 401) {
            std::cerr << "[HTTP] 401 Unauthorized - refreshing token" << std::endl;
            refreshSpotifyTokenPC();
            return false;
        }
        if (httpCode == 429) {
            std::cerr << "[HTTP] 429 Rate limited - backing off" << std::endl;
            return false;
        }
        if (httpCode < 200 || httpCode >= 300) {
            std::cerr << "[HTTP] Unexpected status " << httpCode << " for " << url << std::endl;
            return false;
        }
        if (jsonPart.empty()) {
            // 200/204 with no body (e.g. PUT play sometimes returns empty) - not an error.
            return true;
        }

        std::istringstream jsonStream(jsonPart);
        DeserializationError err = filter ? deserializeJson(doc, jsonStream, DeserializationOption::Filter(*filter)) : deserializeJson(doc, jsonStream);
        if (err != DeserializationError::Ok) {
            std::cerr << "[HTTP] JSON parse error: " << err.c_str() << " for " << url << std::endl;
            return false;
        }
        return true;
    }

#else
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
#endif


// =================================================================
//                    PC SIMULATION ENVIRONMENT
// =================================================================
#if RUN_ON_PC == 1
using namespace std;

unsigned long lastSpotifyUpdate = 0;
unsigned long lastInputTime = 0;
unsigned long tokenStartTimePC = 0;
bool pendingPlay = false;

void refreshSpotifyTokenPC() {
    string postData = "grant_type=refresh_token&refresh_token=" + string(SPOTIFY_REFRESH_TOKEN) + 
                      "&client_id=" + string(SPOTIFY_CLIENT_ID) + 
                      "&client_secret=" + string(SPOTIFY_CLIENT_SECRET);
    JsonDocument doc;
    if (fetchJson("https://accounts.spotify.com/api/token", "POST", postData, doc, nullptr)) {
        accessToken = doc["access_token"].as<string>();
        tokenStartTimePC = sysTime();
        cout << "[SUCCESS] Token acquired!" << endl;
    } else {
        cout << "[ERROR] Token refresh failed - will retry" << endl;
    }
}

void playCurrentAlbumPC() {
    AlbumData current = recommender.getCurrentAlbum();
    if (current.uri == "") return;
    
    string ctx = recommender.isDiscoveryTurn() ? "[DISCOVERY]" : "[LIBRARY]";
    cout << ">> " << ctx << " Sending Play Command: " << current.name << endl;
    
    // Format payload based on URI type
    string body;
    if (current.uri.find("spotify:track:") != string::npos) {
        body = "{\"uris\":[\"" + current.uri + "\"]}";
    } else {
        body = "{\"context_uri\":\"" + current.uri + "\"}";
    }
    
    JsonDocument dummy;
    fetchJson("https://api.spotify.com/v1/me/player/play", "PUT", body, dummy, nullptr);
}

void drawAlbumArtPC(string url) {
    string cmd = "curl -s --max-time 5 --connect-timeout 3 \"" + url + "\" > temp_art.jpg";
    system(cmd.c_str());
    cv::Mat art = cv::imread("temp_art.jpg");
    if (!art.empty()) {
        cv::resize(art, art, cv::Size(240, 240)); 
        cv::Mat screen(240, 320, CV_8UC3, cv::Scalar(0, 0, 0));
        art.copyTo(screen(cv::Rect(0, 0, 240, 240))); 
        cv::imshow("2.8 inch - Album Art", screen);
    }
}

void updateUIPC(string title, string artist, string album, int trackNum, int totalTracks, long progress, long duration) {
    cv::Mat screen(76, 284, CV_8UC3, cv::Scalar(0, 0, 0));
    
    cv::putText(screen, title.substr(0, 20), cv::Point(5, 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    
    if (totalTracks > 0) {
        string countText = to_string(trackNum) + "/" + to_string(totalTracks);
        cv::putText(screen, countText, cv::Point(235, 20), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 255), 1);
    }
    
    cv::putText(screen, artist.substr(0, 16) + " - " + album.substr(0, 16), cv::Point(5, 42), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(180, 180, 180), 1);
    
    string modeLabel = recommender.isDiscoveryTurn() ? "Discover: " + recommender.topGenre : "My Library";
    cv::putText(screen, modeLabel, cv::Point(5, 55), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(200, 100, 200), 1);

    int barWidth = 274;
    float prog = (duration > 0) ? (float)progress / (float)duration : 0.0f;
    cv::rectangle(screen, cv::Point(5, 62), cv::Point(5 + barWidth, 70), cv::Scalar(255, 255, 255), 1);
    cv::rectangle(screen, cv::Point(5, 62), cv::Point(5 + (int)(prog * barWidth), 70), cv::Scalar(0, 255, 0), cv::FILLED);
    cv::imshow("2.25 inch - Progress", screen);
}

void updateCurrentlyPlayingPC() {
    if (accessToken == "") return;
    JsonDocument doc;
    if (fetchJson("https://api.spotify.com/v1/me/player/currently-playing", "GET", "", doc, nullptr)) {
        if (doc["item"]) {
            string trackId = doc["item"]["id"] | "";
            string imageUrl = doc["item"]["album"]["images"][0]["url"] | "";

            updateUIPC(
                doc["item"]["name"] | "Unknown", 
                doc["item"]["artists"][0]["name"] | "Unknown", 
                doc["item"]["album"]["name"] | "Unknown", 
                doc["item"]["track_number"] | 1, 
                doc["item"]["album"]["total_tracks"] | 1, 
                doc["progress_ms"] | 0, 
                doc["item"]["duration_ms"] | 1
            );

            if (trackId != currentTrackId && !imageUrl.empty()) {
                currentTrackId = trackId;
                drawAlbumArtPC(imageUrl);
            }
        }
        // NOTE: an empty/successful response with no "item" (nothing playing) is valid -
        // we simply skip the UI update rather than treating it as an error.
    }
}

int main() {
    cv::namedWindow("2.8 inch - Album Art", cv::WINDOW_NORMAL);
    cv::namedWindow("2.25 inch - Progress", cv::WINDOW_NORMAL);
    cv::resizeWindow("2.25 inch - Progress", 284, 76);
    cv::resizeWindow("2.8 inch - Album Art", 320, 240);

    refreshSpotifyTokenPC();
    recommender.init();

    while (true) {
        int key = cv::waitKey(50); 
        
        // 1. Instantly update screen when skipping, but don't ping Spotify yet
        if (key == 'n' || key == 'N' || key == 'p' || key == 'P') { 
            recommender.cycleNext(key == 'n' || key == 'N'); 
            AlbumData preview = recommender.getCurrentAlbum();
            
            // Show preview UI immediately
            updateUIPC(">>> Queuing Album <<<", preview.name, recommender.isDiscoveryTurn() ? "Discovery" : "Library", 0, 0, 0, 1);
            
            pendingPlay = true;
            lastInputTime = sysTime();
        }
        else if (key == 27) return 0; 

        // 2. Wait for 1.5 seconds of NO input before commanding Spotify to play
        if (pendingPlay && (sysTime() - lastInputTime > 100)) {
            playCurrentAlbumPC();
            pendingPlay = false;
            lastSpotifyUpdate = sysTime() - 3000; // Force immediate refresh next tick
        }

        // 3. Normal polling (Only runs when we aren't scrolling the dial)
        if (!pendingPlay && (sysTime() - lastSpotifyUpdate > 3000)) {
            updateCurrentlyPlayingPC();
            lastSpotifyUpdate = sysTime();
        }

        // 4. Proactive token refresh so a long-running session doesn't silently
        //    start failing every request ~60 min in (Spotify tokens expire in 3600s).
        if (sysTime() - tokenStartTimePC > 3500000) {
            refreshSpotifyTokenPC();
        }
    }
    return 0;
}

#else
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

  ESP32Encoder::useInternalWeakPullResistors = UP;
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
#endif
