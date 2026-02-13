#include "ewm/Monitor/ConnectivityMonitor.hpp"
#include "ewm/Log.hpp"
#include <Arduino.h>
#include <WiFi.h>

namespace ewm
{
    void ConnectivityMonitor::configure(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs)
    {
        enabled_ = enabled;
        checkIntervalMs_ = checkIntervalMs;
        internetTimeoutMs_ = internetTimeoutMs;
    }

    void ConnectivityMonitor::setOnNoConnectivity(std::function<void()> cb, uint32_t delayMs)
    {
        onNoConn_ = std::move(cb);
        noConnSince_ = 0;
        noConnDelayMs_ = delayMs;
    }

    void ConnectivityMonitor::setCheckFn(std::function<bool(uint32_t)> hasInternetFn)
    {
        hasInternetFn_ = std::move(hasInternetFn);
    }

    void ConnectivityMonitor::setRoamFn(std::function<void()> roamFn)
    {
        roamFn_ = std::move(roamFn);
    }

    void ConnectivityMonitor::startIfNeeded()
    {
        if (!enabled_ || task_) return;
        if (WiFi.status() != WL_CONNECTED) return;

        xTaskCreatePinnedToCore(taskThunk, "ewm_mon", 4096, this, 1, &task_, ARDUINO_RUNNING_CORE);
        EWM_LOG("Monitor task started");
    }

    void ConnectivityMonitor::taskThunk(void* arg)
    {
        static_cast<ConnectivityMonitor*>(arg)->loop_();
    }

    void ConnectivityMonitor::loop_()
    {
        for (;;)
        {
            bool ok = (WiFi.status() == WL_CONNECTED);

            if (ok && requireInternet_ && hasInternetFn_)
                ok = hasInternetFn_(internetTimeoutMs_);

            if (!ok)
            {
                if (noConnSince_ == 0) noConnSince_ = millis();

                EWM_LOG("Monitor: not ok -> roam");
                if (roamFn_) roamFn_();

                if (onNoConn_ && (millis() - noConnSince_ >= noConnDelayMs_))
                {
                    EWM_LOG("Monitor: no connectivity too long -> callback");
                    onNoConn_();
                    noConnSince_ = millis();
                }
            }
            else
            {
                noConnSince_ = 0;
            }

            vTaskDelay(pdMS_TO_TICKS(checkIntervalMs_));
        }
    }
}
