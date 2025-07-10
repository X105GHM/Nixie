#include "HSS.hpp"

static constexpr LoggerType logType = LoggerType::HSS;

HSS::HSS() noexcept
{
    gpio_config_t io_conf{};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_160V) | (1ULL << PIN_190V) | (1ULL << PIN_REDUCE) | (1ULL << PIN_HSS_LED);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(PIN_160V, 0);
    gpio_set_level(PIN_190V, 0);
    gpio_set_level(PIN_REDUCE, 0);
    gpio_set_level(PIN_HSS_LED, 0);

    Logger::log(logType, F("GPIOs for HSS initialized (15, 7, 19, 42 = outputs, all LOW)"));
}

void HSS::enable160() const noexcept
{
    enable160V = true;
    gpio_set_level(PIN_160V, 1);
    gpio_set_level(PIN_HSS_LED, 1);
    Logger::log(logType, F("160V enabled (GPIO15 = HIGH)"));
}

void HSS::disable160() const noexcept
{
    enable160V = false;
    gpio_set_level(PIN_160V, 0);
    gpio_set_level(PIN_HSS_LED, 0);
    Logger::log(logType, F("160V disabled (GPIO15 = LOW)"));
}

void HSS::enable190() const noexcept
{
    enable190V = true;
    gpio_set_level(PIN_190V, 1);
    Logger::log(logType, F("190V boost enabled (GPIO7 = HIGH)"));
}

void HSS::disable190() const noexcept
{
    enable190V = false;
    gpio_set_level(PIN_190V, 0);
    Logger::log(logType, F("190V boost disabled (GPIO7 = LOW)"));
}

void HSS::enableResistorReduction() const noexcept
{
    enableResistor = true;
    gpio_set_level(PIN_REDUCE, 1);
    Logger::log(logType, F("Resistors reduced (GPIO19 = HIGH)"));
}

void HSS::disableResistorReduction() const noexcept
{
    enableResistor = false;
    gpio_set_level(PIN_REDUCE, 0);
    Logger::log(logType, F("Resistors restored (GPIO19 = LOW)"));
}

bool HSS::testLoad(const std::function<float()> &readVoltage, float thresholdV, uint32_t discriminationMs, uint32_t maxWaitMs) const noexcept
{
    Logger::log(logType, F("testLoad: charge to 160V, then measure discharge"));

    enable160();
    vTaskDelay(pdMS_TO_TICKS(100));
    float voltage = readVoltage();
    Logger::log(logType, "Initial voltage: %.2f V", voltage);
    TickType_t start = xTaskGetTickCount();
    disable160();

    while (true)
    {
        voltage = readVoltage();
        uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;

        if (voltage < thresholdV)
        {
            Logger::log(logType, "Voltage dropped to %.2f V after %u ms", voltage, elapsed);
            bool load = elapsed < discriminationMs;
            Logger::log(logType, load ? "testLoad: drop-time %ums < %ums => LOAD detected" : "testLoad: drop-time %ums ≥ %ums => NO LOAD", elapsed, discriminationMs);
            return load;
        }

        if (elapsed >= maxWaitMs)
        {
            Logger::log(logType,"testLoad: timeout %u ms reached, voltage still %.2f V => NO LOAD", elapsed, voltage);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
