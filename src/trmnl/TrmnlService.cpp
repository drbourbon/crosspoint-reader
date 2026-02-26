#include "TrmnlService.h"

#include <HTTPClient.h>
#include <HalStorage.h>
#include <WiFi.h>
#include <Logging.h>

TrmnlService::Config TrmnlService::config;
bool TrmnlService::configLoaded = false;
const char* TrmnlService::CONFIG_PATH = "/.crosspoint/trmnl.json";

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
        if (doc["serverUrl"].is<const char*>()) config.serverUrl = doc["serverUrl"].as<std::string>();
        if (doc["apiKey"].is<const char*>()) config.apiKey = doc["apiKey"].as<std::string>();
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
    doc["serverUrl"] = config.serverUrl;
    doc["apiKey"] = config.apiKey;
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
  mac.replace(":", "");
  mac.toLowerCase();
  return std::string(mac.c_str());
}

bool TrmnlService::registerDevice() {
  loadConfig();
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  std::string baseUrl = config.serverUrl;
  if (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  
  // Use GET /api/setup with headers as per BYOS implementation
  std::string url = baseUrl + "/api/setup";
  http.begin(url.c_str());
  http.addHeader("id", getMacAddress().c_str());
  http.addHeader("model-id", "crosspoint");
  http.addHeader("Accept", "application/json");
  
  int httpCode = http.GET();
  bool success = (httpCode == 200);
  
  if (success) {
      String response = http.getString();
      JsonDocument respDoc;
      deserializeJson(respDoc, response);
      if (respDoc["api_key"].is<const char*>()) {
          config.apiKey = respDoc["api_key"].as<std::string>();
          saveConfig();
      }
  } else {
      LOG_ERR("TRMNL_SVC", "Register failed: %d", httpCode);
  }
  
  http.end();
  return success;
}

bool TrmnlService::refreshScreen() {
  loadConfig();
  if (!config.enabled) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  std::string baseUrl = config.serverUrl;
  if (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  
  // 1. Get display status (JSON)
  std::string url = baseUrl + "/api/display"; 
  
  http.begin(url.c_str());
  http.addHeader("id", getMacAddress().c_str());
  if (!config.apiKey.empty()) {
      http.addHeader("access-token", config.apiKey.c_str());
  }
  http.addHeader("rssi", String(WiFi.RSSI()));
  http.addHeader("fw-version", "1.0.0"); // Report version for format negotiation

  http.setTimeout(10000);
  int httpCode = http.GET();
  String imageUrl = "";

  if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, response);
      if (!error && doc["image_url"].is<const char*>()) {
          imageUrl = doc["image_url"].as<String>();
      }
  } else {
      LOG_ERR("TRMNL_SVC", "Display info failed: %d", httpCode);
  }
  http.end();

  if (imageUrl.isEmpty()) return false;

  // 2. Download the image
  http.begin(imageUrl);
  http.setTimeout(30000);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    
    FsFile file;
    // Note: We overwrite sleep.bmp. Ensure Settings -> Sleep Screen is set to "Custom" to use it.
    if (Storage.openFileForWrite("TRMNL", "/sleep.bmp", file)) {
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
  }
  http.end();
  return false;
}
