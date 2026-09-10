#include "AdcService.hpp"

#include "esp_adc/adc_cali_scheme.h"
#include "Logger/Logger.hpp"

namespace
{
    constexpr const char* TAG = "AdcService";
}

AdcService& AdcService::instance() noexcept
{
    static AdcService service;
    return service;
}

AdcService::AdcService() noexcept
    : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_))
{
}

esp_err_t AdcService::initializeLocked() noexcept
{
    if (initializationAttempted_) return initializationResult_;
    initializationAttempted_ = true;

    adc_oneshot_unit_init_cfg_t unitConfig{};
    unitConfig.unit_id = ADC_UNIT_1;
    unitConfig.ulp_mode = ADC_ULP_MODE_DISABLE;
    initializationResult_ = adc_oneshot_new_unit(&unitConfig, &unit_);
    if (initializationResult_ != ESP_OK)
    {
        Logger::log(LoggerType::SENSOR, "adc_oneshot_new_unit failed: %s", esp_err_to_name(initializationResult_));
        return initializationResult_;
    }

    for (auto& channel : channels_)
    {
        adc_unit_t mappedUnit = ADC_UNIT_1;
        adc_channel_t mappedChannel = ADC_CHANNEL_0;
        esp_err_t result = adc_oneshot_io_to_channel(channel.gpio, &mappedUnit, &mappedChannel);
        if (result != ESP_OK || mappedUnit != ADC_UNIT_1 || mappedChannel != channel.channel)
        {
            initializationResult_ = result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
            Logger::log(LoggerType::SENSOR, "GPIO%d ADC mapping mismatch", channel.gpio);
            return initializationResult_;
        }

        adc_oneshot_chan_cfg_t channelConfig{};
        channelConfig.bitwidth = ADC_BITWIDTH_12;
        channelConfig.atten = channel.attenuation;
        result = adc_oneshot_config_channel(unit_, channel.channel, &channelConfig);
        if (result != ESP_OK)
        {
            initializationResult_ = result;
            Logger::log(LoggerType::SENSOR, "GPIO%d channel config failed: %s", channel.gpio, esp_err_to_name(result));
            return initializationResult_;
        }

        adc_cali_curve_fitting_config_t calibrationConfig{};
        calibrationConfig.unit_id = ADC_UNIT_1;
        calibrationConfig.chan = channel.channel;
        calibrationConfig.atten = channel.attenuation;
        calibrationConfig.bitwidth = ADC_BITWIDTH_12;
        result = adc_cali_create_scheme_curve_fitting(&calibrationConfig, &channel.calibration);
        if (result != ESP_OK)
        {
            initializationResult_ = result;
            Logger::log(LoggerType::SENSOR, "GPIO%d calibration failed: %s", channel.gpio, esp_err_to_name(result));
            return initializationResult_;
        }
    }

    initializationResult_ = ESP_OK;
    Logger::log(LoggerType::SENSOR, "ADC1 oneshot initialized with %u calibrated channels", static_cast<unsigned>(channels_.size()));
    return ESP_OK;
}

esp_err_t AdcService::read(Input input, Measurement& measurement) noexcept
{
    const size_t index = static_cast<size_t>(input);
    if (index >= channels_.size() || !mutex_) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;

    esp_err_t result = initializeLocked();
    if (result == ESP_OK)
    {
        const Channel& channel = channels_[index];
        result = adc_oneshot_read(unit_, channel.channel, &measurement.raw);
        if (result == ESP_OK)
        {
            result = adc_cali_raw_to_voltage(channel.calibration, measurement.raw, &measurement.millivolts);
        }
    }

    xSemaphoreGive(mutex_);
    return result;
}
