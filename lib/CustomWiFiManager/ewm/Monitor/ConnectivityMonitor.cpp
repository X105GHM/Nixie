#include "ewm/Monitor/ConnectivityMonitor.hpp"
#include "ewm/Log.hpp"
#include "ewm/Utils/Time.hpp"

namespace ewm
{
    void ConnectivityMonitor::configure(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs)
    {
        enabled_.store(enabled, std::memory_order_release);
        checkIntervalMs_ = checkIntervalMs;
        internetTimeoutMs_ = internetTimeoutMs;

        if (enabled_)
        {
            stopRequested_.store(false, std::memory_order_release);
        }
        else
        {
            stop();
        }
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

    void ConnectivityMonitor::setConnectedFn(std::function<bool()> connectedFn)
    {
        connectedFn_ = std::move(connectedFn);
    }

    void ConnectivityMonitor::setRoamFn(std::function<void()> roamFn)
    {
        roamFn_ = std::move(roamFn);
    }

    void ConnectivityMonitor::startIfNeeded()
    {
        if (!enabled_ || task_) return;

        stopRequested_.store(false, std::memory_order_release);
        TaskHandle_t task = nullptr;
        BaseType_t ok = xTaskCreatePinnedToCore(taskThunk, "ewm_mon", 6144, this, 1, &task, 0);
        if (ok == pdPASS)
        {
            task_.store(task, std::memory_order_release);
            EWM_LOG("Monitor task started");
        }
        else
            task_.store(nullptr, std::memory_order_release);
    }

    void ConnectivityMonitor::stop()
    {
        stopRequested_.store(true, std::memory_order_release);
    }

    void ConnectivityMonitor::taskThunk(void* arg)
    {
        static_cast<ConnectivityMonitor*>(arg)->loop_();
    }

    void ConnectivityMonitor::loop_()
    {
        for (;;)
        {
            if (stopRequested_.load(std::memory_order_acquire))
                break;

            if (!enabled_.load(std::memory_order_acquire))
            {
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }

            const bool stationConnected = connectedFn_ && connectedFn_();
            bool ok = stationConnected;

            if (ok && requireInternet_.load(std::memory_order_acquire) && hasInternetFn_)
                ok = hasInternetFn_(internetTimeoutMs_);

            if (!ok)
            {
                if (noConnSince_ == 0) noConnSince_ = ewm::utils::monotonicMillis();

                if (stationConnected && requireInternet_.load(std::memory_order_acquire))
                {
                    EWM_LOG("Monitor: not ok -> roam");
                    if (roamFn_) roamFn_();
                }

                if (onNoConn_ && (ewm::utils::monotonicMillis() - noConnSince_ >= noConnDelayMs_))
                {
                    EWM_LOG("Monitor: no connectivity too long -> callback");
                    onNoConn_();
                    noConnSince_ = ewm::utils::monotonicMillis();
                }
            }
            else
            {
                noConnSince_ = 0;
            }

            const uint32_t delayMs = checkIntervalMs_ ? checkIntervalMs_ : 1000;
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
        
        task_.store(nullptr, std::memory_order_release);
        stopRequested_.store(false, std::memory_order_release);
        EWM_LOG("Monitor task stopped");
        vTaskDelete(nullptr);
    }
}
