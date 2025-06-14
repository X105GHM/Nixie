#include "Relay.hpp"
#include <cstring>

Relay::Relay() noexcept
    : state_(false)
{
    pinMode((int)RELAY_PIN, OUTPUT);
    digitalWrite((int)RELAY_PIN, LOW);
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
