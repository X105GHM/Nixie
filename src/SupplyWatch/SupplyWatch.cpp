#include "SupplyWatch.hpp"

#include <limits>

#include "esp_log.h"

SupplyWatch::SupplyWatch() noexcept = default;

float SupplyWatch::readCompensated(AdcService::Input input,
                                   float temperatureC,
                                   float rTop,
                                   float rBot,
                                   float driftMvPerC,
                                   float intercept) const noexcept
{
    AdcService::Measurement measurement{};
    const esp_err_t result = AdcService::instance().read(input, measurement);
    if (result != ESP_OK)
    {
        ESP_LOGE("SupplyWatch", "ADC read failed: %s", esp_err_to_name(result));
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float v_adc = mvToVolt(static_cast<uint32_t>(measurement.millivolts));
    const float deltaT = temperatureC - T_REF_C;

    float v_corr = v_adc - (driftMvPerC / 1000.0f) * deltaT;

    return v_corr * ((rTop + rBot) / rBot) + intercept;
}

float SupplyWatch::read5V() const noexcept
{
    return readCompensated(AdcService::Input::Supply5V, temperatureSensor.readTemperatureC(),
                           R5_TOP, R5_BOT, DRIFT_5V_MV_PER_C, C_5V);
}

float SupplyWatch::read12V() const noexcept
{
    return readCompensated(AdcService::Input::Supply12V, temperatureSensor.readTemperatureC(),
                           R12_TOP, R12_BOT, DRIFT_12V_MV_PER_C, C_12V);
}

float SupplyWatch::read3V3() const noexcept
{
    return readCompensated(AdcService::Input::Supply3V3, temperatureSensor.readTemperatureC(),
                           R3V3_TOP, R3V3_BOT, DRIFT_3V3_MV_PER_C, C_3V3);
}

float SupplyWatch::read18V() const noexcept
{
    return readCompensated(AdcService::Input::Supply18V, temperatureSensor.readTemperatureC(),
                           R18_TOP, R18_BOT, DRIFT_18V_MV_PER_C, C_18V);
}

float SupplyWatch::readUHSS() const noexcept
{
    return readCompensated(AdcService::Input::Uhss, temperatureSensor.readTemperatureC(),
                           R_UHSS_TOP, R_UHSS_BOT, DRIFT_UHSS_MV_PER_C, C_UHSS);
}

float SupplyWatch::readCurrent() const noexcept
{
    return readCurrent(temperatureSensor.readTemperatureC());
}

float SupplyWatch::readCurrent(float temperatureC) const noexcept
{
    AdcService::Measurement measurement{};
    const esp_err_t result = AdcService::instance().read(AdcService::Input::Current, measurement);
    if (result != ESP_OK)
    {
        ESP_LOGE("SupplyWatch", "Current ADC read failed: %s", esp_err_to_name(result));
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float v_adc = mvToVolt(static_cast<uint32_t>(measurement.millivolts));
    const float deltaT = temperatureC - T_REF_C;

    float v_corr = v_adc - (DRIFT_5V_MV_PER_C / 1000.0f) * deltaT;
    return v_corr * CURRENT_FACT;
}

SupplyWatch::Telemetry SupplyWatch::readTelemetry() const noexcept
{
    Telemetry telemetry{};
    telemetry.temperatureC = temperatureSensor.readTemperatureC();
    telemetry.voltage5V = readCompensated(AdcService::Input::Supply5V, telemetry.temperatureC,
                                          R5_TOP, R5_BOT, DRIFT_5V_MV_PER_C, C_5V);
    telemetry.voltage12V = readCompensated(AdcService::Input::Supply12V, telemetry.temperatureC,
                                           R12_TOP, R12_BOT, DRIFT_12V_MV_PER_C, C_12V);
    telemetry.voltage3V3 = readCompensated(AdcService::Input::Supply3V3, telemetry.temperatureC,
                                           R3V3_TOP, R3V3_BOT, DRIFT_3V3_MV_PER_C, C_3V3);
    telemetry.voltage18V = readCompensated(AdcService::Input::Supply18V, telemetry.temperatureC,
                                           R18_TOP, R18_BOT, DRIFT_18V_MV_PER_C, C_18V);
    telemetry.voltageUhss = readCompensated(AdcService::Input::Uhss, telemetry.temperatureC,
                                            R_UHSS_TOP, R_UHSS_BOT, DRIFT_UHSS_MV_PER_C, C_UHSS);
    telemetry.currentMa = readCurrent(telemetry.temperatureC);
    return telemetry;
}
