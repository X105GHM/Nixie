#pragma once

#include <string>

#include "esp_http_server.h"

class StaticFileServer
{
public:
    bool mount() noexcept;
    bool serve(httpd_req_t* request, const std::string& uri) const noexcept;

private:
    static const char* contentType(const std::string& path) noexcept;
    static bool safePath(const std::string& path) noexcept;
};
