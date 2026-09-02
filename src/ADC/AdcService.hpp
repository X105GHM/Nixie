#pragma once

#include <array>
#include <cstddef>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class AdcService final
{
public:
    enum class Input : size_t
    {
        Supply12V,
        Supply18V,
        Current,
        Uhss,
        Temperature,
        Supply5V,
        Supply3V3,
        Count
    };

    struct Measurement
    {
        int raw{0};
        int millivolts{0};
    };

    static AdcService& instance() noexcept;
    esp_err_t read(Input input, Measurement& measurement) noexcept;

    AdcService(const AdcService&) = delete;
    AdcService& operator=(const AdcService&) = delete;

private:
    struct Channel
    {
        int gpio;
        adc_channel_t channel;
        adc_atten_t attenuation;
        adc_cali_handle_t calibration{nullptr};
    };

    AdcService() noexcept;
    esp_err_t initializeLocked() noexcept;

    static constexpr size_t CHANNEL_COUNT = static_cast<size_t>(Input::Count);

    std::array<Channel, CHANNEL_COUNT> channels_{
    {
        {1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, nullptr},
        {2, ADC_CHANNEL_1, ADC_ATTEN_DB_12, nullptr},
        {4, ADC_CHANNEL_3, ADC_ATTEN_DB_0,  nullptr},
        {5, ADC_CHANNEL_4, ADC_ATTEN_DB_12, nullptr},
        {6, ADC_CHANNEL_5, ADC_ATTEN_DB_12, nullptr},
        {8, ADC_CHANNEL_7, ADC_ATTEN_DB_12, nullptr},
        {9, ADC_CHANNEL_8, ADC_ATTEN_DB_6,  nullptr},
    }};

    StaticSemaphore_t mutexStorage_{};
    SemaphoreHandle_t mutex_{nullptr};
    adc_oneshot_unit_handle_t unit_{nullptr};
    esp_err_t initializationResult_{ESP_ERR_INVALID_STATE};
    bool initializationAttempted_{false};
};
