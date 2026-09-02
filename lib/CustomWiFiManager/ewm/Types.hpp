#pragma once
#include <array>
#include <cstdint>
#include <string>

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

    struct ScanResult
    {
        std::string ssid;
        int rssi = -127;
    };

    enum class State : uint8_t
    {
        Uninitialized,
        Initialization,
        Connecting,
        Connected,
        Retry,
        Portal,
        Failed
    };

    static_assert(sizeof(StorageHeader) == 12, "Credential header layout must remain NVS-compatible");
    static_assert(sizeof(Credential) == 104, "Credential layout must remain NVS-compatible");
}
