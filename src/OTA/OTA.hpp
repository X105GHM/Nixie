#pragma once

#include <arch/cc.h>
#include <IPAddress.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include <string>
#include <cstdlib>
#include <cstring>
#include <string>
#include <optional>
#include "Globals/Globals.hpp"

class OTAManager
{
public:
    OTAManager() noexcept;

    esp_err_t checkAndUpdate(const std::string &manifestUrl, const std::string &firmwareUrl) noexcept;

private:
    std::optional<std::string> fetchManifestVersion(const std::string &manifestUrl) noexcept;

    esp_err_t performUpdate(const std::string &firmwareUrl) noexcept;
};