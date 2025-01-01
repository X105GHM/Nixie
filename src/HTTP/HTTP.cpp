#include "HTTP/HTTP.h"
#include "HSS/HSS.h"
#include "Digit_Control/Digit.h"
#include "Arduino.h"
#include <WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

HTTPHandler::HTTPHandler(int port) : server(port) {}

void HTTPHandler::begin() {
    server.on("/set/OFF", HTTP_GET, [this]() {
        displayEnabled = false;
        Serial.println("Display disabled");
        server.send(200, "text/plain", "Display disabled");
    });

    server.on("/set/ON", HTTP_GET, [this]() {
        displayEnabled = true;
        Serial.println("Display enabled");
        server.send(200, "text/plain", "Display enabled");
    });

    server.on("/set/reset", HTTP_GET, [this]() {
        handleReset();
    });

    server.on("/set/BUZZER", HTTP_GET, [this]() {
        xSemaphoreGive(gongSemaphore);
    });

    server.on("/get/info?", HTTP_GET, [this]() {
        handleInfo();
    });

    server.begin();
    Serial.println("HTTP server started");
}

void HTTPHandler::handleClient() {
    server.handleClient();
}

void HTTPHandler::handleReset() {
    Serial.println("System resetting...");
    ESP.restart();
}

void HTTPHandler::handleInfo() {

    WiFiManager wm;
    String ssid = wm.getWiFiSSID();
    String password = wm.getWiFiPass();

    String chipModel = ESP.getChipModel();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint64_t chipId = ESP.getEfuseMac();
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashSpeed = ESP.getFlashChipSpeed();
    uint32_t sketchSize = ESP.getSketchSize();
    uint32_t sketchFreeSpace = ESP.getFreeSketchSpace();
    uint32_t cpuFreq = ESP.getCpuFreqMHz();
    String sdkVersion = ESP.getSdkVersion();

    StaticJsonDocument<1024> jsonDoc;
    jsonDoc["Chip"] = chipModel;
    jsonDoc["SSID"] = ssid;
    jsonDoc["Password"] = password;
    jsonDoc["DisplayEnabled"] = displayEnabled;
    jsonDoc["HSS_Voltage"] = HSS_V;
    jsonDoc["Melody"] = melodyID;
    jsonDoc["FreeHeap"] = freeHeap;
    jsonDoc["ChipId"] = String(chipId, HEX);
    jsonDoc["FlashSize"] = flashSize;
    jsonDoc["FlashSpeed"] = flashSpeed;
    jsonDoc["SketchSize"] = sketchSize;
    jsonDoc["SketchFreeSpace"] = sketchFreeSpace;
    jsonDoc["CpuFrequencyMHz"] = cpuFreq;
    jsonDoc["SdkVersion"] = sdkVersion;
    jsonDoc["Autor"] = "X105GHM";

    String jsonResponse;
    serializeJson(jsonDoc, jsonResponse);

    server.send(200, "application/json", jsonResponse);
}


