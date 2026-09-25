#include "crc16_ccitt.h"

uint16_t Crc16_Compute(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu;

    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

