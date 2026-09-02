#pragma once

#include "ewm/Types.hpp"

namespace ewm::utils
{
    inline const char* stateName(State state)
    {
        switch (state)
        {
            case State::Uninitialized: return "Uninitialized";
            case State::Initialization: return "Initialization";
            case State::Connecting: return "Connecting";
            case State::Connected: return "Connected";
            case State::Retry: return "Retry";
            case State::Portal: return "Portal";
            case State::Failed: return "Failed";
            default: return "Unknown";
        }
    }
}
