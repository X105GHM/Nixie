#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <new>

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
#include "Diagnostics/ResetDiagnostics.hpp"

static constexpr LoggerType LOGTYPE = LoggerType::SYSTEM;
static constexpr uint32_t TIME_SYNC_TASK_STACK_BYTES = 6144;
static constexpr uint32_t CLOCK_TASK_STACK_BYTES = 8192;
static constexpr uint32_t BUTTON_TASK_STACK_BYTES = 6144;
static constexpr uint32_t BROWNOUT_STARTER_TASK_STACK_BYTES = 6144;
static constexpr uint32_t HTTP_START_TASK_STACK_BYTES = 16384;
static constexpr uint32_t STATS_TASK_STACK_BYTES = 8192;
static constexpr uint32_t DISPLAY_TASK_STACK_BYTES = 8192;

static bool createPinnedTask(TaskFunction_t task, const char* name, uint32_t stackDepth, void* parameter, UBaseType_t priority, TaskHandle_t* handle, BaseType_t core)
{
    const BaseType_t result = xTaskCreatePinnedToCore(
        task, name, stackDepth, parameter, priority, handle, core);
    if (result != pdPASS)
    {
        Logger::log(LOGTYPE, "Failed to create task %s", name);
        return false;
    }
    return true;
}

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

static void httpStartTask(void *pvParameters)
{
    wifiConnector.waitUntilApplicationNetworkReady();
    httpHandler.begin();
    Logger::log(
        LOGTYPE,
        "HTTPStart initialization returned, minimum free stack=%u bytes",
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    vTaskDelete(nullptr);
}

static void timeSyncTask(void *pvParameters)
{
    wifiConnector.waitUntilConnected();
    initTime();
    vTaskDelete(nullptr);
}

static void brownoutStarter(void *pvParameters) 
{
    brownoutInstance = new (std::nothrow) Brownout(supplyWatch, hssController, storage, 1000);
    if (!brownoutInstance)
    {
        Logger::log(LOGTYPE, "Brownout service allocation failed");
        vTaskDelete(nullptr);
        return;
    }
    brownoutInstance->start();
    vTaskDelete(nullptr);
}

static void statsTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(10000));
    for (;;)
    {
        auto& sm = StatsMonitor::instance();
        sm.update();
        sm.sampleTaskTimes(1000, true);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{

    Logger::begin();
    Logger::log(LOGTYPE, "System start");
    Logger::log(LOGTYPE, "ESP-IDF: %s", esp_get_idf_version());

    const size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psramTotal > 0)
    {
        const size_t psramFree  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        Logger::log(LOGTYPE, "PSRAM OK: total=%u bytes, free=%u bytes", static_cast<unsigned>(psramTotal), static_cast<unsigned>(psramFree));
    }
    else
    {
        Logger::log(LOGTYPE, "PSRAM NOT FOUND / init failed");
    }


    if (storage.init() != ESP_OK) 
    {
        Logger::log(LOGTYPE, "PersistentStorage init failed");
    }
    else 
    {
        Memory::loadGlobals();
        Globals::applyLogConfig();
    }

    ResetDiagnostics::instance().initialize();

    wifiConnector.connect();
    ntpClient.applyTimeZone(Globals::getPosixTZ(Globals::currentTimeZone));
    createPinnedTask(timeSyncTask, "TimeSync", TIME_SYNC_TASK_STACK_BYTES, nullptr, 1, nullptr, 0);

    displayEnabled = true;

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(ClockControl::clockTask, "ClockTask", CLOCK_TASK_STACK_BYTES, &clockControl, 2, &clockTaskHandle, 0))
        Logger::log(LOGTYPE, "ClockTask started");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(buttonTask, "ButtonTask", BUTTON_TASK_STACK_BYTES, nullptr, 0, nullptr, 0))
        Logger::log(LOGTYPE, "ButtonTask started");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(brownoutStarter, "BrownoutStarter", BROWNOUT_STARTER_TASK_STACK_BYTES, nullptr, 4, nullptr, 0))
        Logger::log(LOGTYPE, "BrownoutStarter Task started");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(
            httpStartTask, "HTTPStart", HTTP_START_TASK_STACK_BYTES, nullptr, 1, nullptr, 0))
        Logger::log(LOGTYPE, "HTTP start task created");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(statsTask, "StatsTask", STATS_TASK_STACK_BYTES, nullptr, 1, nullptr, 0))
        Logger::log(LOGTYPE, "StatsTask started");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (createPinnedTask(displayDigitsTask, "DisplayDigits", DISPLAY_TASK_STACK_BYTES, nullptr, 20, &displayTaskHandle, 1))
        Logger::log(LOGTYPE, "DisplayDigits Task started");

    // Loadcheck
    {
        auto readVoltage = [](){ return supplyWatch.readUHSS(); };
        bool hasLoad = hssController.testLoad(readVoltage, 130.0f /*Threshold in Volt*/, 35 /*35 ms → schneller Abfall = Last*/, 80 /*80 ms → maximal warten*/);
        Globals::loadDetected = hasLoad;
        Logger::log(LoggerType::HSS, hasLoad ? "LoadTest: Load detected" : "LoadTest: No load detected");
    }

    vTaskDelete(nullptr);
}
