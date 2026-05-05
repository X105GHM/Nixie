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
    ewm.setRequireInternetOnConnect(true);

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
    if (!baseName || !*baseName)
    {
        baseName = "nixieclock";
    }

    String hostName(baseName);

    delay(100 + (esp_random() % 300));

    if (mdns_init() != ESP_OK)
    {
        Logger::log(logType_, "mdns_init() preflight failed, using base name '%s'", baseName);
    }
    else
    {
        constexpr uint32_t queryTimeoutMs = 750;
        constexpr int maxSuffix = 15;

        esp_ip4_addr_t addr{};
        bool foundFreeName = false;

        for (int suffix = 1; suffix <= maxSuffix; ++suffix)
        {
            hostName = (suffix == 1) ? String(baseName) : (String(baseName) + String(suffix));

            addr.addr = 0;
            esp_err_t err = mdns_query_a(hostName.c_str(), queryTimeoutMs, &addr);

            if (err == ESP_ERR_NOT_FOUND)
            {
                Logger::log(logType_, "mDNS preflight: %s.local seems free", hostName.c_str());
                foundFreeName = true;
                break;
            }

            if (err == ESP_OK)
            {
                Logger::log(logType_, "mDNS preflight: %s.local already exists", hostName.c_str());
            }
            else
            {
                Logger::log(logType_, "mDNS preflight query for %s failed (err=%d)",
                            hostName.c_str(), (int)err);
            }

            delay(120);
        }

        mdns_free();

        if (!foundFreeName)
        {
            Logger::log(logType_, "mDNS preflight found no free suffix, falling back to '%s'",
                        baseName);
            hostName = String(baseName);
        }
    }

    if (!MDNS.begin(hostName.c_str()))
    {
        Logger::log(logType_, "Error in MDNS.begin(%s)", hostName.c_str());
        return;
    }

    MDNS.addService("http", "tcp", 80);
    Logger::log(logType_, "mDNS responder started as %s.local", hostName.c_str());
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
