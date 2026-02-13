#include "ewm/Portal/CaptivePortal.hpp"
#include "ewm/Portal/PortalPage.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include "ewm/Utils/WiFiStatus.hpp"
#include "ewm/Utils/Html.hpp"
#include "ewm/Utils/MiniJson.hpp"
#include "ewm/Log.hpp"
#include <WiFi.h>

namespace ewm
{
    CaptivePortal::CaptivePortal(uint16_t port, SemaphoreHandle_t wifiMutex)
        : port_(port), server_(port), wifiMutex_(wifiMutex) {}

    void CaptivePortal::setAP(const String &ssid, const String &pass)
    {
        apSsid_ = ssid;
        apPass_ = pass;
    }

    void CaptivePortal::startAP_()
    {
        ewm::utils::WiFiLock lk(wifiMutex_);
        WiFi.mode(WIFI_AP_STA);

        WiFi.softAP(apSsid_.c_str(), (apPass_.length() == 0 ? nullptr : apPass_.c_str()));
        delay(120);

        dns_.start(53, "*", WiFi.softAPIP());
    }

    void CaptivePortal::stopAP_()
    {
        ewm::utils::WiFiLock lk(wifiMutex_);
        dns_.stop();
        WiFi.softAPdisconnect(false);
        delay(80);
        WiFi.mode(WIFI_STA);
    }

    void CaptivePortal::setupWeb_(PortalHooks &hooks)
    {
        auto serveProbeOk = [this]()
        {
            server_.sendHeader("Connection", "close");
            server_.send(200, "text/html",
                         "<!doctype html><meta charset='utf-8'>"
                         "<meta http-equiv='refresh' content='0;url=/'/>"
                         "<title>Captive Portal</title>"
                         "<p>Weiter zur Konfigurationsseite…</p>");
        };

        auto handleRoot = [this, &hooks]()
        {
            String list;
            for (auto &c : hooks.listCreds())
            {
                list += "<li class=\"cred\" data-ssid=\"" + ewm::utils::html_escape(c.ssid) + "\">";
                list += "<b>" + ewm::utils::html_escape(c.ssid) + "</b> <span class=\"pri\">Prio: " + String(c.priority) + "</span>";
                list += "<span class=\"buttons\"><button class=\"up\">▲</button><button class=\"down\">▼</button><button class=\"del danger\">Löschen</button></span>";
                list += "</li>";
            }

            String options = "<option value=\"\">(lade…)</option>";

            String page = ewm::portal::kPortalPage;
            page.replace("__OPTIONS__", options);
            page.replace("__LIST__", list);

            server_.sendHeader("Connection", "close");
            server_.send(200, "text/html; charset=utf-8", page);
        };

        server_.on("/", HTTP_GET, handleRoot);

        server_.on("/scan", HTTP_GET, [this]()
                   {
            ewm::utils::WiFiLock lk(wifiMutex_);

            WiFi.disconnect(false, false);
            delay(80);

            int n = WiFi.scanNetworks(false, true);
            String json = "[";

            for (int i = 0; i < n; ++i)
            {
                if (i) json += ',';
                json += "{\"ssid\":\"" + ewm::utils::json_escape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            }
            json += "]";
            WiFi.scanDelete();

            server_.sendHeader("Cache-Control","no-store");
            server_.sendHeader("Connection","close");
            server_.send(200, "application/json", json); });

        server_.on("/status", HTTP_GET, [this, &hooks]()
                   {
            bool conn = hooks.isStaConnected();
            String ip = conn ? hooks.staIp() : "";
            int rssi  = conn ? hooks.staRssi() : 0;

            uint32_t apOffIn = 0;
            if (conn && apGraceUntil_ > millis()) apOffIn = apGraceUntil_ - millis();

            String json = "{";
            json += "\"connected\":"; json += (conn ? "true":"false"); json += ",";
            json += "\"ip\":\""; json += ip; json += "\",";
            json += "\"rssi\":"; json += String(rssi); json += ",";
            json += "\"ap_off_in\":"; json += String(apOffIn);
            json += "}";

            server_.sendHeader("Cache-Control","no-store");
            server_.sendHeader("Connection","close");
            server_.send(200, "application/json", json); });

        server_.on("/add", HTTP_POST, [this, &hooks]()
                   {
            String body = server_.arg("plain");
            String ssid, pw;
            int pr = 100;

            if (!ewm::utils::json_get_string(body, "ssid", ssid)) { server_.send(400, "text/plain", "SSID fehlt."); return; }
            ewm::utils::json_get_string(body, "password", pw);
            ewm::utils::json_get_int(body, "priority", pr);
            pr = constrain(pr, 0, 254);

            if (hooks.addCred(ssid, pw, (uint8_t)pr))
                server_.send(200, "text/plain", "Hinzugefügt/aktualisiert.");
            else
                server_.send(400, "text/plain", "Fehler (max. 10 oder ungültig)."); });

        server_.on("/del", HTTP_POST, [this, &hooks]()
                   {
            String body = server_.arg("plain");
            String ssid;
            if (!ewm::utils::json_get_string(body, "ssid", ssid)) { server_.send(400, "text/plain", "SSID fehlt."); return; }
            if (hooks.delCred(ssid)) server_.send(200, "text/plain", "Gelöscht.");
            else server_.send(404, "text/plain", "Nicht gefunden."); });

        server_.on("/reorder", HTTP_POST, [this, &hooks]()
                   {
            String body = server_.arg("plain");
            std::vector<String> order;
            if (!ewm::utils::json_get_order_array(body, order)) { server_.send(400, "text/plain", "order[] fehlt/ungültig."); return; }
            hooks.reorder(order);
            server_.send(200, "text/plain", "Gespeichert."); });

        server_.on("/erase", HTTP_POST, [this, &hooks]()
                   {
            hooks.eraseAll();
            server_.send(200, "text/plain", "Alle Einträge gelöscht."); });

        server_.on("/ap_off", HTTP_POST, [this]()
                   {
            apGraceUntil_ = millis();
            running_ = false;
            server_.send(200, "application/json", "{\"ok\":true}"); });

        server_.on("/connect", HTTP_POST, [this, &hooks]()
                   {
            String body = server_.arg("plain");
            String ssid, pw;
            int pr = 100;

            if (!ewm::utils::json_get_string(body, "ssid", ssid)) { server_.send(400, "text/plain", "SSID fehlt."); return; }
            ewm::utils::json_get_string(body, "password", pw);
            ewm::utils::json_get_int(body, "priority", pr);
            pr = constrain(pr, 0, 254);

            hooks.connectRequest(ssid, pw, (uint8_t)pr);

            server_.send(202, "application/json", "{\"ok\":true,\"msg\":\"Verbinde...\"}"); });

        // captive portal endpoints
        server_.on("/generate_204", HTTP_ANY, serveProbeOk);
        server_.on("/gen_204", HTTP_ANY, serveProbeOk);
        server_.on("/hotspot-detect.html", HTTP_ANY, serveProbeOk);
        server_.on("/library/test/success.html", HTTP_ANY, serveProbeOk);
        server_.on("/ncsi.txt", HTTP_ANY, serveProbeOk);
        server_.on("/connecttest.txt", HTTP_ANY, serveProbeOk);
        server_.on("/success.txt", HTTP_ANY, serveProbeOk);
        server_.on("/favicon.ico", HTTP_ANY, []() {});

        server_.onNotFound(handleRoot);
        server_.begin();
    }

    void CaptivePortal::runBlocking(PortalHooks hooks)
    {
        apGraceUntil_ = 0;

        startAP_();
        setupWeb_(hooks);
        running_ = true;

        uint32_t lastLog = millis();

        while (running_)
        {
            dns_.processNextRequest();
            server_.handleClient();
            delay(4);

            if ((millis() - lastLog) > 2000)
            {
                lastLog = millis();
                EWM_LOG("Portal loop: STA status=%s connected=%d",
                        ewm::utils::wlStatusStr(WiFi.status()), hooks.isStaConnected());
            }

            if (hooks.isStaConnected())
            {
                if (apGraceUntil_ == 0)
                {
                    // 1x beim ersten Connect
                    if (hooks.onStaConnected)
                        hooks.onStaConnected();

                    apGraceUntil_ = millis() + apGraceMs_;
                    EWM_LOG("STA connected -> AP will stop in %ums", (unsigned)apGraceMs_);
                }

                if ((int32_t)(millis() - apGraceUntil_) >= 0)
                    running_ = false;
            }
        }

        stopAP_();
        EWM_LOG("Portal stopped");
    }

}
