#include "ewm/Utils/Crc32.hpp"

namespace
{
    static uint32_t crc32_update(uint32_t crc, uint8_t data)
    {
        crc = crc ^ data;
        for (int i = 0; i < 8; ++i)
        {
            uint32_t mask = -(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
        return crc;
    }
}

namespace ewm::utils
{
    uint32_t crc32(const uint8_t* data, size_t len)
    {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i)
            crc = crc32_update(crc, data[i]);
        return ~crc;
    }
}
