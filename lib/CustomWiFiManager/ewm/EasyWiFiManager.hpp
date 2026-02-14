#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ewm/Types.hpp"
#include "ewm/Storage/CredentialStorage.hpp"
#include "ewm/Core/CredentialStore.hpp"
#include "ewm/WiFi/WiFiConnector.hpp"
#include "ewm/Portal/CaptivePortal.hpp"
#include "ewm/Monitor/ConnectivityMonitor.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"

namespace ewm
{
    class EasyWiFiManager
    {
    public:
        using ConnectCallback = std::function<void(const IPAddress& ip)>;

        static EasyWiFiManager& instance();

        void setHostname(const String& name);
        void setAPCredentials(const String& apSsid, const String& apPass = "");
        void setInternetProbe(const char* host, uint16_t port = 443);

        void begin(uint32_t connectTimeoutMs = 12000, uint32_t betweenRetryMs = 1500);
        void startConfigPortal();

        bool addCredential(const String& ssid, const String& password, uint8_t priority = 254);
        bool eraseAll();
        bool removeCredential(const String& ssid);
        std::vector<Credential> listCredentials() const;

        wl_status_t status() const;

        void onConnect(ConnectCallback cb);

        void setBackgroundAP(bool enabled);

        void setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs = 10000, uint32_t internetTimeoutMs = 15000);
        void onNoConnectivity(std::function<void()> cb, uint32_t delayMs = 300000);

        void setRequireInternetOnConnect(bool enabled);

        void setPortalUiConfig(const ewm::portal::PortalUiConfig& cfg);
        void setPortalUiConfigJson(const String& json);


    private:
        EasyWiFiManager();

        uint32_t nowSeconds_() const;

        void ensureAPState_();
        void roamTryAll_();

        void portalConnectRequest_(const String& ssid, const String& pass, uint8_t prio);
        void portalOnStaConnected_();

        String pendingSsid_;
        String pendingPass_;
        uint8_t pendingPrio_{100};
        bool pendingSave_{false};

        String hostname_{"esp32-setup"};
        String apSsid_{"ESP32-Setup"};
        String apPass_{""};
        String portalUiCfgJson_{"{}"};

        bool backgroundAP_{false};

        const char* probeHost_{"1.1.1.1"};
        uint16_t probePort_{443};
        bool requireInternetOnConnect_{false};

        SemaphoreHandle_t wifiMutex_{nullptr};

        CredentialStorage storage_;
        CredentialStore store_;
        WiFiConnector wifi_;
        CaptivePortal portal_;
        ConnectivityMonitor monitor_;

        ConnectCallback onConnectCb_;
    };
}
