#include "WiFiConnector.hpp"

WiFiConnector::WiFiConnector(const char *apSsid, const char *apPass) noexcept
    : apSsid_(apSsid), apPass_(apPass)
{
}

void WiFiConnector::connect() noexcept
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::log(logType_, "WiFi already connected: %s", WiFi.localIP().toString().c_str());
        return;
    }

    Logger::log(logType_, "WiFi not connected, starting AP: %s", apSsid_);
    WiFiManager wm;
    wm.setTimeout(180);
    
    if (!wm.autoConnect(apSsid_, apPass_))
    {
        Logger::log(logType_, "WiFiManager timeout, restarting");
        delay(3000);
        ESP.restart();
        return;
    }
    Logger::log(logType_, "WiFi connected, IP=%s", WiFi.localIP().toString().c_str());



    // mDNS mit Kollisionsprüfung
    const String baseName = "nixieclock";
    String      hostName;
    int         suffix  = 1;
    bool        success = false;

    while (!success)
    {
        hostName = (suffix == 1)
            ? baseName
            : baseName + String(suffix);

        if (!MDNS.begin(hostName.c_str()))
        {
            Logger::log(logType_, "Error in MDNS.begin(%s)", hostName.c_str());
            suffix++;
            continue;
        }
        MDNS.addService("http", "tcp", 80);

        delay(200);

        int n = MDNS.queryService("http", "tcp");
        Logger::log(logType_, "MDNS: %d HTTP service(s) found", n);

        int countMe = 0;
        for (int i = 0; i < n; ++i)
        {
            if (String(MDNS.hostname(i)) == hostName)
                ++countMe;
        }

        if (countMe <= 1)
        {
            success = true;
            Logger::log(logType_, "mDNS responder started as %s.local", hostName.c_str());
        }
        else
        {
            Logger::log(logType_, "Hostname conflict for %s, trying next suffix", hostName.c_str());
            MDNS.end();
            suffix++;
        }
    }
}
