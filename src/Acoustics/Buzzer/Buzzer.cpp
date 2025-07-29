#include "Buzzer.hpp"

Buzzer::Buzzer() noexcept
    : state_(false)
{
    gpio_pad_select_gpio(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    setLevel(state_);
}

void Buzzer::setLevel(bool level) noexcept
{
    state_ = level;
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
    if (cricketRunning_) return;
    cricketRunning_ = true;

    Buzzer* self = this;

    auto cricketWrapper = [](void* param) {
        Buzzer* buzzer = static_cast<Buzzer*>(param);
        buzzer->playCricketSound();
        buzzer->cricketRunning_ = false;
        vTaskDelete(nullptr);
    };

    xTaskCreatePinnedToCore(cricketWrapper, "CricketTask", 2048, self, 3, nullptr, 0);

    Logger::log(LoggerType::GENERAL, F("Cricket sound started in task"));
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

                uint32_t start = esp_timer_get_time();
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
    Logger::log(LoggerType::GENERAL, F("Cricket sound chirp finished"));
}