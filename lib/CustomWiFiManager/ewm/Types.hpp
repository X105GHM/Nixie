#pragma once
#include <Arduino.h>
#include <array>

namespace ewm
{
    static constexpr uint32_t MAGIC = 0x45574D31u; // "EWM1"

    struct Credential
    {
        char ssid[33] = {0};
        char password[65] = {0};
        uint8_t priority = 0;   // 0 = höchste
        uint32_t last_ok = 0;   // unix-seconds oder uptime-seconds fallback
    };

    struct StorageHeader
    {
        uint32_t magic  = MAGIC;
        uint16_t version= 0x0001;
        uint16_t count  = 0;
        uint32_t crc32  = 0;
    };

    using CredentialArray = std::array<Credential, 10>;
}
