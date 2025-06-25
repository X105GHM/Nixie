#include "SupplyWatch.hpp"

SupplyWatch::SupplyWatch() noexcept
{
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_5V,  ADC_11db);
    analogSetPinAttenuation(PIN_18V, ADC_11db);
    analogSetPinAttenuation(PIN_3V3, ADC_6db);
    analogSetPinAttenuation(PIN_UHSS,ADC_11db);
    analogSetPinAttenuation(PIN_IGES,ADC_0db);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &cal5V);
    cal12V = cal5V;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_6,  ADC_WIDTH_BIT_12, DEFAULT_VREF, &cal3V3);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &cal18V);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, &calUHSS);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0,  ADC_WIDTH_BIT_12, DEFAULT_VREF, &calI);
}

float SupplyWatch::readCompensated(int pin,
                                   const esp_adc_cal_characteristics_t &chars,
                                   float rTop,
                                   float rBot,
                                   float driftMvPerC,
                                   float intercept) const noexcept
{
    float tempC = temperatureSensor.readTemperatureC();
    int raw = analogRead(pin);
    uint32_t mv  = esp_adc_cal_raw_to_voltage(raw, &chars);
    float v_adc  = mvToVolt(mv);
    float deltaT = tempC - T_REF_C;

    float v_corr = v_adc - (driftMvPerC / 1000.0f) * deltaT;

    return v_corr * ((rTop + rBot) / rBot) + intercept;
}

float SupplyWatch::read5V()   const noexcept { return readCompensated(PIN_5V,  cal5V,  R5_TOP,    R5_BOT,   DRIFT_5V_MV_PER_C,   C_5V);   }
float SupplyWatch::read12V()  const noexcept { return readCompensated(PIN_12V, cal12V, R12_TOP,   R12_BOT,  DRIFT_12V_MV_PER_C,  C_12V);  }
float SupplyWatch::read3V3()  const noexcept { return readCompensated(PIN_3V3, cal3V3, R3V3_TOP,  R3V3_BOT, DRIFT_3V3_MV_PER_C,  C_3V3);  }
float SupplyWatch::read18V()  const noexcept { return readCompensated(PIN_18V, cal18V, R18_TOP,   R18_BOT,  DRIFT_18V_MV_PER_C,  C_18V);  }
float SupplyWatch::readUHSS() const noexcept { return readCompensated(PIN_UHSS,calUHSS,R_UHSS_TOP,R_UHSS_BOT,DRIFT_UHSS_MV_PER_C, C_UHSS);}  

float SupplyWatch::readCurrent() const noexcept
{
    float tempC = temperatureSensor.readTemperatureC();
    int raw = analogRead(PIN_IGES);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &calI);
    float v_adc = mvToVolt(mv);
    float deltaT = tempC - T_REF_C;

    float v_corr = v_adc - (DRIFT_5V_MV_PER_C / 1000.0f) * deltaT;
    return v_corr * CURRENT_FACT;
}