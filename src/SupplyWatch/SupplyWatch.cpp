#include "SupplyWatch.hpp"

SupplyWatch::SupplyWatch() noexcept
{
    analogReadResolution(12);
    for (int pin : {PIN_12V, PIN_18V, PIN_IGES, PIN_UHSS, PIN_5V, PIN_3V3})
    {
        analogSetPinAttenuation(pin, ADC_11db);
    }
}

static inline float mvToVolt(uint32_t mv)
{
    return static_cast<float>(mv) / 1000.0f;
}

float SupplyWatch::readDivider(int pin, float rTop, float rBot, float offset) const noexcept
{
    uint32_t mv = analogReadMilliVolts(pin);
    float v = mvToVolt(mv);
    return v * ((rTop + rBot) / rBot) + offset;
}

float SupplyWatch::read5V() const noexcept
{
    return readDivider(PIN_5V, R5_TOP, R5_BOT, OFF_5V);
}

float SupplyWatch::read12V() const noexcept
{
    return readDivider(PIN_12V, R12_TOP, R12_BOT, OFF_12V);
}

float SupplyWatch::read3V3() const noexcept
{
    return readDivider(PIN_3V3, R3V3_TOP, R3V3_BOT, OFF_3V3);
}

float SupplyWatch::read18V() const noexcept
{
    return readDivider(PIN_18V, R18_TOP, R18_BOT, OFF_18V);
}

float SupplyWatch::readUHSS() const noexcept
{
    return readDivider(PIN_UHSS, R_UHSS_TOP, R_UHSS_BOT, OFF_UHSS);
}

// Mit Hardware Version < V.6.1.0 nicht möglich, da Hardware Bullshit 

float SupplyWatch::readCurrent() const noexcept
{
    uint32_t mv = analogReadMilliVolts(PIN_IGES);
    return mvToVolt(mv) * CURRENT_FACT;
}