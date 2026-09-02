#include "AppState.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

#include "AlarmClock/AlarmClock.hpp"
#include "Digits/Digits.hpp"
#include "EnergyMonitor/EnergyMonitor.hpp"
#include "Globals/Globals.hpp"
#include "HSS/HSS.hpp"
#include "History/ChartHistory.hpp"
#include "Diagnostics/ResetDiagnostics.hpp"
#include "StatsMonitor/StatsMonitor.hpp"
#include "SupplyWatch/SupplyWatch.hpp"
#include "Timer/Timer.hpp"
#include "WiFiConnector/WiFiConnector.hpp"
#include "esp_app_format.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

extern SupplyWatch supplyWatch;
extern HSS hssController;
extern EnergyMonitor energyMonitor;
extern WiFiConnector wifiConnector;
extern Timer timer;
extern AlarmClock alarmClock;

namespace
{
    constexpr const char *TAG = "AppState";

    const char *chipModelName(const esp_chip_info_t &chipInfo) noexcept
    {
        switch (chipInfo.model)
        {
        case CHIP_ESP32:
            return "ESP32";
        case CHIP_ESP32S2:
            return "ESP32-S2";
        case CHIP_ESP32S3:
            return "ESP32-S3";
        case CHIP_ESP32C3:
            return "ESP32-C3";
        case CHIP_ESP32H2:
            return "ESP32-H2";
        default:
            return "UNKNOWN";
        }
    }

    uint32_t configuredFlashSpeedHz() noexcept
    {
        #if CONFIG_ESPTOOLPY_FLASHFREQ_120M
        return 120000000U;
        #elif CONFIG_ESPTOOLPY_FLASHFREQ_80M
        return 80000000U;
        #elif CONFIG_ESPTOOLPY_FLASHFREQ_40M
        return 40000000U;
        #elif CONFIG_ESPTOOLPY_FLASHFREQ_20M
        return 20000000U;
        #else
        return 0U;
        #endif
    }

    uint32_t runningImageSize() noexcept
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (!running)
            return 0;
        const esp_partition_pos_t position{running->address, running->size};
        esp_image_metadata_t metadata{};
        return esp_image_get_metadata(&position, &metadata) == ESP_OK ? metadata.image_len : 0;
    }

    const char *firmwareTargetName(Globals::FirmwareTarget target) noexcept
    {
        switch (target)
        {
        case Globals::FirmwareTarget::NixieV6_std:
            return "NixieV6_std";
        case Globals::FirmwareTarget::NixieV6_dev:
            return "NixieV6_dev";
        case Globals::FirmwareTarget::NixieV6_BOS:
            return "NixieV6_BOS";
        default:
            return "Unknown";
        }
    }

    std::string formatFixed(double value, unsigned decimals = 2)
    {
        if (!std::isfinite(value))
            return "null";
        char buffer[48]{};
        std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(decimals), value);
        return buffer;
    }

    std::string formatHex(uint64_t value)
    {
        char buffer[17]{};
        std::snprintf(buffer, sizeof(buffer), "%llx", static_cast<unsigned long long>(value));
        return buffer;
    }

    std::string jsonString(const std::string &value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 2);
        escaped.push_back('"');
        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (character < 0x20)
                {
                    char unicode[7]{};
                    std::snprintf(unicode, sizeof(unicode), "\\u%04x", character);
                    escaped += unicode;
                }
                else
                {
                    escaped.push_back(static_cast<char>(character));
                }
            }
        }
        escaped.push_back('"');
        return escaped;
    }
}

AppState &AppState::instance() noexcept
{
    static AppState state;
    return state;
}

AppState::AppState() noexcept
    : snapshotMutex_(xSemaphoreCreateMutexStatic(&snapshotMutexStorage_)),
      sourceMutex_(xSemaphoreCreateMutexStatic(&sourceMutexStorage_)),
      clientQueue_(xQueueCreateStatic(
          MAX_SSE_CLIENTS,
          sizeof(httpd_req_t *),
          clientQueueBuffer_.data(),
          &clientQueueStorage_))
{
}

bool AppState::start() noexcept
{
    if (task_)
        return true;
    if (!snapshotMutex_ || !sourceMutex_ || !clientQueue_)
        return false;

    initializeSystemInfo();
    (void)ChartHistory::instance().initialize();
    refresh();

    if (xTaskCreatePinnedToCore(
            taskEntry, "AppTelemetry", TASK_STACK_BYTES, this, 1, &task_, 0) != pdPASS)
    {
        task_ = nullptr;
        return false;
    }
    return true;
}

void AppState::initializeSystemInfo() noexcept
{
    esp_chip_info_t chipInfo{};
    esp_chip_info(&chipInfo);
    systemInfo_.chipModel = chipModelName(chipInfo);

    uint64_t chipId = 0;
    const esp_err_t macResult = esp_efuse_mac_get_default(reinterpret_cast<uint8_t *>(&chipId));
    if (macResult == ESP_OK)
    {
        systemInfo_.chipId = formatHex(chipId);
    }
    else
    {
        ESP_LOGE(TAG, "Could not read base MAC: %s", esp_err_to_name(macResult));
    }
    systemInfo_.sdkVersion = esp_get_idf_version();

    const esp_err_t flashResult = esp_flash_get_physical_size(
        esp_flash_default_chip, &systemInfo_.flashSize);
    if (flashResult != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read flash size: %s", esp_err_to_name(flashResult));
    }
    systemInfo_.flashSpeed = configuredFlashSpeedHz();
    systemInfo_.sketchSize = runningImageSize();
    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(nullptr);
    systemInfo_.sketchFreeSpace = updatePartition ? updatePartition->size : 0;
#if defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
    systemInfo_.cpuFrequencyMhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
#else
    systemInfo_.cpuFrequencyMhz = CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ;
#endif
}

AppState::StatusSnapshot AppState::snapshot() const
{
    if (!snapshotMutex_ || xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE) return {};
    StatusSnapshot copy = snapshot_;
    xSemaphoreGive(snapshotMutex_);
    return copy;
}

bool AppState::executeMutation(const std::function<void()> &action) noexcept
{
    if (!action || !sourceMutex_) return false;
    if (xSemaphoreTake(sourceMutex_, portMAX_DELAY) != pdTRUE) return false;
    action();
    xSemaphoreGive(sourceMutex_);
    return true;
}

void AppState::refresh() noexcept
{
    StatusSnapshot next = captureSourceState();
    if (!snapshotMutex_ || xSemaphoreTake(snapshotMutex_, portMAX_DELAY) != pdTRUE) return;
    next.revision = snapshot_.revision + 1;
    snapshot_ = std::move(next);
    xSemaphoreGive(snapshotMutex_);
}

AppState::StatusSnapshot AppState::captureSourceState() noexcept
{
    StatusSnapshot result{};
    result.capturedAtUs = esp_timer_get_time();
    const int64_t capturedEpochSeconds = static_cast<int64_t>(std::time(nullptr));
    result.capturedAtEpochSeconds = chart_history::validEpoch(capturedEpochSeconds)
        ? capturedEpochSeconds
        : 0;
    if (!sourceMutex_ || xSemaphoreTake(sourceMutex_, portMAX_DELAY) != pdTRUE) return result;

    const auto load = StatsMonitor::instance().getLoadSnapshot();
    const auto telemetry = supplyWatch.readTelemetry();
    const std::string ssid = wifiConnector.getWiFiSSID();
    const std::string deviceIp = wifiConnector.getIpAddress();
    const uint32_t freeHeap = esp_get_free_heap_size();
    const auto energy = energyMonitor.getSnapshot();
    const auto firmwareTarget = Globals::currentFirmwareTarget.load(std::memory_order_relaxed);
    const auto timeZone = Globals::currentTimeZone.load(std::memory_order_relaxed);
    const auto textConfig = Globals::getTextConfig();
    const uint32_t logConfig = Globals::logConfig.load(std::memory_order_relaxed);
    const uint32_t pwmPeriodUs = PWM_PERIOD_US.load(std::memory_order_relaxed);
    const double pwmFrequency = pwmPeriodUs == 0 ? 0.0 : 1e6 / pwmPeriodUs;

    result.displayEnabled = ::displayEnabled.load(std::memory_order_relaxed);
    result.loadDetected = Globals::loadDetected.load(std::memory_order_relaxed);
    result.singleDigitMode = singleDigitACP.load(std::memory_order_relaxed);
    result.timeLimitFrom = textConfig.timeLimitFrom;
    result.timeLimitTo = textConfig.timeLimitTo;

    const bool hss160 = hssController.enable160V.load(std::memory_order_relaxed);
    const bool hss190 = hssController.enable190V.load(std::memory_order_relaxed);
    const bool hssResistor = hssController.enableResistor.load(std::memory_order_relaxed);
    const uint32_t brightnessValue = brightness.load(std::memory_order_relaxed);
    const uint8_t singleDigitValue = singleDigit.load(std::memory_order_relaxed);
    const auto timerState = timer.getSnapshot();
    const auto alarmState = alarmClock.getSnapshot();
    const AlarmTime alarmTime = alarmState.time;

    char alarmTimeBuffer[8]{};
    std::snprintf(alarmTimeBuffer, sizeof(alarmTimeBuffer), "%02u:%02u", alarmTime.hour, alarmTime.minute);

    std::string logBits;
    logBits.reserve(9);
    for (int bit = 8; bit >= 0; --bit)

        logBits += ((logConfig >> bit) & 1U) ? '1' : '0';

    std::string json;
    json.reserve(2200);
    json += "{\n";
    json += "  \"Chip\": " + jsonString(systemInfo_.chipModel) + ",\n";
    json += "  \"SSID\": " + jsonString(ssid) + ",\n";
    json += "  \"IP_Address\": " + jsonString(deviceIp) + ",\n";
    json += "  \"Timestamp\": " + (result.capturedAtEpochSeconds != 0
        ? std::to_string(result.capturedAtEpochSeconds * 1000)
        : std::string("null")) + ",\n";
    json += std::string("  \"DisplayEnabled\": ") + (result.displayEnabled ? "1" : "0") + ",\n";
    json += "  \"Melody\": \"Nokia\",\n";
    json += "  \"FreeHeap\": " + std::to_string(freeHeap) + ",\n";
    json += "  \"CoreLoad0\": " + formatFixed(load.core0) + ",\n";
    json += "  \"CoreLoad1\": " + formatFixed(load.core1) + ",\n";
    json += "  \"TotalLoad\": " + formatFixed(load.total) + ",\n";
    json += "  \"ChipId\": " + jsonString(systemInfo_.chipId) + ",\n";
    json += "  \"FlashSize\": " + std::to_string(systemInfo_.flashSize) + ",\n";
    json += "  \"FlashSpeed\": " + std::to_string(systemInfo_.flashSpeed) + ",\n";
    json += "  \"SketchSize\": " + std::to_string(systemInfo_.sketchSize) + ",\n";
    json += "  \"SketchFreeSpace\": " + std::to_string(systemInfo_.sketchFreeSpace) + ",\n";
    json += "  \"CpuFrequencyMHz\": " + std::to_string(systemInfo_.cpuFrequencyMhz) + ",\n";
    json += "  \"SdkVersion\": " + jsonString(systemInfo_.sdkVersion) + ",\n";
    json += "  \"Autor\": \"X105GHM\",\n";
    json += "  \"Voltage_12V\": " + formatFixed(telemetry.voltage12V) + ",\n";
    json += "  \"Voltage_5V\": " + formatFixed(telemetry.voltage5V) + ",\n";
    json += "  \"Voltage_3V3\": " + formatFixed(telemetry.voltage3V3) + ",\n";
    json += "  \"Voltage_18V\": " + formatFixed(telemetry.voltage18V) + ",\n";
    json += "  \"Voltage_UHSS\": " + formatFixed(telemetry.voltageUhss) + ",\n";
    json += "  \"Current_mA\": " + formatFixed(telemetry.currentMa) + ",\n";
    json += "  \"Power_W\": " + formatFixed(energy.instantPowerW) + ",\n";
    json += "  \"Energy_Wh\": " + jsonString(formatFixed(energy.totalEnergyWh)) + ",\n";
    json += std::string("  \"HSS_Enabled\": ") + (hss160 ? "1" : "0") + ",\n";
    json += std::string("  \"HSS_190V\": ") + (hss190 ? "1" : "0") + ",\n";
    json += std::string("  \"HSS_Resistor\": ") + (hssResistor ? "1" : "0") + ",\n";
    json += "  \"CaseTemperature_C\": " + formatFixed(telemetry.temperatureC) + ",\n";
    json += "  \"Firmware_Target\": " + jsonString(firmwareTargetName(firmwareTarget)) + ",\n";
    json += "  \"Hardware_Version\": " + jsonString(textConfig.hardwareVersion) + ",\n";
    json += "  \"Software_Version\": " + jsonString(textConfig.softwareVersion) + ",\n";
    json += std::string("  \"UpdateAvailable\": ") + (Globals::updateAvailable ? "1" : "0") + ",\n";
    json += "  \"zipCode\": " + jsonString(textConfig.zipCode) + ",\n";
    json += std::string("  \"tickerEnabled\": ") + (Globals::tickerEnabled ? "1" : "0") + ",\n";
    json += std::string("  \"timeLimitEnabled\": ") + (Globals::timeLimitEnabled ? "1" : "0") + ",\n";
    json += std::string("  \"silentModeEnabled\": ") + (Globals::SilentModeEnabled ? "1" : "0") + ",\n";
    json += std::string("  \"noACPatNight\": ") + (Globals::noACPatNight ? "1" : "0") + ",\n";
    json += std::string("  \"manualBrightnessEnabled\": ") + (Globals::manualBrightnessEnabled ? "1" : "0") + ",\n";
    json += std::string("  \"WeatherUpdateEnabled\": ") + (Globals::WeatherUpdateEnabled ? "1" : "0") + ",\n";
    json += std::string("  \"cricketSoundEnabled\": ") + (Globals::cricketSoundEnabled ? "1" : "0") + ",\n";
    json += "  \"logConfigBinary\": " + jsonString(logBits) + ",\n";
    json += std::string("  \"loadDetected\": ") + (result.loadDetected ? "1" : "0") + ",\n";
    json += "  \"Brightness\": " + std::to_string(brightnessValue) + ",\n";
    json += "  \"brightnessNightStartHour\": " + std::to_string(Globals::brightnessNightStartHour) + ",\n";
    json += "  \"brightnessNightEndHour\": " + std::to_string(Globals::brightnessNightEndHour) + ",\n";
    json += "  \"brightnessDimStartHour\": " + std::to_string(Globals::brightnessDimStartHour) + ",\n";
    json += "  \"brightnessDimEndHour\": " + std::to_string(Globals::brightnessDimEndHour) + ",\n";
    json += "  \"brightnessNightValue\": " + std::to_string(Globals::brightnessNightValue) + ",\n";
    json += "  \"brightnessDimValue\": " + std::to_string(Globals::brightnessDimValue) + ",\n";
    json += "  \"brightnessDayValue\": " + std::to_string(Globals::brightnessDayValue) + ",\n";
    json += "  \"timeLimitFrom\": " + jsonString(result.timeLimitFrom) + ",\n";
    json += "  \"timeLimitTo\": " + jsonString(result.timeLimitTo) + ",\n";
    json += std::string("  \"SingleACP\": ") + (result.singleDigitMode ? "1" : "0") + ",\n";
    json += std::string("  \"NixiePWM\": ") + (Globals::PWM_disabled ? "1" : "0") + ",\n";
    json += "  \"PWM_Frequenzy\": " + formatFixed(pwmFrequency) + ",\n";
    json += "  \"SingleDigits\": " + std::to_string(singleDigitValue) + ",\n";
    json += "  \"CurrentTimeZone\": " + std::to_string(static_cast<uint8_t>(timeZone)) + ",\n";
    json += "  \"CurrentTimeZoneIndex\": " + std::to_string(static_cast<uint8_t>(timeZone)) + ",\n";
    json += std::string("  \"TimerActive\": ") + (timerState.running ? "true" : "false") + ",\n";
    json += "  \"TimerConfiguredSeconds\": " + std::to_string(timerState.configuredSeconds) + ",\n";
    json += std::string("  \"AlarmActive\": ") + (alarmState.configured ? "true" : "false") + ",\n";
    json += "  \"AlarmTime\": " + jsonString(alarmTimeBuffer) + "\n";
    json += "}";

    const ChartHistory::SampleValues historyValues = {{
        telemetry.voltage12V,
        telemetry.voltage5V,
        telemetry.voltage3V3,
        telemetry.voltage18V,
        telemetry.voltageUhss,
        telemetry.currentMa,
        static_cast<float>(energy.instantPowerW),
        static_cast<float>(energy.totalEnergyWh),
        telemetry.temperatureC,
        static_cast<float>(freeHeap),
        static_cast<float>(load.total),
        static_cast<float>(load.core0),
        static_cast<float>(load.core1),
    }};
    if (result.capturedAtEpochSeconds != 0)
    {
        (void)ChartHistory::instance().record(result.capturedAtEpochSeconds, historyValues);
    }
    ResetDiagnostics::instance().updateBreadcrumb(
        result.capturedAtEpochSeconds,
        telemetry.voltage12V,
        static_cast<uint8_t>(ChartHistory::instance().state()));

    result.infoJson = std::move(json);
    xSemaphoreGive(sourceMutex_);
    return result;
}

void AppState::taskEntry(void *context)
{
    static_cast<AppState *>(context)->taskLoop();
}

void AppState::taskLoop()
{
    TickType_t nextRefresh = xTaskGetTickCount() + TELEMETRY_PERIOD;
    uint32_t stackCheckCycles = 0;
    UBaseType_t lowestReportedStack = TASK_STACK_BYTES;
    bool stackBaselineLogged = false;
    for (;;)
    {
        drainNewClients();

        const TickType_t now = xTaskGetTickCount();
        if (static_cast<int32_t>(now - nextRefresh) >= 0)
        {
            refresh();
            broadcastSnapshot();

            ++stackCheckCycles;
            if (!stackBaselineLogged || stackCheckCycles >= STACK_CHECK_INTERVAL)
            {
                const UBaseType_t minimumFreeStack = uxTaskGetStackHighWaterMark(nullptr);
                if (!stackBaselineLogged)
                {
                    ESP_LOGI(
                        TAG,
                        "AppTelemetry running; minimum free stack=%u bytes",
                        static_cast<unsigned>(minimumFreeStack));
                }
                else if (
                    minimumFreeStack < STACK_WARNING_BYTES &&
                    minimumFreeStack < lowestReportedStack)
                {
                    ESP_LOGW(
                        TAG,
                        "AppTelemetry stack reserve low: %u bytes",
                        static_cast<unsigned>(minimumFreeStack));
                }
                lowestReportedStack = std::min(lowestReportedStack, minimumFreeStack);
                stackBaselineLogged = true;
                stackCheckCycles = 0;
            }

            nextRefresh += TELEMETRY_PERIOD;
            if (static_cast<int32_t>(xTaskGetTickCount() - nextRefresh) >= 0)
            {
                nextRefresh = xTaskGetTickCount() + TELEMETRY_PERIOD;
            }
        }

        const TickType_t current = xTaskGetTickCount();
        const TickType_t wait = static_cast<int32_t>(nextRefresh - current) > 0 ? nextRefresh - current : 0;
        (void)ulTaskNotifyTake(pdTRUE, wait);
    }
}

AppState::SseAttachResult AppState::attachSseClient(httpd_req_t *request) noexcept
{
    if (!request || !task_ || !clientQueue_) return SseAttachResult::NotStarted;

    size_t reserved = reservedClients_.load(std::memory_order_acquire);
    do
    {
        if (reserved >= MAX_SSE_CLIENTS) return SseAttachResult::AtCapacity;
    } while (!reservedClients_.compare_exchange_weak(reserved, reserved + 1, std::memory_order_acq_rel, std::memory_order_acquire));

    httpd_req_t *asynchronousRequest = nullptr;
    if (httpd_req_async_handler_begin(request, &asynchronousRequest) != ESP_OK)
    {
        reservedClients_.fetch_sub(1, std::memory_order_acq_rel);
        return SseAttachResult::BeginFailed;
    }

    if (xQueueSend(clientQueue_, &asynchronousRequest, 0) != pdTRUE)
    {
        httpd_resp_send_err(asynchronousRequest, HTTPD_500_INTERNAL_SERVER_ERROR, "SSE queue unavailable");
        (void)httpd_req_async_handler_complete(asynchronousRequest);
        reservedClients_.fetch_sub(1, std::memory_order_acq_rel);
        return SseAttachResult::ErrorResponseSent;
    }

    xTaskNotifyGive(task_);
    return SseAttachResult::Attached;
}

void AppState::drainNewClients()
{
    httpd_req_t *request = nullptr;
    while (xQueueReceive(clientQueue_, &request, 0) == pdTRUE)
    {
        const auto slot = std::find_if(clients_.begin(), clients_.end(), [](const SseClient &client)
        { return client.request == nullptr; });

        if (slot == clients_.end())
        {
            (void)httpd_req_async_handler_complete(request);
            reservedClients_.fetch_sub(1, std::memory_order_acq_rel);
            continue;
        }

        slot->request = request;
        slot->headersSent = false;
        const StatusSnapshot current = snapshot();
        if (!sendSnapshot(*slot, current))
        {
            removeClient(static_cast<size_t>(slot - clients_.begin()));
        }
    }
}

void AppState::broadcastSnapshot()
{
    const StatusSnapshot current = snapshot();
    for (size_t index = 0; index < clients_.size(); ++index)
    {
        if (clients_[index].request && !sendSnapshot(clients_[index], current)) removeClient(index);
    }
}

bool AppState::sendSnapshot(SseClient &client, const StatusSnapshot &current)
{
    if (!client.request || current.infoJson.empty())
        return false;

    if (!client.headersSent)
    {
        httpd_resp_set_status(client.request, "200 OK");
        httpd_resp_set_type(client.request, "text/event-stream");
        httpd_resp_set_hdr(client.request, "Cache-Control", "no-cache");
        httpd_resp_set_hdr(client.request, "Connection", "keep-alive");
        httpd_resp_set_hdr(client.request, "X-Accel-Buffering", "no");
        static constexpr char CONNECTED[] = ": connected\n\n";
        if (httpd_resp_send_chunk(client.request, CONNECTED, sizeof(CONNECTED) - 1) != ESP_OK)
            return false;
        client.headersSent = true;
    }

    const std::string frame = makeSseFrame(current);
    return httpd_resp_send_chunk(client.request, frame.data(), frame.size()) == ESP_OK;
}

void AppState::removeClient(size_t index) noexcept
{
    if (index >= clients_.size() || !clients_[index].request) return;
    (void)httpd_req_async_handler_complete(clients_[index].request);
    clients_[index] = {};
    reservedClients_.fetch_sub(1, std::memory_order_acq_rel);
}

std::string AppState::makeSseFrame(const StatusSnapshot &current)
{
    std::string frame;
    frame.reserve(current.infoJson.size() + 256);
    frame += "id: ";
    frame += std::to_string(current.revision);
    frame += "\nevent: info\n";

    size_t begin = 0;
    while (begin <= current.infoJson.size())
    {
        const size_t end = current.infoJson.find('\n', begin);
        frame += "data: ";
        if (end == std::string::npos)
        {
            frame.append(current.infoJson, begin, std::string::npos);
            frame += "\n\n";
            break;
        }
        frame.append(current.infoJson, begin, end - begin);
        frame.push_back('\n');
        begin = end + 1;
    }
    return frame;
}
