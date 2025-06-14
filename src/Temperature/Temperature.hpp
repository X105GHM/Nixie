#pragma once

#include <Arduino.h>
#include <cmath>

class NtcThermistor {
public:
    explicit NtcThermistor() noexcept;
    float readTemperatureC() const noexcept;

private:
    // IO-Pin, entsprechend ADC1_CHANNEL_5 → IO6
    static constexpr int PIN_NTC = 6;

    // NTC-Kennwerte
    static constexpr float R_FIXED     = 10000.0f;  // Oberwiderstand im Spannungsteiler
    static constexpr float R_NOMINAL   = 10000.0f;  // Nennwert des NTC @ 25 °C
    static constexpr float BETA        = 3977.0f;   // B-Konstante
    static constexpr float T0          = 298.15f;   // 25 °C in Kelvin
    static constexpr float T_OFFSET    = -3.0f;     // Offset zur Kalibrierung

    static inline float mvToVolt(uint32_t mv) {
      return static_cast<float>(mv) / 1000.0f;
    }
};
