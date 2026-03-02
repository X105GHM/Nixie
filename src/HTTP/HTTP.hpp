#pragma once

#include <WebServer.h>
#include <SPIFFS.h>
#include <cstdlib>
#include <regex>
#include <functional>
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

    static void runWithClockSuspended(TaskHandle_t clockHandle, std::function<void()> taskFunc)
    {
        auto wrapper = [](void *param)
        {
            auto [handle, func] = *static_cast<std::pair<TaskHandle_t, std::function<void()>> *>(param);
            delete static_cast<std::pair<TaskHandle_t, std::function<void()>> *>(param);

            vTaskSuspend(handle);
            func();
            vTaskResume(handle);

            Logger::log(LoggerType::Webserver, "runWithClockSuspended abgeschlossen");
            vTaskDelete(nullptr);
        };

        auto *params = new std::pair<TaskHandle_t, std::function<void()>>(clockHandle, std::move(taskFunc));
        xTaskCreate(wrapper, "RunWithClockSuspended", 4096, params, 1, nullptr);
    }
};
