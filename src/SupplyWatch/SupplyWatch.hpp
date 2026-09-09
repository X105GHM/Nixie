#pragma once

#include "ADC/AdcService.hpp"
#include "Temperature/Temperature.hpp"

extern NtcThermistor temperatureSensor;

class SupplyWatch
{
public:
    struct Telemetry
    {
        float temperatureC{0.0f};
        float voltage5V{0.0f};
        float voltage12V{0.0f};
        float voltage3V3{0.0f};
        float voltage18V{0.0f};
        float voltageUhss{0.0f};
        float currentMa{0.0f};
    };

    explicit SupplyWatch() noexcept;

    float read5V() const noexcept;
    float read12V() const noexcept;
    float read3V3() const noexcept;
    float read18V() const noexcept;
    float readUHSS() const noexcept;
    float readCurrent() const noexcept;
    Telemetry readTelemetry() const noexcept;

private:
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

    static inline float mvToVolt(uint32_t mv) 
    {
        return static_cast<float>(mv) / 1000.0f;
    }

    float readCompensated(AdcService::Input input, float temperatureC, float rTop, float rBot, float driftMvPerC, float intercept) const noexcept;
    float readCurrent(float temperatureC) const noexcept;
};
