#include "HTTP.hpp"
#include "Config/Secrets.hpp"
#include "Acoustics/Buzzer/Buzzer.hpp"
#include "AlarmClock/AlarmClock.hpp"
#include "Timer/Timer.hpp"
#include "esp_system.h"
#include "Diagnostics/ResetDiagnostics.hpp"
#include "ewm/Utils/MiniJson.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

static constexpr LoggerType LOGTYPE = LoggerType::Webserver;

extern Buzzer buzzer;
extern AlarmClock alarmClock;
extern Timer timer;

static const std::regex timeRegex(R"(^([01]?[0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]$)");

static const char* tzNames[] = {"CET","EET","WET","UTC","EST","CST","MST","PST","HST","JST","IST","AEST","AWST"};

//helper functions

static bool otaBusy() noexcept
{
    return OTAManager::instance().isRunning();
}

static void sendOtaBusy(NativeHttpServer& server) noexcept
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(503, "application/json", "{\"error\":\"OTA active\"}");
}

static bool startUpdateCheckTask(std::string baseUrl) noexcept
{
    auto* taskUrl = new (std::nothrow) std::string(std::move(baseUrl));
    if (!taskUrl) return false;

    const BaseType_t created = xTaskCreatePinnedToCore([](void* context) 
    {
        std::unique_ptr<std::string> url(static_cast<std::string*>(context));
        (void)OTAManager::instance().checkForUpdateAvailable(*url);
        vTaskDelete(nullptr);
    }, "UpdateCheck", 12288, taskUrl, 1, nullptr, 0);

    if (created == pdPASS) return true;
    delete taskUrl;
    return false;
}

static bool isValidZipCode(std::string_view zip) noexcept
{
    if (zip.empty() || zip.length() > 10)
    {
        return false;
    }

    for (size_t i = 0; i < zip.length(); ++i)
    {
        if (zip[i] < '0' || zip[i] > '9')
        {
            return false;
        }
    }

    return true;
}

static bool parseUInt32Strict(std::string_view input, uint32_t& out) noexcept
{
    if (input.empty())
    {
        return false;
    }

    uint32_t value = 0;
    for (size_t i = 0; i < input.length(); ++i)
    {
        char c = input[i];
        if (c < '0' || c > '9')
        {
            return false;
        }

        const uint32_t digit = static_cast<uint32_t>(c - '0');
        if (value > (UINT32_MAX - digit) / 10U)
        {
            return false;
        }

        value = value * 10U + digit;
    }

    out = value;
    return true;
}

static bool parseAlarmTime(std::string_view input, uint8_t& hour, uint8_t& minute) noexcept
{
    if (input.length() != 5 || input[2] != ':')
    {
        return false;
    }

    const auto isAsciiDigit = [](char c) { return c >= '0' && c <= '9'; };
    if (!isAsciiDigit(input[0]) || !isAsciiDigit(input[1]) ||
        !isAsciiDigit(input[3]) || !isAsciiDigit(input[4]))
    {
        return false;
    }

    const int h = (input[0] - '0') * 10 + (input[1] - '0');
    const int m = (input[3] - '0') * 10 + (input[4] - '0');

    if (h < 0 || h > 23 || m < 0 || m > 59)
    {
        return false;
    }

    hour = static_cast<uint8_t>(h);
    minute = static_cast<uint8_t>(m);
    return true;
}
static long parseLongCompatible(const std::string& input) noexcept
{
    return std::strtol(input.c_str(), nullptr, 10);
}

static bool equalsIgnoreCase(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index)
    {
        const auto l = static_cast<unsigned char>(left[index]);
        const auto r = static_cast<unsigned char>(right[index]);
        if (std::tolower(l) != std::tolower(r)) return false;
    }
    return true;
}

constexpr int tzCount = sizeof(tzNames) / sizeof(tzNames[0]);

HTTPHandler::HTTPHandler(int port) noexcept
    : server_(port), secureApi_(server_)
{}

bool HTTPHandler::executeCommand(const std::function<void()>& action) noexcept
{
    const auto result = commands_.run(action);
    if (result == HttpCommandQueue::Result::Queued) return true;
    sendCommandError(result);
    return false;
}

bool HTTPHandler::enqueueCommand(std::function<void()> action) noexcept
{
    const auto result = commands_.enqueue(std::move(action));
    if (result == HttpCommandQueue::Result::Queued)
    {
        server_.sendHeader("X-Command-Status", "queued");
        return true;
    }
    sendCommandError(result);
    return false;
}

void HTTPHandler::sendCommandError(HttpCommandQueue::Result result) noexcept
{
    Logger::log(LOGTYPE, "HTTP command rejected: %u", static_cast<unsigned>(result));
    server_.sendHeader("Cache-Control", "no-store");
    if (result == HttpCommandQueue::Result::Unsupported)
    {
        server_.send(501, "application/json", "{\"error\":\"unsupported command\"}");
    }
    else if (result == HttpCommandQueue::Result::QueueFull || result == HttpCommandQueue::Result::NotStarted)
    {
        server_.send(503, "application/json", "{\"error\":\"command queue unavailable\"}");
    }
    else
    {
        server_.send(500, "application/json", "{\"error\":\"internal command error\"}");
    }
}

void HTTPHandler::begin() noexcept 
{
    if (!staticFiles_.mount())
    {
        Logger::log(LOGTYPE, "SPIFFS Mount failed");
    } 
    else 
    {
        Logger::log(LOGTYPE, "SPIFFS ready");
    }

    server_.onNotFound([this]() noexcept 
    {
       
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

        const std::string& requestUri = server_.uri();
        if (!staticFiles_.serve(server_.nativeRequest(), requestUri))
        {
            server_.sendHeader("Connection", "close");
            server_.send(404, "text/plain", "404: File Not Found");
        }
    });
    
    // Internal compatibility handlers. NativeHttpServer rejects direct external
    // access to /set/* and /get/checkUpdate with 410. SecureApi performs strict
    // validation before invoking these handlers in-process so the established
    // command-queue-backed Fachlogik does not need to be rewritten here.
    server_.on("/set/OFF", HTTP_GET, [this]() noexcept 
    {
        if (!enqueueCommand([]() { displayEnabled = false; Logger::log(LOGTYPE, "Display disabled via HTTP"); })) return;
        server_.send(200, "text/plain", "Display disabled");
    });

    server_.on("/set/ON", HTTP_GET, [this]() noexcept 
    {
        if (!enqueueCommand([]() { displayEnabled = true; Logger::log(LOGTYPE, "Display enabled via HTTP"); })) return;
        server_.send(200, "text/plain", "Display enabled");
    });

    server_.on("/set/reset", HTTP_GET, [this]() noexcept 
    {
        Logger::log(LOGTYPE, "System reset requested via HTTP");
        const std::string requestedSource = server_.arg("source");
        const std::string plannedReason = requestedSource == "ota" ? "ota" : "user_http";
        if (!enqueueCommand([this, plannedReason]() { handleReset(plannedReason); })) return;
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/setOldValue", HTTP_GET, [this]() noexcept 
    {
        Logger::log(LOGTYPE, "System apply loadGlobals requested via HTTP");
        if (!enqueueCommand([]() 
        {
            Memory::loadGlobals();
            Globals::applyLogConfig();
        })) return;
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/loadDetectedOverwrite", HTTP_GET, [this]() noexcept 
    {     
        bool loadDetected = false;
        if (!executeCommand([&loadDetected]() {
            Globals::loadDetected = !Globals::loadDetected;
            loadDetected = Globals::loadDetected;
            Logger::log(LOGTYPE, "loadDetected set to %s via HTTP", loadDetected ? "true" : "false");
        })) return;
        server_.send(200, "text/plain", std::string("loadDetected=") + (loadDetected ? "1" : "0"));
    });

    server_.on("/set/resetValue", HTTP_GET, [this]() noexcept 
    {
        Logger::log(LOGTYPE, "Globals Values reset requested via HTTP");
        if (!enqueueCommand([]() { Memory::StorageReset(); })) return;
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/resetWiFi", HTTP_GET, [this]() noexcept 
    {
        if (!enqueueCommand([]() {
            wifiConnector.eraseCredentials();
            Logger::log(LOGTYPE, "WiFi Credentail erase requested via HTTP");
        })) return;
        server_.send(200, "text/plain", "OK");     
    });

    server_.on("/get/wifiSaved", HTTP_GET, [this]() noexcept 
    {
        auto list = wifiConnector.getSavedNetworks();

        auto esc = [](const std::string& s)->std::string{
            std::string r; r.reserve(s.length()+8);
            for (size_t i=0;i<s.length();++i){
                char c=s[i];
                if (c=='\"') r += "\\\"";
                else if (c=='\\') r += "\\\\";
                else if ((uint8_t)c < 0x20){ char b[7]; sprintf(b,"\\u%04x",(uint8_t)c); r+=b; }
                else r += c;
            }
            return r;
        };

        std::string out = "[";
        for (size_t i=0;i<list.size();++i)
        {
            if(i) out += ",";
            out += "{\"ssid\":\""; out += esc(list[i].ssid); out += "\",";
            out += "\"priority\":"; out += std::to_string(list[i].priority); out += ",";
            out += "\"last_ok\":";  out += std::to_string(list[i].last_ok);
            out += "}";
        }
        out += "]";

        server_.sendHeader("Cache-Control","no-store");
        server_.send(200, "application/json", out);
        Logger::log(LOGTYPE, "HTTP /get/wifiSaved -> %u entries", (unsigned)list.size());
    });

    server_.on("/set/wifiAdd", HTTP_POST, [this]() noexcept 
    {
        if (!server_.hasArg("plain") || server_.arg("plain").length() > 1024) 
        {
            server_.send(400, "text/plain", "Invalid request body");
            Logger::log(LOGTYPE, "HTTP /set/wifiAdd invalid body");
            return;
        }

        const std::string& body = server_.body();
        std::string ssid;
        std::string password;
        int prio = 100;
        if (!ewm::utils::json_get_string(body, "ssid", ssid) ||
            ssid.empty() || ssid.size() > 32 ||
            (ewm::utils::json_get_string(body, "password", password) && password.size() > 64))
        {
            server_.send(400, "text/plain", "Invalid credentials");
            Logger::log(LOGTYPE, "HTTP /set/wifiAdd invalid credentials");
            return;
        }
        ewm::utils::json_get_int(body, "priority", prio);
        prio = std::clamp(prio, 0, 254);

        bool ok = false;
        if (!executeCommand([&]() {
            ok = wifiConnector.addOrUpdateNetwork(ssid, password, static_cast<uint8_t>(prio));
        })) return;
        std::fill(password.begin(), password.end(), '\0');
        Logger::log(LOGTYPE, ok ? "WiFi addOrUpdate OK (prio %d)" : "WiFi addOrUpdate failed", prio);

        server_.send(ok ? 200 : 400, "text/plain", ok ? "OK" : "FAIL");
    });

    server_.on("/set/wifiRemove", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("ssid")) 
        {
            server_.send(400, "text/plain", "Missing 'ssid'");
            Logger::log(LOGTYPE, "HTTP /set/wifiRemove missing ssid");
            return;
        }
        const std::string ssid = server_.arg("ssid");
        bool ok = false;
        if (!executeCommand([&]() { ok = wifiConnector.removeNetwork(ssid.c_str()); })) return;

        Logger::log(LOGTYPE, ok ? "WiFi entry removed" : "WiFi entry removal failed/not found");
        server_.send(ok ? 200 : 404, "text/plain", ok ? "OK" : "Not found");
    });

    server_.on("/set/ACP", HTTP_GET, [this]() noexcept 
    {
        enum class Result { Started, DisplayDisabled, ModeRunning };
        Result result = Result::Started;
        if (!executeCommand([this, &result]() 
        {
            if (!displayEnabled && !Globals::loadDetected)
            {
                result = Result::DisplayDisabled;
                Logger::log(LOGTYPE, "Error: Display not enabled");
                return;
            }
            if (mode_running.load(std::memory_order_relaxed))
            {
                result = Result::ModeRunning;
                Logger::log(LOGTYPE, "Error: Another mode is already running");
                return;
            }

            Logger::log(LOGTYPE, "ACP started via HTTP");
            digits = 0;
            if (!runWithClockSuspended(clockTaskHandle, [this]()
            {
                auto check = [](esp_err_t e, const char* what)
                {
                    if (e != ESP_OK)
                    {
                        Logger::log(LOGTYPE, "%s failed (%d)", what, static_cast<int>(e));
                        return false;
                    }
                    return true;
                };

                Logger::log(LOGTYPE, "ACP task: enabling 190V + resistor reduction");

                if (!check(hssController.enable190(), "enable190")) return;
                vTaskDelay(pdMS_TO_TICKS(10));

                if (!check(hssController.enableResistorReduction(), "enableResistorReduction")) return;
                vTaskDelay(pdMS_TO_TICKS(10));

                ACP();

                Logger::log(LOGTYPE, "ACP task: disabling resistor reduction + 190V");

                if (!check(hssController.disableResistorReduction(), "disableResistorReduction")) return;
                vTaskDelay(pdMS_TO_TICKS(10));

                if (!check(hssController.disable190(), "disable190")) return;
                vTaskDelay(pdMS_TO_TICKS(10));

                Logger::log(LOGTYPE, "ACP task finished");
            })) Logger::log(LOGTYPE, "Failed to start ACP task");
        })) return;

        if (result == Result::DisplayDisabled) server_.send(400, "text/plain", "Display not enabled");
        else if (result == Result::ModeRunning) server_.send(400, "text/plain", "A mode is already running");
        else
        server_.send(200, "text/plain", "ACP started");
    });

    server_.on("/set/tempDisplay", HTTP_GET, [this]() noexcept 
    {
        enum class Result { Started, DisplayDisabled, ModeRunning };
        Result result = Result::Started;
        if (!executeCommand([this, &result]() {
            if (!displayEnabled && !Globals::loadDetected)
            {
                result = Result::DisplayDisabled;
                Logger::log(LOGTYPE, "Error: Display not enabled");
                return;
            }
            if (mode_running.load(std::memory_order_relaxed))
            {
                result = Result::ModeRunning;
                Logger::log(LOGTYPE, "Error: Another mode is already running");
                return;
            }
            Logger::log(LOGTYPE, "Temperature display requested via HTTP");
            if (!runWithClockSuspended(clockTaskHandle, [this]()
            {
                mode_running.store(true, std::memory_order_relaxed);
                zipMaskingEnabled = true;
                const auto config = Globals::getTextConfig();
                digits = config.zipCode.empty() ? 0 : std::stoi(config.zipCode) * 10;
                vTaskDelay(pdMS_TO_TICKS(3000));
                displayWeather();
                tempMaskingEnabled = true;
                zipMaskingEnabled = false;
                vTaskDelay(pdMS_TO_TICKS(5000));
                tempMaskingEnabled = false;
                mode_running.store(false, std::memory_order_relaxed);
            })) Logger::log(LOGTYPE, "Failed to start weather display task");
        })) return;

        if (result == Result::DisplayDisabled) server_.send(400, "text/plain", "Display not enabled");
        else if (result == Result::ModeRunning) server_.send(400, "text/plain", "A mode is already running");
        else server_.send(200, "text/plain", "Temperature display started");
    });

    server_.on("/set/DATE", HTTP_GET, [this]() noexcept 
    {
        enum class Result { Started, DisplayDisabled, ModeRunning };
        Result result = Result::Started;
        if (!executeCommand([this, &result]() {
            if (!displayEnabled && !Globals::loadDetected)
            {
                result = Result::DisplayDisabled;
                Logger::log(LOGTYPE, "Error: Display not enabled");
                return;
            }
            if (mode_running.load(std::memory_order_relaxed))
            {
                result = Result::ModeRunning;
                Logger::log(LOGTYPE, "Error: Another mode is already running");
                return;
            }
            Logger::log(LOGTYPE, "Date display requested via HTTP");
            if (!runWithClockSuspended(clockTaskHandle, [this]()
            {
                mode_running.store(true, std::memory_order_relaxed);
                displayDate();
                vTaskDelay(pdMS_TO_TICKS(5000));
                mode_running.store(false, std::memory_order_relaxed);
            })) Logger::log(LOGTYPE, "Failed to start date display task");
        })) return;

        if (result == Result::DisplayDisabled) server_.send(400, "text/plain", "Display not enabled");
        else if (result == Result::ModeRunning) server_.send(400, "text/plain", "A mode is already running");
        else server_.send(200, "text/plain", "Date display started");
    });

    server_.on("/set/CRICKET", HTTP_GET, [this]() noexcept 
    {
        Logger::log(LOGTYPE, "Cricket sound requested via HTTP");

        if (!enqueueCommand([]() { buzzer.startCricketInTask(); })) return;

        server_.send(200, "text/plain", "Cricket sound started");
    });

    server_.on("/set/ticker", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/ticker missing parameter 'value'");
            return;
        }
        const bool enabled = server_.arg("value") != "0";
        if (!enqueueCommand([enabled]() {
            Globals::tickerEnabled = enabled;
            Logger::log(LOGTYPE, "tickerEnabled set to %s via HTTP", enabled ? "true" : "false");
        })) return;
        server_.send(200, "text/plain", std::string("tickerEnabled=") + (enabled ? "1" : "0"));
    });

    server_.on("/set/singleDigitControl", HTTP_GET, [this]() noexcept{
        const auto state = AppState::instance().snapshot();
        if (!state.displayEnabled && !state.loadDetected)
        {
            server_.send(400, "text/plain", "Display not enabled");
            Logger::log(LOGTYPE, "Error: Display not enabled");
            return;
        }
        else if (mode_running.load(std::memory_order_relaxed)) 
        {
            server_.send(400, "text/plain", "A mode is already running");
            Logger::log(LOGTYPE, "Error: Another mode is already running");
            return;
        }

        const bool hasValue = server_.hasArg("value");
        const bool hasDigit = server_.hasArg("digit");
        if (!hasValue && !hasDigit)
        {
            server_.send(400, "text/plain", "Missing 'value' or 'digit'");
            return;
        }

        const bool enabled = hasValue && server_.arg("value") == "1";
        int digitValue = -1;
        if (hasDigit)
        {
            digitValue = static_cast<int>(parseLongCompatible(server_.arg("digit")));
            if (digitValue < 0 || digitValue >= 60)
            {
                server_.send(400, "text/plain", "Invalid 'digit' (must be 0-59)");
                return;
            }
        }

        if (!enqueueCommand([hasValue, enabled, hasDigit, digitValue]() {
            if (hasValue)
            {
                singleDigitACP = enabled;
                Logger::log(LOGTYPE, "singleDigitACP set to %s via HTTP", enabled ? "true" : "false");
                if (enabled)
                {
                    (void)hssController.enable190();
                    (void)hssController.enableResistorReduction();
                }
                else
                {
                    (void)hssController.disableResistorReduction();
                    (void)hssController.disable190();
                }
            }
            if (hasDigit)
            {
                singleDigit = static_cast<uint8_t>(digitValue);
                Logger::log(LOGTYPE, "singleDigit set to %d via HTTP", digitValue);
            }
        })) return;

        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/NixiePWM", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/NixiePWM missing parameter 'value'");
            return;
        }
        const bool disabled = server_.arg("value") != "0";
        if (!enqueueCommand([disabled]() { Globals::PWM_disabled = disabled; })) return;
        Logger::log(LOGTYPE, "Nixie_PWM set to %s via HTTP", 
                    disabled ? "true" : "false");
        server_.send(200, "text/plain", std::string("NixiePWM=") + (disabled ? "1" : "0"));
    });

    server_.on("/set/PWMPeriod", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' parameter (µs)");
            Logger::log(LOGTYPE, "HTTP /set/PWMPeriod fehlte Parameter 'value'");
            return;
        }

        const std::string val = server_.arg("value");
        const std::uint32_t newPeriod = static_cast<std::uint32_t>(parseLongCompatible(val));

        if (newPeriod == 0) 
        {
            server_.send(400, "text/plain", "Invalid value");
            Logger::log(LOGTYPE, "HTTP /set/PWMPeriod invalid value");
            return;
        }

        if (!enqueueCommand([newPeriod]() { PWM_PERIOD_US = newPeriod; })) return;
        Logger::log(LOGTYPE, "PWM_PERIOD_US set to %lu µs via HTTP", static_cast<unsigned long>(newPeriod));

        server_.send(200, "text/plain", std::string("PWM_PERIOD_US=") + std::to_string(newPeriod));
    });

    server_.on("/set/timeLimit", HTTP_GET, [this]() noexcept{
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/timeLimit missing parameter 'value'");
            return;
        }

        const bool enabled = server_.arg("value") != "0";
        const bool hasFrom = server_.hasArg("from");
        const bool hasTo = server_.hasArg("to");
        const std::string from = hasFrom ? server_.arg("from") : "";
        const std::string to = hasTo ? server_.arg("to") : "";

        if (hasFrom && !std::regex_match(from, timeRegex))
        {
            Logger::log(LOGTYPE, "Invalid 'from' format: %s", from.c_str());
            server_.send(400, "text/plain", "Invalid 'from' time format (expected HH:MM:SS)");
            return;
        }
        if (hasTo && !std::regex_match(to, timeRegex))
        {
            Logger::log(LOGTYPE, "Invalid 'to' format: %s", to.c_str());
            server_.send(400, "text/plain", "Invalid 'to' time format (expected HH:MM:SS)");
            return;
        }

        const auto state = AppState::instance().snapshot();
        const std::string currentFrom = hasFrom ? from : state.timeLimitFrom;
        const std::string currentTo = hasTo ? to : state.timeLimitTo;
        if (!enqueueCommand([enabled, hasFrom, from, hasTo, to]() 
        {
            Globals::timeLimitEnabled = enabled;
            auto config = Globals::getTextConfig();
            if (hasFrom) config.timeLimitFrom = from;
            if (hasTo) config.timeLimitTo = to;
            Globals::setTextConfig(config);
            Logger::log(LOGTYPE, "timeLimitEnabled set to %s via HTTP", enabled ? "true" : "false");
            Logger::log(LOGTYPE, "timeLimit active from %s to %s",
                        config.timeLimitFrom.c_str(), config.timeLimitTo.c_str());
        })) return;

        server_.send(200, "text/plain", std::string("timeLimitEnabled=") + (enabled ? "1" : "0") + "\nfrom=" + currentFrom + "\nto=" + currentTo);
    });

    server_.on("/set/brightnessConfig", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("field") || !server_.hasArg("value"))
        {
            server_.send(400, "text/plain", "Missing 'field' or 'value' parameter");
            Logger::log(LOGTYPE, "HTTP /set/brightnessConfig fehlte 'field' oder 'value'");
            return;
        }

        const std::string field = server_.arg("field");
        const std::string val   = server_.arg("value");
        const long parsedValue = parseLongCompatible(val);

        if (parsedValue < 0 || parsedValue > 255)
        {
            server_.send(400, "text/plain", "Invalid value range");
            Logger::log(LOGTYPE, "HTTP /set/brightnessConfig invalid range: %ld", parsedValue);
            return;
        }

        const uint8_t newValue = static_cast<uint8_t>(parsedValue);
        bool validField = true;

        if (field == "nightStart")
        {
            if (newValue > 23)
            {
                server_.send(400, "text/plain", "nightStart must be 0..23");
                return;
            }
            if (!enqueueCommand([newValue]() { Globals::brightnessNightStartHour = newValue; })) return;
        }
        else if (field == "nightEnd")
        {
            if (newValue > 23)
            {
                server_.send(400, "text/plain", "nightEnd must be 0..23");
                return;
            }
            if (!enqueueCommand([newValue]() { Globals::brightnessNightEndHour = newValue; })) return;
        }
        else if (field == "dimStart")
        {
            if (newValue > 23)
            {
                server_.send(400, "text/plain", "dimStart must be 0..23");
                return;
            }
            if (!enqueueCommand([newValue]() { Globals::brightnessDimStartHour = newValue; })) return;
        }
        else if (field == "dimEnd")
        {
        if (newValue > 23)
        {
            server_.send(400, "text/plain", "dimEnd must be 0..23");
            return;
        }
            if (!enqueueCommand([newValue]() { Globals::brightnessDimEndHour = newValue; })) return;
        }
        else if (field == "nightValue")
        {
            if (!enqueueCommand([newValue]() { Globals::brightnessNightValue = newValue; })) return;
        }
        else if (field == "dimValue")
        {
            if (!enqueueCommand([newValue]() { Globals::brightnessDimValue = newValue; })) return;
        }
        else if (field == "dayValue")
        {
            if (!enqueueCommand([newValue]() { Globals::brightnessDayValue = newValue; })) return;
        }
        else
        {
            validField = false;
        }

        if (!validField)
        {
            server_.send(400, "text/plain","Invalid field. Use: nightStart, nightEnd, dimStart, dimEnd, nightValue, dimValue, dayValue");
            Logger::log(LOGTYPE, "HTTP /set/brightnessConfig invalid field: %s", field.c_str());
            return;
        }

        Logger::log(LOGTYPE, "Brightness config updated: %s=%u", field.c_str(), static_cast<unsigned>(newValue));
        server_.send(200, "text/plain", field + "=" + std::to_string(newValue));
    });

    server_.on("/set/silentMode", HTTP_GET, [this]() noexcept
    {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/silentMode fehlte Parameter 'value'");
            return;
        }

        const bool enabled = server_.arg("value") != "0";
        if (!enqueueCommand([enabled]() {
        Globals::SilentModeEnabled = enabled;
        Logger::log(LOGTYPE, "silentModeEnabled set to %s via HTTP",
                    enabled ? "true" : "false");

        gpio_config_t io_conf = 
        {
            .pin_bit_mask = (1ULL << 40),
            .mode = enabled ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = enabled ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);

        if (!enabled)
        {
            gpio_set_level(GPIO_NUM_40, 0);
        }
        })) return;

        server_.send(200, "text/plain", std::string("SilentModeEnabled=") + (enabled ? "1" : "0"));
    });

    server_.on("/set/noACPatNight", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("value"))
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/noACPatNight fehlte Parameter 'value'");
            return;
        }

        const bool enabled = server_.arg("value") != "0";
        if (!enqueueCommand([enabled]() { Globals::noACPatNight = enabled; })) return;

        Logger::log(LOGTYPE, "noACPatNight set to %s via HTTP", enabled ? "true" : "false");

        server_.send(200, "text/plain", std::string("noACPatNight=") + (enabled ? "1" : "0"));
    });

    server_.on("/set/manualBrightness", HTTP_GET, [this]() noexcept 
    {
        const bool hasEnabled = server_.hasArg("value");
        const bool enabled = hasEnabled && server_.arg("value") == "true";
        const bool hasBrightness = server_.hasArg("brightness");
        long parsedBrightness = hasBrightness ? strtol(server_.arg("brightness").c_str(), nullptr, 10) : 0;
        if (parsedBrightness < 0) parsedBrightness = 0;
        if (parsedBrightness > 100) parsedBrightness = 100;
        if ((hasEnabled || hasBrightness) && !enqueueCommand([=]() {
            if (hasEnabled)
            {
                Globals::manualBrightnessEnabled = enabled;
                Logger::log(LOGTYPE, "manualBrightnessEnabled=%s", enabled ? "true" : "false");
            }
            if (hasBrightness)
            {
                brightness = static_cast<uint32_t>(parsedBrightness);
                Logger::log(LOGTYPE, "brightness=%u", static_cast<unsigned>(parsedBrightness));
            }
        })) return;
        server_.send(200, "text/plain", "OK");
    });

    server_.on("/set/weatherUpdate", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/weatherUpdate fehlte Parameter 'value'");
            return;
        }
        const bool enabled = server_.arg("value") != "0";
        if (!enqueueCommand([enabled]() { Globals::WeatherUpdateEnabled = enabled; })) return;
        Logger::log(LOGTYPE, "WeatherUpdateEnabled set to %s via HTTP", enabled ? "true" : "false");
        server_.send(200, "text/plain", std::string("WeatherUpdateEnabled=") + (enabled ? "1" : "0"));
    });

    server_.on("/set/randomCricket", HTTP_GET, [this]() noexcept {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' (0 or 1)");
            Logger::log(LOGTYPE, "HTTP /set/randomCricket fehlte Parameter 'value'");
            return;
        }
        const bool enabled = server_.arg("value") != "0";
        if (!enqueueCommand([enabled]() { Globals::cricketSoundEnabled = enabled; })) return;
        Logger::log(LOGTYPE, "cricketSoundEnabled set to %s via HTTP", enabled ? "true" : "false");
        server_.send(200, "text/plain", std::string("cricketSoundEnabled=") + (enabled ? "1" : "0"));
    });

    server_.on("/set/logConfig", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("value")) 
        {
            server_.send(400, "text/plain", "Missing 'value' parameter");
            Logger::log(LOGTYPE, "HTTP /set/logConfig missing parameter 'value'");
            return;
        }
        const std::string val = server_.arg("value");
        char* endptr = nullptr;
        unsigned long newConfig = strtoul(val.c_str(), &endptr, 10);
        if (endptr == val.c_str() || *endptr != '\0') 
        {
            server_.send(400, "text/plain", "Invalid 'value' (not a number)");
            Logger::log(LOGTYPE, "HTTP /set/logConfig invalid value: %s", val.c_str());
            return;
        }
        const uint32_t config = static_cast<uint32_t>(newConfig);
        if (!enqueueCommand([config]() {
            Globals::logConfig = config;
            Globals::applyLogConfig();
        })) return;

        std::string binStr;
        binStr.reserve(9);
        for (int i = 8; i >= 0; --i) 
        {
            binStr += ((config >> i) & 1) ? '1' : '0';
        }

        Logger::log(LOGTYPE, "logConfig set to %s and applied", binStr.c_str());
        server_.send(200, "text/plain", std::string("logConfig=") + binStr);
    });

    server_.on("/set/firmware", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("target")) 
        {
            server_.send(400, "text/plain", "Missing 'target' parameter");
            Logger::log(LOGTYPE, "HTTP /set/firmware fehlte Parameter 'target'");
            return;
        }
        const std::string arg = server_.arg("target");
        Globals::FirmwareTarget newTarget = Globals::FirmwareTarget::COUNT;

        if (arg == "NixieV6_std") 
        {
            newTarget = Globals::FirmwareTarget::NixieV6_std;
        }
        else if (arg == "NixieV6_dev") 
        {
            newTarget = Globals::FirmwareTarget::NixieV6_dev;
        }
        else if (arg == "NixieV6_BOS") 
        {
            newTarget = Globals::FirmwareTarget::NixieV6_BOS;
        }

        if (newTarget == Globals::FirmwareTarget::COUNT) 
        {
            server_.send(400, "text/plain", "Invalid 'target' value");
            Logger::log(LOGTYPE, "HTTP /set/firmware invalid value: %s", arg.c_str());
            return;
        }

            if (!enqueueCommand([newTarget]() { Globals::currentFirmwareTarget = newTarget; })) return;
            Logger::log(LOGTYPE, "Firmware target set to %s", arg.c_str());
            server_.send(200, "text/plain", std::string("firmwareTarget=") + arg);
        });

    server_.on("/set/ota", HTTP_GET, [this]() noexcept 
    {
        enum class Result { Started, InvalidTarget, AlreadyRunning, StartFailed };
        Result result = Result::InvalidTarget;
        std::string statusJson;
        if (!executeCommand([&]() {
            const Globals::FirmwareTarget target = Globals::currentFirmwareTarget;
            const std::string baseUrl = Globals::getFirmwareUrl(target);
            displayEnabled = false;
            if (baseUrl.empty())
            {
                Logger::log(LOGTYPE, "OTA failed: invalid firmware target");
                result = Result::InvalidTarget;
                return;
            }
            auto& ota = OTAManager::instance();
            if (ota.isRunning())
            {
                Logger::log(LOGTYPE, "OTA start rejected: already running");
                result = Result::AlreadyRunning;
                return;
            }
            Logger::log(LOGTYPE, "Starting OTA async in folder: %s", baseUrl.c_str());
            if (!ota.startAsync(baseUrl))
            {
                Logger::log(LOGTYPE, "OTA task could not be started");
                result = Result::StartFailed;
                return;
            }
            statusJson = ota.getStatusJson();
            result = Result::Started;
        })) return;

        server_.sendHeader("Cache-Control", "no-store");
        if (result == Result::InvalidTarget)
            server_.send(400, "application/json", "{\"started\":false,\"error\":\"Invalid firmware target\"}");
        else if (result == Result::AlreadyRunning)
            server_.send(409, "application/json", "{\"started\":false,\"error\":\"OTA already running\"}");
        else if (result == Result::StartFailed)
            server_.send(500, "application/json", "{\"started\":false,\"error\":\"Could not start OTA task\"}");
        else server_.send(202, "application/json", statusJson.c_str());
    });

    server_.on("/get/otaStatus", HTTP_GET, [this]() noexcept 
    {
        server_.sendHeader("Cache-Control", "no-store");
        const std::string statusJson = OTAManager::instance().getStatusJson();
        server_.send(200, "application/json", statusJson.c_str());
    });

    server_.on("/set/otaResetStatus", HTTP_GET, [this]() noexcept 
    {
        bool running = false;
        if (!executeCommand([&running]() {
            auto& ota = OTAManager::instance();
            running = ota.isRunning();
            if (!running) ota.resetStatus();
        })) return;
        server_.send(running ? 409 : 200, "text/plain", running ? "OTA still running" : "OK");
    });

    server_.on("/get/checkUpdate", HTTP_GET, [this]() noexcept 
    {
        if (!enqueueCommand([]() {
            const std::string baseUrl = Globals::getFirmwareUrl(Globals::currentFirmwareTarget);
            if (!startUpdateCheckTask(baseUrl))
            {
                Logger::log(LOGTYPE, "Could not start update-check task");
            }
        })) return;
        server_.send(202, "text/plain", "Update check queued");
    });

    server_.on("/set/zip", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("zip"))
        {
            server_.send(400, "text/plain", "Missing 'zip' parameter");
            Logger::log(LOGTYPE, "HTTP /set/zip missing parameter 'zip'");
            return;
        }

        const std::string zipArg = server_.arg("zip");

        if (!isValidZipCode(zipArg))
        {
            server_.send(400, "text/plain", "Invalid 'zip' parameter");
            Logger::log(LOGTYPE, "HTTP /set/zip invalid ZIP: %s", zipArg.c_str());
            return;
        }

        if (!enqueueCommand([zip = zipArg]() 
        {
            auto config = Globals::getTextConfig();
            config.zipCode = zip;
            Globals::setTextConfig(std::move(config));
        })) return;
        Logger::log(LOGTYPE, "ZIP-Code updated to %s", zipArg.c_str());
        server_.send(200, "text/plain", "ZIP-Code updated to " + zipArg);
    });

    server_.on("/set/timezone", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("tz")) 
        {
            server_.send(400, "text/plain", "Missing 'tz' parameter");
            Logger::log(LOGTYPE, "HTTP /set/timezone missing parameter 'tz'");
            return;
        }   

        const std::string tzArg = server_.arg("tz");
        Globals::TimeZone newTz = Globals::TimeZone::CET;
        bool ok = false;

        const int idx = static_cast<int>(parseLongCompatible(tzArg));
        if (tzArg == std::to_string(idx) && idx >= 0 && idx < tzCount)
        {
            newTz = static_cast<Globals::TimeZone>(idx);
            ok = true;
        }
        else 
        {  
            for (int i = 0; i < tzCount; ++i) 
            {
                if (equalsIgnoreCase(tzArg, tzNames[i]))
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

        const char* spec = Globals::getPosixTZ(newTz);
        if (!enqueueCommand([newTz]() { Globals::currentTimeZone = newTz; })) return;
        Logger::log(LOGTYPE, "TimeZone set via HTTP (volatile): %d -> %s", static_cast<int>(newTz), spec);

        server_.send(200, "application/json", std::string("{\n  \"status\": \"ok\",\n  \"CurrentTimeZoneIndex\": ") +
                     std::to_string(static_cast<int>(newTz)) + ",\n  \"CurrentTimeZoneSpec\": \"" + spec + "\"\n}\n");
    });

    server_.on("/set/timer", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("enabled"))
        {
            server_.send(400, "text/plain", "Missing 'enabled' parameter");
            return;
        }

        const std::string enabledArg = server_.arg("enabled");
        if (enabledArg != "0" && enabledArg != "1")
        {
            server_.send(400, "text/plain", "Invalid 'enabled' parameter");
            return;
        }

        const bool enable = enabledArg == "1";

        if (enable)
        {
            if (!server_.hasArg("seconds"))
            {
                server_.send(400, "text/plain", "Missing 'seconds' parameter");
                return;
            }

            uint32_t seconds = 0;
            if (!parseUInt32Strict(server_.arg("seconds"), seconds) || seconds == 0)
            {
                server_.send(400, "text/plain", "Invalid 'seconds' parameter");
                return;
            }

            if (!enqueueCommand([seconds]() { timer.start(seconds, []() {buzzer.startAlarm(3);}); })) return;

            server_.send(200, "text/plain", "Timer started");
        }
        else
        {
            if (!enqueueCommand([]() { timer.stop(); })) return;
            server_.send(200, "text/plain", "Timer stopped");
        }
    });

    server_.on("/set/alarm", HTTP_GET, [this]() noexcept 
    {
        if (!server_.hasArg("enabled"))
        {
            server_.send(400, "text/plain", "Missing 'enabled' parameter");
            return;
        }

        const std::string enabledArg = server_.arg("enabled");
        if (enabledArg != "0" && enabledArg != "1")
        {
            server_.send(400, "text/plain", "Invalid 'enabled' parameter");
            return;
        }

        const bool enable = enabledArg == "1";

        if (enable)
        {
            if (!server_.hasArg("time"))
            {
                server_.send(400, "text/plain", "Missing 'time' parameter");
                return;
            }

            uint8_t hour = 0;
            uint8_t minute = 0;
            if (!parseAlarmTime(server_.arg("time"), hour, minute))
            {
                server_.send(400, "text/plain", "Invalid time format (HH:MM)");
                return;
            }

            if (!enqueueCommand([hour, minute]() { alarmClock.setAlarm(hour, minute, []() {buzzer.startAlarm(5);}); })) return;

            server_.send(200, "text/plain", "Alarm set");
        }
        else
        {
            if (!enqueueCommand([]() { alarmClock.removeAlarm(); })) return;
            server_.send(200, "text/plain", "Alarm cleared");
        }
    });

    server_.on("/get/brownout", HTTP_GET, [this]() noexcept 
    {
        std::string json;
        if (!executeCommand([&json]() { Memory::ReadBrownoutLog(json); })) return;

        if (json.empty()) {
            server_.send(200, "text/plain", "None.");
        }
        else 
        {
            server_.send(200, "application/json", json);
        }
    });

    server_.on("/set/brownout", HTTP_GET, [this]() noexcept 
    {
        if (!enqueueCommand([]() { Memory::BrownoutReset(); })) return;
        server_.send(204, "text/plain", "");
    });

    server_.on("/get/taskStats", HTTP_GET, [this]() noexcept 
    {

        if (otaBusy())
        {
            sendOtaBusy(server_);
            return;
        }

        server_.sendHeader("Cache-Control", "no-store");
        const std::string taskStats = StatsMonitor::instance().getTaskStatsJson(12, true);
        server_.send(200, "application/json", taskStats.c_str());
    });

    server_.on("/get/info", HTTP_GET, [this]() noexcept 
    {
        if (otaBusy())
        {
            sendOtaBusy(server_);
            return;
        }

        const auto snapshot = AppState::instance().snapshot();
        if (snapshot.infoJson.empty())
        {
            server_.send(503, "application/json", "{\"error\":\"telemetry unavailable\"}");
            return;
        }
        server_.sendHeader("Cache-Control", "no-store");
        server_.sendHeader("X-AppState-Revision", std::to_string(snapshot.revision).c_str());
        server_.send(200, "application/json", snapshot.infoJson);
    });

    server_.on("/events", HTTP_GET, [this]() noexcept 
    {
        const auto result = AppState::instance().attachSseClient(server_.nativeRequest());
        if (result == AppState::SseAttachResult::Attached ||
            result == AppState::SseAttachResult::ErrorResponseSent)
        {
            return;
        }
        if (result == AppState::SseAttachResult::AtCapacity ||
            result == AppState::SseAttachResult::NotStarted)
        {
            server_.send(503, "application/json", "{\"error\":\"event stream unavailable\"}");
            return;
        }
        server_.send(500, "application/json", "{\"error\":\"event stream setup failed\"}");
    });

    secureApi_.registerRoutes();

    if (!commands_.start())
    {
        Logger::log(LOGTYPE, "HTTP command task start failed");
        return;
    }
    if (!AppState::instance().start())
    {
        Logger::log(LOGTYPE, "AppState telemetry task start failed");
        return;
    }
    if (!server_.configureBasicAuth(NIXIE_HTTP_USERNAME, NIXIE_HTTP_PASSWORD))
    {
        Logger::log(LOGTYPE, "HTTP authentication configuration invalid; server not started");
        return;
    }
    if (!server_.begin())
    {
        Logger::log(LOGTYPE, "Native HTTP server start failed");
        return;
    }
    Logger::log(LOGTYPE, "Native HTTP server started (authentication %s)",
                (NIXIE_HTTP_USERNAME[0] != '\0' && NIXIE_HTTP_PASSWORD[0] != '\0') ? "enabled" : "disabled");
}

void HTTPHandler::handleReset(std::string plannedReason) noexcept 
{
    Logger::log(LOGTYPE, "Performing system reset...");
    ResetDiagnostics::instance().markPlannedRestart(plannedReason);
    Memory::saveGlobals();
    const BaseType_t created = xTaskCreate([](void*) {
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }, "HTTPReset", 4096, nullptr, 3, nullptr);
    if (created != pdPASS)
    {
        Logger::log(LOGTYPE, "Could not create delayed reset task; resetting immediately");
        esp_restart();
    }
}
