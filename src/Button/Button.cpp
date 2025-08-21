#include "Button.hpp"
#include "Globals/Globals.hpp"
#include "Memory/Memory.hpp"
#include "esp_system.h"

static constexpr LoggerType logType = LoggerType::BUTTON;

ButtonPoll::ButtonPoll(gpio_num_t pin, bool active_low) noexcept
    : pin_(pin),
      active_low_(active_low),
      debounce_us_(10'000),    // 10 ms
      longpress_us_(700'000),  // 700 ms
      multiclick_us_(350'000), // 350 ms
      stablePressed_(false),
      longFired_(false),
      lastTransitionUs_(0),
      pressStartUs_(0),
      lastReleaseUs_(0),
      clickCount_(0)
{
}

void ButtonPoll::setTimings(uint32_t debounce_ms, uint32_t long_ms, uint32_t multi_ms) noexcept
{
    debounce_us_    = debounce_ms   * 1000U;
    longpress_us_   = long_ms       * 1000U;
    multiclick_us_  = multi_ms      * 1000U;
}

esp_err_t ButtonPoll::init() noexcept
{
    gpio_config_t io_conf{};
    io_conf.intr_type       = GPIO_INTR_DISABLE;
    io_conf.mode            = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask    = (1ULL << pin_);
    io_conf.pull_up_en      = active_low_ ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en    = active_low_ ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        Logger::log(logType, "gpio_config for GPIO%d failed (%d)", static_cast<int>(pin_), err);
        return err;
    }

    stablePressed_ = rawToPressed(gpio_get_level(pin_), active_low_);
    lastTransitionUs_ = esp_timer_get_time();
    Logger::log(logType, "Button init on GPIO%d, active_%s, start=%s", static_cast<int>(pin_), active_low_ ? "low" : "high", stablePressed_ ? "PRESSED" : "RELEASED");
    return ESP_OK;
}

void ButtonPoll::poll() noexcept
{
    const int64_t now = esp_timer_get_time();
    const bool rawPressed = rawToPressed(gpio_get_level(pin_), active_low_);

    if (rawPressed != stablePressed_)
    {
        if ((now - lastTransitionUs_) >= debounce_us_)
        {
            stablePressed_ = rawPressed;
            lastTransitionUs_ = now;
            if (stablePressed_)
                handlePress(now);
            else
                handleRelease(now);
        }
    }
    else
    {
        if (stablePressed_ && !longFired_ && (now - pressStartUs_) >= longpress_us_)
        {
            longFired_ = true;
            clickCount_ = 0;
            if (cbLong_)
                cbLong_();
            Logger::log(logType, "Long press");
        }
        if (!stablePressed_ && clickCount_ > 0 && (now - lastReleaseUs_) >= multiclick_us_)
        {
            maybeFlushClicks(now);
        }
    }
}

void ButtonPoll::handlePress(int64_t now) noexcept
{
    pressStartUs_ = now;
    longFired_ = false;
    Logger::log(logType, "Press");
}

void ButtonPoll::handleRelease(int64_t now) noexcept
{
    lastReleaseUs_ = now;
    if (!longFired_)
    {
        if (clickCount_ < 10)
            ++clickCount_;
        Logger::log(logType, "Release (clickCount=%u)", clickCount_);
    }
    else
    {
        longFired_ = false;
        clickCount_ = 0;
        Logger::log(logType, "Release after long");
    }
}

void ButtonPoll::maybeFlushClicks(int64_t) noexcept
{
    if (clickCount_ == 1)
    {
        if (cbShort_)
            cbShort_();
        Logger::log(logType, "Short press");
    }
    else if (clickCount_ >= 3)
    {
        if (cbTriple_)
            cbTriple_();
        Logger::log(logType, "Triple press (%u)", clickCount_);
    }
    else
    {
        Logger::log(logType, "Clicks=%u (ignored)", clickCount_);
    }
    clickCount_ = 0;
}

void ButtonPoll::bindUserActions() noexcept
{
    setTimings(/*debounce=*/10, /*long=*/700, /*multi=*/350);

    // KURZ:
    onShortPress([]
    {
        Logger::log(logType, "ACTION: Toggle display -> %s", displayEnabled ? "ON" : "OFF");
        displayEnabled = !displayEnabled; 
    });

    // LANG: 
    onLongPress([]
    {
        Globals::SilentModeEnabled = !Globals::SilentModeEnabled;
        Logger::log(logType, "ACTION: Toggle SilentMode -> %s", Globals::SilentModeEnabled ? "ON" : "OFF");
        if (Globals::SilentModeEnabled)
        {
            for(int i = 0; i < 10; ++i)
            {
                relay.toggle();
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            buzzer.Silence();
        }
        else
        {
            buzzer.Silence();

            for(int i = 0; i < 2; ++i)
            {
            buzzer.setLevel(1);
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer.setLevel(0);
            vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
    });

    // TRIPLE:
    onTriplePress([]
    {
        Logger::log(logType, "ACTION: HSS On");
        for(int i = 0; i < 3; ++i)
        {
            buzzer.setLevel(1);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer.setLevel(0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        hssController.enable160();
    });
}