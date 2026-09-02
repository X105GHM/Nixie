#pragma once

#include <cstdlib>
#include <regex>
#include <functional>
#include <memory>
#include <new>
#include "Logger/Logger.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "ACP/ACP.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Digits/Digits.hpp"
#include "Globals/Globals.hpp"
#include "OTA/OTA.hpp"
#include "StatsMonitor/StatsMonitor.hpp"
#include "WiFiConnector/WiFiConnector.hpp"
#include "AppState/AppState.hpp"
#include "HTTP/HttpCommandQueue.hpp"
#include "HTTP/NativeHttpServer.hpp"
#include "HTTP/SecureApi.hpp"
#include "HTTP/StaticFileServer.hpp"

extern HSS hssController;
extern WiFiConnector wifiConnector;

extern TaskHandle_t clockTaskHandle;

class HTTPHandler
{
public:
    explicit HTTPHandler(int port = 80) noexcept;
    void begin() noexcept;

private:
    NativeHttpServer server_;
    StaticFileServer staticFiles_;
    HttpCommandQueue commands_;
    SecureApi secureApi_;

    bool executeCommand(const std::function<void()>& action) noexcept;
    bool enqueueCommand(std::function<void()> action) noexcept;
    void sendCommandError(HttpCommandQueue::Result result) noexcept;
    void handleReset(std::string plannedReason) noexcept;

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

        BaseType_t result = xTaskCreate(wrapper, "RunWithClockSuspended", 8192, params, 1, nullptr);

        if (result != pdPASS)
        {
            delete params;
            Logger::log(LoggerType::Webserver, "runWithClockSuspended: failed to create task");
            return false;
        }

        return true;
    }
};
