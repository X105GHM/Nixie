#include "Temperature.hpp"

#include <limits>

#include "ADC/AdcService.hpp"
#include "esp_log.h"

NtcThermistor::NtcThermistor() noexcept = default;

float NtcThermistor::readTemperatureC() const noexcept
{
    AdcService::Measurement measurement{};
    const esp_err_t result = AdcService::instance().read(AdcService::Input::Temperature, measurement);
    if (result != ESP_OK)
    {
        ESP_LOGE("Temperature", "ADC read failed: %s", esp_err_to_name(result));
        return std::numeric_limits<float>::quiet_NaN();
    }

    const float v = mvToVolt(static_cast<uint32_t>(measurement.millivolts));
    if (v <= 0.0f || v >= 3.3f)
    {
        ESP_LOGW("Temperature", "ADC voltage out of NTC range: %d mV", measurement.millivolts);
        return std::numeric_limits<float>::quiet_NaN();
    }
    float r = R_FIXED * (v / (3.3f - v));
    float lnR = log(r / R_NOMINAL);
    float invT = (1.0f / T0) + (lnR / BETA);
    float TK = 1.0f / invT;

    return (TK - 273.15f) + T_OFFSET;
}
