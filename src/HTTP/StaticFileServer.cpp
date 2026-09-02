#include "StaticFileServer.hpp"

#include <array>
#include <cstdio>
#include <string_view>
#include <sys/stat.h>

#include "esp_spiffs.h"

namespace
{
    constexpr const char* BASE_PATH = "/spiffs";
}

bool StaticFileServer::mount() noexcept
{
    esp_vfs_spiffs_conf_t config{};
    config.base_path = BASE_PATH;
    config.partition_label = nullptr;
    config.max_files = 5;
    config.format_if_mount_failed = true;
    const esp_err_t result = esp_vfs_spiffs_register(&config);
    return result == ESP_OK || result == ESP_ERR_INVALID_STATE;
}

bool StaticFileServer::safePath(const std::string& path) noexcept
{
    return !path.empty() && path.front() == '/' &&
           path.find("..") == std::string::npos &&
           path.find('\\') == std::string::npos;
}

const char* StaticFileServer::contentType(const std::string& path) noexcept
{
    const auto endsWith = [&path](const char* suffix)
    {
        const size_t length = std::char_traits<char>::length(suffix);
        return path.size() >= length && path.compare(path.size() - length, length, suffix) == 0;
    };
    if (endsWith(".htm") || endsWith(".html")) return "text/html";
    if (endsWith(".css")) return "text/css";
    if (endsWith(".js")) return "application/javascript";
    if (endsWith(".png")) return "image/png";
    if (endsWith(".jpg") || endsWith(".jpeg")) return "image/jpeg";
    if (endsWith(".ico")) return "image/x-icon";
    if (endsWith(".pdf")) return "application/pdf";
    return "text/plain";
}

bool StaticFileServer::serve(httpd_req_t* request, const std::string& uri) const noexcept
{
    if (!request || !safePath(uri)) return false;

    std::string path = uri;
    if (path.back() == '/') path += "index.html";
    const std::string fullPath = std::string(BASE_PATH) + path;

    struct stat info{};
    if (stat(fullPath.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return false;

    FILE* file = std::fopen(fullPath.c_str(), "rb");
    if (!file) return false;

    const char* responseContentType = contentType(path);
    const bool isPdf = std::string_view(responseContentType) == "application/pdf";

    httpd_resp_set_status(request, "200 OK");
    httpd_resp_set_type(request, responseContentType);
    httpd_resp_set_hdr(request, "Connection", "close");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(request, "X-Frame-Options", isPdf ? "SAMEORIGIN" : "DENY");
    httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
    httpd_resp_set_hdr(request, "Permissions-Policy", "camera=(), microphone=(), geolocation=()");
    if (std::string_view(responseContentType) == "text/html")
    {
        httpd_resp_set_hdr(
            request,
            "Content-Security-Policy",
            "default-src 'self'; script-src 'self' https://cdn.jsdelivr.net; style-src 'self'; "
            "img-src 'self' data:; connect-src 'self'; frame-src 'self'; object-src 'none'; "
            "base-uri 'none'; frame-ancestors 'none'");
    }
    esp_err_t result = ESP_OK;
    if (request->method != HTTP_HEAD)
    {
        std::array<char, 2048> buffer{};
        while (!std::feof(file))
        {
            const size_t count = std::fread(buffer.data(), 1, buffer.size(), file);
            if (count && httpd_resp_send_chunk(request, buffer.data(), count) != ESP_OK)
            {
                result = ESP_FAIL;
                break;
            }
        }
        if (result == ESP_OK) result = httpd_resp_send_chunk(request, nullptr, 0);
    }
    else
    {
        const std::string length = std::to_string(static_cast<unsigned long long>(info.st_size));
        httpd_resp_set_hdr(request, "Content-Length", length.c_str());
        result = httpd_resp_send(request, nullptr, 0);
    }

    std::fclose(file);
    return result == ESP_OK;
}
