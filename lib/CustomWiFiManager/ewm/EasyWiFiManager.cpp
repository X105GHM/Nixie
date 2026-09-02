#include "ewm/EasyWiFiManager.hpp"

#include <cstring>

#include "ewm/Log.hpp"
#include "ewm/Utils/Time.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include "ewm/Utils/WiFiStatus.hpp"

namespace ewm
{
    EasyWiFiManager& EasyWiFiManager::instance()
    {
        static EasyWiFiManager instance;
        return instance;
    }

    EasyWiFiManager::EasyWiFiManager()
        : wifiMutex_(xSemaphoreCreateRecursiveMutex()),
          credentialMutex_(xSemaphoreCreateRecursiveMutex()),
          stateEvents_(xEventGroupCreate()),
          storage_("EWM1", "EWM1B"),
          wifi_(wifiMutex_),
          portal_(80, wifi_)
    {
        wifi_.setEventCallback([this]() { notifyStateTask(); });
        monitor_.setConnectedFn([this]() { return wifi_.isConnected(); });
        monitor_.setCheckFn([this](uint32_t timeoutMs)
        {
            return wifi_.hasInternet(probeHost_.c_str(), probePort_, timeoutMs);
        });
        monitor_.setRoamFn([this]() { requestRoam(); });
    }

    void EasyWiFiManager::setHostname(const std::string& name)
    {
        hostname_ = name;
    }

    void EasyWiFiManager::setAPCredentials(const std::string& apSsid, const std::string& apPass)
    {
        apSsid_ = apSsid;
        apPass_ = apPass;
        portal_.setAP(apSsid_, apPass_);
    }

    void EasyWiFiManager::setInternetProbe(const char* host, uint16_t port)
    {
        probeHost_ = host && host[0] != '\0' ? host : "1.1.1.1";
        probePort_ = port;
    }

    void EasyWiFiManager::setRequireInternetOnConnect(bool enabled)
    {
        requireInternetOnConnect_.store(enabled, std::memory_order_release);
        monitor_.setRequireInternet(enabled);
    }

    void EasyWiFiManager::onConnect(ConnectCallback callback)
    {
        onConnectCallback_ = std::move(callback);
    }

    uint32_t EasyWiFiManager::nowSeconds() const
    {
        const time_t now = time(nullptr);
        if (now < 1600000000)
        {
            return static_cast<uint32_t>(ewm::utils::monotonicMillis() / 1000U);
        }
        return static_cast<uint32_t>(now);
    }

    std::vector<Credential> EasyWiFiManager::listCredentials() const
    {
        ewm::utils::WiFiLock lock(credentialMutex_);
        return store_.list();
    }

    bool EasyWiFiManager::addCredential(const std::string& ssid, const std::string& password, uint8_t priority)
    {
        ewm::utils::WiFiLock lock(credentialMutex_);
        return store_.addOrUpdate(storage_, ssid, password, priority);
    }

    bool EasyWiFiManager::eraseAll()
    {
        ewm::utils::WiFiLock lock(credentialMutex_);
        return store_.eraseAll(storage_);
    }

    bool EasyWiFiManager::removeCredential(const std::string& ssid)
    {
        ewm::utils::WiFiLock lock(credentialMutex_);
        return store_.remove(storage_, ssid);
    }

    void EasyWiFiManager::setBackgroundAP(bool enabled)
    {
        backgroundAP_.store(enabled, std::memory_order_release);
        notifyStateTask();
    }

    void EasyWiFiManager::ensureAPState()
    {
        if (portal_.running())
        {
            return;
        }

        if (backgroundAP_.load(std::memory_order_acquire))
        {
            wifi_.startAccessPoint(apSsid_, apPass_);
        }
        else
        {
            wifi_.stopAccessPoint();
        }
    }

    void EasyWiFiManager::portalConnectRequest(
        const std::string& ssid,
        const std::string& pass,
        uint8_t priority)
    {
        std::string password = pass;
        bool exists = false;
        {
            ewm::utils::WiFiLock lock(credentialMutex_);
            const auto credentials = store_.list();
            for (const auto& credential : credentials)
            {
                if (ssid == credential.ssid)
                {
                    exists = true;
                    if (password.empty()) password = credential.password;
                    break;
                }
            }

            pendingSave_ = !exists || !pass.empty();
            if (pendingSave_)
            {
                pendingSsid_ = ssid;
                pendingPass_ = password;
                pendingPriority_ = priority;
            }
        }

        if (!wifi_.beginConnectAsync(ssid, password))
        {
            EWM_LOG("Portal connection request could not be started");
        }
    }

    void EasyWiFiManager::portalOnStaConnected()
    {
        {
            ewm::utils::WiFiLock lock(credentialMutex_);
            if (pendingSave_)
            {
                store_.addOrUpdate(storage_, pendingSsid_, pendingPass_, pendingPriority_);
                pendingSsid_.clear();
                pendingPass_.clear();
                pendingSave_ = false;
            }
        }

        if (onConnectCallback_)
        {
            onConnectCallback_(wifi_.staIpAddress());
        }
    }

    bool EasyWiFiManager::startPortal()
    {
        setState(State::Portal);
        PortalHooks hooks;
        hooks.listCreds = [this]() { return listCredentials(); };
        hooks.addCred = [this](const std::string& ssid, const std::string& password, uint8_t priority)
        {
            return addCredential(ssid, password, priority);
        };
        hooks.delCred = [this](const std::string& ssid) { return removeCredential(ssid); };
        hooks.reorder = [this](const std::vector<std::string>& order)
        {
            ewm::utils::WiFiLock lock(credentialMutex_);
            store_.reorderBySsidList(storage_, order);
        };
        hooks.eraseAll = [this]() { eraseAll(); };
        hooks.connectRequest = [this](const std::string& ssid, const std::string& password, uint8_t priority)
        {
            portalConnectRequest(ssid, password, priority);
        };
        hooks.onStopRequested = [this]()
        {
            portalStopRequested_.store(true, std::memory_order_release);
            notifyStateTask();
        };
        hooks.scanNetworks = [this]() { return wifi_.scanNetworks(); };
        hooks.isStaConnected = [this]() { return wifi_.isConnected(); };
        hooks.staIp = [this]() { return wifi_.staIpAddress(); };
        hooks.staRssi = [this]() { return wifi_.rssi(); };

        portal_.setAP(apSsid_, apPass_);
        portal_.setUiConfigJson(portalUiConfigJson_);
        portalStopRequested_.store(false, std::memory_order_release);
        return portal_.start(std::move(hooks));
    }

    void EasyWiFiManager::finishPortalConnection()
    {
        portalOnStaConnected();
        portal_.markStaConnected();
        xEventGroupSetBits(stateEvents_, CONNECTED_BIT);
        EWM_LOG("Portal STA connected; grace period started");
    }

    void EasyWiFiManager::handleSuccessfulCredential(const Credential& used)
    {
        {
            ewm::utils::WiFiLock lock(credentialMutex_);
            auto& header = store_.header();
            auto& credentials = store_.data();
            for (size_t index = 0; index < header.count && index < credentials.size(); ++index)
            {
                if (std::strncmp(credentials[index].ssid, used.ssid, sizeof(used.ssid)) == 0)
                {
                    credentials[index].last_ok = nowSeconds();
                    store_.save(storage_);
                    break;
                }
            }
        }

        if (onConnectCallback_)
        {
            onConnectCallback_(wifi_.staIpAddress());
        }
    }

    bool EasyWiFiManager::runConnectionCycle(bool retry)
    {
        setState(retry ? State::Retry : State::Connecting);
        const auto credentials = listCredentials();
        const bool connected = wifi_.tryConnectAll(
            credentials,
            connectTimeoutMs_,
            betweenRetryMs_,
            requireInternetOnConnect_.load(std::memory_order_acquire),
            probeHost_.c_str(),
            probePort_,
            15000,
            [this](const Credential& used) { handleSuccessfulCredential(used); });

        if (!connected)
        {
            return false;
        }

        setState(State::Connected);
        xEventGroupSetBits(stateEvents_, CONNECTED_BIT | APPLICATION_READY_BIT);
        monitor_.startIfNeeded();
        ensureAPState();
        return true;
    }

    void EasyWiFiManager::begin(uint32_t connectTimeoutMs, uint32_t betweenRetryMs)
    {
        if (stateTask_ || state() != State::Uninitialized)
        {
            return;
        }

        connectTimeoutMs_ = connectTimeoutMs;
        betweenRetryMs_ = betweenRetryMs;
        setState(State::Initialization);

        {
            ewm::utils::WiFiLock lock(credentialMutex_);
            store_.load(storage_);
        }

        if (!wifi_.initSta(hostname_))
        {
            setState(State::Failed);
            return;
        }

        if (xTaskCreatePinnedToCore(stateTaskEntry, "ewm_state", 8192, this, 3, &stateTask_, 0) != pdPASS)
        {
            stateTask_ = nullptr;
            setState(State::Failed);
        }
    }

    void EasyWiFiManager::startConfigPortal()
    {
        forcePortalRequested_.store(true, std::memory_order_release);
        notifyStateTask();
    }

    void EasyWiFiManager::stateTaskEntry(void* arg)
    {
        static_cast<EasyWiFiManager*>(arg)->stateLoop();
    }

    void EasyWiFiManager::stateLoop()
    {
        bool portalConnectionHandled = false;

        if (forcePortalRequested_.exchange(false, std::memory_order_acq_rel) || !runConnectionCycle(false))
        {
            if (!startPortal())
            {
                setState(State::Failed);
            }
        }

        for (;;)
        {
            const TickType_t waitTicks = state() == State::Portal ? pdMS_TO_TICKS(200) : portMAX_DELAY;
            ulTaskNotifyTake(pdTRUE, waitTicks);

            if (state() == State::Portal)
            {
                if (portalConnectionHandled && !wifi_.isConnected())
                {
                    portalConnectionHandled = false;
                    portal_.clearStaConnected();
                }

                if (wifi_.isConnected() && !portalConnectionHandled)
                {
                    portalConnectionHandled = true;
                    finishPortalConnection();
                }

                if (portalStopRequested_.load(std::memory_order_acquire) ||
                    (portalConnectionHandled && portal_.graceExpired()))
                {
                    const bool connected = wifi_.isConnected();
                    portal_.stop(!backgroundAP_.load(std::memory_order_acquire));
                    portalStopRequested_.store(false, std::memory_order_release);
                    portalConnectionHandled = false;

                    if (connected)
                    {
                        setState(State::Connected);
                        xEventGroupSetBits(stateEvents_, CONNECTED_BIT | APPLICATION_READY_BIT);
                        monitor_.startIfNeeded();
                        ensureAPState();
                    }
                    else
                    {
                        setState(State::Retry);
                        retryRequested_.store(true, std::memory_order_release);
                    }
                }
                continue;
            }

            if (forcePortalRequested_.exchange(false, std::memory_order_acq_rel))
            {
                if (!(xEventGroupGetBits(stateEvents_) & APPLICATION_READY_BIT))
                {
                    startPortal();
                }
                continue;
            }

            const bool lostConnection = state() == State::Connected && !wifi_.isConnected();
            const bool retry = retryRequested_.exchange(false, std::memory_order_acq_rel) || lostConnection;
            if (retry)
            {
                xEventGroupClearBits(stateEvents_, CONNECTED_BIT);
                if (!runConnectionCycle(true))
                {
                    setState(State::Retry);
                    ewm::utils::delayMilliseconds(10000);
                    retryRequested_.store(true, std::memory_order_release);
                    notifyStateTask();
                }
            }

            ensureAPState();
        }
    }

    void EasyWiFiManager::setState(State newState)
    {
        const State previous = state_.exchange(newState, std::memory_order_acq_rel);
        if (previous != newState)
        {
            EWM_LOG("State: %s -> %s", ewm::utils::stateName(previous), ewm::utils::stateName(newState));
        }
    }

    void EasyWiFiManager::notifyStateTask()
    {
        if (stateTask_) xTaskNotifyGive(stateTask_);
    }

    void EasyWiFiManager::requestRoam()
    {
        retryRequested_.store(true, std::memory_order_release);
        notifyStateTask();
    }

    bool EasyWiFiManager::waitForConnected(TickType_t timeoutTicks) const
    {
        return (xEventGroupWaitBits(stateEvents_, CONNECTED_BIT, pdFALSE, pdTRUE, timeoutTicks) & CONNECTED_BIT) != 0;
    }

    bool EasyWiFiManager::waitForApplicationNetworkReady(TickType_t timeoutTicks) const
    {
        return (xEventGroupWaitBits(stateEvents_, APPLICATION_READY_BIT, pdFALSE, pdTRUE, timeoutTicks) & APPLICATION_READY_BIT) != 0;
    }

    void EasyWiFiManager::setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs, uint32_t internetTimeoutMs)
    {
        monitor_.configure(enabled, checkIntervalMs, internetTimeoutMs);
        if (wifi_.isConnected()) monitor_.startIfNeeded();
    }

    void EasyWiFiManager::onNoConnectivity(std::function<void()> callback, uint32_t delayMs)
    {
        monitor_.setOnNoConnectivity(std::move(callback), delayMs);
    }

    void EasyWiFiManager::setPortalUiConfig(const portal::PortalUiConfig& config)
    {
        portalUiConfigJson_ = config.toJson();
    }

    void EasyWiFiManager::setPortalUiConfigJson(const std::string& json)
    {
        portalUiConfigJson_ = json.empty() ? "{}" : json;
    }
}
