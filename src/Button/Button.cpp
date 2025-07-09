#include "Button.hpp"

static constexpr LoggerType logType = LoggerType::BUTTON;

ButtonPoll::ButtonPoll(gpio_num_t pin) noexcept
    : pin_(pin), lastState_(true)
{}

esp_err_t ButtonPoll::init() noexcept
{
    gpio_config_t io_conf{};
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin_);
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        Logger::log(logType, "gpio_config for GPIO%d failed (%d)", static_cast<int>(pin_), err);
        return err;
    }

    lastState_ = (gpio_get_level(pin_) != 0);
    Logger::log(logType, "ButtonPoll on GPIO%d initialized, startState=%s",static_cast<int>(pin_),lastState_ ? "HIGH" : "LOW");
    return ESP_OK;
}

void ButtonPoll::poll() noexcept
{
    bool curr = (gpio_get_level(pin_) != 0);
    if (lastState_ && !curr) {
        buttonCallback();
    }
    lastState_ = curr;
}

void ButtonPoll::buttonCallback() noexcept
{
    // TODO: implement callback logic here
}
