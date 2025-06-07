#pragma once

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <initializer_list>

class SupplyWatch {
public:
    explicit SupplyWatch() noexcept;
    float read5V()    const noexcept;
    float read12V()   const noexcept;
    float read3V3()   const noexcept;
    float read18V()   const noexcept;
    float readUHSS()  const noexcept;
    float readCurrent() const noexcept;

private:
    static constexpr adc1_channel_t CH_VREF    = ADC1_CHANNEL_9; // Internal Vref channel
    static constexpr adc1_channel_t CH_12V     = ADC1_CHANNEL_0; // IO1
    static constexpr adc1_channel_t CH_18V     = ADC1_CHANNEL_1; // IO2
    static constexpr adc1_channel_t CH_IGES    = ADC1_CHANNEL_3; // IO4
    static constexpr adc1_channel_t CH_UHSS    = ADC1_CHANNEL_4; // IO5
    static constexpr adc1_channel_t CH_5V      = ADC1_CHANNEL_7; // IO8
    static constexpr adc1_channel_t CH_3V3     = ADC1_CHANNEL_8; // IO9

    static constexpr adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;
    static constexpr adc_atten_t      ADC_ATTEN = ADC_ATTEN_DB_12;
    static constexpr uint32_t         V_REF_MV  = 0;            // Use eFuse and dynamic update

    esp_adc_cal_characteristics_t     adc_chars_;

    // Voltage divider resistances (Ohm)
    static constexpr float R12_TOP  = 100000.0f, R12_BOT  = 10000.0f;
    static constexpr float R18_TOP  = 100000.0f, R18_BOT  = 10000.0f;
    static constexpr float R5_TOP   = 10000.0f,  R5_BOT   = 10000.0f;
    static constexpr float R3V3_TOP = 10000.0f,  R3V3_BOT = 10000.0f;
    static constexpr float R_UHSS_TOP =1000000.0f,R_UHSS_BOT=12000.0f;

    // Current sensing
    static constexpr float RSENSE      = 0.01f;
    static constexpr float AMP_GAIN    = 510.0f;
    static constexpr float CURRENT_FACTOR = 1.0f/(RSENSE*AMP_GAIN);

    // Calibration offsets
    static constexpr float OFF_12V  = +5.62f;
    static constexpr float OFF_18V  = +6.49f;
    static constexpr float OFF_5V   = +0.83f;
    static constexpr float OFF_3V3  = +0.74f;
    static constexpr float OFF_UHSS = +11.37f;

    uint32_t measureVrefMv() const noexcept;
    uint32_t readChannelMv(adc1_channel_t ch) const noexcept;
    float    readDivider(adc1_channel_t ch, float rTop, float rBot, float offset) const noexcept;
};