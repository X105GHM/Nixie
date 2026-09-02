#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "ewm/Types.hpp"

namespace ewm
{
    class WiFiConnector
    {
    public:
        using EventCallback = std::function<void()>;

        explicit WiFiConnector(SemaphoreHandle_t wifiMutex);

        bool initSta(const std::string& hostname);
        bool safeSwitchMode(wifi_mode_t targetMode);
        void setEventCallback(EventCallback callback);

        std::vector<ScanResult> scanNetworks();
        std::vector<std::string> scanVisibleUnique();

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

        bool hasInternet(const char* host, uint16_t port, uint32_t timeoutMs) const;
        bool beginConnectAsync(const std::string& ssid, const std::string& pass);
        void disconnect();

        bool startAccessPoint(const std::string& ssid, const std::string& pass);
        bool stopAccessPoint();

        bool isConnected() const noexcept { return connected_.load(std::memory_order_acquire); }
        bool isInitialized() const noexcept { return initialized_.load(std::memory_order_acquire); }
        std::string staIpAddress() const;
        std::string apIpAddress() const;
        std::string connectedSsid() const;
        int rssi() const;
        wifi_mode_t mode() const;
        uint8_t lastDisconnectReason() const noexcept { return lastDisconnectReason_.load(std::memory_order_acquire); }

    private:
        static constexpr EventBits_t CONNECTED_BIT = BIT0;
        static constexpr EventBits_t FAILED_BIT = BIT1;

        static void eventHandler(
            void* arg,
            esp_event_base_t eventBase,
            int32_t eventId,
            void* eventData
        );
        void handleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData);

        bool beginConnection(const char* ssid, const char* pass, bool preserveAccessPoint);
        bool waitForConnectedOrFail(uint32_t timeoutMs);
        bool tryConnectOneWithInternet(
            const Credential& credential,
            uint32_t connectTimeoutMs,
            bool requireInternet,
            const char* probeHost,
            uint16_t probePort,
            uint32_t internetTimeoutMs
        );
        void notifyEvent();

        SemaphoreHandle_t wifiMutex_{nullptr};
        EventGroupHandle_t connectionEvents_{nullptr};
        esp_netif_t* staNetif_{nullptr};
        esp_netif_t* apNetif_{nullptr};
        esp_event_handler_instance_t wifiEventInstance_{nullptr};
        esp_event_handler_instance_t ipEventInstance_{nullptr};

        std::atomic<bool> initialized_{false};
        std::atomic<bool> connected_{false};
        std::atomic<bool> suppressDisconnectEvent_{false};
        std::atomic<uint8_t> lastDisconnectReason_{0};
        EventCallback eventCallback_;
    };
}
