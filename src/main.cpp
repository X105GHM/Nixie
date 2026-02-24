#define configCHECK_FOR_STACK_OVERFLOW 2

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
#include "StatsMonitor/StatsMonitor.hpp"

static constexpr LoggerType LOGTYPE = LoggerType::GENERAL;

extern "C" void idle_task(void* pvParameters);

static void initTime() 
{
    const char* tzSpec = Globals::getPosixTZ(Globals::currentTimeZone);
    ntpClient.initTime(tzSpec);
    Logger::log(LOGTYPE, ("NTP initialized with TZ: %s"), tzSpec);
}

static void buttonTask(void *pvParameters) 
{
    buttonPoll.init();
    buttonPoll.bindUserActions();
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

static TaskHandle_t statsMonitorHandle = nullptr;

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
        Globals::applyLogConfig();
    }

    WiFi.mode(WIFI_STA);
    wifiConnector.connect();

    initTime();

    // Loadcheck
    {
        auto readVoltage = [](){ return supplyWatch.readUHSS(); };
        bool hasLoad = hssController.testLoad(readVoltage, 125.0f /*Threshold in Volt*/, 28 /*28 ms → schneller Abfall = Last*/, 50 /*50 ms → maximal warten*/);
        Globals::loadDetected = hasLoad;
        Logger::log(LoggerType::HSS, hasLoad ? F("LoadTest: Load detected") : F("LoadTest: No load detected"));
    }

    displayEnabled = true;

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreatePinnedToCore(ClockControl::clockTask, "ClockTask", 4096, &clockControl, 2, &clockTaskHandle, 0);
    Logger::log(LOGTYPE, F("ClockTask started"));

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 4096, nullptr, 0, &buttonTaskHandle, 0);
    Logger::log(LOGTYPE, F("ButtonTask started"));

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreatePinnedToCore(brownoutStarter, "BrownoutStarter", 2048, nullptr, 4, &brownoutTaskHandle, 0);
    Logger::log(LOGTYPE, F("BrownoutStarter Task started"));

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreatePinnedToCore(idle_task, "IdleLoad0",2048, nullptr,tskIDLE_PRIORITY, nullptr,0);
    xTaskCreatePinnedToCore(idle_task, "IdleLoad1",2048, nullptr, tskIDLE_PRIORITY, nullptr,1);
    Logger::log(LOGTYPE, F("CPU-Load Task started"));

    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreatePinnedToCore(httpTask, "HTTPTask", 16384, nullptr, 1, &httpTaskHandle, 0);
    Logger::log(LOGTYPE, F("HTTPTask started"));

    vTaskDelay(pdMS_TO_TICKS(100));

    Logger::log(LOGTYPE, F("DisplayDigits Task started"));
    // displayDigitsTask belegt nach dem Start (Core 1, höhere Prio) die CPU so stark;
    // dadurch wird setup()/loopTask verdrängt und die folgenden Zeilen werden verzögert oder nie ausgeführt.
    xTaskCreatePinnedToCore(displayDigitsTask, "DisplayDigits", 4096, nullptr, 3, &displayTaskHandle, 1);
}

void loop() 
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}