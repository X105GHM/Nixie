#include "ewm/Portal/CaptivePortal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_system.h"
#include "ewm/Log.hpp"
#include "ewm/Portal/PortalPage.hpp"
#include "ewm/Utils/Html.hpp"
#include "ewm/Utils/MiniJson.hpp"
#include "ewm/Utils/Time.hpp"
#include "lwip/inet.h"

namespace
{
    constexpr size_t MAX_REQUEST_BODY = 2048;

    void replaceAll(std::string& text, const std::string& marker, const std::string& replacement)
    {
        size_t position = 0;
        while ((position = text.find(marker, position)) != std::string::npos)
        {
            text.replace(position, marker.size(), replacement);
            position += replacement.size();
        }
    }

    ewm::CaptivePortal* portalFrom(httpd_req_t* request)
    {
        if (!request) return nullptr;
        if (request->user_ctx) return static_cast<ewm::CaptivePortal*>(request->user_ctx);
        return static_cast<ewm::CaptivePortal*>(httpd_get_global_user_ctx(request->handle));
    }
}

namespace ewm
{
    CaptivePortal::CaptivePortal(uint16_t port, WiFiConnector& wifi)
        : port_(port), wifi_(wifi)
    {
    }

    void CaptivePortal::setAP(const std::string& ssid, const std::string& pass)
    {
        apSsid_ = ssid;
        apPass_ = pass;
    }

    void CaptivePortal::setUiConfigJson(const std::string& json)
    {
        const size_t first = json.find_first_not_of(" \t\r\n");
        const size_t last = json.find_last_not_of(" \t\r\n");
        uiCfgJson_ = first != std::string::npos && json[first] == '{' && json[last] == '}' ? json.substr(first, last - first + 1) : "{}";
    }

    bool CaptivePortal::start(PortalHooks hooks)
    {
        if (running()) return true;
        hooks_ = std::move(hooks);
        apGraceUntil_.store(0, std::memory_order_release);
        generateCsrfToken();

        if (!startAccessPoint() || !startWebServer())
        {
            stop(true);
            return false;
        }
        running_.store(true, std::memory_order_release);
        EWM_LOG("Captive portal started");
        return true;
    }

    void CaptivePortal::stop(bool stopAccessPoint)
    {
        running_.store(false, std::memory_order_release);
        if (server_)
        {
            httpd_stop(server_);
            server_ = nullptr;
        }
        dns_.stop();
        if (stopAccessPoint)
        {
            wifi_.stopAccessPoint();
        }
        apGraceUntil_.store(0, std::memory_order_release);
        EWM_LOG("Captive portal stopped");
    }

    void CaptivePortal::markStaConnected()
    {
        apGraceUntil_.store(
            ewm::utils::monotonicMillis() + apGraceMs_,
            std::memory_order_release);
    }

    void CaptivePortal::clearStaConnected()
    {
        apGraceUntil_.store(0, std::memory_order_release);
    }

    bool CaptivePortal::graceExpired() const noexcept
    {
        const uint64_t deadline = apGraceUntil_.load(std::memory_order_acquire);
        return deadline != 0 && ewm::utils::monotonicMillis() >= deadline;
    }

    uint32_t CaptivePortal::apOffInMs() const noexcept
    {
        const uint64_t deadline = apGraceUntil_.load(std::memory_order_acquire);
        const uint64_t now = ewm::utils::monotonicMillis();
        return deadline > now ? static_cast<uint32_t>(deadline - now) : 0;
    }

    bool CaptivePortal::startAccessPoint()
    {
        if (!wifi_.startAccessPoint(apSsid_, apPass_))
        {
            return false;
        }

        const std::string address = wifi_.apIpAddress();
        const uint32_t nativeAddress = inet_addr(address.c_str());
        if (address.empty() || nativeAddress == INADDR_NONE || !dns_.start(nativeAddress))
        {
            EWM_LOG("Captive DNS start failed");
            return false;
        }
        return true;
    }

    bool CaptivePortal::startWebServer()
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = port_;
        config.stack_size = 12288;
        config.max_uri_handlers = 32;
        config.lru_purge_enable = true;
        config.global_user_ctx = this;

        if (httpd_start(&server_, &config) != ESP_OK)
        {
            server_ = nullptr;
            return false;
        }

        bool ok = true;
        ok &= registerUri("/", HTTP_GET, rootHandler);
        ok &= registerUri("/scan", HTTP_GET, scanHandler);
        ok &= registerUri("/status", HTTP_GET, statusHandler);
        ok &= registerUri("/add", HTTP_POST, addHandler);
        ok &= registerUri("/del", HTTP_POST, deleteHandler);
        ok &= registerUri("/reorder", HTTP_POST, reorderHandler);
        ok &= registerUri("/erase", HTTP_POST, eraseHandler);
        ok &= registerUri("/connect", HTTP_POST, connectHandler);
        ok &= registerUri("/ap_off", HTTP_POST, accessPointOffHandler);

        static const char* optionUris[] = {
            "/", "/scan", "/status", "/add", "/del", "/reorder", "/erase", "/connect", "/ap_off"
        };
        for (const char* uri : optionUris) ok &= registerUri(uri, HTTP_OPTIONS, optionsHandler);

        static const char* probeUris[] = {
            "/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
            "/ncsi.txt", "/connecttest.txt", "/success.txt"
        };
        for (const char* uri : probeUris) ok &= registerUri(uri, HTTP_GET, probeHandler);
        ok &= registerUri("/favicon.ico", HTTP_GET, faviconHandler);
        ok &= httpd_register_err_handler(server_, HTTPD_404_NOT_FOUND, notFoundHandler) == ESP_OK;
        return ok;
    }

    bool CaptivePortal::registerUri(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*))
    {
        httpd_uri_t definition{};
        definition.uri = uri;
        definition.method = method;
        definition.handler = handler;
        definition.user_ctx = this;
        return httpd_register_uri_handler(server_, &definition) == ESP_OK;
    }

    void CaptivePortal::setCommonHeaders(httpd_req_t* request) const
    {
        // The portal is same-origin only. Wildcard CORS would allow another
        // origin to read the page token and defeat the CSRF protection.
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        httpd_resp_set_hdr(request, "Connection", "close");
    }

    esp_err_t CaptivePortal::send(
        httpd_req_t* request,
        const char* status,
        const char* type,
        const std::string& body) const
    {
        setCommonHeaders(request);
        httpd_resp_set_status(request, status);
        httpd_resp_set_type(request, type);
        return httpd_resp_send(request, body.data(), body.size());
    }

    bool CaptivePortal::csrfValid(httpd_req_t* request) const
    {
        const size_t length = httpd_req_get_hdr_value_len(request, "X-EWM-CSRF");
        if (length == 0 || length != csrfToken_.size()) return false;
        char value[40]{};
        return length < sizeof(value) &&
               httpd_req_get_hdr_value_str(request, "X-EWM-CSRF", value, sizeof(value)) == ESP_OK &&
               csrfToken_ == value;
    }

    bool CaptivePortal::requireCsrf(httpd_req_t* request) const
    {
        if (csrfValid(request)) return true;
        send(request, "403 Forbidden", "text/plain", "CSRF token invalid.");
        return false;
    }

    bool CaptivePortal::readBody(httpd_req_t* request, std::string& body) const
    {
        if (static_cast<size_t>(request->content_len) > MAX_REQUEST_BODY)
        {
            return false;
        }

        body.assign(static_cast<size_t>(request->content_len), '\0');
        size_t received = 0;
        while (received < body.size())
        {
            const int result = httpd_req_recv(request, body.data() + received, body.size() - received);
            if (result <= 0)
            {
                return false;
            }
            received += static_cast<size_t>(result);
        }
        return true;
    }

    void CaptivePortal::generateCsrfToken()
    {
        char buffer[17]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%08lx%08lx",
            static_cast<unsigned long>(esp_random()),
            static_cast<unsigned long>(esp_random()));
        csrfToken_ = buffer;
    }

    esp_err_t CaptivePortal::rootHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal) return ESP_FAIL;

        const auto credentials = portal->hooks_.listCreds ? portal->hooks_.listCreds() : std::vector<Credential>{};
        std::string list;
        for (const auto& credential : credentials)
        {
            const std::string ssid = credential.ssid;
            list += "<li class=\"cred\" draggable=\"true\" data-ssid=\"" + ewm::utils::html_escape(ssid) + "\">";
            list += "<b>" + ewm::utils::html_escape(ssid) + "</b> <span class=\"pri\">Prio: " + std::to_string(credential.priority) + "</span>";
            list += "<span class=\"buttons\"><button class=\"con primary\">Verbinden</button>";
            list += "<button class=\"del danger\">Löschen</button></span></li>";
        }

        uint8_t nextPriority = 0;
        for (;;)
        {
            const bool used = std::any_of(credentials.begin(), credentials.end(), [nextPriority](const Credential& credential)
            {
                return credential.priority == nextPriority;
            });
            if (!used) break;
            ++nextPriority;
        }

        std::string page = ewm::portal::kPortalPage;
        replaceAll(page, "__EWM_PORTAL_CFG__", portal->uiCfgJson_);
        replaceAll(page, "__CSRF_TOKEN__", ewm::utils::json_escape(portal->csrfToken_));
        replaceAll(page, "__OPTIONS__", "<option value=\"\">(lade…)</option>");
        replaceAll(page, "__LIST__", list);
        replaceAll(page, "__DEFAULT_PRIO__", std::to_string(nextPriority));
        return portal->send(request, "200 OK", "text/html; charset=utf-8", page);
    }

    esp_err_t CaptivePortal::scanHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal) return ESP_FAIL;
        const auto networks = portal->hooks_.scanNetworks ? portal->hooks_.scanNetworks() : std::vector<ScanResult>{};
        std::string json = "[";
        for (size_t index = 0; index < networks.size(); ++index)
        {
            if (index) json += ',';
            json += "{\"ssid\":\"" + ewm::utils::json_escape(networks[index].ssid) + "\",\"rssi\":" + std::to_string(networks[index].rssi) + "}";
        }
        json += ']';
        return portal->send(request, "200 OK", "application/json", json);
    }

    esp_err_t CaptivePortal::statusHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal) return ESP_FAIL;
        const bool connected = portal->hooks_.isStaConnected && portal->hooks_.isStaConnected();
        const std::string ip = connected && portal->hooks_.staIp ? portal->hooks_.staIp() : std::string{};
        const int rssi = connected && portal->hooks_.staRssi ? portal->hooks_.staRssi() : -127;
        std::string json = "{\"connected\":";
        json += connected ? "true" : "false";
        json += ",\"ip\":\"" + ewm::utils::json_escape(ip) + "\",\"rssi\":" + std::to_string(rssi);
        json += ",\"ap_off_in\":" + std::to_string(portal->apOffInMs()) + "}";
        httpd_resp_set_hdr(request, "Pragma", "no-cache");
        httpd_resp_set_hdr(request, "Expires", "0");
        return portal->send(request, "200 OK", "application/json", json);
    }

    esp_err_t CaptivePortal::addHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        std::string body, ssid, password;
        int priority = 100;
        if (!portal->readBody(request, body) || !ewm::utils::json_get_string(body, "ssid", ssid))
            return portal->send(request, "400 Bad Request", "text/plain", "SSID fehlt.");
        ewm::utils::json_get_string(body, "password", password);
        ewm::utils::json_get_int(body, "priority", priority);
        priority = std::clamp(priority, 0, 254);
        const bool ok = portal->hooks_.addCred && portal->hooks_.addCred(ssid, password, static_cast<uint8_t>(priority));
        return portal->send(request, ok ? "200 OK" : "400 Bad Request", "text/plain", ok ? "Hinzugefügt/aktualisiert." : "Fehler (max. 10 oder ungültig).");
    }

    esp_err_t CaptivePortal::deleteHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        std::string body, ssid;
        if (!portal->readBody(request, body) || !ewm::utils::json_get_string(body, "ssid", ssid))
            return portal->send(request, "400 Bad Request", "text/plain", "SSID fehlt.");
        const bool ok = portal->hooks_.delCred && portal->hooks_.delCred(ssid);
        return portal->send(request, ok ? "200 OK" : "404 Not Found", "text/plain", ok ? "Gelöscht." : "Nicht gefunden.");
    }

    esp_err_t CaptivePortal::reorderHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        std::string body;
        std::vector<std::string> order;
        if (!portal->readBody(request, body) || !ewm::utils::json_get_order_array(body, order))
            return portal->send(request, "400 Bad Request", "text/plain", "order[] fehlt/ungültig.");
        if (portal->hooks_.reorder) portal->hooks_.reorder(order);
        return portal->send(request, "200 OK", "text/plain", "Gespeichert.");
    }

    esp_err_t CaptivePortal::eraseHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        if (portal->hooks_.eraseAll) portal->hooks_.eraseAll();
        return portal->send(request, "200 OK", "text/plain", "Alle Einträge gelöscht.");
    }

    esp_err_t CaptivePortal::connectHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        std::string body, ssid, password;
        int priority = 100;
        if (!portal->readBody(request, body) || !ewm::utils::json_get_string(body, "ssid", ssid))
            return portal->send(request, "400 Bad Request", "text/plain", "SSID fehlt.");
        ewm::utils::json_get_string(body, "password", password);
        ewm::utils::json_get_int(body, "priority", priority);
        priority = std::clamp(priority, 0, 254);
        if (portal->hooks_.connectRequest) portal->hooks_.connectRequest(ssid, password, static_cast<uint8_t>(priority));
        return portal->send(request, "202 Accepted", "application/json", "{\"ok\":true,\"msg\":\"Verbinde...\"}");
    }

    esp_err_t CaptivePortal::accessPointOffHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal || !portal->requireCsrf(request)) return ESP_OK;
        if (portal->hooks_.onStopRequested) portal->hooks_.onStopRequested();
        return portal->send(request, "200 OK", "application/json", "{\"ok\":true}");
    }

    esp_err_t CaptivePortal::optionsHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        return portal ? portal->send(request, "204 No Content", "text/plain", "") : ESP_FAIL;
    }

    esp_err_t CaptivePortal::probeHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        if (!portal) return ESP_FAIL;
        return portal->send(
            request,
            "200 OK",
            "text/html; charset=utf-8",
            "<!doctype html><meta charset='utf-8'><meta http-equiv='refresh' content='0;url=/'/>"
            "<title>Captive Portal</title><p>Weiter zur Konfigurationsseite…</p>");
    }

    esp_err_t CaptivePortal::faviconHandler(httpd_req_t* request)
    {
        auto* portal = portalFrom(request);
        return portal ? portal->send(request, "204 No Content", "text/plain", "") : ESP_FAIL;
    }

    esp_err_t CaptivePortal::notFoundHandler(httpd_req_t* request, httpd_err_code_t)
    {
        if (request->method == HTTP_OPTIONS) return optionsHandler(request);
        return rootHandler(request);
    }
}
