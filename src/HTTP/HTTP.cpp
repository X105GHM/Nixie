#include "HTTP/HTTP.h"
#include "ACP/ACP.h"
#include "HSS/HSS.h"
#include "Digit_Control/Digit.h"
#include "Logger/Logger.h"
#include "Time/Time.h"
#include "Melody/Melody.h"
#include <WiFiManager.h>

extern bool displayEnabled;
extern float HSS_V;
extern TaskHandle_t timeTaskHandle;
extern TimeControl timeControl;
extern MelodyType currentMelody;

HTTPHandler::HTTPHandler(int port) : server(port) {}

void HTTPHandler::begin() {

    server.on("/set/OFF", HTTP_GET, [this]() {
        displayEnabled = false;
        Logger::log(LoggerType::HTTP, F("Display disabled"));
        server.send(200, "text/plain", "Display disabled");
    });

    server.on("/set/ON", HTTP_GET, [this]() {
        displayEnabled = true;
        Logger::log(LoggerType::HTTP, F("Display enabled"));
        server.send(200, "text/plain", "Display enabled");
    });

    server.on("/set/reset", HTTP_GET, [this]() {
        handleReset();
    });

    server.on("/set/ACP", HTTP_GET, [this]() {
        if(displayEnabled) {
            vTaskSuspend(timeTaskHandle);
            Logger::log(LoggerType::HTTP, F("ACP triggered"));
            server.send(200, "text/plain", "ACP Routine");
            dacWrite(PIN_JFET, ACP_Voltage);
            ACP();
            dacWrite(PIN_JFET, Operating_Voltage);
            vTaskResume(timeTaskHandle);
        }
        else {
            Logger::log(LoggerType::HTTP, F("ERROR: Display is not enabled"));
            server.send(200, "text/plain", "Display is not enabled");
        }
    });

    server.on("/set/DATE", HTTP_GET, [this]() {
        if(displayEnabled) {
            vTaskSuspend(timeTaskHandle);
            Logger::log(LoggerType::HTTP, F("Date is shown"));
            server.send(200, "text/plain", "Date is shown");
            displayDate();
            delay(5000);
            vTaskResume(timeTaskHandle);
        }
        else {
            Logger::log(LoggerType::HTTP, F("ERROR: Display is not enabled"));
            server.send(200, "text/plain", "Display is not enabled");
        }
    });

    
    server.on("/set/BUZZER/gong", HTTP_GET, [this]() {
        static unsigned long lastGongTime = 0;
        constexpr unsigned long debounceGong = 10000;
        unsigned long currentTime = millis();
        if (currentTime - lastGongTime < debounceGong) {
            server.send(200, "text/plain", "Timeout");
            return;
        }
        lastGongTime = currentTime;
    
        currentMelody = GONG;
    
        if (timeControl.getGongSemaphore() != nullptr) {
            xSemaphoreGive(timeControl.getGongSemaphore());
            Logger::log(LoggerType::HTTP, F("Ringing the GONG"));
            server.send(200, "text/plain", "Ringing the GONG");
        } else {
            Logger::log(LoggerType::HTTP, F("ERROR: gongSemaphore is not available"));
            server.send(200, "text/plain", "ERROR: gongSemaphore is not available");
        }
    });


    server.on("/set/BUZZER/mario", HTTP_GET, [this]() {
        static unsigned long lastMarioTime = 0;
        constexpr unsigned long debounceMario = 30000;
        unsigned long currentTime = millis();
        if (currentTime - lastMarioTime < debounceMario) {
            server.send(200, "text/plain", "Timeout");
            return;
        }
        lastMarioTime = currentTime;
    
        currentMelody = MARIO;
    
        if (timeControl.getGongSemaphore() != nullptr) {
            xSemaphoreGive(timeControl.getGongSemaphore());
            Logger::log(LoggerType::HTTP, F("Playing Mario melody"));
            server.send(200, "text/plain", "Playing Mario melody");
            currentMelody = GONG;
        } else {
            Logger::log(LoggerType::HTTP, F("ERROR: gongSemaphore is not available"));
            server.send(200, "text/plain", "ERROR: gongSemaphore is not available");
        }
    });


    server.on("/get/info", HTTP_GET, [this]() {
        Logger::log(LoggerType::HTTP, F("Info requested"));
        handleInfo();
    });

    server.begin();
    Logger::log(LoggerType::HTTP, F("HTTP server started"));
}

void HTTPHandler::handleClient() {
    server.handleClient();
}

void HTTPHandler::handleReset() {
    Logger::log(LoggerType::HTTP, F("System resetting..."));
    ESP.restart();
}

void HTTPHandler::handleInfo() {
    WiFiManager wm;
    auto ssid = wm.getWiFiSSID();
    auto password = wm.getWiFiPass();

    auto chipModel = ESP.getChipModel();
    auto freeHeap = ESP.getFreeHeap();
    auto chipId = ESP.getEfuseMac();
    auto flashSize = ESP.getFlashChipSize();
    auto flashSpeed = ESP.getFlashChipSpeed();
    auto sketchSize = ESP.getSketchSize();
    auto sketchFreeSpace = ESP.getFreeSketchSpace();
    auto cpuFreq = ESP.getCpuFreqMHz();
    auto sdkVersion = ESP.getSdkVersion();

    String jsonResponse = "{\n";
    jsonResponse += String("  \"Chip\": \"") + chipModel + String("\",\n");
    jsonResponse += "  \"SSID\": \"" + ssid + "\",\n";
    jsonResponse += "  \"Password\": \"" + password + "\",\n";
    jsonResponse += "  \"DisplayEnabled\": " + String(displayEnabled ? "true" : "false") + ",\n";
    jsonResponse += "  \"HSS_Voltage\": " + String(HSS_V * 1.5) + ",\n";
    jsonResponse += "  \"Melody\": \"" + String("Nokia") + "\",\n";
    jsonResponse += "  \"FreeHeap\": " + String(freeHeap) + ",\n";
    jsonResponse += "  \"ChipId\": \"" + String(chipId, HEX) + "\",\n";
    jsonResponse += "  \"FlashSize\": " + String(flashSize) + ",\n";
    jsonResponse += "  \"FlashSpeed\": " + String(flashSpeed) + ",\n";
    jsonResponse += "  \"SketchSize\": " + String(sketchSize) + ",\n";
    jsonResponse += "  \"SketchFreeSpace\": " + String(sketchFreeSpace) + ",\n";
    jsonResponse += "  \"CpuFrequencyMHz\": " + String(cpuFreq) + ",\n";
    jsonResponse += String("  \"SdkVersion\": \"") + sdkVersion + String("\",\n");
    jsonResponse += "  \"Autor\": \"" + String("X105GHM") + "\"\n";
    jsonResponse += "}";
    
    server.send(200, "application/json", jsonResponse);
}
