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
