#include "Temperature.hpp"

NtcThermistor::NtcThermistor() noexcept
{
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_NTC, ADC_11db);
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        DEFAULT_VREF,
        &adcChars
    );
}

float NtcThermistor::readTemperatureC() const noexcept
{
    int raw = analogRead(PIN_NTC);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adcChars);

    float v = mvToVolt(mv);
    float r = R_FIXED * (v / (3.3f - v));
    float lnR = log(r / R_NOMINAL);
    float invT = (1.0f / T0) + (lnR / BETA);
    float TK = 1.0f / invT;

    return (TK - 273.15f) + T_OFFSET;
}
