#include "NativeHttpServer.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "mbedtls/base64.h"

namespace
{
    constexpr const char* TAG = "NativeHttpServer";
    constexpr size_t MAX_BODY_SIZE = 4096;

    uint8_t hexValue(char value)
    {
        if (value >= '0' && value <= '9') return static_cast<uint8_t>(value - '0');
        if (value >= 'a' && value <= 'f') return static_cast<uint8_t>(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return static_cast<uint8_t>(value - 'A' + 10);
        return 0xff;
    }

    bool constantTimeEqual(const std::string& left, const std::string& right) noexcept
    {
        if (left.size() != right.size()) return false;
        unsigned char difference = 0;
        for (size_t index = 0; index < left.size(); ++index)
        {
            difference |= static_cast<unsigned char>(left[index]) ^ static_cast<unsigned char>(right[index]);
        }
        return difference == 0;
    }

    const char* methodName(httpd_method_t method) noexcept
    {
        switch (method)
        {
            case HTTP_GET: return "GET";
            case HTTP_POST: return "POST";
            case HTTP_PUT: return "PUT";
            case HTTP_DELETE: return "DELETE";
            case HTTP_PATCH: return "PATCH";
            case HTTP_OPTIONS: return "OPTIONS";
            case HTTP_HEAD: return "HEAD";
            default: return "";
        }
    }
}

NativeHttpServer::NativeHttpServer(uint16_t port) noexcept : port_(port) {}

void NativeHttpServer::on(const char* path, httpd_method_t method, Handler handler)
{
    if (path && handler) routes_.push_back({path, method, std::move(handler)});
}

void NativeHttpServer::onNotFound(Handler handler)
{
    notFound_ = std::move(handler);
}

bool NativeHttpServer::configureBasicAuth(const char* username, const char* password) noexcept
{
    basicAuthEnabled_ = false;
    basicAuthHeader_.clear();

    const std::string user = username ? username : "";
    const std::string pass = password ? password : "";
    if (user.empty() && pass.empty()) return true;
    if (user.empty() || pass.empty() || user.size() > 64 || pass.size() > 128) return false;
    const auto printable = [](unsigned char value) { return value >= 0x20 && value != 0x7f; };
    if (user.find(':') != std::string::npos ||
        !std::all_of(user.begin(), user.end(), printable) ||
        !std::all_of(pass.begin(), pass.end(), printable)) return false;

    std::string credentials = user + ":" + pass;
    const size_t encodedCapacity = 4 * ((credentials.size() + 2) / 3) + 1;
    std::string encoded(encodedCapacity, '\0');
    size_t encodedLength = 0;
    const int result = mbedtls_base64_encode
    (
        reinterpret_cast<unsigned char*>(encoded.data()), encoded.size(), &encodedLength,
        reinterpret_cast<const unsigned char*>(credentials.data()), credentials.size()
    );
    std::fill(credentials.begin(), credentials.end(), '\0');
    if (result != 0) return false;
    encoded.resize(encodedLength);
    basicAuthHeader_ = "Basic " + encoded;
    std::fill(encoded.begin(), encoded.end(), '\0');
    basicAuthEnabled_ = true;
    return true;
}

bool NativeHttpServer::begin() noexcept
{
    if (server_) return true;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    // Keep the native HTTP task below the large OTA worker reservation.  The
    // previous 16 KiB stack could fail allocation after boot when internal
    // heap blocks were fragmented.
    config.stack_size = 12288;
    config.core_id = 0;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.global_user_ctx = this;

    const esp_err_t startResult = httpd_start(&server_, &config);
    if (startResult != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s (0x%x), stack=%u, free_internal=%u, largest_internal=%u",
                 esp_err_to_name(startResult), static_cast<unsigned>(startResult),
                 static_cast<unsigned>(config.stack_size),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        server_ = nullptr;
        return false;
    }

    const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_HEAD, HTTP_PUT, HTTP_DELETE, HTTP_OPTIONS, HTTP_PATCH
    };
    for (const auto method : methods)
    {
        if (!registerWildcard(method))
        {
            ESP_LOGE(TAG, "httpd_register_uri_handler failed for method %s", methodName(method));
            stop();
            return false;
        }
    }
    return true;
}

void NativeHttpServer::stop() noexcept
{
    if (server_)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

bool NativeHttpServer::registerWildcard(httpd_method_t method) noexcept
{
    httpd_uri_t route{};
    route.uri = "/*";
    route.method = method;
    route.handler = requestEntry;
    route.user_ctx = this;
    return httpd_register_uri_handler(server_, &route) == ESP_OK;
}

esp_err_t NativeHttpServer::requestEntry(httpd_req_t* request)
{
    auto* server = request ? static_cast<NativeHttpServer*>(request->user_ctx) : nullptr;
    if (!server && request)
    {
        server = static_cast<NativeHttpServer*>(httpd_get_global_user_ctx(request->handle));
    }
    return server ? server->dispatch(request) : ESP_FAIL;
}

esp_err_t NativeHttpServer::dispatch(httpd_req_t* request)
{
    prepareRequest(request);

    if (!authorizeRequest())
    {
        activeRequest_ = nullptr;
        return ESP_OK;
    }

    if (isLegacyMutation())
    {
        sendHeader("Cache-Control", "no-store");
        send(410, "application/json",
             "{\"error\":\"legacy mutation endpoint disabled\",\"replacement\":\"/api/v1\"}");
        activeRequest_ = nullptr;
        return ESP_OK;
    }

    const auto route = std::find_if(routes_.begin(), routes_.end(), [this, request](const Route& candidate)
    {
        return candidate.method == request->method && candidate.path == requestPath_;
    });

    if (route != routes_.end())
    {
        route->handler();
    }
    else
    {
        const bool pathRegistered = std::any_of(routes_.begin(), routes_.end(), [this](const Route& candidate)
        {
            return candidate.path == requestPath_;
        });
        if (pathRegistered)
        {
            std::string allowedMethods;
            for (const Route& candidate : routes_)
            {
                if (candidate.path != requestPath_) continue;
                if (!allowedMethods.empty()) allowedMethods += ", ";
                allowedMethods += methodName(candidate.method);
            }
            sendHeader("Allow", allowedMethods.c_str());
            send(405, requestPath_.rfind("/api/", 0) == 0 ? "application/json" : "text/plain",
                 requestPath_.rfind("/api/", 0) == 0
                     ? "{\"error\":\"method_not_allowed\"}"
                     : "Method Not Allowed");
        }
        else if (notFound_)
        {
            notFound_();
        }
        else
        {
            send(404, "text/plain", "404: File Not Found");
        }
    }

    activeRequest_ = nullptr;
    return ESP_OK;
}

bool NativeHttpServer::authorizeRequest() noexcept
{
    if (!basicAuthEnabled_) return true;

    const std::string authorization = header("Authorization", 512);
    if (constantTimeEqual(authorization, basicAuthHeader_)) return true;

    sendHeader("WWW-Authenticate", "Basic realm=\"Nixie\", charset=\"UTF-8\"");
    sendHeader("Cache-Control", "no-store");
    send(401, "application/json", "{\"error\":\"authentication required\"}");
    return false;
}

bool NativeHttpServer::isLegacyMutation() const noexcept
{
    return requestPath_.rfind("/set/", 0) == 0 || requestPath_ == "/get/checkUpdate";
}

void NativeHttpServer::prepareRequest(httpd_req_t* request)
{
    activeRequest_ = request;
    responseHeaders_.clear();
    arguments_.clear();
    requestBody_.clear();
    bodyRead_ = false;
    bodyValid_ = false;

    const char* rawUri = request && request->uri ? request->uri : "/";
    const char* query = std::strchr(rawUri, '?');
    requestPath_.assign(rawUri, query ? static_cast<size_t>(query - rawUri) : std::strlen(rawUri));

    if (!query || query[1] == '\0') return;
    const char* position = query + 1;
    while (*position)
    {
        const char* end = std::strchr(position, '&');
        if (!end) end = position + std::strlen(position);
        const char* separator = std::find(position, end, '=');
        const size_t nameLength = static_cast<size_t>(separator - position);
        const char* value = separator < end ? separator + 1 : end;
        arguments_.emplace_back(
            decodeUrlComponent(position, nameLength),
            decodeUrlComponent(value, static_cast<size_t>(end - value)));
        position = *end ? end + 1 : end;
    }
}

std::string NativeHttpServer::decodeUrlComponent(const char* begin, size_t length)
{
    std::string decoded;
    decoded.reserve(length);
    for (size_t index = 0; index < length; ++index)
    {
        const char value = begin[index];
        if (value == '+')
        {
            decoded.push_back(' ');
        }
        else if (value == '%' && index + 2 < length)
        {
            const uint8_t high = hexValue(begin[index + 1]);
            const uint8_t low = hexValue(begin[index + 2]);
            if (high != 0xff && low != 0xff)
            {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
            }
            else
            {
                decoded.push_back(value);
            }
        }
        else
        {
            decoded.push_back(value);
        }
    }
    return decoded;
}

void NativeHttpServer::readBody()
{
    if (bodyRead_ || !activeRequest_) return;
    bodyRead_ = true;
    if (activeRequest_->content_len == 0)
    {
        bodyValid_ = true;
        return;
    }
    if (activeRequest_->content_len > MAX_BODY_SIZE)
    {
        responseHeaders_.emplace_back("Connection", "close");
        return;
    }

    requestBody_.assign(activeRequest_->content_len, '\0');
    size_t received = 0;
    while (received < requestBody_.size())
    {
        const int result = httpd_req_recv(
            activeRequest_, requestBody_.data() + received, requestBody_.size() - received);
        if (result == HTTPD_SOCK_ERR_TIMEOUT)
        {
            requestBody_.clear();
            responseHeaders_.emplace_back("Connection", "close");
            return;
        }
        if (result <= 0)
        {
            requestBody_.clear();
            responseHeaders_.emplace_back("Connection", "close");
            return;
        }
        received += static_cast<size_t>(result);
    }
    bodyValid_ = true;
}

bool NativeHttpServer::hasArg(const char* name) const
{
    if (!name) return false;
    if (std::strcmp(name, "plain") == 0)
    {
        const_cast<NativeHttpServer*>(this)->readBody();
        return bodyValid_;
    }
    return std::any_of(arguments_.begin(), arguments_.end(), [name](const auto& argument)
    {
        return argument.first == name;
    });
}

std::string NativeHttpServer::arg(const char* name) const
{
    if (!name) return {};
    if (std::strcmp(name, "plain") == 0)
    {
        const_cast<NativeHttpServer*>(this)->readBody();
        return requestBody_;
    }
    const auto argument = std::find_if(arguments_.begin(), arguments_.end(), [name](const auto& entry)
    {
        return entry.first == name;
    });
    return argument == arguments_.end() ? std::string{} : argument->second;
}

const std::string& NativeHttpServer::body() const
{
    const_cast<NativeHttpServer*>(this)->readBody();
    return requestBody_;
}

std::string NativeHttpServer::header(const char* name, size_t maximumLength) const
{
    if (!activeRequest_ || !name || maximumLength == 0) return {};
    const size_t length = httpd_req_get_hdr_value_len(activeRequest_, name);
    if (length == 0 || length > maximumLength) return {};

    std::string value(length + 1, '\0');
    if (httpd_req_get_hdr_value_str(activeRequest_, name, value.data(), value.size()) != ESP_OK)
    {
        return {};
    }
    value.resize(length);
    return value;
}

size_t NativeHttpServer::contentLength() const noexcept
{
    return activeRequest_ ? activeRequest_->content_len : 0;
}

void NativeHttpServer::discardBody() noexcept
{
    std::fill(requestBody_.begin(), requestBody_.end(), '\0');
    requestBody_.clear();
    bodyValid_ = false;
}

bool NativeHttpServer::invokeRoute(
    const char* path,
    httpd_method_t registeredMethod,
    std::initializer_list<std::pair<std::string, std::string>> arguments)
{
    if (!path || !activeRequest_) return false;
    const auto route = std::find_if(routes_.begin(), routes_.end(), [path, registeredMethod](const Route& candidate)
    {
        return candidate.method == registeredMethod && candidate.path == path;
    });
    if (route == routes_.end()) return false;

    const std::string savedPath = requestPath_;
    auto savedArguments = std::move(arguments_);
    requestPath_ = path;
    arguments_.assign(arguments.begin(), arguments.end());
    route->handler();
    arguments_ = std::move(savedArguments);
    requestPath_ = savedPath;
    return true;
}

const std::string& NativeHttpServer::uri() const noexcept
{
    return requestPath_;
}

httpd_method_t NativeHttpServer::method() const noexcept
{
    return activeRequest_ ? static_cast<httpd_method_t>(activeRequest_->method) : HTTP_GET;
}

void NativeHttpServer::sendHeader(const char* name, const char* value)
{
    if (name && value) responseHeaders_.emplace_back(name, value);
}

void NativeHttpServer::applyHeaders()
{
    if (!activeRequest_) return;
    for (const auto& header : responseHeaders_)
    {
        httpd_resp_set_hdr(activeRequest_, header.first.c_str(), header.second.c_str());
    }
}

void NativeHttpServer::send(int statusCode, const char* contentType, const char* body)
{
    send(statusCode, contentType, std::string(body ? body : ""));
}

void NativeHttpServer::send(int statusCode, const char* contentType, const std::string& body)
{
    if (!activeRequest_) return;
    httpd_resp_set_status(activeRequest_, statusText(statusCode));
    httpd_resp_set_type(activeRequest_, contentType ? contentType : "text/plain");
    httpd_resp_set_hdr(activeRequest_, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(activeRequest_, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(activeRequest_, "Referrer-Policy", "no-referrer");
    applyHeaders();
    if (activeRequest_->method == HTTP_HEAD || statusCode == 204)
    {
        httpd_resp_send(activeRequest_, nullptr, 0);
    }
    else
    {
        httpd_resp_send(activeRequest_, body.data(), body.size());
    }
    // esp_http_server retains header pointers until httpd_resp_send() returns.
    // Keep the backing strings alive for the complete response transmission.
    responseHeaders_.clear();
}

const char* NativeHttpServer::statusText(int statusCode) noexcept
{
    switch (statusCode)
    {
        case 200: return "200 OK";
        case 202: return "202 Accepted";
        case 204: return "204 No Content";
        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 409: return "409 Conflict";
        case 410: return "410 Gone";
        case 413: return "413 Payload Too Large";
        case 414: return "414 URI Too Long";
        case 415: return "415 Unsupported Media Type";
        case 422: return "422 Unprocessable Content";
        case 500: return "500 Internal Server Error";
        case 501: return "501 Not Implemented";
        case 503: return "503 Service Unavailable";
        default: return "500 Internal Server Error";
    }
}
