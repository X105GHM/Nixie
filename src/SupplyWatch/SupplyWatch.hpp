#pragma once

#include <Arduino.h>

class SupplyWatch
{
public:
    explicit SupplyWatch() noexcept;

    float read5V() const noexcept;
    float read12V() const noexcept;
    float read3V3() const noexcept;
    float read18V() const noexcept;
    float readUHSS() const noexcept;

    float readCurrent() const noexcept;

private:
    static constexpr int PIN_12V = 1;  // IO1 -> ADC1_CHANNEL_0
    static constexpr int PIN_18V = 2;  // IO2 -> ADC1_CHANNEL_1
    static constexpr int PIN_IGES = 4; // IO4 -> ADC1_CHANNEL_3
    static constexpr int PIN_UHSS = 5; // IO5 -> ADC1_CHANNEL_4
    static constexpr int PIN_5V = 8;   // IO8 -> ADC1_CHANNEL_7
    static constexpr int PIN_3V3 = 9;  // IO9 -> ADC1_CHANNEL_8

    static constexpr float R12_TOP = 100000.0f, R12_BOT = 10000.0f;
    static constexpr float R18_TOP = 100000.0f, R18_BOT = 10000.0f;
    static constexpr float R5_TOP = 10000.0f, R5_BOT = 10000.0f;
    static constexpr float R3V3_TOP = 10000.0f, R3V3_BOT = 10000.0f;
    static constexpr float R_UHSS_TOP = 1000000.0f, R_UHSS_BOT = 12000.0f;

    static constexpr float RSENSE = 0.01f;
    static constexpr float AMP_GAIN = 510.0f;
    static constexpr float CURRENT_FACT = 1.0f / (RSENSE * AMP_GAIN);

    static constexpr float OFF_12V = +7.62f;
    static constexpr float OFF_18V = +2.49f;
    static constexpr float OFF_5V = +1.1f;
    static constexpr float OFF_3V3 = +1.0f;
    static constexpr float OFF_UHSS = +0.0f;

    float readDivider(int pin, float rTop, float rBot, float offset) const noexcept;
};