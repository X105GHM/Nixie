#pragma once

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include <cmath>


class NtcThermistor {
public:
    explicit NtcThermistor() noexcept;
    float readTemperatureC() const noexcept;

private:
    static constexpr adc1_channel_t ADC_CH   = ADC1_CHANNEL_5; // IO6
    static constexpr adc1_channel_t ADC_VREF = ADC1_CHANNEL_9; // internal Vref
    static constexpr adc_bits_width_t ADC_W  = ADC_WIDTH_BIT_12;
    static constexpr adc_atten_t      ADC_A  = ADC_ATTEN_DB_12;
    static constexpr uint32_t         VREF   = 0;

    esp_adc_cal_characteristics_t     adc_chars_;

    static constexpr float R_FIXED   = 10000.0f;
    static constexpr float R_NOMINAL = 10000.0f;
    static constexpr float BETA      = 3977.0f;
    static constexpr float T0        = 298.15f;
    static constexpr float T_OFFSET  = -3.0f;

    uint32_t measureVrefMv() const noexcept;
};