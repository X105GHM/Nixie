#include "HTTP/HTTP.h"
#include "ACP/ACP.h"
#include "HSS/HSS.h"
#include "Digit_Control/Digit.h"
#include <WiFiManager.h>

extern bool displayEnabled;
extern float HSS_V;
extern TaskHandle_t timeTaskHandle;

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

    server.on("/set/ACP", HTTP_GET, [this]() {

        if(displayEnabled)
        {
            vTaskSuspend(timeTaskHandle);
            server.send(200, "text/plain", "ACP Routine");
            dacWrite(PIN_JFET, ACP_Voltage);
            ACP();
            dacWrite(PIN_JFET, Operating_Voltage);
            vTaskResume(timeTaskHandle);
        }
        else
        {
           server.send(200, "text/plain", "Display is not enabled"); 
        }
    });

    server.on("/set/DATE", HTTP_GET, [this]() {

        if(displayEnabled)
        {
            vTaskSuspend(timeTaskHandle);
            server.send(200, "text/plain", "Date is shown");
            displayDate();
            delay(5000);
            vTaskResume(timeTaskHandle);
        }
        else
        {
           server.send(200, "text/plain", "Display is not enabled"); 
        }
    });

    server.on("/set/BUZZER", HTTP_GET, [this]() {
        xSemaphoreGive(gongSemaphore);
    });

    server.on("/get/info", HTTP_GET, [this]() {
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

    String jsonResponse = "{\n";
    jsonResponse += "  \"Chip\": \"" + chipModel + "\",\n";
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
    jsonResponse += "  \"SdkVersion\": \"" + sdkVersion + "\",\n";
    jsonResponse += "  \"Autor\": \"" + String("X105GHM") + "\"\n";
    jsonResponse += "}";

    server.send(200, "application/json", jsonResponse);
}