#include "Temperature.hpp"

NtcThermistor::NtcThermistor() noexcept
{
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_NTC, ADC_11db);
}

float NtcThermistor::readTemperatureC() const noexcept
{
    uint32_t mv = analogReadMilliVolts(PIN_NTC);
    float v = mvToVolt(mv);
    float r = R_FIXED * (v / (3.3f - v));
    float lnR = log(r / R_NOMINAL);
    float invT = (1.0f / T0) + (lnR / BETA);
    float TK = 1.0f / invT;

    return (TK - 273.15f) + T_OFFSET;
}
