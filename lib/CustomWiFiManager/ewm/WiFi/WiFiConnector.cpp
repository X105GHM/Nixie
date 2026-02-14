#include "ewm/WiFi/WiFiConnector.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include "ewm/Utils/WiFiStatus.hpp"
#include "ewm/Log.hpp"
#include <algorithm>

namespace ewm
{
    WiFiConnector::WiFiConnector(SemaphoreHandle_t wifiMutex)
        : wifiMutex_(wifiMutex) {}

    bool WiFiConnector::safeSwitchMode(wifi_mode_t targetMode)
    {
        ewm::utils::WiFiLock lk(wifiMutex_);

        if (WiFi.getMode() == targetMode) return true;

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            if (WiFi.mode(targetMode))
            {
                delay(80);
                return true;
            }
            delay(120);
        }
        return (WiFi.getMode() == targetMode);
    }

    bool WiFiConnector::initSta(const String& hostname)
    {
        ewm::utils::WiFiLock lk(wifiMutex_);

        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
        WiFi.setSleep(false);

        bool ok = safeSwitchMode(WIFI_STA);

        #if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 4
            WiFi.setHostname(hostname.c_str());
        #else
            (void)hostname;
        #endif
        return ok;
    }

    std::vector<String> WiFiConnector::scanVisibleUnique()
    {
        ewm::utils::WiFiLock lk(wifiMutex_);

        wifi_mode_t m = WiFi.getMode();
        if (m == WIFI_OFF) WiFi.mode(WIFI_STA);
        else if (m == WIFI_AP) WiFi.mode(WIFI_AP_STA);
        else if (m != WIFI_STA && m != WIFI_AP_STA) WiFi.mode(WIFI_STA);

        delay(80);
        WiFi.disconnect(false, false);
        delay(80);

        int n = WiFi.scanNetworks(false, true);
        std::vector<String> result;
        result.reserve(n > 0 ? n : 0);

        for (int i = 0; i < n; ++i)
        {
            String s = WiFi.SSID(i);
            if (s.length())
            {
                bool seen = false;
                for (auto& e : result) if (e == s) { seen = true; break; }
                if (!seen) result.push_back(s);
            }
        }

        WiFi.scanDelete();
        return result;
    }

    bool WiFiConnector::waitForConnectedOrFail(uint32_t timeoutMs)
    {
        const uint32_t start = millis();
        for (;;)
        {
            wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) return true;

            if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || st == WL_CONNECTION_LOST)
                return false;

            if ((int32_t)(millis() - start) >= (int32_t)timeoutMs)
                return false;

            delay(120);
        }
    }

    bool WiFiConnector::hasInternet(const char* host, uint16_t port, uint32_t timeoutMs)
    {
        if (WiFi.status() != WL_CONNECTED) return false;

        WiFiClient client;
        client.setTimeout(timeoutMs / 1000 + 1);

        if (client.connect(host, port))
        {
            client.stop();
            return true;
        }
        return false;
    }

    bool WiFiConnector::tryConnectOneWithInternet(
        const Credential& c,
        uint32_t connectTimeoutMs,
        bool requireInternet,
        const char* probeHost,
        uint16_t probePort,
        uint32_t internetTimeoutMs)
    {
        if (c.ssid[0] == '\0') return false;

        EWM_LOG("Trying '%s' (prio=%u)...", c.ssid, (unsigned)c.priority);

        if (!safeSwitchMode(WIFI_STA))
        {
            EWM_LOG("  -> FAIL: cannot switch to STA");
            return false;
        }

        {
            ewm::utils::WiFiLock lk(wifiMutex_);
            WiFi.disconnect(false, false);
            delay(120);
            WiFi.begin(c.ssid, c.password);
        }

        if (!waitForConnectedOrFail(connectTimeoutMs))
        {
            EWM_LOG("  -> FAIL: connect timeout/status=%s", ewm::utils::wlStatusStr(WiFi.status()));
            ewm::utils::WiFiLock lk(wifiMutex_);
            WiFi.disconnect(false, false);
            return false;
        }

        EWM_LOG("  -> CONNECTED: ip=%s rssi=%d",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());

        if (requireInternet)
        {
            EWM_LOG("  -> internet probe %s:%u timeout=%ums", probeHost, (unsigned)probePort, (unsigned)internetTimeoutMs);
            if (!hasInternet(probeHost, probePort, internetTimeoutMs))
            {
                EWM_LOG("  -> FAIL: internet probe failed");
                ewm::utils::WiFiLock lk(wifiMutex_);
                WiFi.disconnect(false, false);
                return false;
            }
            EWM_LOG("  -> internet OK");
        }

        return true;
    }

    bool WiFiConnector::tryConnectAll(
        const std::vector<Credential>& creds,
        uint32_t connectTimeoutMs,
        uint32_t betweenRetryMs,
        bool requireInternet,
        const char* probeHost,
        uint16_t probePort,
        uint32_t internetTimeoutMs,
        std::function<void(const Credential&)> onSuccess)
    {
        const auto visible = scanVisibleUnique();

        std::vector<Credential> order;
        order.reserve(creds.size());

        // bevorzugt: sichtbare SSIDs
        for (const auto& c : creds)
        {
            bool vis = false;
            for (auto& v : visible) if (v == c.ssid) { vis = true; break; }
            if (vis) order.push_back(c);
        }
        if (order.empty()) order = creds;

        std::sort(order.begin(), order.end(), [](const Credential& a, const Credential& b)
        {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.last_ok > b.last_ok;
        });

        EWM_LOG("Connect order:");
        for (size_t i = 0; i < order.size(); ++i)
            EWM_LOG("  %u) ssid='%s' prio=%u last_ok=%u",
                    (unsigned)i, order[i].ssid, (unsigned)order[i].priority, (unsigned)order[i].last_ok);

        for (const auto& c : order)
        {
            if (tryConnectOneWithInternet(c, connectTimeoutMs, requireInternet, probeHost, probePort, internetTimeoutMs))
            {
                if (onSuccess) onSuccess(c);
                return true;
            }
            delay(betweenRetryMs);
        }
        return false;
    }

    bool WiFiConnector::beginConnectAsync(const String& ssid, const String& pass)
    {
        ewm::utils::WiFiLock lk(wifiMutex_);
        safeSwitchMode(WIFI_STA);
        WiFi.disconnect(false, false);
        delay(80);
        WiFi.begin(ssid.c_str(), pass.c_str());
        return true;
    }
}
