#pragma once

#include <atomic>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ewm/Core/CredentialStore.hpp"
#include "ewm/Monitor/ConnectivityMonitor.hpp"
#include "ewm/Portal/CaptivePortal.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"
#include "ewm/Storage/CredentialStorage.hpp"
#include "ewm/Types.hpp"
#include "ewm/WiFi/WiFiConnector.hpp"

namespace ewm
{
    class EasyWiFiManager
    {
    public:
        using ConnectCallback = std::function<void(const std::string& ip)>;

        static EasyWiFiManager& instance();

        void setHostname(const std::string& name);
        void setAPCredentials(const std::string& apSsid, const std::string& apPass = {});
        void setInternetProbe(const char* host, uint16_t port = 443);

        void begin(uint32_t connectTimeoutMs = 12000, uint32_t betweenRetryMs = 1500);
        void startConfigPortal();

        bool addCredential(const std::string& ssid, const std::string& password, uint8_t priority = 254);
        bool eraseAll();
        bool removeCredential(const std::string& ssid);
        std::vector<Credential> listCredentials() const;

        bool isConnected() const noexcept { return wifi_.isConnected(); }
        State state() const noexcept { return state_.load(std::memory_order_acquire); }
        std::string staIpAddress() const { return wifi_.staIpAddress(); }
        std::string apIpAddress() const { return wifi_.apIpAddress(); }
        std::string connectedSsid() const { return wifi_.connectedSsid(); }
        int rssi() const { return wifi_.rssi(); }

        bool waitForConnected(TickType_t timeoutTicks) const;
        bool waitForApplicationNetworkReady(TickType_t timeoutTicks) const;

        void onConnect(ConnectCallback cb);
        void setBackgroundAP(bool enabled);
        void setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs = 10000, uint32_t internetTimeoutMs = 15000);
        void onNoConnectivity(std::function<void()> cb, uint32_t delayMs = 300000);
        void setRequireInternetOnConnect(bool enabled);
        void setPortalUiConfig(const ewm::portal::PortalUiConfig& cfg);
        void setPortalUiConfigJson(const std::string& json);

    private:
        static constexpr EventBits_t CONNECTED_BIT = BIT0;
        static constexpr EventBits_t APPLICATION_READY_BIT = BIT1;

        EasyWiFiManager();
        static void stateTaskEntry(void* arg);
        void stateLoop();

        uint32_t nowSeconds() const;
        void setState(State state);
        void notifyStateTask();
        bool runConnectionCycle(bool retry);
        bool startPortal();
        void finishPortalConnection();
        void handleSuccessfulCredential(const Credential& used);
        void ensureAPState();
        void requestRoam();

        void portalConnectRequest(const std::string& ssid, const std::string& pass, uint8_t priority);
        void portalOnStaConnected();

        std::string pendingSsid_;
        std::string pendingPass_;
        uint8_t pendingPriority_{100};
        bool pendingSave_{false};

        std::string hostname_{"esp32-setup"};
        std::string apSsid_{"ESP32-Setup"};
        std::string apPass_{};
        std::string portalUiConfigJson_{"{}"};
        std::string probeHost_{"1.1.1.1"};
        uint16_t probePort_{443};

        std::atomic<State> state_{State::Uninitialized};
        std::atomic<bool> backgroundAP_{false};
        std::atomic<bool> requireInternetOnConnect_{false};
        std::atomic<bool> forcePortalRequested_{false};
        std::atomic<bool> retryRequested_{false};
        std::atomic<bool> portalStopRequested_{false};

        uint32_t connectTimeoutMs_{12000};
        uint32_t betweenRetryMs_{1500};

        SemaphoreHandle_t wifiMutex_{nullptr};
        SemaphoreHandle_t credentialMutex_{nullptr};
        EventGroupHandle_t stateEvents_{nullptr};
        TaskHandle_t stateTask_{nullptr};

        CredentialStorage storage_;
        CredentialStore store_;
        WiFiConnector wifi_;
        CaptivePortal portal_;
        ConnectivityMonitor monitor_;
        ConnectCallback onConnectCallback_;
    };
}
