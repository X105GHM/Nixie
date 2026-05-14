#include "ewm/Portal/CaptivePortal.hpp"
#include "ewm/Portal/PortalPage.hpp"
#include "ewm/Utils/WiFiLock.hpp"
#include "ewm/Utils/WiFiStatus.hpp"
#include "ewm/Utils/Html.hpp"
#include "ewm/Utils/MiniJson.hpp"
#include "ewm/Log.hpp"
#include <WiFi.h>
#include <esp_system.h>

namespace ewm
{
    CaptivePortal::CaptivePortal(uint16_t port, SemaphoreHandle_t wifiMutex)
        : port_(port), server_(port), wifiMutex_(wifiMutex) {}

    void CaptivePortal::setAP(const String &ssid, const String &pass)
    {
        apSsid_ = ssid;
        apPass_ = pass;
    }

    void CaptivePortal::setUiConfigJson(const String& json)
    {
        String t = json;
        t.trim();
        if (!t.startsWith("{") || !t.endsWith("}")) t = "{}";
            uiCfgJson_ = t;
    }

    void CaptivePortal::sendCommonHeaders_()
    {
        server_.sendHeader("Cache-Control", "no-store");
        server_.sendHeader("Connection", "close");
    }

    void CaptivePortal::generateCsrfToken_()
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "%08lx%08lx",
                 static_cast<unsigned long>(esp_random()),
                 static_cast<unsigned long>(esp_random()));
        csrfToken_ = buf;
    }

    bool CaptivePortal::isCsrfValid_()
    {
        return csrfToken_.length() > 0 && server_.header("X-EWM-CSRF") == csrfToken_;
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
        static const char *headerKeys[] = {"X-EWM-CSRF"};
        server_.collectHeaders(headerKeys, 1);

        auto addCors = [this]()
        {
            sendCommonHeaders_();
        };

        auto requireCsrf = [this, addCors]() -> bool
        {
            if (isCsrfValid_())
                return true;

            addCors();
            server_.send(403, "text/plain", "CSRF token invalid.");
            return false;
        };

        auto handleOptions = [this, addCors]()
        {
            addCors();
            server_.send(204, "text/plain", "");
        };

        auto serveProbeOk = [this, addCors]()
        {
            addCors();
            server_.send(200, "text/html",
                         "<!doctype html><meta charset='utf-8'>"
                         "<meta http-equiv='refresh' content='0;url=/'/>"
                         "<title>Captive Portal</title>"
                         "<p>Weiter zur Konfigurationsseite…</p>");
        };

        auto handleRoot = [this, &hooks, addCors]()
        {
            String list;

            for (auto &c : hooks.listCreds())
            {
                list += "<li class=\"cred\" draggable=\"true\" data-ssid=\"" + ewm::utils::html_escape(c.ssid) + "\">";
                list += "<b>" + ewm::utils::html_escape(c.ssid) + "</b> <span class=\"pri\">Prio: " + String(c.priority) + "</span>";
                list += "<span class=\"buttons\">"
                        "<button class=\"con primary\">Verbinden</button>"
                        "<button class=\"del danger\">Löschen</button>"
                        "</span>";
                list += "</li>";
            }

            uint8_t nextPrio = 0;
            for (;;)
            {
                bool used = false;
                for (auto &c : hooks.listCreds())
                {
                    if (c.priority == nextPrio)
                    {
                        used = true;
                        break;
                    }
                }
                if (!used)
                    break;
                nextPrio++;
            }

            String options = "<option value=\"\">(lade…)</option>";

            String page = ewm::portal::kPortalPage;
            page.replace("__EWM_PORTAL_CFG__", uiCfgJson_);
            page.replace("__CSRF_TOKEN__", ewm::utils::json_escape(csrfToken_));
            page.replace("__OPTIONS__", options);
            page.replace("__LIST__", list);
            page.replace("__DEFAULT_PRIO__", String(nextPrio));

            addCors();
            server_.sendHeader("Cache-Control", "no-store");
            server_.sendHeader("Connection", "close");
            server_.send(200, "text/html; charset=utf-8", page);
        };

        server_.on("/", HTTP_GET, handleRoot);
        server_.on("/", HTTP_OPTIONS, handleOptions);

        server_.on("/scan", HTTP_GET, [this, addCors]()
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

            addCors();
            server_.sendHeader("Cache-Control", "no-store");
            server_.sendHeader("Connection", "close");
            server_.send(200, "application/json", json); 
        });
        
        server_.on("/scan", HTTP_OPTIONS, handleOptions);

        server_.on("/status", HTTP_GET, [this, &hooks, addCors]()
        {
            bool conn = false;
            String ip = "";
            int rssi = -127;

            {
                ewm::utils::WiFiLock lk(wifiMutex_);
                conn = (WiFi.status() == WL_CONNECTED);
                if (conn) 
                {
                    ip = WiFi.localIP().toString();
                    rssi = WiFi.RSSI();
                }
            }

            if (!conn) conn = hooks.isStaConnected();
            if (ip.length() == 0 && conn) ip = hooks.staIp();
            if ((rssi == -127 || rssi == 0) && conn) rssi = hooks.staRssi();

            uint32_t apOffIn = 0;
            if (conn && apGraceUntil_ > millis()) apOffIn = apGraceUntil_ - millis();

            String json = "{";
            json += "\"connected\":"; json += (conn ? "true":"false"); json += ",";
            json += "\"ip\":\""; json += ip; json += "\",";
            json += "\"rssi\":"; json += String(rssi); json += ",";
            json += "\"ap_off_in\":"; json += String(apOffIn);
            json += "}";

            addCors();
            server_.sendHeader("Cache-Control","no-store, no-cache, must-revalidate");
            server_.sendHeader("Pragma","no-cache");
            server_.sendHeader("Expires","0");
            server_.sendHeader("Connection","close");
            server_.send(200, "application/json", json); 
        });

        server_.on("/status", HTTP_OPTIONS, handleOptions);

        server_.on("/add", HTTP_POST, [this, &hooks, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            String body = server_.arg("plain");
            String ssid, pw;
            int pr = 100;

            if (!ewm::utils::json_get_string(body, "ssid", ssid))
            {
                addCors();
                server_.send(400, "text/plain", "SSID fehlt.");
                return;
            }

            ewm::utils::json_get_string(body, "password", pw);
            ewm::utils::json_get_int(body, "priority", pr);
            pr = constrain(pr, 0, 254);

            addCors();
            server_.sendHeader("Connection", "close");

            if (hooks.addCred(ssid, pw, (uint8_t)pr))
            server_.send(200, "text/plain", "Hinzugefügt/aktualisiert.");
            else
            server_.send(400, "text/plain", "Fehler (max. 10 oder ungültig)."); 
        });

        server_.on("/add", HTTP_OPTIONS, handleOptions);

        server_.on("/del", HTTP_POST, [this, &hooks, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            String body = server_.arg("plain");
            String ssid;

            if (!ewm::utils::json_get_string(body, "ssid", ssid))
            {
                addCors();
                server_.send(400, "text/plain", "SSID fehlt.");
                return;
            }

            addCors();
            server_.sendHeader("Connection", "close");

            if (hooks.delCred(ssid))
                server_.send(200, "text/plain", "Gelöscht.");
            else
                server_.send(404, "text/plain", "Nicht gefunden."); 
        });

        server_.on("/del", HTTP_OPTIONS, handleOptions);

        server_.on("/reorder", HTTP_POST, [this, &hooks, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            String body = server_.arg("plain");
            std::vector<String> order;

            if (!ewm::utils::json_get_order_array(body, order))
            {
                addCors();
                server_.send(400, "text/plain", "order[] fehlt/ungültig.");
                return;
            }

            hooks.reorder(order);

            addCors();
            server_.sendHeader("Connection", "close");
            server_.send(200, "text/plain", "Gespeichert."); 
        });

        server_.on("/reorder", HTTP_OPTIONS, handleOptions);

        server_.on("/erase", HTTP_POST, [this, &hooks, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            hooks.eraseAll();
            addCors();
            server_.sendHeader("Connection", "close");
            server_.send(200, "text/plain", "Alle Einträge gelöscht."); 
        });

        server_.on("/erase", HTTP_OPTIONS, handleOptions);

        server_.on("/ap_off", HTTP_POST, [this, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            apGraceUntil_ = millis();
            running_ = false;
            addCors();
            server_.sendHeader("Connection", "close");
            server_.send(200, "application/json", "{\"ok\":true}");
        });

        server_.on("/ap_off", HTTP_OPTIONS, handleOptions);

        server_.on("/connect", HTTP_POST, [this, &hooks, addCors, requireCsrf]()
        {
            if(!requireCsrf()) return;

            String body = server_.arg("plain");
            String ssid, pw;
            int pr = 100;

            if (!ewm::utils::json_get_string(body, "ssid", ssid))
            {
                addCors();
                server_.send(400, "text/plain", "SSID fehlt.");
                return;
            }

            ewm::utils::json_get_string(body, "password", pw);
            ewm::utils::json_get_int(body, "priority", pr);
            pr = constrain(pr, 0, 254);

            hooks.connectRequest(ssid, pw, (uint8_t)pr);

            addCors();
            server_.sendHeader("Connection", "close");
            server_.send(202, "application/json", "{\"ok\":true,\"msg\":\"Verbinde...\"}"); 
        });

        server_.on("/connect", HTTP_OPTIONS, handleOptions);

        // --- Captive portal endpoints
        server_.on("/generate_204", HTTP_ANY, serveProbeOk);
        server_.on("/gen_204", HTTP_ANY, serveProbeOk);
        server_.on("/hotspot-detect.html", HTTP_ANY, serveProbeOk);
        server_.on("/library/test/success.html", HTTP_ANY, serveProbeOk);
        server_.on("/ncsi.txt", HTTP_ANY, serveProbeOk);
        server_.on("/connecttest.txt", HTTP_ANY, serveProbeOk);
        server_.on("/success.txt", HTTP_ANY, serveProbeOk);

        server_.on("/favicon.ico", HTTP_ANY, [this, addCors]()
        {
            addCors();
            server_.sendHeader("Connection", "close");
            server_.send(204, "text/plain", ""); 
        });

        server_.onNotFound([this, addCors, handleRoot]()
        {
            EWM_LOG("HTTP notfound: method=%d uri=%s", (int)server_.method(), server_.uri().c_str());

            if (server_.method() == HTTP_OPTIONS)
            {
                addCors();
                server_.sendHeader("Connection", "close");
                server_.send(204, "text/plain", "");
                return;
            }

            handleRoot(); 
        });

        server_.begin();
    }

    void CaptivePortal::runBlocking(PortalHooks hooks)
    {
        apGraceUntil_ = 0;

        generateCsrfToken_();

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
        server_.stop();
        EWM_LOG("Portal stopped");
    }
}