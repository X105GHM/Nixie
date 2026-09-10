#include "SecureApi.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "History/ChartHistory.hpp"
#include "Diagnostics/ResetDiagnostics.hpp"
#include "Globals/Globals.hpp"
#include "cJSON.h"
#include "Logger/Logger.hpp"

namespace
{
    constexpr const char* TAG = "SecureApi";

    class ChunkedJsonWriter final
    {
    public:
        explicit ChunkedJsonWriter(httpd_req_t* request) noexcept : request_(request) {}

        bool append(std::string_view text) noexcept
        {
            while (ok_ && !text.empty())
            {
                const size_t available = buffer_.size() - used_;
                if (available == 0 && !flush()) return false;
                const size_t count = std::min(buffer_.size() - used_, text.size());
                std::memcpy(buffer_.data() + used_, text.data(), count);
                used_ += count;
                text.remove_prefix(count);
            }
            return ok_;
        }

        bool appendUnsigned(uint64_t value) noexcept
        {
            char number[32]{};
            const int length = std::snprintf(
                number, sizeof(number), "%llu", static_cast<unsigned long long>(value));
            return length > 0 && append(std::string_view(number, static_cast<size_t>(length)));
        }

        bool appendSigned(int64_t value) noexcept
        {
            char number[32]{};
            const int length = std::snprintf(
                number, sizeof(number), "%lld", static_cast<long long>(value));
            return length > 0 && append(std::string_view(number, static_cast<size_t>(length)));
        }

        bool appendFloat(float value) noexcept
        {
            if (!std::isfinite(value)) return append("null");
            char number[32]{};
            const int length = std::snprintf(number, sizeof(number), "%.7g", static_cast<double>(value));
            return length > 0 && append(std::string_view(number, static_cast<size_t>(length)));
        }

        bool finish() noexcept
        {
            if (!flush()) return false;
            ok_ = request_ && httpd_resp_send_chunk(request_, nullptr, 0) == ESP_OK;
            return ok_;
        }

    private:
        bool flush() noexcept
        {
            if (!ok_ || used_ == 0) return ok_;
            ok_ = request_ && httpd_resp_send_chunk(request_, buffer_.data(), used_) == ESP_OK;
            used_ = 0;
            return ok_;
        }

        httpd_req_t* request_{nullptr};
        std::array<char, 1024> buffer_{};
        size_t used_{0};
        bool ok_{true};
    };

    class HistorySnapshotGuard final
    {
    public:
        HistorySnapshotGuard() = default;
        ~HistorySnapshotGuard() { ChartHistory::instance().releaseSnapshot(); }
        HistorySnapshotGuard(const HistorySnapshotGuard&) = delete;
        HistorySnapshotGuard& operator=(const HistorySnapshotGuard&) = delete;
    };

    bool equalsIgnoreCase(std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size()) return false;
        for (size_t index = 0; index < left.size(); ++index)
        {
            const unsigned char a = static_cast<unsigned char>(left[index]);
            const unsigned char b = static_cast<unsigned char>(right[index]);
            if (std::tolower(a) != std::tolower(b))
                return false;
        }
        return true;
    }

    bool isJsonContentType(const std::string &contentType) noexcept
    {
        const size_t separator = contentType.find(';');
        const size_t mediaTypeLength =
            separator == std::string::npos ? contentType.size() : separator;
        std::string_view mediaType(contentType.data(), mediaTypeLength);
        while (!mediaType.empty() && std::isspace(static_cast<unsigned char>(mediaType.front())))
            mediaType.remove_prefix(1);
        while (!mediaType.empty() && std::isspace(static_cast<unsigned char>(mediaType.back())))
            mediaType.remove_suffix(1);
        return equalsIgnoreCase(mediaType, "application/json");
    }

    bool isKnownTimezone(const std::string &value) noexcept
    {
        static constexpr const char *NAMES[] = {
            "CET", "EET", "WET", "UTC", "EST", "CST", "MST", "PST", "HST", "JST", "IST", "AEST", "AWST"};
        return std::any_of(std::begin(NAMES), std::end(NAMES), [&value](const char *name)
                           { return equalsIgnoreCase(value, name); });
    }

    std::string boolArgument(bool value)
    {
        return value ? "1" : "0";
    }
}

void SecureApi::registerRoutes()
{
    server_.on("/api/v1/commands", HTTP_POST, [this]()
               { handleCommand(); });
    server_.on("/api/v1/wifi/networks", HTTP_PUT, [this]()
               { handleWifiUpsert(); });
    server_.on("/api/v1/wifi/networks", HTTP_DELETE, [this]()
               { handleWifiRemove(); });
    server_.on("/api/v1/wifi/credentials", HTTP_DELETE, [this]()
               { handleWifiErase(); });

    // Read-only aliases allow the frontend to leave the legacy namespace too.
    server_.on("/api/v1/status", HTTP_GET, [this]()
               { (void)invoke("/get/info", HTTP_GET); });
    server_.on("/api/v1/events", HTTP_GET, [this]()
               { (void)invoke("/events", HTTP_GET); });
    server_.on("/api/v1/history", HTTP_GET, [this]()
               { handleHistory(); });
    server_.on("/api/v1/wifi/networks", HTTP_GET, [this]()
               { (void)invoke("/get/wifiSaved", HTTP_GET); });
    server_.on("/api/v1/ota/status", HTTP_GET, [this]()
               { (void)invoke("/get/otaStatus", HTTP_GET); });
    server_.on("/api/v1/task-stats", HTTP_GET, [this]()
               { (void)invoke("/get/taskStats", HTTP_GET); });
    server_.on("/api/v1/brownout-log", HTTP_GET, [this]()
               { (void)invoke("/get/brownout", HTTP_GET); });
    server_.on("/api/v1/reset-log", HTTP_GET, [this]()
               { handleResetLog(); });
}

void SecureApi::handleResetLog()
{
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", ResetDiagnostics::instance().json());
}

void SecureApi::handleHistory()
{
    const std::string rangeName = server_.arg("range");
    ChartHistory::Snapshot snapshot{};
    const ChartHistory::SnapshotResult result =
        ChartHistory::instance().acquireSnapshot(rangeName, snapshot);

    if (result == ChartHistory::SnapshotResult::InvalidRange)
    {
        sendError(400, "invalid_range", "range must be one of 10m, 1h, 6h, 12h, 24h or 7d");
        return;
    }
    if (result == ChartHistory::SnapshotResult::Unavailable)
    {
        sendError(503, "history_unavailable", "PSRAM history is unavailable; live telemetry remains active");
        return;
    }
    if (result == ChartHistory::SnapshotResult::Busy)
    {
        sendError(503, "history_busy", "Another history snapshot is currently being transferred");
        return;
    }

    HistorySnapshotGuard snapshotGuard;
    httpd_req_t* request = server_.nativeRequest();
    if (!request)
    {
        Logger::log(LoggerType::HTTP, "History request lost its native request context");
        return;
    }

    esp_err_t headerResult = httpd_resp_set_status(request, "200 OK");
    if (headerResult == ESP_OK) headerResult = httpd_resp_set_type(request, "application/json");
    if (headerResult == ESP_OK) headerResult = httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (headerResult == ESP_OK) headerResult = httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    if (headerResult == ESP_OK) headerResult = httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    if (headerResult != ESP_OK)
    {
        Logger::log(LoggerType::HTTP, "Could not prepare history response: %s", esp_err_to_name(headerResult));
        return;
    }

    ChunkedJsonWriter writer(request);
    bool ok = writer.append("{\"range\":\"") &&
              writer.append(snapshot.range->name) &&
              writer.append("\",\"resolutionSeconds\":") &&
              writer.appendUnsigned(snapshot.resolutionSeconds) &&
              writer.append(",\"start\":") &&
              writer.appendSigned(snapshot.startTimestampSeconds * 1000) &&
              writer.append(",\"end\":") &&
              writer.appendSigned(snapshot.endTimestampSeconds * 1000) &&
              writer.append(",\"count\":") &&
              writer.appendUnsigned(snapshot.count) &&
              writer.append(",\"series\":[");

    for (size_t series = 0; ok && series < chart_history::SERIES_COUNT; ++series)
    {
        if (series != 0) ok = writer.append(",");
        ok = ok && writer.append("\"") && writer.append(chart_history::SERIES_NAMES[series]) && writer.append("\"");
    }
    ok = ok && writer.append("],\"timestamps\":[");
    for (size_t index = 0; ok && index < snapshot.count; ++index)
    {
        if (index != 0) ok = writer.append(",");
        ok = ok && writer.appendSigned(snapshot.points[index].timestampSeconds * 1000);
    }
    ok = ok && writer.append("],\"sampleCounts\":[");
    for (size_t index = 0; ok && index < snapshot.count; ++index)
    {
        if (index != 0) ok = writer.append(",");
        ok = ok && writer.appendUnsigned(snapshot.points[index].sampleCount);
    }
    ok = ok && writer.append("]");

    const auto appendMatrix = [&](std::string_view name, float chart_history::AggregateValue::* field) -> bool
    {
        if (!writer.append(",\"") || !writer.append(name) || !writer.append("\":[")) return false;
        for (size_t series = 0; series < chart_history::SERIES_COUNT; ++series)
        {
            if (series != 0 && !writer.append(",")) return false;
            if (!writer.append("[")) return false;
            for (size_t index = 0; index < snapshot.count; ++index)
            {
                if (index != 0 && !writer.append(",")) return false;
                if (!writer.appendFloat(snapshot.points[index].values[series].*field)) return false;
            }
            if (!writer.append("]")) return false;
        }
        return writer.append("]");
    };

    ok = ok && appendMatrix("average", &chart_history::AggregateValue::average);
    ok = ok && appendMatrix("minimum", &chart_history::AggregateValue::minimum);
    ok = ok && appendMatrix("maximum", &chart_history::AggregateValue::maximum);
    ok = ok && writer.append("}") && writer.finish();
    if (!ok) Logger::log(LoggerType::HTTP, "History response transmission failed");
}

cJSON *SecureApi::parseJsonRequest()
{
    if (!validateCsrfRequest()) return nullptr;
    if (server_.contentLength() == 0)
    {
        sendError(400, "empty_body", "A JSON request body is required");
        return nullptr;
    }
    if (server_.contentLength() > MAX_JSON_BODY_SIZE)
    {
        server_.sendHeader("Connection", "close");
        sendError(413, "body_too_large", "The request body exceeds 1024 bytes");
        return nullptr;
    }
    if (!isJsonContentType(server_.header("Content-Type", 128)))
    {
        sendError(415, "unsupported_media_type", "Content-Type must be application/json");
        return nullptr;
    }
    if (!server_.hasArg("plain"))
    {
        sendError(400, "body_read_failed", "The request body could not be read");
        return nullptr;
    }

    const std::string &body = server_.body();
    const char *parseEnd = nullptr;
    cJSON *root = cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, &parseEnd, false);
    if (!root || !cJSON_IsObject(root))
    {
        if (root) cJSON_Delete(root);
        sendError(400, "invalid_json", "The body must contain one JSON object");
        return nullptr;
    }

    const char *end = body.c_str() + body.size();
    while (parseEnd && parseEnd < end && std::isspace(static_cast<unsigned char>(*parseEnd))) ++parseEnd;
    if (!parseEnd || parseEnd != end)
    {
        cJSON_Delete(root);
        sendError(400, "invalid_json", "Trailing data after the JSON object is not allowed");
        return nullptr;
    }
    return root;
}

bool SecureApi::validateCsrfRequest()
{
    if (server_.header("X-Nixie-CSRF", 32) != "1")
    {
        sendError(403, "csrf_rejected", "Missing or invalid CSRF request header");
        return false;
    }

    const std::string fetchSite = server_.header("Sec-Fetch-Site", 32);
    if (equalsIgnoreCase(fetchSite, "cross-site"))
    {
        sendError(403, "csrf_rejected", "Cross-site requests are not allowed");
        return false;
    }

    const std::string origin = server_.header("Origin", 256);
    if (!origin.empty())
    {
        const std::string host = server_.header("Host", 128);
        if (host.empty() || (origin != "http://" + host && origin != "https://" + host))
        {
            sendError(403, "csrf_rejected", "The Origin header does not match this device");
            return false;
        }
    }
    return true;
}

void SecureApi::handleCommand()
{
    cJSON *root = parseJsonRequest();
    if (!root)
        return;

    std::string command;
    if (!getRequiredString(root, "command", command, 1, 40))
    {
        cJSON_Delete(root);
        sendError(400, "invalid_command", "command must be a non-empty string");
        return;
    }

    const auto noArguments = [this, root, &command](const char *name, const char *path) -> bool
    {
        if (command != name)
            return false;
        if (!hasOnlyFields(root, {"command"}))
            sendError(400, "invalid_fields", "This command does not accept additional fields");
        else
            (void)invoke(path, HTTP_GET);
        return true;
    };

    if (command == "system.restart")
    {
        const cJSON* reasonItem = cJSON_GetObjectItemCaseSensitive(root, "reason");
        const std::string reason = cJSON_IsString(reasonItem) && reasonItem->valuestring
            ? reasonItem->valuestring
            : "user";
        if (!hasOnlyFields(root, {"command", "reason"}) ||
            (reason != "user" && reason != "ota"))
        {
            sendError(400, "invalid_fields", "reason must be user or ota");
        }
        else
        {
            (void)invoke("/set/reset", HTTP_GET, {{"source", reason}});
        }
        cJSON_Delete(root);
        return;
    }

    if (noArguments("config.reload", "/set/setOldValue") ||
        noArguments("config.reset", "/set/resetValue") ||
        noArguments("load.toggle", "/set/loadDetectedOverwrite") ||
        noArguments("display.acp", "/set/ACP") ||
        noArguments("display.weather", "/set/tempDisplay") ||
        noArguments("display.date", "/set/DATE") ||
        noArguments("sound.cricket", "/set/CRICKET") ||
        noArguments("ota.start", "/set/ota") ||
        noArguments("ota.status.reset", "/set/otaResetStatus") ||
        noArguments("update.check", "/get/checkUpdate") ||
        noArguments("brownout.clear", "/set/brownout"))
    {
        cJSON_Delete(root);
        return;
    }

    bool enabled = false;
    const auto booleanCommand = [this, root, &command, &enabled](const char *name, const char *path) -> bool
    {
        if (command != name) return false;
        if (!hasOnlyFields(root, {"command", "enabled"}) ||
            !getRequiredBool(root, "enabled", enabled))
            sendError(400, "invalid_fields", "enabled must be a JSON boolean");
        else
            (void)invoke(path, HTTP_GET, {{"value", boolArgument(enabled)}});
        return true;
    };

    if (command == "display.set")
    {
        if (!hasOnlyFields(root, {"command", "enabled"}) || !getRequiredBool(root, "enabled", enabled))
            sendError(400, "invalid_fields", "enabled must be a JSON boolean");
        else
            (void)invoke(enabled ? "/set/ON" : "/set/OFF", HTTP_GET);
    }
    else if (booleanCommand("ticker.set", "/set/ticker") ||
             booleanCommand("pwm.set", "/set/NixiePWM") ||
             booleanCommand("silent.set", "/set/silentMode") ||
             booleanCommand("acp_night.set", "/set/noACPatNight") ||
             booleanCommand("weather.set", "/set/weatherUpdate") ||
             booleanCommand("cricket.set", "/set/randomCricket"))
    {
    }
    else if (command == "display.single")
    {
        bool hasEnabled = false;
        bool hasDigit = false;
        int64_t digit = 0;
        if (!hasOnlyFields(root, {"command", "enabled", "digit"}) ||
            !getOptionalBool(root, "enabled", hasEnabled, enabled) ||
            !getOptionalInteger(root, "digit", 0, 59, hasDigit, digit) ||
            (!hasEnabled && !hasDigit))
        {
            sendError(400, "invalid_fields", "Provide enabled and/or a digit from 0 to 59");
        }
        else if (hasEnabled && hasDigit)
            (void)invoke("/set/singleDigitControl", HTTP_GET,
                         {{"value", boolArgument(enabled)}, {"digit", std::to_string(digit)}});
        else if (hasEnabled)
            (void)invoke("/set/singleDigitControl", HTTP_GET, {{"value", boolArgument(enabled)}});
        else
            (void)invoke("/set/singleDigitControl", HTTP_GET, {{"digit", std::to_string(digit)}});
    }
    else if (command == "pwm.period")
    {
        int64_t microseconds = 0;
        if (!hasOnlyFields(root, {"command", "microseconds"}) ||
            !getRequiredInteger(root, "microseconds", 100, 1000000, microseconds))
            sendError(400, "invalid_fields", "microseconds must be an integer from 100 to 1000000");
        else
            (void)invoke("/set/PWMPeriod", HTTP_GET, {{"value", std::to_string(microseconds)}});
    }
    else if (command == "time_limit.set")
    {
        const cJSON *fromItem = cJSON_GetObjectItemCaseSensitive(root, "from");
        const cJSON *toItem = cJSON_GetObjectItemCaseSensitive(root, "to");
        const std::string from = cJSON_IsString(fromItem) && fromItem->valuestring ? fromItem->valuestring : "";
        const std::string to = cJSON_IsString(toItem) && toItem->valuestring ? toItem->valuestring : "";
        if (!hasOnlyFields(root, {"command", "enabled", "from", "to"}) ||
            !getRequiredBool(root, "enabled", enabled) ||
            (fromItem && !isTime(from, true)) || (toItem && !isTime(to, true)))
        {
            sendError(400, "invalid_fields", "enabled is required; from/to must use HH:MM:SS");
        }
        else if (fromItem && toItem)
            (void)invoke("/set/timeLimit", HTTP_GET,
                         {{"value", boolArgument(enabled)}, {"from", from}, {"to", to}});
        else if (fromItem)
            (void)invoke("/set/timeLimit", HTTP_GET, {{"value", boolArgument(enabled)}, {"from", from}});
        else if (toItem)
            (void)invoke("/set/timeLimit", HTTP_GET, {{"value", boolArgument(enabled)}, {"to", to}});
        else
            (void)invoke("/set/timeLimit", HTTP_GET, {{"value", boolArgument(enabled)}});
    }
    else if (command == "brightness.schedule")
    {
        std::string field;
        int64_t value = 0;
        if (!hasOnlyFields(root, {"command", "field", "value"}) ||
            !getRequiredString(root, "field", field, 1, 16) ||
            !getRequiredInteger(root, "value", 0, 100, value))
        {
            sendError(400, "invalid_fields", "field and an integer value are required");
        }
        else
        {
            const bool hourField = field == "nightStart" || field == "nightEnd" || field == "dimStart" || field == "dimEnd";
            const bool valueField = field == "nightValue" || field == "dimValue" || field == "dayValue";
            if ((!hourField && !valueField) || (hourField && value > 23))
                sendError(400, "invalid_fields", "Unknown field or value outside its valid range");
            else
                (void)invoke("/set/brightnessConfig", HTTP_GET, {{"field", field}, {"value", std::to_string(value)}});
        }
    }
    else if (command == "brightness.manual")
    {
        bool hasEnabled = false;
        bool hasBrightness = false;
        int64_t brightness = 0;
        if (!hasOnlyFields(root, {"command", "enabled", "brightness"}) ||
            !getOptionalBool(root, "enabled", hasEnabled, enabled) ||
            !getOptionalInteger(root, "brightness", 0, 100, hasBrightness, brightness) ||
            (!hasEnabled && !hasBrightness))
        {
            sendError(400, "invalid_fields", "Provide enabled and/or brightness from 0 to 100");
        }
        else if (hasEnabled && hasBrightness)
            (void)invoke("/set/manualBrightness", HTTP_GET,
                         {{"value", enabled ? "true" : "false"}, {"brightness", std::to_string(brightness)}});
        else if (hasEnabled)
            (void)invoke("/set/manualBrightness", HTTP_GET, {{"value", enabled ? "true" : "false"}});
        else
            (void)invoke("/set/manualBrightness", HTTP_GET, {{"brightness", std::to_string(brightness)}});
    }
    else if (command == "logging.set")
    {
        int64_t value = 0;
        // The historic mask range ("value", 0, 511) remains valid; the
        // extended mask accepts the additional category bits as well.
        if (!hasOnlyFields(root, {"command", "value"}) ||
            !getRequiredInteger(root, "value", 0, Globals::LOG_CONFIG_MASK, value))
            sendError(400, "invalid_fields", "value must be an integer from 0 to 65535");
        else
            (void)invoke("/set/logConfig", HTTP_GET, {{"value", std::to_string(value)}});
    }
    else if (command == "firmware.set")
    {
        std::string target;
        if (!hasOnlyFields(root, {"command", "target"}) ||
            !getRequiredString(root, "target", target, 1, 24) ||
            (target != "NixieV6_std" && target != "NixieV6_dev" && target != "NixieV6_BOS"))
            sendError(400, "invalid_fields", "Unknown firmware target");
        else
            (void)invoke("/set/firmware", HTTP_GET, {{"target", target}});
    }
    else if (command == "zip.set")
    {
        std::string zip;
        if (!hasOnlyFields(root, {"command", "zip"}) ||
            !getRequiredString(root, "zip", zip, 1, 10) || !isZip(zip))
            sendError(400, "invalid_fields", "zip must contain 1 to 10 digits");
        else
            (void)invoke("/set/zip", HTTP_GET, {{"zip", zip}});
    }
    else if (command == "timezone.set")
    {
        const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(root, "timezone");
        std::string value;
        bool valid = hasOnlyFields(root, {"command", "timezone"});
        if (valid && cJSON_IsNumber(timezone) && std::isfinite(timezone->valuedouble) &&
            std::floor(timezone->valuedouble) == timezone->valuedouble &&
            timezone->valuedouble >= 0 && timezone->valuedouble <= 12)
            value = std::to_string(static_cast<int>(timezone->valuedouble));
        else if (valid && cJSON_IsString(timezone) && timezone->valuestring && isKnownTimezone(timezone->valuestring))
            value = timezone->valuestring;
        else
            valid = false;
        if (!valid)
            sendError(400, "invalid_fields", "timezone must be an index from 0 to 12 or a known name");
        else
            (void)invoke("/set/timezone", HTTP_GET, {{"tz", value}});
    }
    else if (command == "timer.set")
    {
        bool hasSeconds = false;
        int64_t seconds = 0;
        if (!hasOnlyFields(root, {"command", "enabled", "seconds"}) ||
            !getRequiredBool(root, "enabled", enabled) ||
            !getOptionalInteger(root, "seconds", 1, 604800, hasSeconds, seconds) ||
            (enabled && !hasSeconds) || (!enabled && hasSeconds))
        {
            sendError(400, "invalid_fields", "seconds from 1 to 604800 are required only when enabled");
        }
        else if (enabled)
            (void)invoke("/set/timer", HTTP_GET,
                         {{"enabled", "1"}, {"seconds", std::to_string(seconds)}});
        else
            (void)invoke("/set/timer", HTTP_GET, {{"enabled", "0"}});
    }
    else if (command == "alarm.set")
    {
        const cJSON *timeItem = cJSON_GetObjectItemCaseSensitive(root, "time");
        const std::string time = cJSON_IsString(timeItem) && timeItem->valuestring ? timeItem->valuestring : "";
        if (!hasOnlyFields(root, {"command", "enabled", "time"}) ||
            !getRequiredBool(root, "enabled", enabled) ||
            (enabled && !isTime(time, false)) || (!enabled && timeItem))
        {
            sendError(400, "invalid_fields", "time in HH:MM is required only when enabled");
        }
        else if (enabled)
            (void)invoke("/set/alarm", HTTP_GET, {{"enabled", "1"}, {"time", time}});
        else
            (void)invoke("/set/alarm", HTTP_GET, {{"enabled", "0"}});
    }
    else
    {
        sendError(422, "unsupported_command", "The requested command is not supported");
    }

    cJSON_Delete(root);
}

void SecureApi::handleWifiUpsert()
{
    cJSON *root = parseJsonRequest();
    if (!root)
    {
        server_.discardBody();
        return;
    }

    std::string ssid;
    std::string password;
    int64_t priority = 100;
    const cJSON *priorityItem = cJSON_GetObjectItemCaseSensitive(root, "priority");
    bool valid = hasOnlyFields(root, {"ssid", "password", "priority"}) &&
                 getRequiredString(root, "ssid", ssid, 1, 32) && isSsid(ssid) &&
                 getRequiredString(root, "password", password, 0, 64) && isWifiPassword(password);
    if (valid && priorityItem)
        valid = getRequiredInteger(root, "priority", 0, 254, priority);

    if (!valid)
    {
        const cJSON *passwordItem = cJSON_GetObjectItemCaseSensitive(root, "password");
        if (cJSON_IsString(passwordItem) && passwordItem->valuestring)
            std::fill(passwordItem->valuestring,
                      passwordItem->valuestring + std::strlen(passwordItem->valuestring), '\0');
        server_.discardBody();
        cJSON_Delete(root);
        sendError(400, "invalid_credentials", "SSID, password or priority is invalid");
        return;
    }

    (void)invoke("/set/wifiAdd", HTTP_POST);
    std::fill(password.begin(), password.end(), '\0');
    cJSON *passwordItem = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(passwordItem) && passwordItem->valuestring)
        std::fill(passwordItem->valuestring,
                  passwordItem->valuestring + std::strlen(passwordItem->valuestring), '\0');
    server_.discardBody();
    cJSON_Delete(root);
}

void SecureApi::handleWifiRemove()
{
    cJSON *root = parseJsonRequest();
    if (!root)
        return;
    std::string ssid;
    if (!hasOnlyFields(root, {"ssid"}) ||
        !getRequiredString(root, "ssid", ssid, 1, 32) || !isSsid(ssid))
    {
        cJSON_Delete(root);
        sendError(400, "invalid_ssid", "ssid must contain 1 to 32 valid bytes");
        return;
    }
    (void)invoke("/set/wifiRemove", HTTP_GET, {{"ssid", ssid}});
    cJSON_Delete(root);
}

void SecureApi::handleWifiErase()
{
    cJSON *root = parseJsonRequest();
    if (!root)
        return;
    if (!hasOnlyFields(root, {}))
        sendError(400, "invalid_fields", "This request requires an empty JSON object");
    else
        (void)invoke("/set/resetWiFi", HTTP_GET);
    cJSON_Delete(root);
}

bool SecureApi::invoke(
    const char *path,
    httpd_method_t registeredMethod,
    std::initializer_list<std::pair<std::string, std::string>> arguments)
{
    server_.sendHeader("Cache-Control", "no-store");
    if (server_.invokeRoute(path, registeredMethod, arguments))
        return true;
    sendError(500, "internal_error", "The command handler is unavailable");
    return false;
}

void SecureApi::sendError(int status, const char *code, const char *message)
{
    server_.sendHeader("Cache-Control", "no-store");
    std::string body = "{\"error\":\"";
    body += code ? code : "error";
    body += "\",\"message\":\"";
    body += message ? message : "Request failed";
    body += "\"}";
    server_.send(status, "application/json", body);
}

bool SecureApi::hasOnlyFields(const cJSON *object, std::initializer_list<const char *> fields)
{
    if (!cJSON_IsObject(object))
        return false;
    for (const cJSON *item = object->child; item; item = item->next)
    {
        if (!item->string)
            return false;
        const bool allowed = std::any_of(fields.begin(), fields.end(), [item](const char *field)
                                         { return std::strcmp(item->string, field) == 0; });
        if (!allowed)
            return false;
        for (const cJSON *previous = object->child; previous != item; previous = previous->next)
        {
            if (previous->string && std::strcmp(previous->string, item->string) == 0)
                return false;
        }
    }
    return true;
}

bool SecureApi::getRequiredString(
    const cJSON *object, const char *name, std::string &value, size_t minimum, size_t maximum)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsString(item) || !item->valuestring)
        return false;
    const size_t length = std::strlen(item->valuestring);
    if (length < minimum || length > maximum)
        return false;
    value.assign(item->valuestring, length);
    return true;
}

bool SecureApi::getRequiredBool(const cJSON *object, const char *name, bool &value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsBool(item))
        return false;
    value = cJSON_IsTrue(item);
    return true;
}

bool SecureApi::getOptionalBool(const cJSON *object, const char *name, bool &present, bool &value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    present = item != nullptr;
    if (!present)
        return true;
    if (!cJSON_IsBool(item))
        return false;
    value = cJSON_IsTrue(item);
    return true;
}

bool SecureApi::getRequiredInteger(const cJSON *object, const char *name, int64_t minimum, int64_t maximum, int64_t &value)
{
    bool present = false;
    if (!getOptionalInteger(object, name, minimum, maximum, present, value))
        return false;
    return present;
}

bool SecureApi::getOptionalInteger(
    const cJSON *object, const char *name, int64_t minimum, int64_t maximum,
    bool &present, int64_t &value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    present = item != nullptr;
    if (!present)
        return true;
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble ||
        item->valuedouble < static_cast<double>(minimum) ||
        item->valuedouble > static_cast<double>(maximum))
        return false;
    value = static_cast<int64_t>(item->valuedouble);
    return true;
}

bool SecureApi::isTime(const std::string &value, bool secondsRequired) noexcept
{
    const size_t expected = secondsRequired ? 8 : 5;
    if (value.size() != expected || value[2] != ':' || (secondsRequired && value[5] != ':'))
        return false;
    for (size_t index = 0; index < value.size(); ++index)
    {
        if (index == 2 || (secondsRequired && index == 5))
            continue;
        if (value[index] < '0' || value[index] > '9')
            return false;
    }
    const int hour = (value[0] - '0') * 10 + value[1] - '0';
    const int minute = (value[3] - '0') * 10 + value[4] - '0';
    const int second = secondsRequired ? (value[6] - '0') * 10 + value[7] - '0' : 0;
    return hour <= 23 && minute <= 59 && second <= 59;
}

bool SecureApi::isZip(const std::string &value) noexcept
{
    return !value.empty() && value.size() <= 10 &&
           std::all_of(value.begin(), value.end(), [](char value)
                       { return value >= '0' && value <= '9'; });
}

bool SecureApi::isSsid(const std::string &value) noexcept
{
    if (value.empty() || value.size() > 32)
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char value)
                       { return value >= 0x20 && value != 0x7f; });
}

bool SecureApi::isWifiPassword(const std::string &value) noexcept
{
    if (value.empty())
        return true;
    if (value.size() == 64)
    {
        return std::all_of(value.begin(), value.end(), [](unsigned char character)
                           { return (character >= '0' && character <= '9') ||
                                    (character >= 'a' && character <= 'f') ||
                                    (character >= 'A' && character <= 'F'); });
    }
    if (value.size() < 8 || value.size() > 63)
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character)
                       { return character >= 0x20 && character <= 0x7e; });
}
