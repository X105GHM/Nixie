#pragma once
#include <stdint.h>
#include <string>

namespace ewm::portal
{
    struct PortalUiFinish
    {
        bool redirect{false};
        std::string redirectUrl{};
        bool closeTab{false};
        std::string closeMode{"blank"};
        uint32_t finishDelayMs{250};
        bool countdownAutoFinish{false};
        uint32_t countdownThresholdMs{1200};
    };

    struct PortalUiTiming
    {
        uint32_t pollMs{800};
        uint32_t connectTimeoutMs{45000};
        uint8_t switchToStaAfterFails{3};
        uint8_t giveUpAfterFails{10};
    };

    struct PortalUiConfig
    {
        std::string mdnsHost{};
        PortalUiFinish finish{};
        PortalUiTiming timing{};
        std::string toJson() const;
    };
}
