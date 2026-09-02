#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "esp_http_server.h"

#include "ewm/Portal/CaptiveDns.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"
#include "ewm/Types.hpp"
#include "ewm/WiFi/WiFiConnector.hpp"

namespace ewm
{
    struct PortalHooks
    {
        std::function<std::vector<Credential>()> listCreds;
        std::function<bool(const std::string&, const std::string&, uint8_t)> addCred;
        std::function<bool(const std::string&)> delCred;
        std::function<void(const std::vector<std::string>&)> reorder;
        std::function<void()> eraseAll;
        std::function<void(const std::string&, const std::string&, uint8_t)> connectRequest;
        std::function<void()> onStopRequested;
        std::function<std::vector<ScanResult>()> scanNetworks;
        std::function<bool()> isStaConnected;
        std::function<std::string()> staIp;
        std::function<int()> staRssi;
    };

    class CaptivePortal
    {
    public:
        CaptivePortal(uint16_t port, WiFiConnector& wifi);

        void setAP(const std::string& ssid, const std::string& pass);
        void setGraceMs(uint32_t ms) { apGraceMs_ = ms; }
        void setUiConfigJson(const std::string& json);

        bool start(PortalHooks hooks);
        void stop(bool stopAccessPoint = true);
        void markStaConnected();
        void clearStaConnected();

        bool running() const noexcept { return running_.load(std::memory_order_acquire); }
        bool graceExpired() const noexcept;
        uint32_t apOffInMs() const noexcept;

    private:
        static esp_err_t rootHandler(httpd_req_t* request);
        static esp_err_t scanHandler(httpd_req_t* request);
        static esp_err_t statusHandler(httpd_req_t* request);
        static esp_err_t addHandler(httpd_req_t* request);
        static esp_err_t deleteHandler(httpd_req_t* request);
        static esp_err_t reorderHandler(httpd_req_t* request);
        static esp_err_t eraseHandler(httpd_req_t* request);
        static esp_err_t connectHandler(httpd_req_t* request);
        static esp_err_t accessPointOffHandler(httpd_req_t* request);
        static esp_err_t optionsHandler(httpd_req_t* request);
        static esp_err_t probeHandler(httpd_req_t* request);
        static esp_err_t faviconHandler(httpd_req_t* request);
        static esp_err_t notFoundHandler(httpd_req_t* request, httpd_err_code_t error);

        bool startAccessPoint();
        bool startWebServer();
        bool registerUri(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*));
        bool csrfValid(httpd_req_t* request) const;
        bool requireCsrf(httpd_req_t* request) const;
        bool readBody(httpd_req_t* request, std::string& body) const;
        void setCommonHeaders(httpd_req_t* request) const;
        esp_err_t send(httpd_req_t* request, const char* status, const char* type, const std::string& body) const;
        void generateCsrfToken();

        uint16_t port_{80};
        WiFiConnector& wifi_;
        CaptiveDns dns_{};
        httpd_handle_t server_{nullptr};
        PortalHooks hooks_{};

        std::string apSsid_{"ESP32-Setup"};
        std::string apPass_{};
        std::string uiCfgJson_{"{}"};
        std::string csrfToken_{};

        std::atomic<bool> running_{false};
        uint32_t apGraceMs_{15000};
        std::atomic<uint64_t> apGraceUntil_{0};
    };
}
