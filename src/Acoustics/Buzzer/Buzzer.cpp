#include "Buzzer.hpp"

Buzzer::Buzzer() noexcept
{
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    setLevel(false);
}

void Buzzer::setLevel(bool level) noexcept
{
    state_.store(level, std::memory_order_relaxed);
    gpio_set_level(BUZZER_PIN, level ? 1 : 0);
}

void Buzzer::startAlarm(uint8_t repeatCount) noexcept
{
    remainingToggles_ = repeatCount * 2;
    toneState_ = false;
    alarmRunning_ = true;
    lastToggleTime_ = esp_timer_get_time() / 1000;
    setLevel(false);
}

void Buzzer::stopAlarm() noexcept
{
    alarmRunning_ = false;
    remainingToggles_ = 0;
    setLevel(false);
}

void Buzzer::update() noexcept
{
    if (!alarmRunning_ || remainingToggles_ == 0)
        return;

    uint32_t now = esp_timer_get_time() / 1000;

    if (now - lastToggleTime_ >= 200)
    {
        toneState_ = !toneState_;
        setLevel(toneState_);
        lastToggleTime_ = now;
        --remainingToggles_;

        if (remainingToggles_ == 0)
        {
            stopAlarm();
        }
    }
}

void Buzzer::startCricketInTask() noexcept
{
    bool expected = false;
    if (!cricketRunning_.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_relaxed)) return;

    Buzzer* self = this;

    auto cricketWrapper = [](void* param)
    {
        Buzzer* buzzer = static_cast<Buzzer*>(param);
        buzzer->playCricketSound();

        const UBaseType_t minimumFreeStack = uxTaskGetStackHighWaterMark(nullptr);
        Logger::log(LoggerType::GENERAL,
                    "Cricket task finished, minimum free stack=%u bytes",
                    static_cast<unsigned>(minimumFreeStack));

        buzzer->cricketRunning_.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
    };

    const BaseType_t result = xTaskCreatePinnedToCore(
        cricketWrapper,
        "CricketTask",
        CRICKET_TASK_STACK_BYTES,
        self,
        3,
        nullptr,
        0);
    if (result != pdPASS)
    {
        cricketRunning_.store(false, std::memory_order_release);
        Logger::log(LoggerType::GENERAL, "Cricket task creation failed");
        return;
    }

    Logger::log(LoggerType::GENERAL, "Cricket sound started in task");
}

void Buzzer::playCricketSound() noexcept
{
    constexpr int baseFreq = 4500;
    constexpr int maxFreq = 5000;
    constexpr int step = 100;
    constexpr int pulseDurationMs = 4;

    for (int i = 0; i < 3; i++)
    {
        for (int chirp = 0; chirp < 4; ++chirp)
        {
            for (int freq = baseFreq; freq <= maxFreq; freq += step)
            {
                int period_us = 1000000 / freq;
                int halfPeriod = period_us / 2;

                const int64_t start = esp_timer_get_time();
                while ((esp_timer_get_time() - start) < (pulseDurationMs * 1000))
                {
                    gpio_set_level(BUZZER_PIN, 1);
                    delayMicrosYield(halfPeriod * 0.2);
                    gpio_set_level(BUZZER_PIN, 0);
                    delayMicrosYield(halfPeriod * 0.8);
                }
            }

            int pauseMs = 30 + chirp * 10;
            vTaskDelay(pdMS_TO_TICKS(pauseMs));
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    gpio_set_level(BUZZER_PIN, 0);
    Logger::log(LoggerType::GENERAL, "Cricket sound chirp finished");
}

void Buzzer::Silence() noexcept
{
    gpio_config_t io_conf = 
    {
        .pin_bit_mask = (1ULL << 40),
        .mode = Globals::SilentModeEnabled ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = Globals::SilentModeEnabled ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    if (!Globals::SilentModeEnabled) 
    {
        gpio_set_level(GPIO_NUM_40, 0);
        Globals::tickerEnabled = true;
    }
    else 
    {
        Globals::tickerEnabled = false;
    }
}
