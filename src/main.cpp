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
static constexpr uint32_t LOAD_TEST_TIMEOUT_MS = 500;
static constexpr float    LOAD_TEST_THRESHOLD_V = 130.0f;

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

    gpio_set_level(GPIO_NUM_13, 0);

    if (storage.init() != ESP_OK) 
    {
        Logger::log(LOGTYPE, "PersistentStorage init failed");
    }
    else 
    {
        storage.load();
    }

    WiFi.mode(WIFI_STA);
    wifiConnector.connect();

    initTime();

    // Trigger load test
    {
        auto readVoltage = []() -> float {
            return supplyWatch.readUHSS();
        };
        bool hasLoad = hssController.testLoad(readVoltage, LOAD_TEST_TIMEOUT_MS, LOAD_TEST_THRESHOLD_V);
        Globals::loadDetected = hasLoad;
        Logger::log(LoggerType::HSS, hasLoad ? F("LoadTest: Last erkannt") : F("LoadTest: Keine Last erkannt"));
    }
/*
    // Tasks erstellen
    xTaskCreatePinnedToCore(displayDigitsTask, "DisplayDigits", 4096, nullptr, 3, &displayTaskHandle, 0);
    Logger::log(LOGTYPE, F("DisplayDigits Task started"));
*/
    xTaskCreatePinnedToCore(ClockControl::clockTask, "ClockTask", 2048, &clockControl, 2, &clockTaskHandle, 1);
    Logger::log(LOGTYPE, F("ClockTask started"));

    xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 2048, nullptr, 1, &buttonTaskHandle, 1);
    Logger::log(LOGTYPE, F("ButtonTask started"));

    xTaskCreatePinnedToCore(brownoutStarter, "BrownoutStarter", 2048, nullptr, 4, &brownoutTaskHandle, 1);
    Logger::log(LOGTYPE, F("BrownoutStarter Task started"));

    xTaskCreatePinnedToCore(httpTask, "HTTPTask", 4096, nullptr, 0, &httpTaskHandle, 1);
    Logger::log(LOGTYPE, F("HTTPTask started"));

    displayEnabled = true;
}

void loop() 
{
    vTaskDelay(pdMS_TO_TICKS(10000));
}
