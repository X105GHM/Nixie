#include "HTTP.hpp"

static constexpr LoggerType LOGTYPE = LoggerType::Webserver;

HTTPHandler::HTTPHandler(int port) noexcept
    : server_(port)
{}

String HTTPHandler::getContentType(const String &filename) noexcept {
    if (filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css"))   return "text/css";
    if (filename.endsWith(".js"))    return "application/javascript";
    if (filename.endsWith(".png"))   return "image/png";
    if (filename.endsWith(".jpg"))   return "image/jpeg";
    if (filename.endsWith(".ico"))   return "image/x-icon";
    return "text/plain";
}

bool HTTPHandler::handleFileRead(const String &path) noexcept {
    String filePath = path;
    if (filePath.endsWith("/")) filePath += "index.html";
    if (!SPIFFS.exists(filePath)) return false;

    File file = SPIFFS.open(filePath, "r");
    String ct = getContentType(filePath);
    server_.streamFile(file, ct);
    file.close();
    return true;
}

void HTTPHandler::begin() noexcept 
{
    if (!SPIFFS.begin(true)) {
        Logger::log(LOGTYPE, F("SPIFFS Mount failed"));
    } else {
        Logger::log(LOGTYPE, F("SPIFFS ready"));
    }

    server_.onNotFound([this]() noexcept {
        if (!handleFileRead(server_.uri())) {
            server_.send(404, "text/plain", "404: File Not Found");
            Logger::log(LOGTYPE, "404 Not Found: %s", server_.uri().c_str());
        }
    });

    server_.on("/set/OFF", HTTP_GET, [this]() noexcept {
        displayEnabled = false;
        Logger::log(LOGTYPE, F("Display disabled via HTTP"));
        server_.send(200, "text/plain", "Display disabled");
    });

    server_.on("/set/ON", HTTP_GET, [this]() noexcept {
        displayEnabled = true;
        Logger::log(LOGTYPE, F("Display enabled via HTTP"));
        server_.send(200, "text/plain", "Display enabled");
    });

    server_.on("/set/reset", HTTP_GET, [this]() noexcept {
        Logger::log(LOGTYPE, F("System reset requested via HTTP"));       
        handleReset();
    });

    server_.on("/set/ACP", HTTP_GET, [this]() noexcept {
        if (!displayEnabled) {
            Logger::log(LOGTYPE, F("Error: Display not enabled for ACP"));
            server_.send(400, "text/plain", "Display not enabled");
            return;
        }
        vTaskSuspend(clockTaskHandle);
        Logger::log(LOGTYPE, F("ACP triggered via HTTP"));
        ACP();
        vTaskResume(clockTaskHandle);
        server_.send(200, "text/plain", "ACP routine completed");
    });

    server_.on("/set/DATE", HTTP_GET, [this]() noexcept {
        if (!displayEnabled) {
            Logger::log(LOGTYPE, F("Error: Display not enabled for DATE"));
            server_.send(400, "text/plain", "Display not enabled");
            return;
        }
        vTaskSuspend(clockTaskHandle);
        Logger::log(LOGTYPE, F("Date display requested via HTTP"));
        displayDate(); 
        vTaskDelay(pdMS_TO_TICKS(5000));
        vTaskResume(clockTaskHandle);
        server_.send(200, "text/plain", "Date shown");
    });

    server_.on("/set/zip", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("zip")) {
            server_.send(400, "text/plain", "Missing 'zip' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/zip fehlte Parameter 'zip'"));
            return;
        }
        String zipArg = server_.arg("zip");
        Globals::zipCode = std::string(zipArg.c_str());
        Logger::log(LOGTYPE, "ZIP-Code per HTTP gesetzt auf %s", zipArg.c_str());
        server_.send(200, "text/plain", "ZIP-Code updated");
    });

    server_.on("/set/ticker", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/ticker fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::tickerEnabled = (val != "0");
        Logger::log(LOGTYPE, "tickerEnabled set to %s via HTTP", 
                    Globals::tickerEnabled ? "true" : "false");
        server_.send(200, "text/plain", 
                     String("tickerEnabled=") + (Globals::tickerEnabled ? "1" : "0"));
    });

    server_.on("/set/timeLimit", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/timeLimit fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::timeLimitEnabled = (val != "0");
        Logger::log(LOGTYPE, "timeLimitEnabled set to %s via HTTP", 
                    Globals::timeLimitEnabled ? "true" : "false");
        server_.send(200, "text/plain", 
                     String("timeLimitEnabled=") + (Globals::timeLimitEnabled ? "1" : "0"));
    });

    server_.on("/set/silentMode", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/silentMode fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::SilentlModeEnabled = (val != "0");
        Logger::log(LOGTYPE, "silentModeEnabled set to %s via HTTP", 
                    Globals::SilentlModeEnabled ? "true" : "false");
        server_.send(200, "text/plain", 
                     String("SilentModeEnabled=") + (Globals::SilentlModeEnabled ? "1" : "0"));
    });

    server_.on("/set/manualBrightness", HTTP_GET, [this]() noexcept {
        if (server_.hasArg("value")) {
            String v = server_.arg("value");
            Globals::manualBrightnessEnabled = (v == "true");
            Logger::log(LOGTYPE, "manualBrightnessEnabled=%s", Globals::manualBrightnessEnabled ? "true" : "false");
        }
        if (server_.hasArg("brightness")) {
            String b = server_.arg("brightness");
            long lv = strtol(b.c_str(), nullptr, 10);
            if (lv < 0) lv = 0;
            if (lv > 100) lv = 100;
            brightness = static_cast<uint32_t>(lv);
            Logger::log(LOGTYPE, "brightness=%u", brightness);
        }
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/weatherUpdate", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/weatherUpdate fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::WeatherUpdateEnabled = (val != "0");
        Logger::log(LOGTYPE, "WeatherUpdateEnabled set to %s via HTTP", 
                    Globals::WeatherUpdateEnabled ? "true" : "false");
        server_.send(200, "text/plain", 
                     String("WeatherUpdateEnabled=") + (Globals::WeatherUpdateEnabled ? "1" : "0"));
    });

    server_.on("/set/logConfig", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/logConfig fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        char* endptr = nullptr;
        unsigned long newConfig = strtoul(val.c_str(), &endptr, 10);
        if (endptr == val.c_str() || *endptr != '\0') {
            server_.send(400, "text/plain", "Invalid 'value' (not a number)");
            Logger::log(LOGTYPE, "HTTP /set/logConfig ungültiger Wert: %s", val.c_str());
            return;
        }
        Globals::logConfig = static_cast<uint32_t>(newConfig);
        Globals::applyLogConfig();
        Logger::log(LOGTYPE, "logConfig auf %u gesetzt und übernommen", Globals::logConfig);
        server_.send(200, "text/plain", String("logConfig=") + String(Globals::logConfig));
    });

    server_.on("/set/firmware", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("target")) {
            server_.send(400, "text/plain", "Missing 'target' parameter");
            Logger::log(LOGTYPE, "HTTP /set/firmware fehlte Parameter 'target'");
            return;
        }
        String arg = server_.arg("target");
        Globals::FirmwareTarget newTarget = Globals::FirmwareTarget::COUNT;

        if (arg == "NixieV6_std") {
            newTarget = Globals::FirmwareTarget::NixieV6_std;
        }
        else if (arg == "NixieV6_dev") {
            newTarget = Globals::FirmwareTarget::NixieV6_dev;
        }
        else if (arg == "NixieV6_BOS") {
            newTarget = Globals::FirmwareTarget::NixieV6_BOS;
        }

        if (newTarget == Globals::FirmwareTarget::COUNT) {
            server_.send(400, "text/plain", "Invalid 'target' value");
            Logger::log(LOGTYPE, "HTTP /set/firmware ungültiger Wert: %s", arg.c_str());
            return;
        }

        Globals::currentFirmwareTarget = newTarget;
        Logger::log(LOGTYPE, "FirmwareTarget gesetzt auf %s", arg.c_str());
        server_.send(200, "text/plain", String("firmwareTarget=") + arg);
    });

    server_.on("/set/ota", HTTP_GET, [this]() noexcept {
        Globals::FirmwareTarget target = Globals::currentFirmwareTarget;
        std::string url = Globals::getFirmwareUrl(target);
        if (url.empty()) {
            server_.send(400, "text/plain", "Invalid firmware target");
            Logger::log(LOGTYPE, "OTA fehlgeschlagen: ungültiges Firmware-Target");
            return;
        }
        Logger::log(LOGTYPE, "Starte OTA-Update von URL: %s", url.c_str());
        OTAManager ota;
        esp_err_t result = ota.performUpdate(url);

        if (result == ESP_OK) {
            Logger::log(LOGTYPE, "OTA-Update erfolgreich");
            server_.send(200, "text/plain", "OTA successful");
        } 
        else 
        {
            Logger::log(LOGTYPE, "OTA-Update fehlgeschlagen (Error %d)", result);
            server_.send(500, "text/plain", "OTA failed");
        }
    });

    server_.on("/set/zip", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("zip")) {
            server_.send(400, "text/plain", "Missing 'zip' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/zip fehlte Parameter 'zip'"));
            return;
        }
        String zipArg = server_.arg("zip");
        Globals::zipCode = std::string(zipArg.c_str());
        Logger::log(LOGTYPE, "ZIP-Code per HTTP gesetzt auf %s", zipArg.c_str());
        server_.send(200, "text/plain", "ZIP-Code updated to " + zipArg);
    });

    server_.on("/get/info", HTTP_GET, [this]() noexcept {
        Logger::log(LOGTYPE, F("Info requested via HTTP"));
        handleInfo();
    });

    server_.begin();
    Logger::log(LOGTYPE, F("HTTP server started"));
}

void HTTPHandler::handleClient() noexcept {
    server_.handleClient();
}

void HTTPHandler::handleReset() noexcept {
    Logger::log(LOGTYPE, F("Performing system reset..."));
    storage.save();
    ESP.restart();
}

void HTTPHandler::handleInfo() noexcept
{
    WiFiManager wm;
    auto ssid            = wm.getWiFiSSID();
    auto password        = wm.getWiFiPass();
    auto chipModel       = ESP.getChipModel();
    auto freeHeap        = ESP.getFreeHeap();
    auto chipId          = ESP.getEfuseMac();
    auto flashSize       = ESP.getFlashChipSize();
    auto flashSpeed      = ESP.getFlashChipSpeed();
    auto sketchSize      = ESP.getSketchSize();
    auto sketchFreeSpace = ESP.getFreeSketchSpace();
    auto cpuFreq         = ESP.getCpuFreqMHz();
    auto sdkVersion      = ESP.getSdkVersion();
    auto voltage_12V     = supplyWatch.read12V();
    auto voltage_5V      = supplyWatch.read5V();
    auto voltage_3V3     = supplyWatch.read3V3();
    auto voltage_18V     = supplyWatch.read18V();
    auto voltage_UHSS    = supplyWatch.readUHSS();
    auto current_mA      = supplyWatch.readCurrent();
    auto power_W         = energyMonitor.getInstantPowerW();
    auto energy_Wh       = energyMonitor.getTotalEnergyWh();
    auto case_temp       = temperatureSensor.readTemperatureC();
    auto ftName          = firmwareTargetToString(Globals::currentFirmwareTarget);
    auto cfg             = Globals::logConfig;
    String binStr;
    binStr.reserve(9);
    for (int i = 8; i >= 0; --i) {
        binStr += ((cfg >> i) & 1) ? '1' : '0';
    }

    String jsonResponse;
    jsonResponse.reserve(2000);
    jsonResponse  = "{\n";
    jsonResponse += "  \"Chip\": \""               + String(chipModel)             + "\",\n";
    jsonResponse += "  \"SSID\": \""               + ssid                           + "\",\n";
    jsonResponse += "  \"Password\": \""           + password                       + "\",\n";
    jsonResponse += "  \"DisplayEnabled\": "      + String(displayEnabled)         + ",\n";
    jsonResponse += "  \"Melody\": \""             + String("Nokia")                + "\",\n";
    jsonResponse += "  \"FreeHeap\": "            + String(freeHeap)               + ",\n";
    jsonResponse += "  \"ChipId\": \""             + String(chipId, HEX)            + "\",\n";
    jsonResponse += "  \"FlashSize\": "           + String(flashSize)              + ",\n";
    jsonResponse += "  \"FlashSpeed\": "          + String(flashSpeed)             + ",\n";
    jsonResponse += "  \"SketchSize\": "          + String(sketchSize)             + ",\n";
    jsonResponse += "  \"SketchFreeSpace\": "     + String(sketchFreeSpace)        + ",\n";
    jsonResponse += "  \"CpuFrequencyMHz\": "     + String(cpuFreq)                + ",\n";
    jsonResponse += "  \"SdkVersion\": \""         + String(sdkVersion)             + "\",\n";
    jsonResponse += "  \"Autor\": \""              + String("X105GHM")              + "\",\n";
    jsonResponse += "  \"Voltage_12V\": "         + String(voltage_12V, 2)         + ",\n";
    jsonResponse += "  \"Voltage_5V\": "          + String(voltage_5V, 2)          + ",\n";
    jsonResponse += "  \"Voltage_3V3\": "         + String(voltage_3V3, 2)         + ",\n";
    jsonResponse += "  \"Voltage_18V\": "         + String(voltage_18V, 2)         + ",\n";
    jsonResponse += "  \"Voltage_UHSS\": "        + String(voltage_UHSS, 2)        + ",\n";
    jsonResponse += "  \"Current_mA\": "          + String(current_mA, 2)          + ",\n";
    jsonResponse += "  \"Power_W\": "             + String(power_W, 2)             + ",\n";
    jsonResponse += "  \"Energy_Wh\": \""          + String(energy_Wh, 2)           + "\",\n";
    jsonResponse += "  \"HSS_Enabled\": "         + String(hssController.enable160V) + ",\n";
    jsonResponse += "  \"HSS_190V\": "            + String(hssController.enable190V) + ",\n";
    jsonResponse += "  \"HSS_Resistor\": "        + String(hssController.enableResistor) + ",\n";
    jsonResponse += "  \"CaseTemperature_C\": "   + String(case_temp, 2)           + ",\n";
    jsonResponse += "  \"Firmware_Target\": \""    + String(ftName)                 + "\",\n";
    jsonResponse += "  \"Hardware_Version\": \""   + String(Globals::HardwareVersion.c_str()) + "\",\n";
    jsonResponse += "  \"Software_Version\": \""   + String(Globals::SoftwareVersion.c_str()) + "\",\n";
    jsonResponse += "  \"zipCode\": \""            + String(Globals::zipCode.c_str()) + "\",\n";
    jsonResponse += "  \"tickerEnabled\": "       + String(Globals::tickerEnabled) + ",\n";
    jsonResponse += "  \"timeLimitEnabled\": "    + String(Globals::timeLimitEnabled) + ",\n";
    jsonResponse += "  \"silentModeEnabled\": "   + String(Globals::SilentlModeEnabled) + ",\n";
    jsonResponse += "  \"manualBrightnessEnabled\": " + String(Globals::manualBrightnessEnabled) + ",\n";
    jsonResponse += "  \"WeatherUpdateEnabled\": "    + String(Globals::WeatherUpdateEnabled) + ",\n";
    jsonResponse += "  \"logConfigBinary\": \""    + binStr                         + "\",\n";
    jsonResponse += "  \"loadDetected\": "        + String(Globals::loadDetected)  + ",\n";
    jsonResponse += "  \"Brightness\": "          + String(brightness)             + "\n";
    jsonResponse += "}";

    server_.send(200, "application/json", jsonResponse);
}
