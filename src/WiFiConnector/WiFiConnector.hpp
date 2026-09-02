#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "CustomWiFiManager.h"
#include "Logger/Logger.hpp"
#include "Memory/Memory.hpp"

class WiFiConnector
{
public:
    explicit WiFiConnector(const char* apSsid = "Nixie Clock", const char* apPass = nullptr) noexcept;

    void connect() noexcept;
    void eraseCredentials() noexcept;

    std::vector<ewm::Credential> getSavedNetworks() const noexcept;
    bool addOrUpdateNetwork(const std::string& ssid, const std::string& password, uint8_t priority = 100) noexcept;
    bool removeNetwork(const std::string& ssid) noexcept;

    bool isConnected() const noexcept;
    bool waitUntilConnected(TickType_t timeoutTicks = portMAX_DELAY) const noexcept;
    bool waitUntilApplicationNetworkReady(TickType_t timeoutTicks = portMAX_DELAY) const noexcept;

    std::string getWiFiSSID() const noexcept;
    std::string getWiFiPass() const noexcept;
    std::string getIpAddress() const noexcept;

private:
    std::atomic<bool> mdnsStarted_{false};
    std::atomic<bool> mdnsStartPending_{false};
    const char* apSsid_;
    const char* apPass_;
    static constexpr LoggerType logType_ = LoggerType::WiFi;

    void startMDNSWithCollisionCheck_(const char* baseName) noexcept;
    void startMDNSOnce_() noexcept;
    static void mdnsStartTask_(void* context) noexcept;
};
