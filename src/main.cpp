#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Logger/Logger.hpp"
#include "WiFiConnector/WiFiConnector.hpp"
#include "Digits/Digits.hpp"
#include "ClockControl/ClockControl.hpp"
#include "Button/Button.hpp"
#include "SupplyWatch/SupplyWatch.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "Brownout/Brownout.hpp"
#include "TimeWeather/NTPClient/NTPClient.hpp"
#include "HTTP/HTTP.hpp"
#include "Acoustics/Relay/Relay.hpp"
#include "Temperature/Temperature.hpp"
#include "OTA/OTA.hpp"
#include "Globals/SharedObjects.hpp"

static constexpr LoggerType LOGTYPE = LoggerType::GENERAL;

static void initTime() 
{
    ntpClient.initTime("CET-1CEST,M3.5.0,M10.5.0/3");
    Logger::log(LOGTYPE, F("NTP initialized"));
}

static void buttonTask(void *pvParameters) 
{
    buttonPoll.init();
    for (;;) 
    {
        buttonPoll.poll();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void httpTask(void *pvParameters) 
{
    httpHandler.begin();
    for (;;) 
    {
        httpHandler.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void brownoutStarter(void *pvParameters) 
{
    brownoutInstance = new Brownout(supplyWatch, hssController, storage, 1000);
    brownoutInstance->start();
    vTaskDelete(nullptr);
}

void setup() 
{
    Serial.begin(115200);

    Logger::begin(Serial);
    Logger::log(LOGTYPE, F("System start"));

    if (storage.init() != ESP_OK) 
    {
        Logger::log(LOGTYPE, "PersistentStorage init failed");
    }
    else 
    {
        Memory::loadGlobals();
    }

    WiFi.mode(WIFI_STA);
    wifiConnector.connect();

    initTime();

    {
        auto readVoltage = []() -> float {
            return supplyWatch.readUHSS();
        };
        bool hasLoad = hssController.testLoad(readVoltage, 200/*ms*/, 127.0f/*V*/);
        Globals::loadDetected = hasLoad;
        Logger::log(LoggerType::HSS, hasLoad ? F("LoadTest: Last erkannt") : F("LoadTest: Keine Last erkannt"));
    }

    displayEnabled = true;

    xTaskCreatePinnedToCore(ClockControl::clockTask, "ClockTask", 2048, &clockControl, 2, &clockTaskHandle, 0);
    Logger::log(LOGTYPE, F("ClockTask started"));

    xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 2048, nullptr, 1, &buttonTaskHandle, 0);
    Logger::log(LOGTYPE, F("ButtonTask started"));

    xTaskCreatePinnedToCore(brownoutStarter, "BrownoutStarter", 2048, nullptr, 4, &brownoutTaskHandle, 0);
    Logger::log(LOGTYPE, F("BrownoutStarter Task started"));

    xTaskCreatePinnedToCore(httpTask, "HTTPTask", 8192, nullptr, 0, &httpTaskHandle, 0);
    Logger::log(LOGTYPE, F("HTTPTask started"));

    xTaskCreatePinnedToCore(displayDigitsTask, "DisplayDigits", 4096, nullptr, 3, &displayTaskHandle, 1);
    Logger::log(LOGTYPE, F("DisplayDigits Task started"));
}

void loop() 
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
