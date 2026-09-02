#include "ewm/Portal/PortalUiConfig.hpp"
#include "ewm/Utils/MiniJson.hpp"

using ewm::utils::json_escape;

namespace
{
    static std::string q(const std::string& s) { return "\"" + json_escape(s) + "\""; }
    static std::string b(bool v) { return v ? "true" : "false"; }
}

namespace ewm::portal
{
    std::string PortalUiConfig::toJson() const
    {
        std::string j = "{";

        j += "\"mdnsHost\":" + q(mdnsHost) + ",";

        j += "\"finish\":{";
        j += "\"redirect\":" + b(finish.redirect) + ",";
        j += "\"redirectUrl\":" + q(finish.redirectUrl) + ",";
        j += "\"closeTab\":" + b(finish.closeTab) + ",";
        j += "\"closeMode\":" + q(finish.closeMode) + ",";
        j += "\"finishDelayMs\":" + std::to_string(finish.finishDelayMs) + ",";
        j += "\"countdownAutoFinish\":" + b(finish.countdownAutoFinish) + ",";
        j += "\"countdownThresholdMs\":" + std::to_string(finish.countdownThresholdMs);
        j += "},";

        j += "\"timing\":{";
        j += "\"pollMs\":" + std::to_string(timing.pollMs) + ",";
        j += "\"connectTimeoutMs\":" + std::to_string(timing.connectTimeoutMs) + ",";
        j += "\"switchToStaAfterFails\":" + std::to_string(timing.switchToStaAfterFails) + ",";
        j += "\"giveUpAfterFails\":" + std::to_string(timing.giveUpAfterFails);
        j += "}";

        j += "}";
        return j;
    }
}
