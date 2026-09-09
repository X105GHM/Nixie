#include "Relay.hpp"
#include <cstring>

Relay::Relay() noexcept : state_(false)  
{
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    setLevel(state_);
}

void Relay::setLevel(bool level) noexcept
{
    gpio_set_level(RELAY_PIN, level ? 1 : 0);
}

void Relay::toggle() noexcept
{
    state_ = !state_;
    setLevel(state_);
}
