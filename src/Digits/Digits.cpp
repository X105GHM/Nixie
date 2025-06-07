#include "Digits.hpp"

constexpr gpio_num_t PIN_OE = GPIO_NUM_13;
constexpr gpio_num_t PIN_CLK = GPIO_NUM_12;
constexpr gpio_num_t PIN_DIN = GPIO_NUM_11;

bool displayEnabled = false;
std::int32_t digits = 0;
std::uint8_t singleDigit = 0;
std::uint32_t brightness = 100;
const std::uint32_t symbolArray[10] = {
    512, 1, 2, 4, 8, 16, 32, 64, 128, 256};

static inline void initPinOe() noexcept
{
    gpio_set_direction(PIN_OE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_OE, 0);
}

static inline void initSPI() noexcept
{
    SPI.begin(PIN_CLK, -1, PIN_DIN, -1); // Wir nutzen nur clock und MOSI
    SPI.setDataMode(SPI_MODE2);
    SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/8 = 2MHz
}

void displayDigitsTask(void *pvParameters) noexcept
{
    initPinOe();
    initSPI();
    for (;;)
    {
        ets_delay_us(ON_TIME_US);
        if (!displayEnabled)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (runningManualACP)
        {
            gpio_set_level(PIN_OE, 0);
            std::uint32_t var32 = 0;
            if (singleDigit < 30)
            {
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
                var32 |= (symbolArray[singleDigit % 10]
                          << (singleDigit - (singleDigit % 10)));
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
            }
            else
            {
                var32 |= (symbolArray[singleDigit % 10]
                          << (singleDigit - (singleDigit % 10) - 30));
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
                var32 = 0;
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
            }
            gpio_set_level(PIN_OE, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        std::int64_t copy = digits;
        gpio_set_level(PIN_OE, 0);
        std::uint32_t var32 = 0;
        if (!runningACP1)
        {
            var32 |= (static_cast<std::uint32_t>(symbolArray[copy % 10]) << 20);
        }
        copy /= 10;
        if (!runningACP2)
        {
            var32 |= (static_cast<std::uint32_t>(symbolArray[copy % 10]) << 10);
        }
        copy /= 10;
        if (!runningACP1)
        {
            var32 |= symbolArray[copy % 10];
        }
        copy /= 10;
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
        var32 = 0;
        if (!runningACP2)
        {
            var32 |= (static_cast<std::uint32_t>(symbolArray[copy % 10]) << 20);
        }
        copy /= 10;
        if (!runningACP1)
        {
            var32 |= (static_cast<std::uint32_t>(symbolArray[copy % 10]) << 10);
        }
        copy /= 10;
        if (!runningACP2)
        {
            var32 |= symbolArray[copy % 10];
        }
        copy /= 10;
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
        if (ADAPTIVE_BRIGHTNESS && (!runningACP1 || !runningACP2))
        {
            std::uint32_t b = std::min(brightness, static_cast<std::uint32_t>(100));
            std::uint32_t offTime = (ON_TIME_US * 100 - ON_TIME_US * b) / b;
            ets_delay_us(offTime);
        }
        gpio_set_level(PIN_OE, 1);
        if (runningACP1 || runningACP2)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void displayTime() noexcept
{
    std::time_t t = std::time(nullptr);
    std::tm timeInfo{};
    localtime_r(&t, &timeInfo);
    digits = 0;
    digits += timeInfo.tm_hour * 10000;
    digits += timeInfo.tm_min * 100;
    digits += timeInfo.tm_sec;
}

void displayDate() noexcept
{
    std::time_t t = std::time(nullptr);
    std::tm timeInfo{};
    localtime_r(&t, &timeInfo);
    digits = 0;
    digits += timeInfo.tm_mday * 10000;
    digits += (timeInfo.tm_mon + 1) * 100;
    digits += (timeInfo.tm_year + 1900) % 100;
}
