#include "TrmnlService.h"

#include <HTTPClient.h>
#include <HalStorage.h>
#include <WiFi.h>
#include <Logging.h>
#include <HalPowerManager.h>
#include "components/UITheme.h"

TrmnlService::Config TrmnlService::config;
bool TrmnlService::configLoaded = false;
const char* TrmnlService::CONFIG_PATH = "/.crosspoint/trmnl.json";

bool TrmnlService::isEnabled() {
  if (!configLoaded) loadConfig();
  return config.enabled && config.apiKey.length() > 0;
}

void TrmnlService::loadConfig() {
  if (configLoaded) return;
  LOG_DBG("TRMNL_SVC", "loadConfig: loading from %s", CONFIG_PATH);

  if (Storage.exists(CONFIG_PATH)) {
    FsFile file;
    if (Storage.openFileForRead("TRMNL", CONFIG_PATH, file)) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        config.enabled = doc["enabled"] | false;
        config.customServer = doc["customServer"] | false;
        if (doc["serverUrl"].is<const char*>()) config.serverUrl = doc["serverUrl"].as<std::string>();
        if (doc["apiKey"].is<const char*>()) config.apiKey = doc["apiKey"].as<std::string>();
        if (doc["friendlyId"].is<const char*>()) config.friendlyId = doc["friendlyId"].as<std::string>();
        LOG_DBG("TRMNL_SVC", "loadConfig: values loaded (url:%s, api:%s)", config.serverUrl.c_str(), config.apiKey.c_str());
      } else {
        LOG_ERR("TRMNL_SVC", "loadConfig: json parse error: %s", error.c_str());
      }
      file.close();
    } else {
      LOG_ERR("TRMNL_SVC", "loadConfig: failed to open file for reading");
    }
  } else {
    LOG_DBG("TRMNL_SVC", "loadConfig: file does not exist, using defaults");
  }
  configLoaded = true;
}

void TrmnlService::saveConfig() {
  LOG_DBG("TRMNL_SVC", "saveConfig: saving to %s (enabled:%d, url:%s, api:%s)", 
    CONFIG_PATH, config.enabled, config.serverUrl.c_str(), config.apiKey.c_str());
  FsFile file;
  if (Storage.openFileForWrite("TRMNL", CONFIG_PATH, file)) {
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["customServer"] = config.customServer;
    doc["serverUrl"] = config.serverUrl;
    doc["apiKey"] = config.apiKey;
    doc["friendlyId"] = config.friendlyId;
    serializeJson(doc, file);
    file.close();
    LOG_DBG("TRMNL_SVC", "saveConfig: done");
  } else {
    LOG_ERR("TRMNL_SVC", "saveConfig: failed to open file for writing");
  }
}

void TrmnlService::setConfig(const Config config) {
  LOG_DBG("TRMNL_SVC", "setConfig called");
  TrmnlService::config = config;
}

TrmnlService::Config& TrmnlService::getConfig() {
  LOG_DBG("TRMNL_SVC", "getConfig called");
  loadConfig();
  LOG_DBG("TRMNL_SVC", "getConfig returning config object");
  return config;
}

std::string TrmnlService::getMacAddress() {
  String mac = WiFi.macAddress();
  return std::string(mac.c_str());
}

bool TrmnlService::registerDevice(std::string& message) {
  loadConfig();
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  std::string baseUrl = config.customServer ? config.serverUrl : "https://trmnl.app";
  if (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  
  // Use GET /api/setup with headers as per BYOS implementation
  std::string url = baseUrl + "/api/setup";
  http.begin(url.c_str());
  http.addHeader("id", getMacAddress().c_str());
  http.addHeader("model-id", "crosspoint");
  http.addHeader("Accept", "application/json");
  
  int httpCode = http.GET();
  bool success = (httpCode == 200);
  String response = http.getString();
  JsonDocument respDoc;
  deserializeJson(respDoc, response);
  message = respDoc["message"].as<std::string>();

  if (success) {
      if (respDoc["api_key"].is<const char*>()) {
          config.apiKey = respDoc["api_key"].as<std::string>();
          config.friendlyId = respDoc["friendly_id"].as<std::string>();
          saveConfig();
      }
  } else {
      LOG_ERR("TRMNL_SVC", "Register failed: %d", httpCode);
      return false;
  }
  
  http.end();
  return success;
}

void TrmnlService::fetchBeforeSleep(GfxRenderer renderer) {
  if (!isEnabled()) return;

  LOG_DBG("TRMNL_SVC", "Connecting to WiFi to fetch TRMNL screen");
  
  if (WiFi.status() != WL_CONNECTED) {
      GUI.drawPopup(renderer, "Enabling WiFi...");
      WiFi.persistent(true);
      delay(100);
      WiFi.begin();
      int retries = 0;
      while (WiFi.status() != WL_CONNECTED && retries < 30) {
          delay(100);
          retries++;
      }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    LOG_DBG("TRMNL_SVC", "Wifi connected. Fetching TRMNL screen");
    GUI.drawPopup(renderer, "Updating TRMNL...");
    refreshScreen();
    WiFi.mode(WIFI_OFF);
  } else {
    WiFi.mode(WIFI_OFF);
    Storage.remove("/.crosspoint/trmnl.bmp");
    LOG_ERR("TRMNL_SVC", "Can't connect to Wifi to Fetch TRMNL screen");
  }
}

bool TrmnlService::refreshScreen() {
  loadConfig();
  if (!config.enabled) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  std::string baseUrl = config.customServer ? config.serverUrl : "https://trmnl.app";
  if (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  
  // 1. Get display status (JSON)
  std::string url = baseUrl + "/api/display"; 
  
  LOG_DBG("TRMNL_SVC", "Calling BYOS server (%s)", url.c_str());

  float batteryVoltage = (4.2f * powerManager.getBatteryPercentage())/100.0f;
  LOG_DBG("TRMNL_SVC", "Battery voltage: %f", batteryVoltage);

  http.begin(url.c_str());
  http.addHeader("id", getMacAddress().c_str());
  if (!config.apiKey.empty()) {
      http.addHeader("access-token", config.apiKey.c_str());
  }
  http.addHeader("rssi", String(WiFi.RSSI()));
  http.addHeader("fw-version", "1.7.7"); // Report version for format negotiation
  http.addHeader("Battery-Voltage", String(batteryVoltage));
  

  http.setTimeout(10000);
  int httpCode = http.GET();
  String imageUrl = "";

  if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, response);
      if (!error && doc["image_url"].is<const char*>()) {
          imageUrl = doc["image_url"].as<String>();
          LOG_DBG("TRMNL_SVC", "Got image url: %s", imageUrl.c_str());
      } else {
        LOG_ERR("TRMNL_SVC", "Error response (%s)", response.c_str());
      }
  } else {
      LOG_ERR("TRMNL_SVC", "Display info failed: %d", httpCode);
  }
  http.end();

  if (imageUrl.isEmpty()) {
    Storage.remove("/.crosspoint/trmnl.bmp");
    return false;
  }

  // 2. Download the image
  LOG_DBG("TRMNL_SVC", "Downloading image from %s", imageUrl.c_str());

  http.begin(imageUrl);
  http.setTimeout(30000);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    
    FsFile file;
    if (Storage.openFileForWrite("TRMNL", "/.crosspoint/trmnl.bmp", file)) {
        uint8_t buff[512];
        int remaining = len;
        unsigned long lastDataTime = millis();
        while (http.connected() && (len > 0 || len == -1)) {
            size_t size = stream->available();
            if (size) {
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                file.write(buff, c);
                if (len > 0) len -= c;
                lastDataTime = millis();
            } else if (millis() - lastDataTime > 15000) {
                break; // Timeout
            }
            delay(1);
        }
        file.close();
        http.end();
        return true;
    }
  } else {
      LOG_ERR("TRMNL_SVC", "Download failed: %d", httpCode);
      Storage.remove("/.crosspoint/trmnl.bmp");
  }
  http.end();
  return false;
}
