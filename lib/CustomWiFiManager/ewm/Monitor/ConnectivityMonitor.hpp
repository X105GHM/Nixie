#pragma once
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ewm
{
    class ConnectivityMonitor
    {
    public:
        void configure(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs);
        void setRequireInternet(bool require) { requireInternet_ = require; }

        void setOnNoConnectivity(std::function<void()> cb, uint32_t delayMs);

        // callbacks vom Manager
        void setCheckFn(std::function<bool(uint32_t)> hasInternetFn);
        void setRoamFn(std::function<void()> roamFn);

        void startIfNeeded();
        bool running() const { return task_ != nullptr; }

    private:
        static void taskThunk(void* arg);
        void loop_();

    private:
        bool enabled_{false};
        bool requireInternet_{false};

        uint32_t checkIntervalMs_{10000};
        uint32_t internetTimeoutMs_{15000};

        std::function<bool(uint32_t)> hasInternetFn_;
        std::function<void()> roamFn_;

        std::function<void()> onNoConn_;
        uint32_t noConnSince_{0};
        uint32_t noConnDelayMs_{300000};

        TaskHandle_t task_{nullptr};
    };
}
