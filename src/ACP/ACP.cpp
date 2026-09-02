#include "ACP.hpp"

std::atomic_bool runningACP1{false};
std::atomic_bool runningACP2{false};
std::atomic_bool ACP_enabled{false};

void ACP() noexcept
{
    mode_running.store(true, std::memory_order_relaxed);
    ACP_enabled = true;
    const uint8_t lastBrightness = static_cast<uint8_t>(brightness.load(std::memory_order_relaxed));
    brightness = 100;

    for (uint8_t number = 0; number <= 9; number++)
    {
        digits = 111111 * number;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    runningACP1 = true;

    for (uint8_t number = 0; number <= 9; number++)
    {
        digits = 101010 * number;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    for (uint8_t i = 0; i <= 8; i++)
    {
        for (uint8_t number = 0; number <= 9; number++)
        {
            digits = 101010 * number;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    runningACP1 = false;
    runningACP2 = true;

    for (uint8_t i = 0; i <= 4; i++)
    {
        for (uint8_t number = 0; number <= 9; number++)
        {
            digits = 10101 * number;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    for (uint8_t number = 0; number <= 9; number++)
    {
        digits = (90909 - (10101 * number));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    runningACP2 = false;

    for (int8_t number = 9; number >= 0; number--)
    {
        digits = 111111 * number;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    runningACP1 = true;

    int32_t displaySequence[] = {306060, 407070, 508080, 609090, 706060, 807070, 908080, 309090};
    for (int i = 0; i < 8; i++)
    {
        for (uint8_t number = 0; number <= 9; number++)
        {
            digits = 111111 * number;
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        for (int8_t number = 8; number >= 0; number--)
        {
            digits = 111111 * number;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ACP_enabled = false;
        digits = displaySequence[i];
        vTaskDelay(pdMS_TO_TICKS(4488));
        ACP_enabled = true;
    }
    runningACP1 = false;
    brightness = lastBrightness;
    ACP_enabled = false;
    mode_running.store(false, std::memory_order_relaxed);
}
