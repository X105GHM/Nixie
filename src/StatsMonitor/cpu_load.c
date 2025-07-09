// src/cpu_load.c
#include "cpu_load.h"
#include "esp_timer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static volatile float last_load[2] = {0.0f, 0.0f};

void idle_task(void *pv)
{
    const int64_t slice_us = 1000000LL / configTICK_RATE_HZ;
    const uint8_t core = (uint8_t)xPortGetCoreID() & 1;
    for (;;)
    {
        int64_t t0 = esp_timer_get_time();
        vTaskDelay(0);
        int64_t t1 = esp_timer_get_time();
        int64_t work = t1 - t0;
        int64_t idle = slice_us - work;
        float load = 100.0f * (1.0f - (float)idle / (float)slice_us);
        last_load[core] = (load < 0 ? 0.0f : (load > 100 ? 100.0f : load));
    }
}

float cpu_load_get_core(uint8_t coreId)
{
    return (coreId < 2) ? last_load[coreId] : 0.0f;
}

float cpu_load_get_total(void)
{
    return 0.5f * (last_load[0] + last_load[1]);
}
