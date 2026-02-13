#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include "Logger/Logger.hpp"
#include "CustomWiFiManager.h"
#include "Memory/Memory.hpp"

class WiFiConnector
{
public:
    explicit WiFiConnector(const char *apSsid = "Nixie Clock", const char *apPass = nullptr) noexcept;

    void connect() noexcept;

    void eraseCredentials() noexcept;

    std::vector<ewm::Credential> getSavedNetworks() const noexcept;

    bool addOrUpdateNetwork(const String &ssid, const String &password, uint8_t priority = 100) noexcept;

    bool removeNetwork(const String &ssid) noexcept;

    inline String getWiFiSSID() noexcept
    {
        if (WiFi.status() == WL_CONNECTED)
            return WiFi.SSID();
        return String();
    }

    inline String getWiFiPass() noexcept
    {
        const String ssid = getWiFiSSID();
        if (ssid.isEmpty())
            return String();

        for (const auto &c : ewm::EasyWiFiManager::instance().listCredentials())
        {
            if (ssid.equals(c.ssid))
            {
                return String(c.password);
            }
        }
        return String();
    }

    inline String getIpAddress() noexcept
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            return WiFi.localIP().toString();
        }

        wifi_mode_t mode = WiFi.getMode();
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
        {
            return WiFi.softAPIP().toString();
        }

        return String();
    }

private:
    
    bool mdnsStarted_ = false;
    const char *apSsid_;
    const char *apPass_;
    static constexpr LoggerType logType_ = LoggerType::WiFi;

    void startMDNSWithCollisionCheck_(const char *baseName) noexcept;
    void startMDNSOnce_() noexcept;
};
