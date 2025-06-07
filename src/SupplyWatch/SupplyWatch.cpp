#include "SupplyWatch.hpp"


static const char *TAG = "SupplyWatch";

SupplyWatch::SupplyWatch() noexcept {
    adc1_config_width(ADC_WIDTH);
    for (auto ch : {CH_VREF, CH_12V, CH_18V, CH_IGES, CH_UHSS, CH_5V, CH_3V3}) {
        adc1_config_channel_atten(ch, ADC_ATTEN);
    }
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, V_REF_MV, &adc_chars_);
}

uint32_t SupplyWatch::measureVrefMv() const noexcept {
    int raw = adc1_get_raw(CH_VREF);
    return esp_adc_cal_raw_to_voltage(raw, &adc_chars_);
}

uint32_t SupplyWatch::readChannelMv(adc1_channel_t ch) const noexcept {
    const_cast<esp_adc_cal_characteristics_t&>(adc_chars_).vref = measureVrefMv();
    int raw = adc1_get_raw(ch);
    return esp_adc_cal_raw_to_voltage(raw, &adc_chars_);
}

float SupplyWatch::readDivider(adc1_channel_t ch, float rTop, float rBot, float offset) const noexcept {
    float v = static_cast<float>(readChannelMv(ch)) / 1000.0f;
    return v * ((rTop + rBot) / rBot) + offset;
}

float SupplyWatch::read12V()    const noexcept { return readDivider(CH_12V,  R12_TOP,   R12_BOT,  OFF_12V);  }
float SupplyWatch::read18V()    const noexcept { return readDivider(CH_18V,  R18_TOP,   R18_BOT,  OFF_18V);  }
float SupplyWatch::read5V()     const noexcept { return readDivider(CH_5V,    R5_TOP,    R5_BOT,   OFF_5V);   }
float SupplyWatch::read3V3()    const noexcept { return readDivider(CH_3V3,  R3V3_TOP,  R3V3_BOT, OFF_3V3);  }
float SupplyWatch::readUHSS()   const noexcept { return readDivider(CH_UHSS, R_UHSS_TOP,R_UHSS_BOT,OFF_UHSS); }
float SupplyWatch::readCurrent()const noexcept {
    const_cast<esp_adc_cal_characteristics_t&>(adc_chars_).vref = measureVrefMv();
    float mv = static_cast<float>(readChannelMv(CH_IGES));
    return (mv / 1000.0f) * CURRENT_FACTOR;
}