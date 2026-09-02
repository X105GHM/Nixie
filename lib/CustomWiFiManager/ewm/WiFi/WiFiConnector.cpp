#include "ewm/WiFi/WiFiConnector.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "esp_idf_version.h"
#include "ewm/Log.hpp"
#include "ewm/Utils/Time.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include <fcntl.h>
#include "lwip/netdb.h"
#include "lwip/sockets.h"

namespace
{
    bool isAcceptableInitResult(esp_err_t result)
    {
        return result == ESP_OK || result == ESP_ERR_INVALID_STATE;
    }

    bool modeHasAccessPoint(wifi_mode_t mode)
    {
        return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
    }
}

namespace ewm
{
    WiFiConnector::WiFiConnector(SemaphoreHandle_t wifiMutex)
        : wifiMutex_(wifiMutex)
    {
    }

    bool WiFiConnector::initSta(const std::string& hostname)
    {
        if (initialized_.load(std::memory_order_acquire))
        {
            return true;
        }

        if (!isAcceptableInitResult(esp_netif_init()) ||
            !isAcceptableInitResult(esp_event_loop_create_default()))
        {
            EWM_LOG("Native network initialization failed");
            return false;
        }

        staNetif_ = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!staNetif_)
        {
            staNetif_ = esp_netif_create_default_wifi_sta();
        }
        apNetif_ = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!apNetif_)
        {
            apNetif_ = esp_netif_create_default_wifi_ap();
        }
        if (!staNetif_ || !apNetif_)
        {
            EWM_LOG("Default WiFi netif creation failed");
            return false;
        }

        wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
        const esp_err_t initResult = esp_wifi_init(&initConfig);
        if (initResult != ESP_OK)
        {
            EWM_LOG("esp_wifi_init failed: %s", esp_err_to_name(initResult));
            return false;
        }

        connectionEvents_ = xEventGroupCreate();
        if (!connectionEvents_)
        {
            EWM_LOG("WiFi event group creation failed");
            return false;
        }

        esp_err_t result = esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &WiFiConnector::eventHandler,
            this,
            &wifiEventInstance_);
        if (result == ESP_OK)
        {
            result = esp_event_handler_instance_register(
                IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &WiFiConnector::eventHandler,
                this,
                &ipEventInstance_);
        }
        if (result != ESP_OK)
        {
            EWM_LOG("WiFi event registration failed: %s", esp_err_to_name(result));
            return false;
        }

        if (!hostname.empty())
        {
            result = esp_netif_set_hostname(staNetif_, hostname.c_str());
            if (result != ESP_OK)
            {
                EWM_LOG("STA hostname configuration failed: %s", esp_err_to_name(result));
            }
        }

        result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
        if (result == ESP_OK) result = esp_wifi_set_mode(WIFI_MODE_STA);
        if (result == ESP_OK) result = esp_wifi_set_ps(WIFI_PS_NONE);
        if (result == ESP_OK) result = esp_wifi_start();
        if (result != ESP_OK)
        {
            EWM_LOG("Native WiFi start failed: %s", esp_err_to_name(result));
            return false;
        }

        initialized_.store(true, std::memory_order_release);
        return true;
    }

    void WiFiConnector::setEventCallback(EventCallback callback)
    {
        eventCallback_ = std::move(callback);
    }

    bool WiFiConnector::safeSwitchMode(wifi_mode_t targetMode)
    {
        if (!isInitialized())
        {
            return false;
        }

        ewm::utils::WiFiLock lock(wifiMutex_);
        wifi_mode_t currentMode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&currentMode) != ESP_OK)
        {
            return false;
        }
        if (currentMode == targetMode)
        {
            return true;
        }
        return esp_wifi_set_mode(targetMode) == ESP_OK;
    }

    std::vector<ScanResult> WiFiConnector::scanNetworks()
    {
        std::vector<ScanResult> result;
        if (!isInitialized())
        {
            return result;
        }

        ewm::utils::WiFiLock lock(wifiMutex_);
        const wifi_mode_t currentMode = mode();
        if (currentMode == WIFI_MODE_AP)
        {
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return result;
        }
        else if (currentMode == WIFI_MODE_NULL)
        {
            if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) return result;
        }

        wifi_scan_config_t scanConfig{};
        scanConfig.show_hidden = true;
        if (esp_wifi_scan_start(&scanConfig, true) != ESP_OK)
        {
            return result;
        }

        uint16_t count = 0;
        if (esp_wifi_scan_get_ap_num(&count) != ESP_OK || count == 0)
        {
            return result;
        }

        std::vector<wifi_ap_record_t> records(count);
        if (esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK)
        {
            return result;
        }

        result.reserve(count);
        for (uint16_t index = 0; index < count; ++index)
        {
            const auto* ssid = reinterpret_cast<const char*>(records[index].ssid);
            if (!ssid || ssid[0] == '\0')
            {
                continue;
            }

            const auto duplicate = std::find_if(result.begin(), result.end(), [ssid](const ScanResult& entry)
            {
                return entry.ssid == ssid;
            });
            if (duplicate == result.end())
            {
                result.push_back({ssid, records[index].rssi});
            }
        }
        return result;
    }

    std::vector<std::string> WiFiConnector::scanVisibleUnique()
    {
        const auto networks = scanNetworks();
        std::vector<std::string> result;
        result.reserve(networks.size());
        for (const auto& network : networks)
        {
            result.push_back(network.ssid);
        }
        return result;
    }

    bool WiFiConnector::beginConnection(const char* ssid, const char* pass, bool preserveAccessPoint)
    {
        if (!isInitialized() || !ssid || ssid[0] == '\0')
        {
            return false;
        }

        ewm::utils::WiFiLock lock(wifiMutex_);
        const wifi_mode_t targetMode = preserveAccessPoint ? WIFI_MODE_APSTA : WIFI_MODE_STA;
        if (esp_wifi_set_mode(targetMode) != ESP_OK)
        {
            return false;
        }

        suppressDisconnectEvent_.store(true, std::memory_order_release);
        esp_wifi_disconnect();
        connected_.store(false, std::memory_order_release);
        ewm::utils::delayMilliseconds(50);

        wifi_config_t stationConfig{};
        std::strncpy(reinterpret_cast<char*>(stationConfig.sta.ssid), ssid, sizeof(stationConfig.sta.ssid) - 1);
        if (pass)
        {
            std::strncpy(reinterpret_cast<char*>(stationConfig.sta.password), pass, sizeof(stationConfig.sta.password) - 1);
        }
        stationConfig.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        stationConfig.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        stationConfig.sta.threshold.authmode = WIFI_AUTH_OPEN;
        stationConfig.sta.pmf_cfg.capable = true;
        stationConfig.sta.pmf_cfg.required = false;

        connected_.store(false, std::memory_order_release);
        xEventGroupClearBits(connectionEvents_, CONNECTED_BIT | FAILED_BIT);

        esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &stationConfig);
        if (result == ESP_OK)
        {
            suppressDisconnectEvent_.store(false, std::memory_order_release);
            result = esp_wifi_connect();
        }
        else
        {
            suppressDisconnectEvent_.store(false, std::memory_order_release);
        }
        if (result != ESP_OK)
        {
            EWM_LOG("STA connection start failed: %s", esp_err_to_name(result));
            return false;
        }
        return true;
    }

    bool WiFiConnector::waitForConnectedOrFail(uint32_t timeoutMs)
    {
        const EventBits_t bits = xEventGroupWaitBits(
            connectionEvents_,
            CONNECTED_BIT | FAILED_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(timeoutMs));
        return (bits & CONNECTED_BIT) != 0 && isConnected();
    }

    bool WiFiConnector::hasInternet(const char* host, uint16_t port, uint32_t timeoutMs) const
    {
        if (!isConnected() || !host || host[0] == '\0')
        {
            return false;
        }

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* addresses = nullptr;
        const std::string service = std::to_string(port);
        if (getaddrinfo(host, service.c_str(), &hints, &addresses) != 0)
        {
            return false;
        }

        bool connected = false;
        for (addrinfo* address = addresses; address && !connected; address = address->ai_next)
        {
            const int socketFd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socketFd < 0)
            {
                continue;
            }

            const int originalFlags = fcntl(socketFd, F_GETFL, 0);
            fcntl(socketFd, F_SETFL, originalFlags | O_NONBLOCK);
            const int result = connect(socketFd, address->ai_addr, address->ai_addrlen);
            if (result == 0)
            {
                connected = true;
            }
            else if (errno == EINPROGRESS)
            {
                fd_set writeSet;
                FD_ZERO(&writeSet);
                FD_SET(socketFd, &writeSet);
                timeval timeout{};
                timeout.tv_sec = timeoutMs / 1000;
                timeout.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000) * 1000);
                if (select(socketFd + 1, nullptr, &writeSet, nullptr, &timeout) > 0)
                {
                    int socketError = 0;
                    socklen_t length = sizeof(socketError);
                    connected = getsockopt(socketFd, SOL_SOCKET, SO_ERROR, &socketError, &length) == 0 && socketError == 0;
                }
            }
            close(socketFd);
        }

        freeaddrinfo(addresses);
        return connected;
    }

    bool WiFiConnector::tryConnectOneWithInternet(
        const Credential& credential,
        uint32_t connectTimeoutMs,
        bool requireInternet,
        const char* probeHost,
        uint16_t probePort,
        uint32_t internetTimeoutMs)
    {
        if (credential.ssid[0] == '\0')
        {
            return false;
        }

        EWM_LOG("Trying saved network (priority=%u)", static_cast<unsigned>(credential.priority));
        if (!beginConnection(credential.ssid, credential.password, false) ||
            !waitForConnectedOrFail(connectTimeoutMs))
        {
            disconnect();
            return false;
        }

        if (requireInternet && !hasInternet(probeHost, probePort, internetTimeoutMs))
        {
            EWM_LOG("Internet probe failed");
            disconnect();
            return false;
        }
        return true;
    }

    bool WiFiConnector::tryConnectAll(
        const std::vector<Credential>& credentials,
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
        order.reserve(credentials.size());

        for (const auto& credential : credentials)
        {
            const bool isVisible = std::find(visible.begin(), visible.end(), credential.ssid) != visible.end();
            if (isVisible)
            {
                order.push_back(credential);
            }
        }
        if (order.empty())
        {
            order = credentials;
        }

        std::sort(order.begin(), order.end(), [](const Credential& left, const Credential& right)
        {
            if (left.priority != right.priority) return left.priority < right.priority;
            return left.last_ok > right.last_ok;
        });

        EWM_LOG("Connection cycle with %u candidate(s)", static_cast<unsigned>(order.size()));
        for (const auto& credential : order)
        {
            if (tryConnectOneWithInternet(
                    credential,
                    connectTimeoutMs,
                    requireInternet,
                    probeHost,
                    probePort,
                    internetTimeoutMs))
            {
                if (onSuccess) onSuccess(credential);
                return true;
            }
            ewm::utils::delayMilliseconds(betweenRetryMs);
        }
        return false;
    }

    bool WiFiConnector::beginConnectAsync(const std::string& ssid, const std::string& pass)
    {
        return beginConnection(ssid.c_str(), pass.c_str(), modeHasAccessPoint(mode()));
    }

    void WiFiConnector::disconnect()
    {
        if (!isInitialized()) return;
        ewm::utils::WiFiLock lock(wifiMutex_);
        suppressDisconnectEvent_.store(true, std::memory_order_release);
        esp_wifi_disconnect();
        connected_.store(false, std::memory_order_release);
        ewm::utils::delayMilliseconds(30);
        suppressDisconnectEvent_.store(false, std::memory_order_release);
    }

    bool WiFiConnector::startAccessPoint(const std::string& ssid, const std::string& pass)
    {
        if (!isInitialized() || ssid.empty() || ssid.size() > 32 || pass.size() > 63 || (!pass.empty() && pass.size() < 8))
        {
            return false;
        }

        ewm::utils::WiFiLock lock(wifiMutex_);
        wifi_config_t accessPointConfig{};
        std::strncpy(reinterpret_cast<char*>(accessPointConfig.ap.ssid), ssid.c_str(), sizeof(accessPointConfig.ap.ssid) - 1);
        std::strncpy(reinterpret_cast<char*>(accessPointConfig.ap.password), pass.c_str(), sizeof(accessPointConfig.ap.password) - 1);
        accessPointConfig.ap.ssid_len = static_cast<uint8_t>(ssid.size());
        accessPointConfig.ap.channel = 1;
        accessPointConfig.ap.max_connection = 4;
        accessPointConfig.ap.authmode = pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        accessPointConfig.ap.pmf_cfg.capable = true;
        accessPointConfig.ap.pmf_cfg.required = false;
#endif

        esp_err_t result = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (result == ESP_OK)
        {
            result = esp_wifi_set_config(WIFI_IF_AP, &accessPointConfig);
        }
        if (result != ESP_OK)
        {
            EWM_LOG("Access point start failed: %s", esp_err_to_name(result));
        }
        return result == ESP_OK;
    }

    bool WiFiConnector::stopAccessPoint()
    {
        if (!isInitialized()) return false;
        ewm::utils::WiFiLock lock(wifiMutex_);
        return esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK;
    }

    std::string WiFiConnector::staIpAddress() const
    {
        if (!staNetif_ || !isConnected()) return {};
        esp_netif_ip_info_t info{};
        char buffer[16]{};
        if (esp_netif_get_ip_info(staNetif_, &info) != ESP_OK || !esp_ip4addr_ntoa(&info.ip, buffer, sizeof(buffer)))
        {
            return {};
        }
        return buffer;
    }

    std::string WiFiConnector::apIpAddress() const
    {
        if (!apNetif_ || !modeHasAccessPoint(mode())) return {};
        esp_netif_ip_info_t info{};
        char buffer[16]{};
        if (esp_netif_get_ip_info(apNetif_, &info) != ESP_OK || !esp_ip4addr_ntoa(&info.ip, buffer, sizeof(buffer)))
        {
            return {};
        }
        return buffer;
    }

    std::string WiFiConnector::connectedSsid() const
    {
        if (!isConnected()) return {};
        wifi_ap_record_t record{};
        if (esp_wifi_sta_get_ap_info(&record) != ESP_OK) return {};
        return reinterpret_cast<const char*>(record.ssid);
    }

    int WiFiConnector::rssi() const
    {
        if (!isConnected()) return -127;
        wifi_ap_record_t record{};
        return esp_wifi_sta_get_ap_info(&record) == ESP_OK ? record.rssi : -127;
    }

    wifi_mode_t WiFiConnector::mode() const
    {
        wifi_mode_t currentMode = WIFI_MODE_NULL;
        if (isInitialized()) esp_wifi_get_mode(&currentMode);
        return currentMode;
    }

    void WiFiConnector::eventHandler(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData)
    {
        auto* self = static_cast<WiFiConnector*>(arg);
        if (self) self->handleEvent(eventBase, eventId, eventData);
    }

    void WiFiConnector::handleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData)
    {
        if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED)
        {
            const auto* disconnected = static_cast<wifi_event_sta_disconnected_t*>(eventData);
            lastDisconnectReason_.store(disconnected ? disconnected->reason : 0, std::memory_order_release);
            connected_.store(false, std::memory_order_release);
            if (!suppressDisconnectEvent_.load(std::memory_order_acquire))
            {
                xEventGroupSetBits(connectionEvents_, FAILED_BIT);
                notifyEvent();
            }
        }
        else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP)
        {
            connected_.store(true, std::memory_order_release);
            xEventGroupSetBits(connectionEvents_, CONNECTED_BIT);
            notifyEvent();
        }
    }

    void WiFiConnector::notifyEvent()
    {
        if (eventCallback_) eventCallback_();
    }
}
