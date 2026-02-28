#include "cpu_load.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_timer.h"
#include "esp_freertos_hooks.h"

static volatile uint32_t idle_count[2] = {0, 0};
static volatile float last_load[2] = {0.0f, 0.0f};

static uint32_t idle_max_per_sec[2] = {0, 0};
static int64_t last_us = 0;
static int calibrated = 0;
static int hooks_registered = 0;

static bool idle_hook_cb(void)
{
    uint32_t core = xPortGetCoreID();
    if (core < 2) 
    {
        idle_count[core]++;
    }
    return true;
}

static void ensure_hooks_registered(void)
{
    if (hooks_registered) return;

    esp_register_freertos_idle_hook_for_cpu(idle_hook_cb, 0);
    esp_register_freertos_idle_hook_for_cpu(idle_hook_cb, 1);

    hooks_registered = 1;
}

static void cpu_load_update_internal(void)
{
    ensure_hooks_registered();

    int64_t now = esp_timer_get_time();
    if (last_us == 0) { last_us = now; return; }

    int64_t dt_us = now - last_us;
    if (dt_us < 250000) return;

    uint32_t c0 = idle_count[0]; idle_count[0] = 0;
    uint32_t c1 = idle_count[1]; idle_count[1] = 0;

    float scale = 1000000.0f / (float)dt_us;
    uint32_t per_sec0 = (uint32_t)((float)c0 * scale);
    uint32_t per_sec1 = (uint32_t)((float)c1 * scale);

    if (!calibrated)
    {
        if (per_sec0 > idle_max_per_sec[0]) idle_max_per_sec[0] = per_sec0;
        if (per_sec1 > idle_max_per_sec[1]) idle_max_per_sec[1] = per_sec1;

        static int64_t start_us = 0;
        if (start_us == 0) start_us = now;

        if ((now - start_us) > 3000000)
        {
            if (idle_max_per_sec[0] == 0) idle_max_per_sec[0] = 1;
            if (idle_max_per_sec[1] == 0) idle_max_per_sec[1] = 1;
            calibrated = 1;
        }

        last_load[0] = 0.0f;
        last_load[1] = 0.0f;
        last_us = now;
        return;
    }

    float idle_frac0 = (float)per_sec0 / (float)idle_max_per_sec[0];
    float idle_frac1 = (float)per_sec1 / (float)idle_max_per_sec[1];

    float load0 = 100.0f * (1.0f - idle_frac0);
    float load1 = 100.0f * (1.0f - idle_frac1);

    if (load0 < 0) {load0 = 0;} if (load0 > 100) {load0 = 100;} 
    if (load1 < 0) {load1 = 0;} if (load1 > 100) {load1 = 100;} 

    last_load[0] = load0;
    last_load[1] = load1;

    last_us = now;
}

float cpu_load_get_core(uint8_t coreId)
{
    cpu_load_update_internal();
    return (coreId < 2) ? last_load[coreId] : 0.0f;
}

float cpu_load_get_total(void)
{
    cpu_load_update_internal();
    return 0.5f * (last_load[0] + last_load[1]);
}