#pragma once

#include "SupplyWatch/SupplyWatch.hpp"
#include "HSS/HSS.hpp"
#include "Memory/Memory.hpp"
#include "esp_log.h"
#include <esp_timer.h>

class Brownout
{
public:
    Brownout(SupplyWatch &sw, HSS &hss, Memory::PersistentStorage &storage, uint32_t interval_us = 1000) noexcept;
    void start() noexcept;
    void stop() noexcept;

private:
    static void timerCallback(void *arg);

    SupplyWatch &sw_;
    HSS &hss_;
    Memory::PersistentStorage &storage_;
    esp_timer_handle_t timer_;
    uint32_t interval_us_;
};
