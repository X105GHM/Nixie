#include "ACP.hpp"

bool runningACP1 = false;
bool runningACP2 = false;
bool ACP_enabled = false;

void ACP() noexcept
{
    ACP_enabled = true;
    uint8_t lastBrightness = brightness;
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

    for (uint8_t number = 9; number >= 0; number--)
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

        for (uint8_t number = 8; number >= 0; number--)
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
}
