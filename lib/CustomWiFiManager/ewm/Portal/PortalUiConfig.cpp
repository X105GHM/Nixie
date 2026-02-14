#include "ewm/Portal/PortalUiConfig.hpp"
#include "ewm/Utils/MiniJson.hpp"

using ewm::utils::json_escape;

namespace
{
    static String q(const String& s) { return String("\"") + json_escape(s) + "\""; }
    static String b(bool v) { return v ? "true" : "false"; }
}

namespace ewm::portal
{
    String PortalUiConfig::toJson() const
    {
        String j = "{";

        j += "\"mdnsHost\":" + q(mdnsHost) + ",";

        j += "\"finish\":{";
        j += "\"redirect\":" + b(finish.redirect) + ",";
        j += "\"redirectUrl\":" + q(finish.redirectUrl) + ",";
        j += "\"closeTab\":" + b(finish.closeTab) + ",";
        j += "\"closeMode\":" + q(finish.closeMode) + ",";
        j += "\"finishDelayMs\":" + String(finish.finishDelayMs) + ",";
        j += "\"countdownAutoFinish\":" + b(finish.countdownAutoFinish) + ",";
        j += "\"countdownThresholdMs\":" + String(finish.countdownThresholdMs);
        j += "},";

        j += "\"timing\":{";
        j += "\"pollMs\":" + String(timing.pollMs) + ",";
        j += "\"connectTimeoutMs\":" + String(timing.connectTimeoutMs) + ",";
        j += "\"switchToStaAfterFails\":" + String(timing.switchToStaAfterFails) + ",";
        j += "\"giveUpAfterFails\":" + String(timing.giveUpAfterFails);
        j += "}";

        j += "}";
        return j;
    }
}
