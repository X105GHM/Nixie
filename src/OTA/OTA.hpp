#pragma once

#include <arch/cc.h>

// 2. Arduino-Deklaration VOR den LwIP-Makros
#include <IPAddress.h>

// 3. Jetzt erst die IDF/HTTP-Includes, die LwIP nachladen
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

// 4. Restliche Includes
#include <string>
#include <cstdlib>
#include <cstring>
#include "Globals/Globals.hpp"

class OTAManager
{
public:
    OTAManager() noexcept;
    esp_err_t performUpdate(const std::string &firmwareUrl) noexcept;
};
