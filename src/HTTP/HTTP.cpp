#include "HTTP.hpp"

static constexpr LoggerType LOGTYPE = LoggerType::Webserver;

static const std::regex timeRegex(R"(^([01]?[0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]$)");

static const char* tzNames[] = {"CET","EET","WET","UTC","EST","CST","MST","PST","HST","JST","IST","AEST","AWST"};

static bool otaBusy() noexcept
{
    return OTAManager::instance().isRunning();
}

static void sendOtaBusy(WebServer& server) noexcept
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(503, "application/json", "{\"error\":\"OTA active\"}");
}

constexpr int tzCount = sizeof(tzNames) / sizeof(tzNames[0]);

HTTPHandler::HTTPHandler(int port) noexcept
    : server_(port)
{}

String HTTPHandler::getContentType(const String &filename) noexcept 
{
    if (filename.endsWith(".htm") || filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css"))   return "text/css";
    if (filename.endsWith(".js"))    return "application/javascript";
    if (filename.endsWith(".png"))   return "image/png";
    if (filename.endsWith(".jpg"))   return "image/jpeg";
    if (filename.endsWith(".ico"))   return "image/x-icon";
    if (filename.endsWith(".pdf"))   return "application/pdf";
    return "text/plain";
}

bool HTTPHandler::handleFileRead(const String &path) noexcept
{
    if (!server_.client() || !server_.client().connected()) return false;

    String filePath = path;
    if (filePath.endsWith("/")) filePath += "index.html";
    if (!SPIFFS.exists(filePath)) return false;

    File file = SPIFFS.open(filePath, "r");
    if (!file) return false;

    server_.sendHeader("Connection", "close");
    String ct = getContentType(filePath);
    server_.streamFile(file, ct);
    file.close();
    return true;
}

void HTTPHandler::begin() noexcept 
{
    if (!SPIFFS.begin(true)) 
    {
        Logger::log(LOGTYPE, F("SPIFFS Mount failed"));
    } 
    else 
    {
        Logger::log(LOGTYPE, F("SPIFFS ready"));
    }

    server_.onNotFound([this]() noexcept {
       
        if (otaBusy())
        {
            sendOtaBusy(server_);
            return;
        }

        if (server_.uri().length() > 128) 
        {
            server_.sendHeader("Connection", "close");
            server_.send(414, "text/plain", "URI too long");
            return;
        }

        auto m = server_.method();
        if (m != HTTP_GET && m != HTTP_HEAD) 
        {
            server_.sendHeader("Connection", "close");
            server_.send(405, "text/plain", "Method Not Allowed");
            return;
        }

        if (!handleFileRead(server_.uri())) 
        {
            server_.sendHeader("Connection", "close");
            server_.send(404, "text/plain", "404: File Not Found");
        }
    });
    
    server_.on("/", HTTP_POST, [this]() noexcept {
        server_.send(405, "text/plain", "Method Not Allowed");
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
        server_.send(200, "text/plain", "OK");
        handleReset();
    });

    server_.on("/set/setOldValue", HTTP_GET, [this]() noexcept {
        Logger::log(LOGTYPE, F("System apply loadGlobals requested via HTTP"));       
        Memory::loadGlobals();
        Globals::applyLogConfig();
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/loadDetectedOverwrite", HTTP_GET, [this]() noexcept {     
        Globals::loadDetected = !Globals::loadDetected;
        Logger::log(LOGTYPE, "loadDetected set to %s via HTTP", Globals::loadDetected ? "true" : "false");
        server_.send(200, "text/plain", String("loadDetected=") + (Globals::loadDetected ? "1" : "0"));
    });

    server_.on("/set/resetValue", HTTP_GET, [this]() noexcept {
        Logger::log(LOGTYPE, F("Globals Values reset requested via HTTP"));       
        Memory::StorageReset();
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/resetWiFi", HTTP_GET, [this]() noexcept {
        wifiConnector.eraseCredentials();
        Logger::log(LOGTYPE, F("WiFi Credentail erase requested via HTTP"));  
        server_.send(200, "text/plain", "OK");     
    });

    server_.on("/get/wifiSaved", HTTP_GET, [this]() noexcept {
        auto list = wifiConnector.getSavedNetworks();

        auto esc = [](const String& s)->String{
            String r; r.reserve(s.length()+8);
            for (size_t i=0;i<s.length();++i){
                char c=s[i];
                if (c=='\"') r += "\\\"";
                else if (c=='\\') r += "\\\\";
                else if ((uint8_t)c < 0x20){ char b[7]; sprintf(b,"\\u%04x",(uint8_t)c); r+=b; }
                else r += c;
            }
            return r;
        };

        String out = "[";
        for (size_t i=0;i<list.size();++i)
        {
            if(i) out += ",";
            out += "{\"ssid\":\""; out += esc(String(list[i].ssid)); out += "\",";
            out += "\"priority\":"; out += String(list[i].priority); out += ",";
            out += "\"last_ok\":";  out += String(list[i].last_ok);
            out += "}";
        }
        out += "]";

        server_.sendHeader("Cache-Control","no-store");
        server_.send(200, "application/json", out);
        Logger::log(LOGTYPE, "HTTP /get/wifiSaved -> %u entries", (unsigned)list.size());
    });

    server_.on("/set/wifiAdd", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("ssid")) {
            server_.send(400, "text/plain", "Missing 'ssid'");
            Logger::log(LOGTYPE, "HTTP /set/wifiAdd missing ssid");
            return;
        }
        String ssid = server_.arg("ssid");
        String pwd  = server_.hasArg("pwd")  ? server_.arg("pwd")  : "";
        int prio    = server_.hasArg("prio") ? server_.arg("prio").toInt() : 100;
        prio = constrain(prio, 0, 254);

        bool ok = wifiConnector.addOrUpdateNetwork(ssid, pwd, (uint8_t)prio);
        Logger::log(LOGTYPE, ok ? "WiFi addOrUpdate OK: '%s' (prio %d)" : "WiFi addOrUpdate FAIL: '%s'", ssid.c_str(), prio);

        server_.send(ok ? 200 : 400, "text/plain", ok ? "OK" : "FAIL");
    });

    server_.on("/set/wifiRemove", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("ssid")) 
        {
            server_.send(400, "text/plain", "Missing 'ssid'");
            Logger::log(LOGTYPE, "HTTP /set/wifiRemove missing ssid");
            return;
        }
        String ssid = server_.arg("ssid");
        bool ok = wifiConnector.removeNetwork(ssid);

        Logger::log(LOGTYPE, ok ? "WiFi removed: '%s'" : "WiFi remove failed/not found: '%s'", ssid.c_str());
        server_.send(ok ? 200 : 404, "text/plain", ok ? "OK" : "Not found");
    });

    server_.on("/set/ACP", HTTP_GET, [this]() noexcept {

        if (!displayEnabled && !Globals::loadDetected) 
        {
            server_.send(400, "text/plain", "Display not enabled");
            Logger::log(LOGTYPE, F("Error: Display not enabled"));
            return;
        }
        else if (mode_running.load(std::memory_order_relaxed)) 
        {
            server_.send(400, "text/plain", "A mode is already running");
            Logger::log(LOGTYPE, F("Error: Another mode is already running"));
            return;
        }
        
        Logger::log(LOGTYPE, "ACP started via HTTP");
        digits = 0;

        runWithClockSuspended(clockTaskHandle, [this]() {
            auto check = [](esp_err_t e, const char* what) {
                if (e != ESP_OK) {
                    Logger::log(LOGTYPE, "%s failed (%d)", what, static_cast<int>(e));
                    return false;
                }
                return true;
            };

            Logger::log(LOGTYPE, "ACP task: enabling 190V + resistor reduction");

            if (!check(hssController.enable190(), "Failed to enable 190V")) return;
            vTaskDelay(pdMS_TO_TICKS(10));

            if (!check(hssController.enableResistorReduction(), "Failed to reduce resistors")) return;
            vTaskDelay(pdMS_TO_TICKS(10));

            ACP();

            Logger::log(LOGTYPE, "ACP task: disabling resistor reduction + 190V");

            if (!check(hssController.disableResistorReduction(), "Failed to disable resistor reduction")) return;
            vTaskDelay(pdMS_TO_TICKS(10));

            if (!check(hssController.disable190(), "Failed to disable 190V")) return;
            vTaskDelay(pdMS_TO_TICKS(10));

            Logger::log(LOGTYPE, "ACP task finished");
        });

        server_.send(200, "text/plain", "ACP started");
    });

    server_.on("/set/tempDisplay", HTTP_GET, [this]() noexcept {

        if (!displayEnabled && !Globals::loadDetected) 
        {
            server_.send(400, "text/plain", "Display not enabled");
            Logger::log(LOGTYPE, F("Error: Display not enabled"));
            return;
        }
        else if (mode_running.load(std::memory_order_relaxed)) 
        {
            server_.send(400, "text/plain", "A mode is already running");
            Logger::log(LOGTYPE, F("Error: Another mode is already running"));
            return;
        }

        Logger::log(LOGTYPE, F("Temperature display requested via HTTP"));

        server_.send(200, "text/plain", "Temperature display started");

            runWithClockSuspended(clockTaskHandle, [this]() {
            mode_running.store(true, std::memory_order_relaxed);
            zipMaskingEnabled = true;
            digits = Globals::zipCode.empty() ? 0 : std::stoi(Globals::zipCode)*10;
            vTaskDelay(pdMS_TO_TICKS(3000));
            displayWeather();
            tempMaskingEnabled = true;
            zipMaskingEnabled = false;
            vTaskDelay(pdMS_TO_TICKS(5000));
            tempMaskingEnabled = false;
            mode_running.store(false, std::memory_order_relaxed);
        });
    });

    server_.on("/set/DATE", HTTP_GET, [this]() noexcept {

        if (!displayEnabled && !Globals::loadDetected) 
        {
            server_.send(400, "text/plain", "Display not enabled");
            Logger::log(LOGTYPE, F("Error: Display not enabled"));
            return;
        }
        else if (mode_running.load(std::memory_order_relaxed)) 
        {
            server_.send(400, "text/plain", "A mode is already running");
            Logger::log(LOGTYPE, F("Error: Another mode is already running"));
            return;
        }

        Logger::log(LOGTYPE, F("Date display requested via HTTP"));

        server_.send(200, "text/plain", "Date display started");

        runWithClockSuspended(clockTaskHandle, [this]() 
        {
            mode_running.store(true, std::memory_order_relaxed);
            displayDate();
            vTaskDelay(pdMS_TO_TICKS(5000));
            mode_running.store(false, std::memory_order_relaxed);
        });
    });

    server_.on("/set/CRICKET", HTTP_GET, [this]() noexcept {

        Logger::log(LOGTYPE, F("Cricket sound requested via HTTP"));

        buzzer.startCricketInTask();

        server_.send(200, "text/plain", "Cricket sound started");
    });

    server_.on("/set/ticker", HTTP_GET, [this]() noexcept {

        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/ticker missing parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::tickerEnabled = (val != "0");
        Logger::log(LOGTYPE, "tickerEnabled set to %s via HTTP", Globals::tickerEnabled ? "true" : "false");
        server_.send(200, "text/plain", String("tickerEnabled=") + (Globals::tickerEnabled ? "1" : "0"));
    });

    server_.on("/set/singleDigitControl", HTTP_GET, [this]() noexcept{

        if (!displayEnabled && !Globals::loadDetected) 
        {
            server_.send(400, "text/plain", "Display not enabled");
            Logger::log(LOGTYPE, F("Error: Display not enabled"));
            return;
        }
        else if (mode_running.load(std::memory_order_relaxed)) 
        {
            server_.send(400, "text/plain", "A mode is already running");
            Logger::log(LOGTYPE, F("Error: Another mode is already running"));
            return;
        }

        bool handled = false;

        if (server_.hasArg("value"))
        {
            String v = server_.arg("value");
            singleDigitACP = (v == "1");
            Logger::log(LOGTYPE, "singleDigitACP set to %s via HTTP", singleDigitACP ? "true" : "false");

            if(singleDigitACP)
            {
                (void)hssController.enable190();
                (void)hssController.enableResistorReduction();
            }
            else
            {
                (void)hssController.disableResistorReduction();
                (void)hssController.disable190();
            }
            handled = true;
        }

        if (server_.hasArg("digit"))
        {
            int d = server_.arg("digit").toInt();
            if (d >= 0 && d < 60)
            {
                singleDigit = static_cast<uint8_t>(d);
                Logger::log(LOGTYPE, "singleDigit set to %d via HTTP", singleDigit);
                handled = true;
            }
            else
            {
                server_.send(400, "text/plain", "Invalid 'digit' (must be 0–59)");
                return;
            }
        }

        if (handled)
        {
            server_.send(200, "text/plain", "OK");
        }
        else
        {
            server_.send(400, "text/plain", "Missing 'value' or 'digit'");
        } 
    });

    server_.on("/set/NixiePWM", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/NixiePWM missing parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::PWM_disabled = (val != "0");
        Logger::log(LOGTYPE, "Nixie_PWM set to %s via HTTP", 
                    Globals::PWM_disabled ? "true" : "false");
        server_.send(200, "text/plain", String("tickerEnabled=") + (Globals::PWM_disabled ? "1" : "0"));
    });

    server_.on("/set/PWMPeriod", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' parameter (µs)");
            Logger::log(LOGTYPE, F("HTTP /set/PWMPeriod fehlte Parameter 'value'"));
            return;
        }

        const String val = server_.arg("value");
        const std::uint32_t newPeriod = val.toInt();

        if (newPeriod == 0) 
        {
            server_.send(400, "text/plain", "Invalid value");
            Logger::log(LOGTYPE, F("HTTP /set/PWMPeriod invalid value"));
            return;
        }

        PWM_PERIOD_US = newPeriod;
        Logger::log(LOGTYPE, "PWM_PERIOD_US set to %lu µs via HTTP", static_cast<unsigned long>(newPeriod));

        server_.send(200, "text/plain", String("PWM_PERIOD_US=") + newPeriod);
    });

    server_.on("/set/timeLimit", HTTP_GET, [this]() noexcept{
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/timeLimit missing parameter 'value'"));
            return;
        }

        Globals::timeLimitEnabled = (server_.arg("value") != "0");

        if (server_.hasArg("from"))
        {
            std::string from = server_.arg("from").c_str();
            if (std::regex_match(from, timeRegex))
            {
                Globals::timeLimitFrom = from;
            }
            else
            {
                server_.send(400, "text/plain", "Invalid 'from' time format (expected HH:MM:SS)");
                Logger::log(LOGTYPE, "Invalid 'from' format: %s", from.c_str());
                return;
            }
        }

        if (server_.hasArg("to"))
        {
            std::string to = server_.arg("to").c_str();
            if (std::regex_match(to, timeRegex))
            {
                Globals::timeLimitTo = to;
            }
            else
            {
                server_.send(400, "text/plain","Invalid 'to' time format (expected HH:MM:SS)");
                Logger::log(LOGTYPE, "Invalid 'to' format: %s", to.c_str());
                return;
            }
        }

        Logger::log(LOGTYPE, "timeLimitEnabled set to %s via HTTP",Globals::timeLimitEnabled ? "true" : "false");

        Logger::log(LOGTYPE, "timeLimit active from %s to %s",Globals::timeLimitFrom.c_str(), Globals::timeLimitTo.c_str());

        server_.send(200, "text/plain",String("timeLimitEnabled=") + (Globals::timeLimitEnabled ? "1" : "0") +"\nfrom=" + Globals::timeLimitFrom.c_str() +"\nto=" + Globals::timeLimitTo.c_str());
    });

    server_.on("/set/silentMode", HTTP_GET, [this]() noexcept{
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/silentMode fehlte Parameter 'value'"));
            return;
        }

        String val = server_.arg("value");
        Globals::SilentModeEnabled = (val != "0");
        Logger::log(LOGTYPE, "silentModeEnabled set to %s via HTTP",
                    Globals::SilentModeEnabled ? "true" : "false");

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << 40),
            .mode = Globals::SilentModeEnabled ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = Globals::SilentModeEnabled ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);

        if (!Globals::SilentModeEnabled) 
        {
            gpio_set_level(GPIO_NUM_40, 0);
        }

        server_.send(200, "text/plain",
                     String("SilentModeEnabled=") + (Globals::SilentModeEnabled ? "1" : "0")); 
    });

    server_.on("/set/manualBrightness", HTTP_GET, [this]() noexcept {
        if (server_.hasArg("value")) 
        {
            String v = server_.arg("value");
            Globals::manualBrightnessEnabled = (v == "true");
            Logger::log(LOGTYPE, "manualBrightnessEnabled=%s", Globals::manualBrightnessEnabled ? "true" : "false");
        }
        if (server_.hasArg("brightness")) 
        {
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
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/weatherUpdate fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::WeatherUpdateEnabled = (val != "0");
        Logger::log(LOGTYPE, "WeatherUpdateEnabled set to %s via HTTP", Globals::WeatherUpdateEnabled ? "true" : "false");
        server_.send(200, "text/plain", String("WeatherUpdateEnabled=") + (Globals::WeatherUpdateEnabled ? "1" : "0"));
    });

    server_.on("/set/randomCricket", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, F("HTTP /set/randomCricket fehlte Parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        Globals::cricketSoundEnabled = (val != "0");
        Logger::log(LOGTYPE, "cricketSoundEnabled set to %s via HTTP", Globals::cricketSoundEnabled ? "true" : "false");
        server_.send(200, "text/plain", String("cricketSoundEnabled=") + (Globals::cricketSoundEnabled ? "1" : "0"));
    });

    server_.on("/set/logConfig", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/logConfig missing parameter 'value'"));
            return;
        }
        String val = server_.arg("value");
        char* endptr = nullptr;
        unsigned long newConfig = strtoul(val.c_str(), &endptr, 10);
        if (endptr == val.c_str() || *endptr != '\0') 
        {
            server_.send(400, "text/plain", "Invalid 'value' (not a number)");
            Logger::log(LOGTYPE, "HTTP /set/logConfig invalid value: %s", val.c_str());
            return;
        }
        Globals::logConfig = static_cast<uint32_t>(newConfig);
        Globals::applyLogConfig();

        String binStr;
        binStr.reserve(9);
        for (int i = 8; i >= 0; --i) 
        {
            binStr += ((Globals::logConfig >> i) & 1) ? '1' : '0';
        }

        Logger::log(LOGTYPE, "logConfig set to %s and applied", binStr.c_str());
        server_.send(200, "text/plain", String("logConfig=") + binStr);
    });

    server_.on("/set/firmware", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("target")) 
        {
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

        if (newTarget == Globals::FirmwareTarget::COUNT) 
        {
            server_.send(400, "text/plain", "Invalid 'target' value");
            Logger::log(LOGTYPE, "HTTP /set/firmware invalid value: %s", arg.c_str());
            return;
        }

            Globals::currentFirmwareTarget = newTarget;
            Logger::log(LOGTYPE, "Firmware target set to %s", arg.c_str());
            server_.send(200, "text/plain", String("firmwareTarget=") + arg);
        });

    server_.on("/set/ota", HTTP_GET, [this]() noexcept {
        Globals::FirmwareTarget target = Globals::currentFirmwareTarget;
        std::string baseUrl = Globals::getFirmwareUrl(target);
        displayEnabled = false;

        if (baseUrl.empty())
        {
            Logger::log(LOGTYPE, "OTA failed: invalid firmware target");
            server_.sendHeader("Cache-Control", "no-store");
            server_.send(400, "application/json", "{\"started\":false,\"error\":\"Invalid firmware target\"}");
            return;
        }

        auto& ota = OTAManager::instance();

        if (ota.isRunning())
        {
            Logger::log(LOGTYPE, "OTA start rejected: already running");
            server_.sendHeader("Cache-Control", "no-store");
            server_.send(409, "application/json", "{\"started\":false,\"error\":\"OTA already running\"}");
            return;
        }

        Logger::log(LOGTYPE, "Starting OTA async in folder: %s", baseUrl.c_str());

        if (!ota.startAsync(baseUrl))
        {
            Logger::log(LOGTYPE, "OTA task could not be started");
            server_.sendHeader("Cache-Control", "no-store");
            server_.send(500, "application/json", "{\"started\":false,\"error\":\"Could not start OTA task\"}");
            return;
        }

        server_.sendHeader("Cache-Control", "no-store");
        server_.send(202, "application/json", ota.getStatusJson());
    });

    server_.on("/get/otaStatus", HTTP_GET, [this]() noexcept {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", OTAManager::instance().getStatusJson());
    });

    server_.on("/set/otaResetStatus", HTTP_GET, [this]() noexcept {
        auto& ota = OTAManager::instance();

        if (ota.isRunning())
        {
            server_.send(409, "text/plain", "OTA still running");
            return;
        }

        ota.resetStatus();
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/get/checkUpdate", HTTP_GET, [this]() noexcept {
        const std::string baseUrl = Globals::getFirmwareUrl(Globals::currentFirmwareTarget);
        (void)OTAManager::instance().checkForUpdateAvailable(baseUrl);
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/zip", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("zip")) 
        {
            server_.send(400, "text/plain", "Missing 'zip' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/zip missing parameter 'zip'"));
            return;
        }
        String zipArg = server_.arg("zip");
        Globals::zipCode = std::string(zipArg.c_str());
        Logger::log(LOGTYPE, "ZIP-Code updated to %s", zipArg.c_str());
        server_.send(200, "text/plain", "ZIP-Code updated to " + zipArg);
    });

    server_.on("/set/timezone", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("tz")) 
        {
            server_.send(400, "text/plain", "Missing 'tz' parameter");
            Logger::log(LOGTYPE, F("HTTP /set/timezone missing parameter 'tz'"));
            return;
        }   

        String tzArg = server_.arg("tz");
        Globals::TimeZone newTz = Globals::TimeZone::CET;
        bool ok = false;

        int idx = tzArg.toInt();
        if (tzArg == String(idx) && idx >= 0 && idx < tzCount) 
        {
            newTz = static_cast<Globals::TimeZone>(idx);
            ok = true;
        }
        else 
        {  
            for (int i = 0; i < tzCount; ++i) 
            {
                if (tzArg.equalsIgnoreCase(tzNames[i])) 
                {
                    newTz = static_cast<Globals::TimeZone>(i);
                    ok = true;
                    break;
                }
            }
        }

        if (!ok) 
        {
            server_.send(400, "text/plain", "Invalid 'tz' parameter");
            Logger::log(LOGTYPE, "HTTP /set/timezone invalid 'tz': %s", tzArg.c_str());
            return;
        }

        Globals::currentTimeZone = newTz;

        const char* spec = Globals::getPosixTZ(newTz);
        Logger::log(LOGTYPE, "TimeZone set via HTTP (volatile): %d -> %s", static_cast<int>(newTz), spec);

        server_.send(200, "application/json","{\n""  \"status\": \"ok\",\n""  \"CurrentTimeZoneIndex\": " + String(static_cast<int>(newTz)) + ",\n""  \"CurrentTimeZoneSpec\": \"" + String(spec) + "\"\n""}\n");
    });

    server_.on("/set/timer", HTTP_GET, [this]() {
        if (!server_.hasArg("enabled")) 
        {
            server_.send(400, "text/plain", "Missing 'enabled' parameter");
            return;
        }

        bool enable = server_.arg("enabled") == "1";

        if (enable) 
        {
            if (!server_.hasArg("seconds")) 
            {
                server_.send(400, "text/plain", "Missing 'seconds' parameter");
                return;
            }

            uint32_t seconds = server_.arg("seconds").toInt();
            timer.start(seconds, [&]() {
            buzzer.startAlarm(3);
            });

            server_.send(200, "text/plain", "Timer started");
        } 
        else 
        {
            timer.stop();
            server_.send(200, "text/plain", "Timer stopped");
        }
    });

    server_.on("/set/alarm", HTTP_GET, [this]() {
        if (!server_.hasArg("enabled")) 
        {
            server_.send(400, "text/plain", "Missing 'enabled' parameter");
            return;
        }

        bool enable = server_.arg("enabled") == "1";

        if (enable) 
        {
            if (!server_.hasArg("time")) 
            {
                server_.send(400, "text/plain", "Missing 'time' parameter");
                return;
            }

            String t = server_.arg("time");
            int sep = t.indexOf(':');
            if (sep < 0 || sep >= t.length() - 1) 
            {
                server_.send(400, "text/plain", "Invalid time format (HH:MM)");
                return;
            }

            int h = t.substring(0, sep).toInt();
            int m = t.substring(sep + 1).toInt();

            alarmClock.setAlarm(h, m, [&]() {
            buzzer.startAlarm(5);
            });

            server_.send(200, "text/plain", "Alarm set");
        } 
        else     
        {
            alarmClock.removeAlarm();
            server_.send(200, "text/plain", "Alarm cleared");
        }
    });

    server_.on("/get/brownout", HTTP_GET, [this]() noexcept {
        std::string json;
        Memory::ReadBrownoutLog(json);

        if (json.empty()) {
            server_.send(200, "text/plain", "None.");
        }
        else 
        {
            server_.send(200, "application/json", String(json.c_str()));
        }
    });

    server_.on("/set/brownout", HTTP_GET, [this]() noexcept {
        Memory::BrownoutReset();
        server_.send(204, "text/plain", "");
    });

    server_.on("/get/taskStats", HTTP_GET, [this]() noexcept {

        if (otaBusy())
        {
            sendOtaBusy(server_);
            return;
        }

        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", StatsMonitor::instance().getTaskStatsJson(12, true));
    });

    server_.on("/get/info", HTTP_GET, [this]() noexcept {
        if (otaBusy())
        {
            sendOtaBusy(server_);
            return;
        }

        Logger::log(LOGTYPE, F("Info requested via HTTP"));
        handleInfo();
    });

    server_.begin();
    Logger::log(LOGTYPE, F("HTTP server started"));
}

void HTTPHandler::handleClient() noexcept 
{
    server_.handleClient();
    vTaskDelay(1);
}

void HTTPHandler::handleReset() noexcept 
{
    Logger::log(LOGTYPE, F("Performing system reset..."));
    Memory::saveGlobals();
    ESP.restart();
}

void HTTPHandler::handleInfo() noexcept
{
    auto &sm = StatsMonitor::instance();
    auto ssid            = wifiConnector.getWiFiSSID();
    auto deviceIP      = wifiConnector.getIpAddress();
    //auto password        = wifiConnector.getWiFiPass();
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
    auto pwmFreq         = 1e6 / PWM_PERIOD_US;
    auto timerActive     = timer.isRunning();
    auto timerPreset     = timer.getConfiguredSeconds();
    auto alarmSet        = alarmClock.isAlarmConfigured();
    AlarmTime at         = alarmClock.getAlarmTime();
    char alarmTimeBuf[8];

    snprintf(alarmTimeBuf, sizeof(alarmTimeBuf), "%02u:%02u", at.hour, at.minute);
    String binStr;
    binStr.reserve(9);
    for (int i = 8; i >= 0; --i) 
    {
        binStr += ((cfg >> i) & 1) ? '1' : '0';
    }

    String jsonResponse;
    jsonResponse.reserve(2000);
    jsonResponse  = "{\n";
    jsonResponse += "  \"Chip\": \""               + String(chipModel)             + "\",\n";
    jsonResponse += "  \"SSID\": \""               + ssid                           + "\",\n";
    jsonResponse += "  \"IP_Address\": \""         + deviceIP                       + "\",\n";
    //jsonResponse += "  \"Password\": \""           + password                       + "\",\n";
    jsonResponse += "  \"DisplayEnabled\": "      + String(displayEnabled)         + ",\n";
    jsonResponse += "  \"Melody\": \""             + String("Nokia")                + "\",\n";
    jsonResponse += "  \"FreeHeap\": "            + String(freeHeap)               + ",\n";
    jsonResponse += "  \"CoreLoad0\": "            + String(sm.coreLoad0_)         + ",\n";
    jsonResponse += "  \"CoreLoad1\": "            + String(sm.coreLoad1_)         + ",\n";
    jsonResponse += "  \"TotalLoad\": "            + String(sm.totalLoad_)         + ",\n";
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
    jsonResponse += "  \"UpdateAvailable\": "          + String(Globals::updateAvailable)  + ",\n";
    jsonResponse += "  \"zipCode\": \""            + String(Globals::zipCode.c_str()) + "\",\n";
    jsonResponse += "  \"tickerEnabled\": "       + String(Globals::tickerEnabled) + ",\n";
    jsonResponse += "  \"timeLimitEnabled\": "    + String(Globals::timeLimitEnabled) + ",\n";
    jsonResponse += "  \"silentModeEnabled\": "   + String(Globals::SilentModeEnabled) + ",\n";
    jsonResponse += "  \"manualBrightnessEnabled\": " + String(Globals::manualBrightnessEnabled) + ",\n";
    jsonResponse += "  \"WeatherUpdateEnabled\": "    + String(Globals::WeatherUpdateEnabled) + ",\n";
    jsonResponse += "  \"cricketSoundEnabled\": "    + String(Globals::cricketSoundEnabled) + ",\n";
    jsonResponse += "  \"logConfigBinary\": \""    + binStr                         + "\",\n";
    jsonResponse += "  \"loadDetected\": "        + String(Globals::loadDetected)  + ",\n";
    jsonResponse += "  \"Brightness\": "          + String(brightness)             + ",\n";
    jsonResponse += "  \"timeLimitFrom\": \""   + String(Globals::timeLimitFrom.c_str()) + "\",\n";
    jsonResponse += "  \"timeLimitTo\": \""     + String(Globals::timeLimitTo.c_str()) + "\",\n";
    jsonResponse += "  \"SingleACP\": "         + String(singleDigitACP) + ",\n";
    jsonResponse += "  \"NixiePWM\": "          + String(Globals::PWM_disabled)  + ",\n";
    jsonResponse += "  \"PWM_Frequenzy\": "          + String(pwmFreq)  + ",\n";
    jsonResponse += "  \"SingleDigits\": "       + String(singleDigit) + ",\n";
    jsonResponse += "  \"CurrentTimeZone\": "       + String(static_cast<uint8_t>(Globals::currentTimeZone)) + ",\n";
    jsonResponse += "  \"TimerActive\": "       + String(timerActive ? "true" : "false") + ",\n";
    jsonResponse += "  \"TimerConfiguredSeconds\": " + String(timerPreset) + ",\n";
    jsonResponse += "  \"AlarmActive\": "       + String(alarmSet ? "true" : "false") + ",\n";
    jsonResponse += "  \"AlarmTime\": \""       + String(alarmTimeBuf) + "\"\n";
    jsonResponse += "}";

    server_.send(200, "application/json", jsonResponse);
}
