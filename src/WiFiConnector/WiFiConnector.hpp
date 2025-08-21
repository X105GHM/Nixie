#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include "Logger/Logger.hpp"
#include "CustomWiFiManager.h"
#include "Memory/Memory.hpp"

class WiFiConnector
{
public:
    explicit WiFiConnector(const char *apSsid = "Nixie Clock",
                           const char *apPass = nullptr) noexcept;

    void connect() noexcept;

private:
    const char *apSsid_;
    const char *apPass_;
    static constexpr LoggerType logType_ = LoggerType::WiFi;

    void startMDNSWithCollisionCheck_(const char* baseName) noexcept;
};
