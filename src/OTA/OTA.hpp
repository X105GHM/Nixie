#pragma once

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_crt_bundle.h"

#include <optional>
#include <atomic>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "Logger/Logger.hpp"
#include "Digits/Digits.hpp"

class OTAManager
{
public:
    enum class Stage : uint8_t
    {
        Idle,
        Manifest,
        Firmware,
        SPIFFS,
        Done,
        Error
    };

    struct Status
    {
        bool running = false;
        bool rebootRequired = false;
        bool firmwareUpdated = false;
        bool spiffsUpdated = false;

        Stage stage = Stage::Idle;
        esp_err_t lastResult = ESP_OK;

        int appProgressPct = 0;
        int spiffsProgressPct = 0;

        size_t appBytesDone = 0;
        size_t appBytesTotal = 0;

        size_t spiffsBytesDone = 0;
        size_t spiffsBytesTotal = 0;

        std::string message = "Idle";
        std::string currentVersion;
        std::string targetVersion;
    };

public:
    static OTAManager& instance() noexcept;

    OTAManager() noexcept;

    bool checkForUpdateAvailable(const std::string& baseUrl) noexcept;
    bool startAsync(const std::string& baseUrl) noexcept;
    bool isRunning() const noexcept;

    std::string getStatusJson() const noexcept;
    void resetStatus() noexcept;

    esp_err_t checkAndUpdate(const std::string &baseUrl) noexcept;

private:
    static void otaTaskEntry_(void* arg) noexcept;
    void otaTask_() noexcept;

    std::optional<std::string> fetchManifestVersion(const std::string &manifestUrl) noexcept;
    esp_err_t performFirmwareUpdate(const std::string &firmwareUrl) noexcept;
    esp_err_t performSPIFFSUpdate(const std::string &spiffsUrl) noexcept;

    void setStageMessage_(Stage stage, const char* message) noexcept;
    void setError_(esp_err_t err, const char* message) noexcept;
    void setDone_(const char* message) noexcept;

    void setAppProgress_(size_t done, size_t total, const char* message) noexcept;
    void setSpiffsProgress_(size_t done, size_t total, const char* message) noexcept;

    void setVersions_(const char* currentVersion, const char* targetVersion) noexcept;
    void markFirmwareUpdated_() noexcept;
    void markSpiffsUpdated_() noexcept;

    Status getStatusCopy_() const noexcept;

    static const char* stageToString_(Stage s) noexcept;
    static std::string escapeJson_(const std::string& s) noexcept;
    static int calcPercent_(size_t done, size_t total) noexcept;

private:
    static constexpr uint32_t OTA_TASK_STACK_BYTES = 18432;
    // The singleton lives in internal BSS. Reserve the flash-writing task's
    // stack before runtime heap fragmentation; PSRAM is not suitable here.
    alignas(16) StackType_t otaTaskStack_[OTA_TASK_STACK_BYTES / sizeof(StackType_t)]{};
    StaticTask_t otaTaskStorage_{};
    StaticSemaphore_t statusMutexStorage_{};
    mutable SemaphoreHandle_t statusMutex_ = nullptr;
    TaskHandle_t otaTaskHandle_ = nullptr;
    std::atomic_bool workerBusy_{false};
    std::atomic_uint32_t stackMinimumFreeBytes_{0};

    Status status_;
    std::string pendingBaseUrl_;
};
