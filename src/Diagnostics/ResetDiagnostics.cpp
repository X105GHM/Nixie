#include "ResetDiagnostics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "History/ChartHistory.hpp"
#include "Logger/Logger.hpp"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"

namespace
{
    constexpr const char* TAG = "ResetDiagnostics";
    constexpr const char* NVS_NAMESPACE = "resetdiag";
    constexpr const char* NVS_KEY = "boot_log";
    constexpr uint32_t LOG_MAGIC = 0x524C4F47U;        // RLOG
    constexpr uint32_t BREADCRUMB_MAGIC = 0x52425244U; // RBRD
    constexpr uint16_t LOG_VERSION = 1;
    constexpr uint16_t BREADCRUMB_VERSION = 1;

    struct RtcBreadcrumb
    {
        uint32_t magic;
        uint16_t version;
        uint16_t size;
        int64_t lastEpochSeconds;
        uint64_t uptimeMs;
        uint32_t freeInternalHeap;
        uint32_t freePsram;
        float voltage12V;
        uint8_t historyState;
        uint8_t telemetryValid;
        uint8_t reserved[2];
        std::array<char, 24> plannedReason;
        uint32_t checksum;
    };

    struct PersistentLog
    {
        uint32_t magic{0};
        uint16_t version{0};
        uint16_t size{0};
        uint32_t nextBootId{1};
        uint16_t writeIndex{0};
        uint16_t count{0};
        std::array<ResetDiagnostics::Event, ResetDiagnostics::MAX_EVENTS> events{};
        uint32_t checksum{0};
    };

    static_assert(std::is_trivially_copyable_v<RtcBreadcrumb>);
    static_assert(std::is_trivial_v<RtcBreadcrumb>);
    static_assert(std::is_trivially_copyable_v<PersistentLog>);

    RTC_NOINIT_ATTR RtcBreadcrumb rtcBreadcrumb;
    portMUX_TYPE rtcMux = portMUX_INITIALIZER_UNLOCKED;

    uint32_t checksum(const void* data, size_t size) noexcept
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        uint32_t hash = 2166136261U;
        for (size_t index = 0; index < size; ++index)
        {
            hash ^= bytes[index];
            hash *= 16777619U;
        }
        return hash;
    }

    template <typename T>
    uint32_t structureChecksum(const T& value) noexcept
    {
        static_assert(std::is_standard_layout_v<T>);
        static_assert(offsetof(T, checksum) + sizeof(value.checksum) <= sizeof(T));
        return checksum(&value, offsetof(T, checksum));
    }

    bool validBreadcrumb(const RtcBreadcrumb& value) noexcept
    {
        return value.magic == BREADCRUMB_MAGIC &&
               value.version == BREADCRUMB_VERSION &&
               value.size == sizeof(RtcBreadcrumb) &&
               value.checksum == structureChecksum(value);
    }

    void finalizeBreadcrumb(RtcBreadcrumb& value) noexcept
    {
        value.magic = BREADCRUMB_MAGIC;
        value.version = BREADCRUMB_VERSION;
        value.size = sizeof(RtcBreadcrumb);
        value.checksum = structureChecksum(value);
    }

    void initializeLog(PersistentLog& log) noexcept
    {
        log = {};
        log.magic = LOG_MAGIC;
        log.version = LOG_VERSION;
        log.size = sizeof(PersistentLog);
        log.nextBootId = 1;
        log.checksum = structureChecksum(log);
    }

    bool validLog(const PersistentLog& log) noexcept
    {
        return log.magic == LOG_MAGIC &&
               log.version == LOG_VERSION &&
               log.size == sizeof(PersistentLog) &&
               log.writeIndex < ResetDiagnostics::MAX_EVENTS &&
               log.count <= ResetDiagnostics::MAX_EVENTS &&
               log.checksum == structureChecksum(log);
    }

    esp_err_t loadLog(PersistentLog& log) noexcept
    {
        nvs_handle_t handle = 0;
        esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (result != ESP_OK) return result;

        size_t size = sizeof(log);
        result = nvs_get_blob(handle, NVS_KEY, &log, &size);
        nvs_close(handle);
        if (result == ESP_OK && size != sizeof(log)) return ESP_ERR_INVALID_SIZE;
        return result;
    }

    esp_err_t saveLog(PersistentLog& log) noexcept
    {
        log.checksum = structureChecksum(log);
        nvs_handle_t handle = 0;
        esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (result != ESP_OK) return result;
        result = nvs_set_blob(handle, NVS_KEY, &log, sizeof(log));
        if (result == ESP_OK) result = nvs_commit(handle);
        nvs_close(handle);
        return result;
    }

    void copyReason(std::array<char, 24>& destination, std::string_view source) noexcept
    {
        destination.fill('\0');
        const size_t count = std::min(source.size(), destination.size() - 1U);
        if (count != 0) std::memcpy(destination.data(), source.data(), count);
    }

    const char* historyStateName(uint8_t state) noexcept
    {
        return ChartHistory::stateName(static_cast<ChartHistory::State>(state));
    }

    void appendJsonString(std::string& json, const char* value)
    {
        json.push_back('"');
        if (value)
        {
            for (const unsigned char character : std::string_view(value))
            {
                switch (character)
                {
                    case '"': json += "\\\""; break;
                    case '\\': json += "\\\\"; break;
                    case '\b': json += "\\b"; break;
                    case '\f': json += "\\f"; break;
                    case '\n': json += "\\n"; break;
                    case '\r': json += "\\r"; break;
                    case '\t': json += "\\t"; break;
                    default:
                        if (character < 0x20U)
                        {
                            char escaped[7]{};
                            std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                            json += escaped;
                        }
                        else
                        {
                            json.push_back(static_cast<char>(character));
                        }
                        break;
                }
            }
        }
        json.push_back('"');
    }
}

ResetDiagnostics& ResetDiagnostics::instance() noexcept
{
    static ResetDiagnostics diagnostics;
    return diagnostics;
}

ResetDiagnostics::ResetDiagnostics() noexcept
    : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_))
{
}

void ResetDiagnostics::initialize() noexcept
{
    const esp_reset_reason_t currentReason = esp_reset_reason();

    RtcBreadcrumb previous{};
    portENTER_CRITICAL(&rtcMux);
    previous = rtcBreadcrumb;
    const bool breadcrumbWasValid = validBreadcrumb(previous);
    rtcBreadcrumb = {};
    finalizeBreadcrumb(rtcBreadcrumb);
    portEXIT_CRITICAL(&rtcMux);

    PersistentLog log{};
    esp_err_t loadResult = loadLog(log);
    if (loadResult == ESP_ERR_NVS_NOT_FOUND || loadResult == ESP_ERR_NVS_NOT_INITIALIZED || !validLog(log))
    {
        if (loadResult != ESP_ERR_NVS_NOT_FOUND && loadResult != ESP_ERR_NVS_NOT_INITIALIZED && loadResult != ESP_OK)
        {
            Logger::log(LoggerType::SYSTEM,
                        "%s: reset log unavailable or invalid (%s); starting a new log",
                        TAG, esp_err_to_name(loadResult));
        }
        initializeLog(log);
    }

    Event event{};
    event.bootId = log.nextBootId == 0 ? 1 : log.nextBootId;
    event.resetReason = static_cast<int32_t>(currentReason);
    event.breadcrumbValid = breadcrumbWasValid && previous.telemetryValid ? 1U : 0U;
    if (breadcrumbWasValid)
    {
        event.plannedReason = previous.plannedReason;
    }
    if (event.breadcrumbValid)
    {
        event.previousLastEpochSeconds = previous.lastEpochSeconds;
        event.previousUptimeMs = previous.uptimeMs;
        event.previousFreeInternalHeap = previous.freeInternalHeap;
        event.previousFreePsram = previous.freePsram;
        event.previousVoltage12V = previous.voltage12V;
        event.previousHistoryState = previous.historyState;
    }

    log.events[log.writeIndex] = event;
    log.writeIndex = static_cast<uint16_t>((log.writeIndex + 1U) % MAX_EVENTS);
    log.count = static_cast<uint16_t>(std::min<size_t>(log.count + 1U, MAX_EVENTS));
    log.nextBootId = event.bootId == std::numeric_limits<uint32_t>::max() ? 1U : event.bootId + 1U;

    const esp_err_t saveResult = saveLog(log);
    const bool nvsHealthy = saveResult == ESP_OK;
    if (!nvsHealthy)
    {
        Logger::log(LoggerType::SYSTEM, "%s: could not persist reset event: %s", TAG, esp_err_to_name(saveResult));
    }

    Snapshot next{};
    next.currentBootId = event.bootId;
    next.currentResetReason = currentReason;
    next.nvsHealthy = nvsHealthy;
    next.count = log.count;
    for (size_t index = 0; index < next.count; ++index)
    {
        const size_t physical = (log.writeIndex + MAX_EVENTS - 1U - index) % MAX_EVENTS;
        next.events[index] = log.events[physical];
    }

    if (mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE)
    {
        snapshot_ = next;
        xSemaphoreGive(mutex_);
    }
    else
    {
        snapshot_ = next;
    }

    Logger::log(LoggerType::SYSTEM,
                "%s: boot=%u reset=%s (%d), planned=%s, previous uptime=%llu ms",
                TAG, static_cast<unsigned>(event.bootId), resetReasonName(currentReason),
                static_cast<int>(currentReason),
                event.plannedReason[0] ? event.plannedReason.data() : "none",
                static_cast<unsigned long long>(event.previousUptimeMs));
}

void ResetDiagnostics::updateBreadcrumb(
    int64_t epochSeconds,
    float voltage12V,
    uint8_t historyState) noexcept
{
    portENTER_CRITICAL(&rtcMux);
    RtcBreadcrumb next = validBreadcrumb(rtcBreadcrumb) ? rtcBreadcrumb : RtcBreadcrumb{};
    if (epochSeconds > 0) next.lastEpochSeconds = epochSeconds;
    next.uptimeMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    next.freeInternalHeap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    next.freePsram = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    next.voltage12V = std::isfinite(voltage12V) ? voltage12V : 0.0f;
    next.historyState = historyState;
    next.telemetryValid = 1U;
    finalizeBreadcrumb(next);
    rtcBreadcrumb = next;
    portEXIT_CRITICAL(&rtcMux);
}

void ResetDiagnostics::markPlannedRestart(std::string_view reason) noexcept
{
    portENTER_CRITICAL(&rtcMux);
    RtcBreadcrumb next = validBreadcrumb(rtcBreadcrumb) ? rtcBreadcrumb : RtcBreadcrumb{};
    copyReason(next.plannedReason, reason);
    finalizeBreadcrumb(next);
    rtcBreadcrumb = next;
    portEXIT_CRITICAL(&rtcMux);
}

ResetDiagnostics::Snapshot ResetDiagnostics::snapshot() const noexcept
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return snapshot_;
    const Snapshot copy = snapshot_;
    xSemaphoreGive(mutex_);
    return copy;
}

std::string ResetDiagnostics::json() const
{
    const Snapshot resetSnapshot = snapshot();
    const auto& history = ChartHistory::instance();
    const uint64_t uptimeMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    const uint32_t freeInternalHeap = static_cast<uint32_t>(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    const uint32_t freePsram = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    std::string json;
    json.reserve(5200);
    json += "{\"capacity\":" + std::to_string(MAX_EVENTS);
    json += ",\"count\":" + std::to_string(resetSnapshot.count);
    json += ",\"nvsHealthy\":";
    json += resetSnapshot.nvsHealthy ? "true" : "false";
    json += ",\"current\":{\"bootId\":" + std::to_string(resetSnapshot.currentBootId);
    json += ",\"resetReason\":";
    appendJsonString(json, resetReasonName(resetSnapshot.currentResetReason));
    json += ",\"resetReasonCode\":" + std::to_string(static_cast<int>(resetSnapshot.currentResetReason));
    json += ",\"uptimeMs\":" + std::to_string(uptimeMs);
    json += ",\"freeInternalHeap\":" + std::to_string(freeInternalHeap);
    json += ",\"freePsram\":" + std::to_string(freePsram);
    json += ",\"history\":{\"state\":";
    appendJsonString(json, ChartHistory::stateName(history.state()));
    json += ",\"available\":";
    json += history.available() ? "true" : "false";
    json += ",\"allocatedBytes\":" + std::to_string(history.allocatedBytes());
    json += ",\"freePsramAfterAllocation\":" + std::to_string(history.freePsramAfterAllocation());
    json += "}},\"events\":[";

    for (size_t index = 0; index < resetSnapshot.count; ++index)
    {
        const Event& event = resetSnapshot.events[index];
        if (index != 0) json.push_back(',');
        json += "{\"bootId\":" + std::to_string(event.bootId);
        json += ",\"resetReason\":";
        appendJsonString(json, resetReasonName(static_cast<esp_reset_reason_t>(event.resetReason)));
        json += ",\"resetReasonCode\":" + std::to_string(event.resetReason);
        json += ",\"plannedReason\":";
        if (event.plannedReason[0]) appendJsonString(json, event.plannedReason.data());
        else json += "null";
        json += ",\"previous\":";
        if (!event.breadcrumbValid)
        {
            json += "null}";
            continue;
        }

        json += "{\"lastSeen\":";
        if (event.previousLastEpochSeconds > 0)
            json += std::to_string(event.previousLastEpochSeconds * 1000);
        else
            json += "null";
        json += ",\"uptimeMs\":" + std::to_string(event.previousUptimeMs);
        json += ",\"freeInternalHeap\":" + std::to_string(event.previousFreeInternalHeap);
        json += ",\"freePsram\":" + std::to_string(event.previousFreePsram);
        json += ",\"voltage12V\":";
        if (std::isfinite(event.previousVoltage12V))
        {
            char voltage[24]{};
            std::snprintf(voltage, sizeof(voltage), "%.3f", static_cast<double>(event.previousVoltage12V));
            json += voltage;
        }
        else
        {
            json += "null";
        }
        json += ",\"historyState\":";
        appendJsonString(json, historyStateName(event.previousHistoryState));
        json += "}}";
    }
    json += "]}";
    return json;
}

const char* ResetDiagnostics::resetReasonName(esp_reset_reason_t reason) noexcept
{
    switch (reason)
    {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_USB: return "usb";
        case ESP_RST_JTAG: return "jtag";
        case ESP_RST_EFUSE: return "efuse";
        case ESP_RST_PWR_GLITCH: return "power_glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu_lockup";
        case ESP_RST_UNKNOWN:
        default: return "unknown";
    }
}
