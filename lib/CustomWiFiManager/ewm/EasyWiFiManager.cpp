#include "ewm/EasyWiFiManager.hpp"
#include "ewm/Log.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include "ewm/Utils/WiFiStatus.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"
#include <WiFi.h>

namespace ewm
{
    EasyWiFiManager &EasyWiFiManager::instance()
    {
        static EasyWiFiManager inst;
        return inst;
    }

    EasyWiFiManager::EasyWiFiManager()
        : wifiMutex_(xSemaphoreCreateRecursiveMutex()), 
          storage_("EWM1", "EWM1B"),
          wifi_(wifiMutex_),
          portal_(80, wifiMutex_)
    {
        // Monitor wiring
        monitor_.setCheckFn([this](uint32_t t) { return wifi_.hasInternet(probeHost_, probePort_, t); });
        monitor_.setRoamFn([this]() { roamTryAll_(); });
    }

    void EasyWiFiManager::setHostname(const String &name) { hostname_ = name; }

    void EasyWiFiManager::setAPCredentials(const String &apSsid, const String &apPass)
    {
        apSsid_ = apSsid;
        apPass_ = apPass;
        portal_.setAP(apSsid_, apPass_);
    }

    void EasyWiFiManager::setInternetProbe(const char *host, uint16_t port)
    {
        probeHost_ = host;
        probePort_ = port;
    }

    void EasyWiFiManager::setRequireInternetOnConnect(bool enabled)
    {
        requireInternetOnConnect_ = enabled;
        monitor_.setRequireInternet(enabled);
    }

    wl_status_t EasyWiFiManager::status() const { return wifi_.status(); }

    void EasyWiFiManager::onConnect(ConnectCallback cb) { onConnectCb_ = std::move(cb); }

    uint32_t EasyWiFiManager::nowSeconds_() const
    {
        time_t t = time(nullptr);
        if (t < 1600000000)
            return millis() / 1000;
        return (uint32_t)t;
    }

    std::vector<Credential> EasyWiFiManager::listCredentials() const
    {
        return store_.list();
    }

    bool EasyWiFiManager::addCredential(const String &ssid, const String &password, uint8_t priority)
    {
        return store_.addOrUpdate(storage_, ssid, password, priority);
    }

    bool EasyWiFiManager::eraseAll()
    {
        return store_.eraseAll(storage_);
    }

    bool EasyWiFiManager::removeCredential(const String &ssid)
    {
        return store_.remove(storage_, ssid);
    }

    void EasyWiFiManager::setBackgroundAP(bool enabled)
    {
        backgroundAP_ = enabled;
        ensureAPState_();
    }

    void EasyWiFiManager::ensureAPState_()
    {
        ewm::utils::WiFiLock lk(wifiMutex_);

        if (backgroundAP_)
        {
            if (WiFi.getMode() != WIFI_AP_STA)
                WiFi.mode(WIFI_AP_STA);

            if (WiFi.softAPSSID() != apSsid_)
                WiFi.softAP(apSsid_.c_str(), (apPass_.length() == 0 ? nullptr : apPass_.c_str()));
        }
        else
        {
            // Wenn kein Portal läuft: AP aus
            WiFi.softAPdisconnect(false);
            if (WiFi.getMode() == WIFI_AP_STA)
                WiFi.mode(WIFI_STA);
        }
    }

    void EasyWiFiManager::portalConnectRequest_(const String &ssid, const String &pass, uint8_t prio)
    {
        bool exists = false;
        String storedPass;

        auto &hdr = store_.header();
        auto &arr = store_.data();
        for (size_t i = 0; i < hdr.count && i < arr.size(); ++i)
        {
            if (ssid == arr[i].ssid)
            {
                exists = true;
                storedPass = String(arr[i].password);
                break;
            }
        }

        String usePass = pass;
        if (usePass.length() == 0 && exists)
            usePass = storedPass;

        const bool shouldAutoSave = (!exists) || (pass.length() > 0);

        pendingSave_ = shouldAutoSave;
        if (pendingSave_)
        {
            pendingSsid_ = ssid;
            pendingPass_ = usePass;
            pendingPrio_ = prio;
        }

        wifi_.beginConnectAsync(ssid, usePass);
    }

    void EasyWiFiManager::portalOnStaConnected_()
    {
        if (pendingSave_)
        {
            store_.addOrUpdate(storage_, pendingSsid_, pendingPass_, pendingPrio_);
            pendingSave_ = false;
        }

        // Callback/Info
        if (onConnectCb_)
            onConnectCb_(WiFi.localIP());
    }

    void EasyWiFiManager::startConfigPortal()
    {
        PortalHooks hooks;
        hooks.listCreds = [this]()
        { return store_.list(); };
        hooks.addCred = [this](const String &s, const String &p, uint8_t pr)
        { return store_.addOrUpdate(storage_, s, p, pr); };
        hooks.delCred = [this](const String &s)
        { return store_.remove(storage_, s); };
        hooks.reorder = [this](const std::vector<String> &order)
        { store_.reorderBySsidList(storage_, order); };
        hooks.eraseAll = [this]()
        { store_.eraseAll(storage_); };

        hooks.connectRequest = [this](const String &s, const String &p, uint8_t pr)
        { portalConnectRequest_(s, p, pr); };
        hooks.onStaConnected = [this]()
        { portalOnStaConnected_(); };

        hooks.isStaConnected = []()
        { return WiFi.status() == WL_CONNECTED; };
        hooks.staIp = []()
        { return WiFi.localIP().toString(); };
        hooks.staRssi = []()
        { return WiFi.RSSI(); };

        portal_.setAP(apSsid_, apPass_);
        portal_.setUiConfigJson(portalUiCfgJson_);
        portal_.runBlocking(hooks);
    }

    void EasyWiFiManager::roamTryAll_()
    {
        // kurz andere SSIDs probieren (kein Portal)
        std::vector<Credential> creds = store_.list();

        bool ok = wifi_.tryConnectAll(
            creds,
            8000,
            500,
            requireInternetOnConnect_,
            probeHost_,
            probePort_,
            15000,
            [this](const Credential &used)
            {
                // last_ok updaten im Store-Array
                auto &hdr = store_.header();
                auto &arr = store_.data();
                for (size_t i = 0; i < hdr.count && i < arr.size(); ++i)
                {
                    if (strncmp(arr[i].ssid, used.ssid, sizeof(used.ssid)) == 0)
                    {
                        arr[i].last_ok = nowSeconds_();
                        store_.save(storage_);
                        break;
                    }
                }

                if (onConnectCb_)
                    onConnectCb_(WiFi.localIP());
            });

        (void)ok;
        ensureAPState_();
    }

    void EasyWiFiManager::setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs)
    {
        monitor_.configure(enabled, checkIntervalMs, internetTimeoutMs);
        monitor_.startIfNeeded();
    }

    void EasyWiFiManager::onNoConnectivity(std::function<void()> cb, uint32_t delayMs)
    {
        monitor_.setOnNoConnectivity(std::move(cb), delayMs);
    }

    void ewm::EasyWiFiManager::setPortalUiConfig(const portal::PortalUiConfig& cfg)
    {
        portalUiCfgJson_ = cfg.toJson();
    }

    void ewm::EasyWiFiManager::setPortalUiConfigJson(const String& json)
    {
        portalUiCfgJson_ = json.length() ? json : "{}";
    }

    void EasyWiFiManager::begin(uint32_t connectTimeoutMs, uint32_t betweenRetryMs)
    {
        EWM_LOG("begin(): start");

        store_.load(storage_);

        for (auto &c : store_.list())
            EWM_LOG("  ssid='%s' prio=%u last_ok=%u", c.ssid, (unsigned)c.priority, (unsigned)c.last_ok);

        wifi_.initSta(hostname_);

        bool connected = wifi_.tryConnectAll(
            store_.list(),
            connectTimeoutMs,
            betweenRetryMs,
            requireInternetOnConnect_,
            probeHost_,
            probePort_,
            15000,
            [this](const Credential &used)
            {
                auto &hdr = store_.header();
                auto &arr = store_.data();
                for (size_t i = 0; i < hdr.count && i < arr.size(); ++i)
                {
                    if (strncmp(arr[i].ssid, used.ssid, sizeof(used.ssid)) == 0)
                    {
                        arr[i].last_ok = nowSeconds_();
                        store_.save(storage_);
                        break;
                    }
                }

                if (onConnectCb_)
                    onConnectCb_(WiFi.localIP());
            });

        EWM_LOG("tryConnectAll() => %s", connected ? "CONNECTED" : "FAILED");

        if (!connected)
        {
            EWM_LOG("No known network worked -> starting config portal (AP='%s')", apSsid_.c_str());
            startConfigPortal();
        }

        ensureAPState_();
        monitor_.startIfNeeded();

        EWM_LOG("begin(): end");
    }
}
