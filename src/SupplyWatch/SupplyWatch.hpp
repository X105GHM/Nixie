#pragma once

#include <Arduino.h>
#include "esp_adc_cal.h"
#include "Temperature/Temperature.hpp"

extern NtcThermistor temperatureSensor;

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
  
    static constexpr int PIN_12V  = 1;
    static constexpr int PIN_18V  = 2;
    static constexpr int PIN_IGES = 4;
    static constexpr int PIN_UHSS = 5;
    static constexpr int PIN_5V   = 8;
    static constexpr int PIN_3V3  = 9;

    static constexpr float R12_TOP    = 100000.0f, R12_BOT    = 10000.0f;
    static constexpr float R18_TOP    = 100000.0f, R18_BOT    = 10000.0f;
    static constexpr float R5_TOP     = 10000.0f,  R5_BOT     = 10000.0f;
    static constexpr float R3V3_TOP   = 10000.0f,  R3V3_BOT   = 10000.0f;
    static constexpr float R_UHSS_TOP = 1000000.0f, R_UHSS_BOT = 12000.0f;

    static constexpr float RSENSE       = 0.01f;
    static constexpr float AMP_GAIN     = 510.0f;
    static constexpr float CURRENT_FACT = 1.0f / (RSENSE * AMP_GAIN);

    static constexpr float T_REF_C             = 25.0f;
    static constexpr float DRIFT_5V_MV_PER_C   = -19.35f;
    static constexpr float DRIFT_12V_MV_PER_C  = -44.84f;
    static constexpr float DRIFT_3V3_MV_PER_C  = +1.43f;
    static constexpr float DRIFT_18V_MV_PER_C  = -39.91f;
    static constexpr float DRIFT_UHSS_MV_PER_C = +1.0f;

    static constexpr float C_5V    = 0.724197f;
    static constexpr float C_12V   = 5.164102f;
    static constexpr float C_3V3   = 0.68f;
    static constexpr float C_18V   = 6.234f;
    static constexpr float C_UHSS  = 0.0f;

    static constexpr uint32_t DEFAULT_VREF = 1100;
    esp_adc_cal_characteristics_t cal5V;
    esp_adc_cal_characteristics_t cal12V;
    esp_adc_cal_characteristics_t cal3V3;
    esp_adc_cal_characteristics_t cal18V;
    esp_adc_cal_characteristics_t calUHSS;
    esp_adc_cal_characteristics_t calI;

    static inline float mvToVolt(uint32_t mv) 
    {
        return static_cast<float>(mv) / 1000.0f;
    }

    float readCompensated(int pin,
                          const esp_adc_cal_characteristics_t &chars,
                          float rTop,
                          float rBot,
                          float driftMvPerC,
                          float intercept) const noexcept;
};