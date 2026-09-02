#pragma once

#include "SupplyWatch/SupplyWatch.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "esp_log.h"
#include <esp_timer.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class Brownout
{
public:
    Brownout(SupplyWatch &sw, HSS &hss, Memory::PersistentStorage &storage, uint32_t interval_us = 1000) noexcept;
    void start() noexcept;
    void stop() noexcept;

private:
    static void timerCallback(void *arg);
    static void restoreDisplayCallback(void *arg);
    static void monitorTask(void *arg);
    bool processSample() noexcept;

    SupplyWatch &sw_;
    HSS &hss_;
    Memory::PersistentStorage &storage_;
    esp_timer_handle_t timer_;
    esp_timer_handle_t restore_timer_;
    std::atomic<TaskHandle_t> monitor_task_;
    std::atomic<bool> stop_requested_;
    uint32_t interval_us_;
};
