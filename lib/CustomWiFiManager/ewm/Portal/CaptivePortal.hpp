#pragma once
#include <WebServer.h>
#include <DNSServer.h>
#include <functional>
#include <vector>
#include "ewm/Types.hpp"
#include "ewm/Portal/PortalUiConfig.hpp"

namespace ewm
{
    struct PortalHooks
    {
        std::function<std::vector<Credential>()> listCreds;
        std::function<bool(const String&, const String&, uint8_t)> addCred;
        std::function<bool(const String&)> delCred;
        std::function<void(const std::vector<String>&)> reorder;
        std::function<void()> eraseAll;
        std::function<void(const String&, const String&, uint8_t)> connectRequest;
        std::function<void()> onStaConnected;
        std::function<bool()> isStaConnected;
        std::function<String()> staIp;
        std::function<int()> staRssi;
    };

    class CaptivePortal
    {
    public:
        CaptivePortal(uint16_t port, SemaphoreHandle_t wifiMutex);

        void setAP(const String& ssid, const String& pass);
        void setGraceMs(uint32_t ms) { apGraceMs_ = ms; }

        void runBlocking(PortalHooks hooks);

        void setUiConfigJson(const String& json);

        bool running() const { return running_; }

    private:
        void startAP_();
        void stopAP_();

        void setupWeb_(PortalHooks& hooks);

        void sendCommonHeaders_();
        void generateCsrfToken_();
        bool isCsrfValid_();

        uint16_t port_{80};
        DNSServer dns_{};
        WebServer server_;
        SemaphoreHandle_t wifiMutex_{nullptr};

        String apSsid_{"ESP32-Setup"};
        String apPass_{""};
        String uiCfgJson_{"{}"};
        String csrfToken_{};

        ewm::portal::PortalUiConfig uiCfg_;

        bool running_{false};

        uint32_t apGraceMs_{15000};
        uint32_t apGraceUntil_{0};
    };
}
