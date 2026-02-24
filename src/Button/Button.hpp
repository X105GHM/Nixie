#pragma once

#include <functional>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "Logger/Logger.hpp"
#include "HSS/HSS.hpp"
#include "Acoustics/Buzzer/Buzzer.hpp"
#include "Acoustics/Relay/Relay.hpp"
#include "WiFiConnector/WiFiConnector.hpp"

static constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_41;

extern HSS hssController;
extern Buzzer buzzer;
extern Relay relay;
extern WiFiConnector wificonnector;

class ButtonPoll
{
public:
    explicit ButtonPoll(gpio_num_t pin = BUTTON_PIN, bool active_low = true) noexcept;

    esp_err_t init() noexcept;
    void poll() noexcept;

    void bindUserActions() noexcept;
    void onShortPress(std::function<void()> cb) noexcept { cbShort_ = std::move(cb); }
    void onLongPress(std::function<void()> cb) noexcept { cbLong_ = std::move(cb); }
    void onTriplePress(std::function<void()> cb) noexcept { cbTriple_ = std::move(cb); }
    void setTimings(uint32_t debounce_ms, uint32_t long_ms, uint32_t multi_ms) noexcept;

private:

    gpio_num_t pin_;
    bool active_low_;
    uint32_t debounce_us_;
    uint32_t longpress_us_;
    uint32_t multiclick_us_;

    bool stablePressed_;
    bool longFired_;
    int64_t lastTransitionUs_;
    int64_t pressStartUs_;
    int64_t lastReleaseUs_;
    uint8_t clickCount_;

    std::function<void()> cbShort_;
    std::function<void()> cbLong_;
    std::function<void()> cbTriple_;

    static inline bool rawToPressed(int raw, bool active_low) noexcept
    {
        return active_low ? (raw == 0) : (raw != 0);
    }

    void handlePress(int64_t now) noexcept;
    void handleRelease(int64_t now) noexcept;
    void maybeFlushClicks(int64_t now) noexcept;
};
