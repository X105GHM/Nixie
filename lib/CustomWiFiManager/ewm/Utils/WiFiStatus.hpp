#pragma once
#include <WiFi.h>

namespace ewm::utils
{
    inline const char* wlStatusStr(wl_status_t st)
    {
        switch (st)
        {
            case WL_IDLE_STATUS:      return "IDLE";
            case WL_NO_SSID_AVAIL:    return "NO_SSID";
            case WL_SCAN_COMPLETED:   return "SCAN_DONE";
            case WL_CONNECTED:        return "CONNECTED";
            case WL_CONNECT_FAILED:   return "CONNECT_FAILED";
            case WL_CONNECTION_LOST:  return "CONNECTION_LOST";
            case WL_DISCONNECTED:     return "DISCONNECTED";
            default:                  return "UNKNOWN";
        }
    }
}
