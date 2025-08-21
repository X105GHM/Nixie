#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>
#include <array>
#include <functional>

namespace ewm
{

    static constexpr uint32_t MAGIC = 0x45574D31u;

    struct Credential
    {
        char ssid[33]       = {0};
        char password[65]   = {0};
        uint8_t priority    = 0;
        uint32_t last_ok    = 0;
    };

    struct StorageHeader
    {
        uint32_t magic      = MAGIC;    // Library-ID
        uint16_t version    = 0x0001;   // Strukturversion
        uint16_t count      = 0;        // Anzahl gültiger Einträge
        uint32_t crc32      = 0;        // CRC über Header (ohne crc32) + Daten
    };

    class EasyWiFiManager
    {
    public:
        using ConnectCallback = std::function<void(const IPAddress &ip)>;

        static EasyWiFiManager &instance();

        void setHostname(const String &name);
        void setAPCredentials(const String &apSsid, const String &apPass = "");

        void begin(uint32_t connectTimeoutMs = 12000, uint32_t betweenRetryMs = 1500);

        void startConfigPortal();

        bool addCredential(const String &ssid, const String &password, uint8_t priority = 254);
        bool eraseAll();

        wl_status_t status() const { return WiFi.status(); }
        std::vector<Credential> listCredentials() const;

        void onConnect(ConnectCallback cb) { onConnect_ = std::move(cb); }

        void setBackgroundAP(bool enabled);
        void setConnectivityMonitor(bool enabled, uint32_t checkIntervalMs = 10000, uint32_t internetTimeoutMs = 15000);
        void setInternetProbe(const char *host, uint16_t port = 53);
        void onNoConnectivity(std::function<void()> cb, uint32_t delayMs = 300000);

    private:
        EasyWiFiManager();

        bool load();
        bool save();
        bool saveToNamespace(const char *ns, const StorageHeader &hdr, const std::array<Credential, 10> &data);
        bool loadFromNamespace(const char *ns, StorageHeader &hdr, std::array<Credential, 10> &data);
        void tryRecover();
        static uint32_t crc32(const uint8_t *data, size_t len);

        bool tryConnectAll(uint32_t connectTimeoutMs, uint32_t betweenRetryMs);
        bool tryConnectOne(const Credential &c, uint32_t connectTimeoutMs);

        void startAP();
        void stopAP();
        void setupWeb();
        void loopWeb();

        void ensureAPState();
        static void monitorTaskThunk(void *arg);
        void monitorTaskLoop();
        bool hasInternet(uint32_t timeoutMs);
        void roamTryAll();

    private:
        String hostname_    = "esp32-setup";
        String apSsid_      = "ESP32-Setup";
        String apPass_      = ""; // optional

        Preferences pref_;
        static constexpr const char *NS_PRIMARY = "EWM1";
        static constexpr const char *NS_BACKUP  = "EWM1B";

        StorageHeader hdr_{};
        std::array<Credential, 10> creds_{};

        DNSServer dns_;
        WebServer server_{80};
        bool portalRunning_ = false;

        ConnectCallback onConnect_;

        bool backgroundAP_ = false;
        bool monitorEnabled_ = false;
        uint32_t checkIntervalMs_ = 10000;
        uint32_t internetTimeoutMs_ = 15000;
        const char *probeHost_ = "1.1.1.1";
        uint16_t probePort_ = 53;
        std::function<void()> onNoConn_;
        uint32_t noConnSince_ = 0;
        uint32_t noConnCallbackDelayMs_ = 300000;
        TaskHandle_t monitorTask_ = nullptr;
    };
}