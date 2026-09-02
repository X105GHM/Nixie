#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ResetDiagnostics final
{
public:
    static constexpr size_t MAX_EVENTS = 12;

    struct Event
    {
        uint32_t bootId{0};
        int32_t resetReason{ESP_RST_UNKNOWN};
        int64_t previousLastEpochSeconds{0};
        uint64_t previousUptimeMs{0};
        uint32_t previousFreeInternalHeap{0};
        uint32_t previousFreePsram{0};
        float previousVoltage12V{0.0f};
        uint8_t previousHistoryState{0};
        uint8_t breadcrumbValid{0};
        std::array<char, 24> plannedReason{};
    };

    struct Snapshot
    {
        std::array<Event, MAX_EVENTS> events{};
        size_t count{0};
        uint32_t currentBootId{0};
        esp_reset_reason_t currentResetReason{ESP_RST_UNKNOWN};
        bool nvsHealthy{false};
    };

    static ResetDiagnostics& instance() noexcept;

    void initialize() noexcept;
    void updateBreadcrumb(
        int64_t epochSeconds,
        float voltage12V,
        uint8_t historyState) noexcept;
    void markPlannedRestart(std::string_view reason) noexcept;

    Snapshot snapshot() const noexcept;
    std::string json() const;

    static const char* resetReasonName(esp_reset_reason_t reason) noexcept;

    ResetDiagnostics(const ResetDiagnostics&) = delete;
    ResetDiagnostics& operator=(const ResetDiagnostics&) = delete;

private:
    ResetDiagnostics() noexcept;

    StaticSemaphore_t mutexStorage_{};
    SemaphoreHandle_t mutex_{nullptr};
    Snapshot snapshot_{};
};
