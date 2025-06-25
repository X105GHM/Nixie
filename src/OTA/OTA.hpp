#pragma once

#include <arch/cc.h>
#include <IPAddress.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include <string>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <string>
#include <optional>
#include "Globals/Globals.hpp"
#include "Logger/Logger.hpp"
#include "HTTP/certs.hpp"

class OTAManager
{
public:
    OTAManager() noexcept;

    esp_err_t checkAndUpdate(const std::string &baseUrl) noexcept;

private:
    std::optional<std::string> fetchManifestVersion(const std::string &manifestUrl) noexcept;
    esp_err_t performFirmwareUpdate(const std::string &firmwareUrl) noexcept;
    esp_err_t performSPIFFSUpdate(const std::string &spiffsUrl) noexcept;
};