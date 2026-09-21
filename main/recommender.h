#ifndef RECOMMENDER_H
#define RECOMMENDER_H

#include <string>
#include "ArduinoJson.h"

extern bool fetchJson(std::string url, std::string method, std::string body, JsonDocument& doc, JsonDocument* filter = nullptr);

#define MAX_LIB_ALBUMS 30
#define MAX_DISC_ALBUMS 10
#define MAX_GENRES 40

struct AlbumData {
    std::string uri;
    std::string name;
};

struct GenreTally {
    std::string name;
    int count;
};

class SpotifyRecommender {
public:
    AlbumData libraryAlbums[MAX_LIB_ALBUMS];
    int libCount = 0;

    AlbumData discoveryAlbums[MAX_DISC_ALBUMS];
    int discCount = 0;
    
    int cycleRatio = 3;  
    int interactions = 0; 
    int libIndex = 0;
    int discIndex = 0;
    
    std::string topGenre = "synthwave"; 

    void init() {
        loadLibrary();
        buildDiscovery();
    }

    void loadLibrary() {
        JsonDocument filter;
        filter["items"][0]["album"]["uri"] = true;
        filter["items"][0]["album"]["name"] = true;

        JsonDocument doc;
        if (fetchJson("https://api.spotify.com/v1/me/albums?limit=" + std::to_string(MAX_LIB_ALBUMS), "GET", "", doc, &filter)) {
            if (doc["items"].is<JsonArray>()) {
                for (JsonObject item : doc["items"].as<JsonArray>()) {
                    if (libCount >= MAX_LIB_ALBUMS) break;
                    libraryAlbums[libCount].uri = item["album"]["uri"] | "";
                    libraryAlbums[libCount].name = item["album"]["name"] | "";
                    libCount++;
                }
            }
        }

        // FALLBACK: If user lacks 'user-library-read' scope or has an empty library
        if (libCount == 0) {
            libraryAlbums[0] = {"spotify:album:4m2880jivSbbyEGAKfITCa", "The Dark Side of the Moon"};
            libraryAlbums[1] = {"spotify:album:1ATL5QOzEoiDhuLAvSC4dy", "OK Computer"};
            libraryAlbums[2] = {"spotify:album:0ETFjACtuP2ADo6LFhL6HN", "Abbey Road"};
            libraryAlbums[3] = {"spotify:album:6dVIQhpCfKm0ylToea1UVI", "Hotel California"};
            libCount = 4;
        }
    }

    void buildDiscovery() {
        JsonDocument trackFilter;
        trackFilter["items"][0]["artists"][0]["id"] = true;
        
        JsonDocument tracksDoc;
        std::string artistIds = "";
        if (fetchJson("https://api.spotify.com/v1/me/top/tracks?limit=20", "GET", "", tracksDoc, &trackFilter)) {
            if (tracksDoc["items"].is<JsonArray>()) {
                for (JsonObject item : tracksDoc["items"].as<JsonArray>()) {
                    std::string id = item["artists"][0]["id"] | "";
                    if (!id.empty()) artistIds += id + ",";
                }
                if (!artistIds.empty()) artistIds.pop_back(); 
            }
        }

        if (!artistIds.empty()) {
            JsonDocument artistFilter;
            artistFilter["artists"][0]["genres"][0] = true;
            
            JsonDocument artistsDoc;
            GenreTally tallies[MAX_GENRES];
            int uniqueGenres = 0;

            if (fetchJson("https://api.spotify.com/v1/artists?ids=" + artistIds, "GET", "", artistsDoc, &artistFilter)) {
                for (JsonObject artist : artistsDoc["artists"].as<JsonArray>()) {
                    for (std::string genre : artist["genres"].as<JsonArray>()) {
                        bool found = false;
                        for (int i = 0; i < uniqueGenres; i++) {
                            if (tallies[i].name == genre) {
                                tallies[i].count++;
                                found = true;
                                break;
                            }
                        }
                        if (!found && uniqueGenres < MAX_GENRES) {
                            tallies[uniqueGenres].name = genre;
                            tallies[uniqueGenres].count = 1;
                            uniqueGenres++;
                        }
                    }
                }
            }

            int maxCount = 0;
            for (int i = 0; i < uniqueGenres; i++) {
                if (tallies[i].count > maxCount) {
                    maxCount = tallies[i].count;
                    topGenre = tallies[i].name;
                }
            }

            JsonDocument recFilter;
            recFilter["tracks"][0]["album"]["uri"] = true;
            recFilter["tracks"][0]["album"]["name"] = true;
            
            JsonDocument recDoc;
            std::string recUrl = "https://api.spotify.com/v1/recommendations?seed_genres=" + topGenre + "&limit=" + std::to_string(MAX_DISC_ALBUMS);
            
            if (fetchJson(recUrl, "GET", "", recDoc, &recFilter)) {
                for (JsonObject track : recDoc["tracks"].as<JsonArray>()) {
                    std::string uri = track["album"]["uri"] | "";
                    std::string name = track["album"]["name"] | "";
                    
                    bool exists = false;
                    for (int i = 0; i < discCount; i++) {
                        if (discoveryAlbums[i].uri == uri) { exists = true; break; }
                    }
                    if (!exists && !uri.empty() && discCount < MAX_DISC_ALBUMS) {
                        discoveryAlbums[discCount] = {uri, name};
                        discCount++;
                    }
                }
            }
        }

        // FALLBACK: If user lacks 'user-top-read' scope or algo failed
        if (discCount == 0) {
            topGenre = "electronic (fallback)";
            discoveryAlbums[0] = {"spotify:album:4uG8q3CGpZvRwqwJJsiCjJ", "Discovery"}; // Daft Punk
            discoveryAlbums[1] = {"spotify:album:55RhLsokJCmOQv6L2wKjM2", "Cross"}; // Justice
            discoveryAlbums[2] = {"spotify:album:5s0rmjP8XOPhP6HhqOhuyC", "Demon Days"}; // Gorillaz
            discCount = 3;
        }
    }

    void cycleNext(bool forward) {
        interactions += (forward ? 1 : -1);
        if (interactions < 0) interactions = 0;

        if (isDiscoveryTurn()) {
            if (discCount > 0) {
                discIndex = forward ? (discIndex + 1) % discCount : (discIndex - 1 + discCount) % discCount;
            }
        } else {
            if (libCount > 0) {
                libIndex = forward ? (libIndex + 1) % libCount : (libIndex - 1 + libCount) % libCount;
            }
        }
    }

    bool isDiscoveryTurn() {
        return (interactions % (cycleRatio + 1)) == cycleRatio;
    }

    AlbumData getCurrentAlbum() {
        if (isDiscoveryTurn() && discCount > 0) return discoveryAlbums[discIndex];
        if (libCount > 0) return libraryAlbums[libIndex];
        return {"", "No Albums Loaded"};
    }
};

#endif