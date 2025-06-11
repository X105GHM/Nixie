#include "WiFiConnector.hpp"

WiFiConnector::WiFiConnector(const char *apSsid, const char *apPass) noexcept
    : apSsid_(apSsid), apPass_(apPass)
{
}

void WiFiConnector::connect() noexcept
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::log(logType_,"WiFi already connected: %s", WiFi.localIP().toString().c_str());
        return;
    }

    Logger::log(logType_,"WiFi not connected, starting AP: %s", apSsid_);
    WiFiManager wm;
    wm.setTimeout(180);
    
    if (!wm.autoConnect(apSsid_, apPass_))
    {
        Logger::log(logType_,"WiFiManager timeout, restarting");
        delay(3000);
        ESP.restart();
        return;
    }
    Logger::log(logType_,"WiFi connected, IP=%s",WiFi.localIP().toString().c_str());

    if (!MDNS.begin("nixieclock"))
    {
        Logger::log(logType_, F("Fehler beim Starten des mDNS responders"));
    }
    else
    {
        Logger::log(logType_, F("mDNS responder gestartet"));
        MDNS.addService("http", "tcp", 80);
    }
}