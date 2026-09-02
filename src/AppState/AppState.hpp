#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class AppState final
{
public:
    struct StatusSnapshot
    {
        uint64_t revision{0};
        int64_t capturedAtUs{0};
        int64_t capturedAtEpochSeconds{0};
        bool displayEnabled{false};
        bool loadDetected{false};
        bool singleDigitMode{false};
        std::string timeLimitFrom;
        std::string timeLimitTo;
        std::string infoJson;
    };

    enum class SseAttachResult : uint8_t
    {
        Attached,
        NotStarted,
        AtCapacity,
        BeginFailed,
        ErrorResponseSent
    };

    static AppState& instance() noexcept;

    bool start() noexcept;
    StatusSnapshot snapshot() const;
    bool executeMutation(const std::function<void()>& action) noexcept;
    SseAttachResult attachSseClient(httpd_req_t* request) noexcept;

    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;

private:
    struct SystemInfo
    {
        std::string chipModel;
        std::string chipId;
        std::string sdkVersion;
        uint32_t flashSize{0};
        uint32_t flashSpeed{0};
        uint32_t sketchSize{0};
        uint32_t sketchFreeSpace{0};
        uint32_t cpuFrequencyMhz{0};
    };

    struct SseClient
    {
        httpd_req_t* request{nullptr};
        bool headersSent{false};
    };

    static constexpr size_t MAX_SSE_CLIENTS = 3;
    static constexpr TickType_t TELEMETRY_PERIOD = pdMS_TO_TICKS(1000);
    static constexpr uint32_t TASK_STACK_BYTES = 16384;
    static constexpr UBaseType_t STACK_WARNING_BYTES = 4096;
    static constexpr uint32_t STACK_CHECK_INTERVAL = 60;

    AppState() noexcept;

    void initializeSystemInfo() noexcept;
    void refresh() noexcept;
    StatusSnapshot captureSourceState() noexcept;

    static void taskEntry(void* context);
    void taskLoop();
    void drainNewClients();
    void broadcastSnapshot();
    bool sendSnapshot(SseClient& client, const StatusSnapshot& snapshot);
    void removeClient(size_t index) noexcept;
    static std::string makeSseFrame(const StatusSnapshot& snapshot);

    mutable StaticSemaphore_t snapshotMutexStorage_{};
    mutable SemaphoreHandle_t snapshotMutex_{nullptr};
    StaticSemaphore_t sourceMutexStorage_{};
    SemaphoreHandle_t sourceMutex_{nullptr};

    StatusSnapshot snapshot_{};
    SystemInfo systemInfo_{};

    alignas(portBYTE_ALIGNMENT) std::array<uint8_t, MAX_SSE_CLIENTS * sizeof(httpd_req_t*)> clientQueueBuffer_{};
    StaticQueue_t clientQueueStorage_{};
    QueueHandle_t clientQueue_{nullptr};
    std::array<SseClient, MAX_SSE_CLIENTS> clients_{};
    std::atomic<size_t> reservedClients_{0};
    TaskHandle_t task_{nullptr};
};
