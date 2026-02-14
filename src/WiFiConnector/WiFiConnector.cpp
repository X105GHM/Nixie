#include "WiFiConnector.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"
using ewm::EasyWiFiManager;

WiFiConnector::WiFiConnector(const char *apSsid, const char *apPass) noexcept
    : apSsid_(apSsid), apPass_(apPass)
{
}

void WiFiConnector::connect() noexcept
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::log(logType_, "WiFi already connected: %s", WiFi.localIP().toString().c_str());
        startMDNSOnce_();
        return;
    }

    auto &ewm = EasyWiFiManager::instance();
    ewm.setHostname("nixieclock");
    ewm.setAPCredentials(apSsid_, (apPass_ ? apPass_ : ""));
    ewm.setInternetProbe("1.1.1.1", 53);

    ewm::portal::PortalUiConfig ui;
    ui.mdnsHost = "nixieclock.local";
    ui.finish.countdownAutoFinish = true;
    ui.finish.redirect = true;
    ui.finish.redirectUrl = "http://nixieclock.local/";
    ui.finish.closeTab = false;
    ewm.setPortalUiConfig(ui);

    ewm.onConnect([this](const IPAddress &ip)
    {
        Logger::log(logType_, "WiFi connected, IP=%s", ip.toString().c_str());
        startMDNSOnce_(); 
    });

    ewm.begin();

    ewm.setConnectivityMonitor(true, 10000, 60000);

    ewm.onNoConnectivity([]
    {
        Logger::log(LoggerType::WiFi, "No connectivity for extended period -> restarting");
        Memory::saveGlobals();
        ESP.restart(); 
    },  300000);
}

void WiFiConnector::startMDNSWithCollisionCheck_(const char *baseName) noexcept
{
    String hostName;
    int suffix = 1;
    bool okName = false;

    while (!okName)
    {
        hostName = (suffix == 1) ? baseName : (String(baseName) + String(suffix));

        if (!MDNS.begin(hostName.c_str()))
        {
            Logger::log(logType_, "Error in MDNS.begin(%s)", hostName.c_str());
            suffix++;
            continue;
        }
        MDNS.addService("http", "tcp", 80);

        delay(200);

        int n = MDNS.queryService("http", "tcp");
        Logger::log(logType_, "mDNS: %d HTTP service(s) found", n);

        int countMe = 0;
        for (int i = 0; i < n; ++i)
        {
            if (String(MDNS.hostname(i)) == hostName)
                ++countMe;
        }

        if (countMe <= 1)
        {
            okName = true;
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

void WiFiConnector::startMDNSOnce_() noexcept
{
    if (mdnsStarted_)
        return;

    MDNS.end();

    startMDNSWithCollisionCheck_("nixieclock");
    mdnsStarted_ = true;
}

void WiFiConnector::eraseCredentials() noexcept
{
    if (ewm::EasyWiFiManager::instance().eraseAll())
    {
        Logger::log(logType_, "WiFi credentials erased successfully");
    }
    else
    {
        Logger::log(logType_, "Failed to erase WiFi credentials");
    }
}

std::vector<ewm::Credential> WiFiConnector::getSavedNetworks() const noexcept
{
    return EasyWiFiManager::instance().listCredentials();
}

bool WiFiConnector::addOrUpdateNetwork(const String &ssid, const String &password, uint8_t priority) noexcept
{
    if (ssid.isEmpty())
        return false;
    bool ok = EasyWiFiManager::instance().addCredential(ssid, password, priority);
    Logger::log(logType_, ok ? "Saved WiFi '%s' (prio %u)" : "Failed to save WiFi '%s'", ssid.c_str(), priority);
    return ok;
}

bool WiFiConnector::removeNetwork(const String &ssid) noexcept
{
    bool ok = EasyWiFiManager::instance().removeCredential(ssid);
    Logger::log(logType_, ok ? "Removed WiFi '%s'" : "WiFi '%s' not found", ssid.c_str());
    return ok;
}
