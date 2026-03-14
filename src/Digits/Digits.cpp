#include "Digits.hpp"

constexpr gpio_num_t PIN_OE = GPIO_NUM_13;
constexpr gpio_num_t PIN_CLK = GPIO_NUM_12;
constexpr gpio_num_t PIN_DIN = GPIO_NUM_11;

static SPISettings dispSPISettings(2'000'000, MSBFIRST, SPI_MODE2);

bool displayEnabled = false;
bool singleDigitACP = false;
bool zipMaskingEnabled = false;
bool tempMaskingEnabled = false;
std::atomic<bool> mode_running{false};
uint32_t PWM_PERIOD_US = 10000; // 100 Hz, Periode = 10 ms //* Kann über HTTP geändert werden
int32_t digits = 0;
uint8_t singleDigit = 0;
int32_t lastdigits = 717111; // zufälliger Startwert, damit Display initialisiert wird
uint32_t brightness;
const  uint32_t symbolArray[10] = {512, 1, 2, 4, 8, 16, 32, 64, 128, 256};

static inline void initPinOe() noexcept
{
    pinMode((int)PIN_OE, OUTPUT);
    digitalWrite((int)PIN_OE, LOW);
}

static inline void initSPI() noexcept
{
    SPI.begin(PIN_CLK, -1, PIN_DIN, -1); // Wir nutzen nur clock und MOSI
}

void displayDigitsTask(void *pvParameters) noexcept
{
    initSPI();
    initPinOe();

    Logger::log(LoggerType::DIGIT, "Display task started on core ", String(xPortGetCoreID()));


    // Loadcheck
    {
        uint32_t all8 = 0x3FFFFFFF;
        gpio_set_level(PIN_OE, 0);

        SPI.transfer(all8 >> 24);
        SPI.transfer(all8 >> 16);
        SPI.transfer(all8 >> 8);
        SPI.transfer(all8);

        SPI.transfer(all8 >> 24);
        SPI.transfer(all8 >> 16);
        SPI.transfer(all8 >> 8);
        SPI.transfer(all8);

        SPI.endTransaction(); 

        gpio_set_level(PIN_OE, 1);
        vTaskDelay(pdMS_TO_TICKS(800));
        gpio_set_level(PIN_OE, 0);
    }

    for (;;)
    {
        esp_task_wdt_reset();

        if (!ACP_enabled) 
        {
            uint32_t b      = min(brightness, (uint32_t)100);
            uint32_t onTime = (PWM_PERIOD_US * b) / 100;
            delayMicrosYield(onTime);
        }

        if (!displayEnabled || !Globals::loadDetected)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (singleDigitACP)
        {
            SPI.beginTransaction(dispSPISettings);
            gpio_set_level(PIN_OE, 0);
            uint32_t var32 = 0;
            if (singleDigit < 30)
            {
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
                var32 |= symbolArray[singleDigit % 10] << (singleDigit - (singleDigit % 10));
                SPI.transfer(var32 >> 24);
                SPI.transfer(var32 >> 16);
                SPI.transfer(var32 >> 8);
                SPI.transfer(var32);
            }
            else
            {
                var32 |= symbolArray[singleDigit % 10] << (singleDigit - (singleDigit % 10) - 30);
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
            SPI.endTransaction();
            gpio_set_level(PIN_OE, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (ACP_enabled == true || Globals::PWM_disabled)
        {
            if (lastdigits == digits)
                continue;

            lastdigits = digits;

            SPI.beginTransaction(dispSPISettings);

            int64_t copy = digits;
            gpio_set_level(PIN_OE, 0);
            uint32_t var32 = 0;

            //---------------------------------- REG 1 -----------------------------------------------

            // 00 0000000000 0000000000 0000000000 00 0000000000 0000000000 0000000000
            //        s2         s1         m2            m1         h2         h1
            // -- 0987654321 0987654321 0987654321 -- 0987654321 0987654321 0987654321

            if (!runningACP1)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
            }

            copy /= 10;

            if (!runningACP2)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
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

            //---------------------------------- REG 0 -----------------------------------------------

            var32 = 0;

            if (!runningACP2)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
            }

            copy /= 10;

            if (!runningACP1)
            {
                var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
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

            SPI.endTransaction();

            gpio_set_level(PIN_OE, 1);

            continue;
        }

        SPI.beginTransaction(dispSPISettings);

        int64_t copy = digits;
        gpio_set_level(PIN_OE, 0);
        uint32_t var32 = 0;

        //---------------------------------- REG 1 -----------------------------------------------

        // 00 0000000000 0000000000 0000000000 00 0000000000 0000000000 0000000000
        //        s2         s1         m2            m1         h2         h1
        // -- 0987654321 0987654321 0987654321 -- 0987654321 0987654321 0987654321

        if (!runningACP1 && !zipMaskingEnabled && !tempMaskingEnabled)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
        }

        copy /= 10;

        if (!runningACP2 && !tempMaskingEnabled)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
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

        //---------------------------------- REG 0 -----------------------------------------------

        var32 = 0;

        if (!runningACP2)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 20);
        }

        copy /= 10;

        if (!runningACP1)
        {
            var32 |= (static_cast<uint32_t>(symbolArray[copy % 10]) << 10);
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

        SPI.endTransaction();

        if (ADAPTIVE_BRIGHTNESS && (!runningACP1 || !runningACP2))
        {
            uint32_t b       = min(brightness, (uint32_t)100);
            uint32_t onTime  = (PWM_PERIOD_US * b) / 100;
            uint32_t offTime = PWM_PERIOD_US - onTime;
            delayMicrosYield(offTime);
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
    time_t t =  time(nullptr);
    tm timeInfo{};
    localtime_r(&t, &timeInfo);
    digits = 0;
    digits += timeInfo.tm_hour * 10000;
    digits += timeInfo.tm_min * 100;
    digits += timeInfo.tm_sec;
}

void displayDate() noexcept
{
    time_t t =  time(nullptr);
    tm timeInfo{};
    localtime_r(&t, &timeInfo);
    digits  = 0;
    digits += timeInfo.tm_mday * 10000;
    digits += (timeInfo.tm_mon + 1) * 100;
    digits += (timeInfo.tm_year + 1900) % 100;
}

void displayWeather() noexcept
{
    static const std::string apiKey{OPENWEATHER_API_KEY};
    WeatherClient weather(apiKey);
    const std::string &zip = Globals::zipCode;

    Logger::log(LoggerType::GENERAL, "displayWeather(): going to fetch temp for ZIP: %s", zip.c_str());

    float temp = weather.getTemperatureByZip(zip);
    if (! isnan(temp))
    {
        int temp100 = static_cast<int>(roundf(temp * 10000.0f));

        if (temp100 < 0)
        {
            temp100 = 0;
        }

        digits = temp100;
    }
    else
    {
        digits = 0;

        Logger::log(LoggerType::DIGIT, "Error fetching weather data for ZIP: %s", zip.c_str());
    }
}
