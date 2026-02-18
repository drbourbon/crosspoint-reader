// src/activities/calendar/CalendarActivity.cpp
#include "CalendarActivity.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include "HalDisplay.h"
#include "fontIds.h"

extern HalDisplay display; // Defined in main.cpp

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef CALENDAR_API_URL
#define CALENDAR_API_URL "https://example.com/api/cal"
#endif

#ifndef CALENDAR_BEARER_TOKEN
#define CALENDAR_BEARER_TOKEN "your_token_here"
#endif

const char* API_URL = CALENDAR_API_URL;
const char* BEARER_TOKEN = CALENDAR_BEARER_TOKEN;

CalendarActivity::CalendarActivity(GfxRenderer& renderer, MappedInputManager& inputManager, std::function<void()> onBack)
    : Activity("Calendar", renderer, inputManager), renderer(renderer), inputManager(inputManager), onBack(onBack) {}

void CalendarActivity::onEnter() {
    renderer.clearScreen();
    renderer.drawText(UI_12_FONT_ID, 240, 400, "Connecting to WiFi...");
    renderer.displayBuffer();

    // Try to connect using saved credentials if not already connected
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(); 
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            retries++;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        renderer.clearScreen();
        renderer.drawText(UI_12_FONT_ID, 240, 400, "WiFi Connection Failed");
        renderer.displayBuffer();
        return;
    }

    fetchAndRenderCalendar();
    loaded = true;
}

void CalendarActivity::loop() {
    if (inputManager.isPressed(MappedInputManager::Button::Back)) {
        onBack();
    }
}

void CalendarActivity::onExit() {
}

void CalendarActivity::fetchAndRenderCalendar() {
    renderer.clearScreen();
    renderer.drawText(UI_12_FONT_ID, 240, 400, "Downloading Calendar...");
    renderer.displayBuffer();

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Authorization", String("Bearer ") + BEARER_TOKEN);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            renderer.clearScreen();
            renderer.drawText(UI_12_FONT_ID, 240, 400, "JSON Parse Error");
            renderer.displayBuffer();
        } else {
            renderer.clearScreen();
            
            int y = 40;
            renderer.drawText(UI_12_FONT_ID, 240, y, "Calendario Eventi");
            y += 40;

            JsonArray eventi = doc["eventi"];
            String currentDay = "";

            for (JsonObject evento : eventi) {
                String day = evento["start"]["giorno"].as<String>();
                String time = evento["start"]["ora"].as<String>();
                String summary = evento["summary"].as<String>();
                
                if (day != currentDay) {
                    y += 10;
                    if (y > 750) break;
                    renderer.drawText(UI_10_FONT_ID, 10, y, day.c_str());
                    renderer.drawLine(10, y + 20, 470, y + 20, 0);
                    currentDay = day;
                    y += 30;
                }

                String fullText = time + " - " + summary;
                int maxWidth = 440;
                
                while (fullText.length() > 0) {
                    if (y > 780) break;

                    String lineToDraw = fullText;
                    
                    if (renderer.getTextWidth(SMALL_FONT_ID, lineToDraw.c_str()) > maxWidth) {
                        String currentBuild = "";
                        int nextWordStart = 0;
                        bool firstWord = true;
                        
                        while (nextWordStart < fullText.length()) {
                            int nextSpace = fullText.indexOf(' ', nextWordStart);
                            if (nextSpace == -1) nextSpace = fullText.length();
                            
                            String word = fullText.substring(nextWordStart, nextSpace);
                            String testLine = firstWord ? word : currentBuild + " " + word;
                            
                            if (renderer.getTextWidth(SMALL_FONT_ID, testLine.c_str()) > maxWidth) {
                                if (firstWord) {
                                    currentBuild = word;
                                    nextWordStart = nextSpace + 1;
                                }
                                break;
                            }
                            
                            currentBuild = testLine;
                            nextWordStart = nextSpace + 1;
                            firstWord = false;
                        }
                        
                        lineToDraw = currentBuild;
                        if (nextWordStart < fullText.length()) {
                             fullText = fullText.substring(nextWordStart);
                        } else {
                             fullText = "";
                        }
                    } else {
                        fullText = "";
                    }
                    
                    renderer.drawText(SMALL_FONT_ID, 20, y, lineToDraw.c_str());
                    y += 20;
                }
                
                if (y > 780) break;
            }

            renderer.displayBuffer();
            saveSleepImage();
            
            // Small indication that screenshot was saved
            renderer.drawText(SMALL_FONT_ID, 240, 780, "Saved to sleep.bmp");
            renderer.displayBuffer();
        }
    } else {
        renderer.clearScreen();
        String msg = "HTTP Error: " + String(httpCode);
        renderer.drawText(UI_12_FONT_ID, 240, 400, msg.c_str());
        renderer.displayBuffer();
    }
    http.end();
}

void CalendarActivity::saveSleepImage() {
    FsFile file;
    if (!Storage.openFileForWrite("CAL", "/sleep.bmp", file)) return;

    uint32_t width = 480;
    uint32_t height = 800;
    uint32_t rowSize = (width * 3 + 3) & ~3; // 24-bit RGB aligned to 4 bytes
    uint32_t imageSize = rowSize * height;
    uint32_t fileSize = 54 + imageSize;

    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(width), (uint8_t)(width >> 8), (uint8_t)(width >> 16), (uint8_t)(width >> 24),
        (uint8_t)(height), (uint8_t)(height >> 8), (uint8_t)(height >> 16), (uint8_t)(height >> 24),
        1, 0,
        24, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    file.write(header, 54);

    uint8_t* fb = display.getFrameBuffer();
    uint8_t lineBuffer[rowSize];
    memset(lineBuffer, 0, rowSize);

    // Convert framebuffer to 24-bit BMP (Bottom-up).
    // Framebuffer is 1bpp, 800x480 (Landscape), mapped to 480x800 (Portrait).
    // Mapping: Dest(x, y) <- Source(y, 479 - x)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int px = y;
            int py = 479 - x;
            int byteIdx = py * 100 + (px / 8);
            int bitIdx = 7 - (px % 8);
            bool isWhite = (fb[byteIdx] >> bitIdx) & 1;
            uint8_t color = isWhite ? 255 : 0;

            // BMP is BGR
            lineBuffer[x * 3] = color;
            lineBuffer[x * 3 + 1] = color;
            lineBuffer[x * 3 + 2] = color;
        }
        file.write(lineBuffer, rowSize);
    }
    
    file.close();
}
