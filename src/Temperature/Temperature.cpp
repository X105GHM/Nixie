#include "Temperature.hpp"

static const char *TAG_NTC = "NtcThermistor";

NtcThermistor::NtcThermistor() noexcept {
    adc1_config_width(ADC_W);
    adc1_config_channel_atten(ADC_CH, ADC_A);
    adc1_config_channel_atten(ADC_VREF, ADC_A);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_A, ADC_W, VREF, &adc_chars_);
    ESP_LOGI(TAG_NTC, "NTC init: ch=%d, vrefch=%d, att=%d, width=%d", ADC_CH, ADC_VREF, ADC_A, ADC_W);
}

uint32_t NtcThermistor::measureVrefMv() const noexcept {
    int raw = adc1_get_raw(ADC_VREF);
    return esp_adc_cal_raw_to_voltage(raw, &adc_chars_);
}

float NtcThermistor::readTemperatureC() const noexcept {
    const_cast<esp_adc_cal_characteristics_t&>(adc_chars_).vref = measureVrefMv();
    int raw = adc1_get_raw(ADC_CH);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars_);
    float v    = static_cast<float>(mv) / 1000.0f;
    float r    = R_FIXED * (v / (3.3f - v));
    float lnR  = std::log(r / R_NOMINAL);
    float invT = (1.0f / T0) + (lnR / BETA);
    float TK   = 1.0f / invT;
    return (TK - 273.15f) + T_OFFSET;
}
