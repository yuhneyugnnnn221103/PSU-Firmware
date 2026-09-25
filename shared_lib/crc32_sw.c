#include "crc32_sw.h"

uint32_t Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return crc;
}

uint32_t Crc32_Compute(const uint8_t *data, uint32_t len)
{
    return Crc32_Finalize(Crc32_Update(CRC32_INIT, data, len));
}
