#pragma once

#include <WebServer.h>
#include <SPIFFS.h>
#include <cstdlib>
#include "Logger/Logger.hpp"
#include "SupplyWatch/SupplyWatch.hpp"
#include "Brownout/Brownout.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "ClockControl/ClockControl.hpp"    
#include "EnergyMonitor/EnergyMonitor.hpp"  
#include "ACP/ACP.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFiManager.h>
#include "Digits/Digits.hpp"
#include "Temperature/Temperature.hpp"
#include "Globals/Globals.hpp"
#include "OTA/OTA.hpp"

extern bool displayEnabled;
extern bool enable160V;
extern bool enable190V;
extern bool enableResistor;

extern PersistentStorage storage;
extern SupplyWatch       supplyWatch;
extern HSS               hssController;
extern ClockControl      clockControl;
extern EnergyMonitor     energyMonitor;
extern NtcThermistor     temperatureSensor;

extern TaskHandle_t clockTaskHandle;

static const char* firmwareTargetToString(Globals::FirmwareTarget target) 
{
    switch (target) 
    {
        case Globals::FirmwareTarget::NixieV6_std: return "NixieV6_std";
        case Globals::FirmwareTarget::NixieV6_dev: return "NixieV6_dev";
        case Globals::FirmwareTarget::NixieV6_BOS: return "NixieV6_BOS";
        default:                                   return "Unknown";
    }
}

class HTTPHandler {
public:
    explicit HTTPHandler(int port = 80) noexcept;
    void begin() noexcept;
    void handleClient() noexcept;

private:
    WebServer server_;
    bool handleFileRead(const String &path) noexcept;
    String getContentType(const String &filename) noexcept;

    void handleReset() noexcept;
    void handleInfo() noexcept;
};
