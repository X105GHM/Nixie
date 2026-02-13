#pragma once
#include <cstddef>
#include <cstdint>

namespace ewm::utils
{
    uint32_t crc32(const uint8_t* data, size_t len);
}
