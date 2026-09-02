#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "esp_http_server.h"

class NativeHttpServer
{
public:
    using Handler = std::function<void()>;

    explicit NativeHttpServer(uint16_t port) noexcept;

    void on(const char* path, httpd_method_t method, Handler handler);
    void onNotFound(Handler handler);
    bool configureBasicAuth(const char* username, const char* password) noexcept;

    bool begin() noexcept;
    void stop() noexcept;

    bool hasArg(const char* name) const;
    std::string arg(const char* name) const;
    const std::string& body() const;
    std::string header(const char* name, size_t maximumLength = 512) const;
    size_t contentLength() const noexcept;
    void discardBody() noexcept;
    const std::string& uri() const noexcept;
    httpd_method_t method() const noexcept;

    bool invokeRoute(
        const char* path,
        httpd_method_t registeredMethod,
        std::initializer_list<std::pair<std::string, std::string>> arguments = {});

    void sendHeader(const char* name, const char* value);
    void send(int statusCode, const char* contentType, const char* body);
    void send(int statusCode, const char* contentType, const std::string& body);

    httpd_req_t* nativeRequest() const noexcept { return activeRequest_; }

private:
    struct Route
    {
        std::string path;
        httpd_method_t method;
        Handler handler;
    };

    static esp_err_t requestEntry(httpd_req_t* request);
    esp_err_t dispatch(httpd_req_t* request);
    bool registerWildcard(httpd_method_t method) noexcept;
    void prepareRequest(httpd_req_t* request);
    void readBody();
    void applyHeaders();
    bool authorizeRequest() noexcept;
    bool isLegacyMutation() const noexcept;

    static std::string decodeUrlComponent(const char* begin, size_t length);
    static const char* statusText(int statusCode) noexcept;

    uint16_t port_{80};
    httpd_handle_t server_{nullptr};
    httpd_req_t* activeRequest_{nullptr};
    std::vector<Route> routes_;
    Handler notFound_;
    std::vector<std::pair<std::string, std::string>> responseHeaders_;
    std::vector<std::pair<std::string, std::string>> arguments_;
    std::string requestPath_;
    std::string requestBody_;
    std::string basicAuthHeader_;
    bool bodyRead_{false};
    bool bodyValid_{false};
    bool basicAuthEnabled_{false};
};
