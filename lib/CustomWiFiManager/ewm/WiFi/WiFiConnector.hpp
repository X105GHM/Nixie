#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <vector>
#include <functional>
#include "ewm/Types.hpp"

namespace ewm
{
    class WiFiConnector
    {
    public:
        explicit WiFiConnector(SemaphoreHandle_t wifiMutex);

        bool initSta(const String& hostname);
        bool safeSwitchMode(wifi_mode_t targetMode);

        std::vector<String> scanVisibleUnique();

        // Versucht alle Credentials; ruft onSuccess mit dem tatsächlich verbundenen Credential auf.
        bool tryConnectAll(
            const std::vector<Credential>& creds,
            uint32_t connectTimeoutMs,
            uint32_t betweenRetryMs,
            bool requireInternet,
            const char* probeHost,
            uint16_t probePort,
            uint32_t internetTimeoutMs,
            std::function<void(const Credential&)> onSuccess
        );

        bool hasInternet(const char* host, uint16_t port, uint32_t timeoutMs);

        bool beginConnectAsync(const String& ssid, const String& pass);

        wl_status_t status() const { return WiFi.status(); }

    private:
        bool waitForConnectedOrFail(uint32_t timeoutMs);
        bool tryConnectOneWithInternet(
            const Credential& c,
            uint32_t connectTimeoutMs,
            bool requireInternet,
            const char* probeHost,
            uint16_t probePort,
            uint32_t internetTimeoutMs
        );

        SemaphoreHandle_t wifiMutex_{nullptr};
    };
}
