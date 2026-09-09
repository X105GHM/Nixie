#include "WiFiConnector.hpp"

#include "esp_system.h"
#include "esp_random.h"
#include "mdns.h"
#include "ewm/Portal/PortalUiConfig.hpp"
#include "ewm/Utils/Time.hpp"
#include "Diagnostics/ResetDiagnostics.hpp"

using ewm::EasyWiFiManager;

WiFiConnector::WiFiConnector(const char* apSsid, const char* apPass) noexcept
    : apSsid_(apSsid), apPass_(apPass)
{
}

void WiFiConnector::connect() noexcept
{
    auto& manager = EasyWiFiManager::instance();
    manager.setHostname("nixieclock");
    manager.setAPCredentials(apSsid_ ? apSsid_ : "Nixie Clock", apPass_ ? apPass_ : "");
    manager.setInternetProbe("1.1.1.1", 53);
    manager.setRequireInternetOnConnect(true);

    ewm::portal::PortalUiConfig ui;
    ui.mdnsHost = "nixieclock.local";
    ui.finish.countdownAutoFinish = true;
    ui.finish.redirect = true;
    ui.finish.redirectUrl = "http://nixieclock.local/";
    ui.finish.closeTab = false;
    manager.setPortalUiConfig(ui);

    manager.onConnect([this](const std::string& ip)
    {
        Logger::log(logType_, "WiFi connected, IP=%s", ip.c_str());
        startMDNSOnce_();
    });

    manager.begin();
    manager.setConnectivityMonitor(true, 10000, 60000);
    manager.onNoConnectivity([]
    {
        Logger::log(LoggerType::WiFi, "No connectivity for extended period -> restarting");
        ResetDiagnostics::instance().markPlannedRestart("connectivity");
        Memory::saveGlobals();
        esp_restart();
    }, 300000);
}

void WiFiConnector::startMDNSWithCollisionCheck_(const char* baseName) noexcept
{
    if (!baseName || !*baseName) baseName = "nixieclock";
    if (mdns_init() != ESP_OK)
    {
        Logger::log(logType_, "Native mDNS initialization failed");
        return;
    }

    std::string hostName = baseName;
    ewm::utils::delayMilliseconds(100 + (esp_random() % 300));

    constexpr uint32_t queryTimeoutMs = 750;
    constexpr int maxSuffix = 15;
    bool foundFreeName = false;
    esp_ip4_addr_t address{};

    for (int suffix = 1; suffix <= maxSuffix; ++suffix)
    {
        hostName = suffix == 1 ? std::string(baseName) : std::string(baseName) + std::to_string(suffix);
        address.addr = 0;
        const esp_err_t queryResult = mdns_query_a(hostName.c_str(), queryTimeoutMs, &address);
        if (queryResult == ESP_ERR_NOT_FOUND)
        {
            foundFreeName = true;
            break;
        }
        ewm::utils::delayMilliseconds(120);
    }

    if (!foundFreeName) hostName = baseName;

    esp_err_t result = mdns_hostname_set(hostName.c_str());
    if (result == ESP_OK) result = mdns_instance_name_set("Nixie Clock");
    if (result == ESP_OK) result = mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    if (result != ESP_OK)
    {
        Logger::log(logType_, "Native mDNS responder setup failed: %s", esp_err_to_name(result));
        mdns_free();
        return;
    }

    mdnsStarted_.store(true, std::memory_order_release);
    Logger::log(logType_, "mDNS responder started as %s.local", hostName.c_str());
}

void WiFiConnector::startMDNSOnce_() noexcept
{
    if (mdnsStarted_.load(std::memory_order_acquire)) return;

    bool expected = false;
    if (!mdnsStartPending_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

    if (xTaskCreatePinnedToCore(mdnsStartTask_, "NativeMDNS", 6144, this, 1, nullptr, 0) != pdPASS)
    {
        mdnsStartPending_.store(false, std::memory_order_release);
        Logger::log(logType_, "Native mDNS task creation failed");
    }
}

void WiFiConnector::mdnsStartTask_(void* context) noexcept
{
    auto* connector = static_cast<WiFiConnector*>(context);
    if (connector)
    {
        connector->startMDNSWithCollisionCheck_("nixieclock");
        connector->mdnsStartPending_.store(false, std::memory_order_release);
    }
    vTaskDelete(nullptr);
}

void WiFiConnector::eraseCredentials() noexcept
{
    const bool ok = EasyWiFiManager::instance().eraseAll();
    Logger::log(logType_, ok ? "WiFi credentials erased successfully" : "Failed to erase WiFi credentials");
}

std::vector<ewm::Credential> WiFiConnector::getSavedNetworks() const noexcept
{
    return EasyWiFiManager::instance().listCredentials();
}

bool WiFiConnector::addOrUpdateNetwork(const std::string& ssid, const std::string& password, uint8_t priority) noexcept
{
    if (ssid.empty()) return false;
    const bool ok = EasyWiFiManager::instance().addCredential(ssid, password, priority);
    Logger::log(logType_, ok ? "WiFi network saved (priority %u)" : "Failed to save WiFi network", priority);
    return ok;
}

bool WiFiConnector::removeNetwork(const std::string& ssid) noexcept
{
    const bool ok = EasyWiFiManager::instance().removeCredential(ssid);
    Logger::log(logType_, ok ? "WiFi network removed" : "WiFi network not found");
    return ok;
}

bool WiFiConnector::isConnected() const noexcept
{
    return EasyWiFiManager::instance().isConnected();
}

bool WiFiConnector::waitUntilConnected(TickType_t timeoutTicks) const noexcept
{
    return EasyWiFiManager::instance().waitForConnected(timeoutTicks);
}

bool WiFiConnector::waitUntilApplicationNetworkReady(TickType_t timeoutTicks) const noexcept
{
    return EasyWiFiManager::instance().waitForApplicationNetworkReady(timeoutTicks);
}

std::string WiFiConnector::getWiFiSSID() const noexcept
{
    return EasyWiFiManager::instance().connectedSsid();
}

std::string WiFiConnector::getWiFiPass() const noexcept
{
    const std::string ssid = getWiFiSSID();
    if (ssid.empty()) return {};
    for (const auto& credential : EasyWiFiManager::instance().listCredentials())
    {
        if (ssid == credential.ssid) return credential.password;
    }
    return {};
}

std::string WiFiConnector::getIpAddress() const noexcept
{
    auto& manager = EasyWiFiManager::instance();
    const std::string stationAddress = manager.staIpAddress();
    return stationAddress.empty() ? manager.apIpAddress() : stationAddress;
}
