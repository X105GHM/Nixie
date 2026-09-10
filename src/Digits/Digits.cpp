#include "Digits.hpp"
#include "Config/Secrets.hpp"
#include <cmath>
#include <driver/spi_master.h>

constexpr gpio_num_t PIN_OE = GPIO_NUM_13;
constexpr gpio_num_t PIN_CLK = GPIO_NUM_12;
constexpr gpio_num_t PIN_DIN = GPIO_NUM_11;

namespace
{
    constexpr spi_host_device_t DISPLAY_SPI_HOST = SPI2_HOST;
    constexpr int DISPLAY_SPI_CLOCK_HZ = 2'000'000;
    constexpr uint8_t DISPLAY_SPI_MODE = 2;
    constexpr size_t DISPLAY_SPI_FRAME_BYTES = 8;

    class DisplaySpi final
    {
    public:
        esp_err_t init() noexcept
        {
            spi_bus_config_t busConfig{};
            busConfig.mosi_io_num = PIN_DIN;
            busConfig.miso_io_num = -1;
            busConfig.sclk_io_num = PIN_CLK;
            busConfig.quadwp_io_num = -1;
            busConfig.quadhd_io_num = -1;
            busConfig.max_transfer_sz = DISPLAY_SPI_FRAME_BYTES;

            esp_err_t err = spi_bus_initialize(DISPLAY_SPI_HOST, &busConfig, SPI_DMA_DISABLED);
            if (err != ESP_OK)
            {
                return err;
            }

            spi_device_interface_config_t deviceConfig{};
            deviceConfig.mode = DISPLAY_SPI_MODE;
            deviceConfig.clock_speed_hz = DISPLAY_SPI_CLOCK_HZ;
            deviceConfig.spics_io_num = -1;
            deviceConfig.queue_size = 1;

            err = spi_bus_add_device(DISPLAY_SPI_HOST, &deviceConfig, &device_);
            if (err != ESP_OK)
            {
                spi_bus_free(DISPLAY_SPI_HOST);
                device_ = nullptr;
            }
            return err;
        }

        esp_err_t transmit(uint32_t register1, uint32_t register0) const noexcept
        {
            if (!device_)
            {
                return ESP_ERR_INVALID_STATE;
            }

            const uint8_t payload[DISPLAY_SPI_FRAME_BYTES] = 
            {
                static_cast<uint8_t>(register1 >> 24),
                static_cast<uint8_t>(register1 >> 16),
                static_cast<uint8_t>(register1 >> 8),
                static_cast<uint8_t>(register1),
                static_cast<uint8_t>(register0 >> 24),
                static_cast<uint8_t>(register0 >> 16),
                static_cast<uint8_t>(register0 >> 8),
                static_cast<uint8_t>(register0),
            };

            spi_transaction_t transaction{};
            transaction.length = DISPLAY_SPI_FRAME_BYTES * 8;
            transaction.tx_buffer = payload;
            return spi_device_polling_transmit(device_, &transaction);
        }

    private:
        spi_device_handle_t device_ = nullptr;
    };

    DisplaySpi displaySpi;

    void transmitDisplayFrame(uint32_t register1, uint32_t register0) noexcept
    {
        const esp_err_t err = displaySpi.transmit(register1, register0);
        if (err != ESP_OK)
        {
            Logger::log(LoggerType::DIGIT, "Display SPI transfer failed: %s", esp_err_to_name(err));
        }
    }
} // namespace

std::atomic_bool displayEnabled{false};
std::atomic_bool singleDigitACP{false};
std::atomic_bool zipMaskingEnabled{false};
std::atomic_bool tempMaskingEnabled{false};
std::atomic<bool> mode_running{false};
std::atomic_uint32_t PWM_PERIOD_US{10000}; // 100 Hz, Periode = 10 ms //* Kann über HTTP geändert werden
std::atomic_int32_t digits{0};
std::atomic_uint8_t singleDigit{0};
std::atomic_uint32_t brightness{0};
static int32_t lastdigits = 717111; // zufälliger Startwert, damit Display initialisiert wird
static constexpr uint32_t symbolArray[10] = {512, 1, 2, 4, 8, 16, 32, 64, 128, 256};

static inline void initPinOe() noexcept
{
    gpio_set_direction(PIN_OE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_OE, 0);
}

void displayDigitsTask(void *pvParameters) noexcept
{
    const esp_err_t watchdogResult = esp_task_wdt_add(nullptr);
    const bool watchdogRegistered = watchdogResult == ESP_OK;
    if (watchdogResult != ESP_OK)
    {
        Logger::log(LoggerType::DIGIT, "Display watchdog registration failed: %s",
                    esp_err_to_name(watchdogResult));
    }

    initPinOe();

    const esp_err_t spiInitResult = displaySpi.init();
    if (spiInitResult != ESP_OK)
    {
        Logger::log(LoggerType::DIGIT, "Display SPI init failed: %s", esp_err_to_name(spiInitResult));
        for (;;)
        {
            if (watchdogRegistered)
                esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    Logger::log(LoggerType::DIGIT, "Display task started on core %d", xPortGetCoreID());

    // Loadcheck
    {
        uint32_t all8 = 0x3FFFFFFF;
        gpio_set_level(PIN_OE, 0);

        transmitDisplayFrame(all8, all8);

        gpio_set_level(PIN_OE, 1);
        vTaskDelay(pdMS_TO_TICKS(800));
        gpio_set_level(PIN_OE, 0);
    }

    for (;;)
    {
        if (watchdogRegistered)
            esp_task_wdt_reset();

        const bool acpEnabled = ACP_enabled.load(std::memory_order_relaxed);
        if (!acpEnabled)
        {
            const uint32_t b = std::min(brightness.load(std::memory_order_relaxed), uint32_t{100});
            const uint32_t periodUs = PWM_PERIOD_US.load(std::memory_order_relaxed);
            const uint32_t onTime = (periodUs * b) / 100;
            delayMicrosYield(onTime);
        }

        if (!displayEnabled.load(std::memory_order_relaxed) ||
            !Globals::loadDetected.load(std::memory_order_relaxed))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (singleDigitACP.load(std::memory_order_relaxed))
        {
            gpio_set_level(PIN_OE, 0);
            uint32_t register1 = 0;
            uint32_t register0 = 0;
            const uint8_t digit = singleDigit.load(std::memory_order_relaxed);
            if (digit < 30)
            {
                register0 |= symbolArray[digit % 10] << (digit - (digit % 10));
            }
            else
            {
                register1 |= symbolArray[digit % 10] << (digit - (digit % 10) - 30);
            }
            transmitDisplayFrame(register1, register0);
            gpio_set_level(PIN_OE, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const bool runningAcp1 = runningACP1.load(std::memory_order_relaxed);
        const bool runningAcp2 = runningACP2.load(std::memory_order_relaxed);
        const bool zipMasking = zipMaskingEnabled.load(std::memory_order_relaxed);
        const bool tempMasking = tempMaskingEnabled.load(std::memory_order_relaxed);

        if (acpEnabled || Globals::PWM_disabled.load(std::memory_order_relaxed))
        {
            const int32_t currentDigits = digits.load(std::memory_order_relaxed);
            if (lastdigits == currentDigits)
                continue;

            lastdigits = currentDigits;

            int64_t copy = currentDigits;
            gpio_set_level(PIN_OE, 0);
            uint32_t var32 = 0;

            //---------------------------------- REG 1 -----------------------------------------------

            // 00 0000000000 0000000000 0000000000 00 0000000000 0000000000 0000000000
            //        s2         s1         m2            m1         h2         h1
            // -- 0987654321 0987654321 0987654321 -- 0987654321 0987654321 0987654321

            if (!runningAcp1)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
            }

            copy /= 10;

            if (!runningAcp2)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
            }

            copy /= 10;

            if (!runningAcp1)
            {
                var32 |= symbolArray[copy % 10];
            }

            copy /= 10;

            const uint32_t register1 = var32;

            //---------------------------------- REG 0 -----------------------------------------------

            var32 = 0;

            if (!runningAcp2)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
            }

            copy /= 10;

            if (!runningAcp1)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
            }

            copy /= 10;

            if (!runningAcp2)
            {
                var32 |= symbolArray[copy % 10];
            }

            copy /= 10;

            transmitDisplayFrame(register1, var32);

            gpio_set_level(PIN_OE, 1);

            continue;
        }

        int64_t copy = digits.load(std::memory_order_relaxed);
        gpio_set_level(PIN_OE, 0);
        uint32_t var32 = 0;

        //---------------------------------- REG 1 -----------------------------------------------

        // 00 0000000000 0000000000 0000000000 00 0000000000 0000000000 0000000000
        //        s2         s1         m2            m1         h2         h1
        // -- 0987654321 0987654321 0987654321 -- 0987654321 0987654321 0987654321

        if (!runningAcp1 && !zipMasking && !tempMasking)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
        }

        copy /= 10;

        if (!runningAcp2 && !tempMasking)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
        }

        copy /= 10;

        if (!runningAcp1)
        {
            var32 |= symbolArray[copy % 10];
        }

        copy /= 10;

        const uint32_t register1 = var32;

        //---------------------------------- REG 0 -----------------------------------------------

        var32 = 0;

        if (!runningAcp2)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
        }

        copy /= 10;

        if (!runningAcp1)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
        }

        copy /= 10;

        if (!runningAcp2)
        {
            var32 |= symbolArray[copy % 10];
        }

        copy /= 10;

        transmitDisplayFrame(register1, var32);

        if (ADAPTIVE_BRIGHTNESS && (!runningAcp1 || !runningAcp2))
        {
            const uint32_t b = std::min(brightness.load(std::memory_order_relaxed), uint32_t{100});
            const uint32_t periodUs = PWM_PERIOD_US.load(std::memory_order_relaxed);
            const uint32_t onTime = (periodUs * b) / 100;
            const uint32_t offTime = periodUs - onTime;
            delayMicrosYield(offTime);
        }

        gpio_set_level(PIN_OE, 1);

        if (runningAcp1 || runningAcp2)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void displayTime() noexcept
{
    time_t t = time(nullptr);
    tm timeInfo{};
    localtime_r(&t, &timeInfo);
    const int32_t value = timeInfo.tm_hour * 10000 + timeInfo.tm_min * 100 + timeInfo.tm_sec;
    digits.store(value, std::memory_order_relaxed);
}

void displayDate() noexcept
{
    time_t t = time(nullptr);
    tm timeInfo{};
    localtime_r(&t, &timeInfo);
    const int32_t value = timeInfo.tm_mday * 10000 +
                          (timeInfo.tm_mon + 1) * 100 +
                          (timeInfo.tm_year + 1900) % 100;
    digits.store(value, std::memory_order_relaxed);
}

void displayWeather() noexcept
{
    static const std::string apiKey{OPENWEATHER_API_KEY};
    WeatherClient weather(apiKey);
    const std::string zip = Globals::getTextConfig().zipCode;

    Logger::log(LoggerType::WEATHER, "displayWeather(): going to fetch temp for ZIP: %s", zip.c_str());

    float temp = weather.getTemperatureByZip(zip);
    if (!std::isnan(temp))
    {
        int temp100 = static_cast<int>(std::round(temp * 10000.0f));

        if (temp100 < 0)
        {
            temp100 = 0;
        }

        digits = temp100;
    }
    else
    {
        digits = 0;

        Logger::log(LoggerType::WEATHER, "Error fetching weather data for ZIP: %s", zip.c_str());
    }
}
