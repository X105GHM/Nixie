#pragma once

#include <WebServer.h>
#include <SPIFFS.h>
#include <cstdlib>
#include <regex>
#include <functional>
#include <memory>
#include <new>
#include "Logger/Logger.hpp"
#include "SupplyWatch/SupplyWatch.hpp"
#include "Brownout/Brownout.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "ClockControl/ClockControl.hpp"
#include "EnergyMonitor/EnergyMonitor.hpp"
#include "ACP/ACP.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Digits/Digits.hpp"
#include "Temperature/Temperature.hpp"
#include "Globals/Globals.hpp"
#include "OTA/OTA.hpp"
#include "StatsMonitor/StatsMonitor.hpp"
#include "WiFiConnector/WiFiConnector.hpp"

extern Memory::PersistentStorage storage;
extern SupplyWatch supplyWatch;
extern HSS hssController;
extern ClockControl clockControl;
extern EnergyMonitor energyMonitor;
extern NtcThermistor temperatureSensor;
extern WiFiConnector wifiConnector;

extern TaskHandle_t clockTaskHandle;

[[maybe_unused]] static const char *firmwareTargetToString(Globals::FirmwareTarget target)
{
    switch (target)
    {
    case Globals::FirmwareTarget::NixieV6_std:
        return "NixieV6_std";
    case Globals::FirmwareTarget::NixieV6_dev:
        return "NixieV6_dev";
    case Globals::FirmwareTarget::NixieV6_BOS:
        return "NixieV6_BOS";
    default:
        return "Unknown";
    }
}

class HTTPHandler
{
public:
    explicit HTTPHandler(int port = 80) noexcept;
    void begin() noexcept;
    void handleClient() noexcept;

private:
    WebServer server_;
    bool handleFileRead(const String &path) noexcept;
    String getContentType(const String &filename) noexcept;

    void handleReset() noexcept;
    void handleInfo() noexcept;

    static bool runWithClockSuspended(TaskHandle_t clockHandle, std::function<void()> taskFunc)
    {
        if (!taskFunc)
        {
            Logger::log(LoggerType::Webserver, "runWithClockSuspended: empty task function");
            return false;
        }

        struct Params
        {
            TaskHandle_t handle;
            std::function<void()> func;
        };

        struct SuspendGuard
        {
            TaskHandle_t handle;

            explicit SuspendGuard(TaskHandle_t h) : handle(h)
            {
                if (handle != nullptr)
                {
                    vTaskSuspend(handle);
                }
            }

            ~SuspendGuard()
            {
                if (handle != nullptr)
                {
                    vTaskResume(handle);
                }
            }

            SuspendGuard(const SuspendGuard &) = delete;
            SuspendGuard &operator=(const SuspendGuard &) = delete;
        };

        auto wrapper = [](void *param)
        {
            std::unique_ptr<Params> params(static_cast<Params *>(param));

            {
                SuspendGuard guard(params->handle);
                params->func();
            }

            Logger::log(LoggerType::Webserver, "runWithClockSuspended completed");
            vTaskDelete(nullptr);
        };

        auto *params = new (std::nothrow) Params
        {
            clockHandle,
            std::move(taskFunc)
        };

        if (params == nullptr)
        {
            Logger::log(LoggerType::Webserver, "runWithClockSuspended: failed to allocate task parameters");
            return false;
        }

        BaseType_t result = xTaskCreate(wrapper, "RunWithClockSuspended", 4096, params, 1, nullptr);

        if (result != pdPASS)
        {
            delete params;
            Logger::log(LoggerType::Webserver, "runWithClockSuspended: failed to create task");
            return false;
        }

        return true;
    }
};
