#include <string.h>
#include "psu_frame.h"
#include "crc16_ccitt.h"

uint16_t Psu_RxFrameLen(uint16_t cmd)
{
    switch (cmd) {
    case PSU_CMD_CONTROL:     return PSU_LEN_CONTROL;
    case PSU_CMD_MON_REQ:     return PSU_LEN_MON_REQ;
    case PSU_CMD_CFG_QUERY:   return PSU_LEN_CFG_QUERY;
    case PSU_CMD_CFG_WRITE:   return PSU_LEN_CFG_WRITE;
    case PSU_CMD_FW_BEGIN:    return PSU_LEN_FW_BEGIN;
    case PSU_CMD_FW_DATA:     return PSU_LEN_FW_DATA;
    case PSU_CMD_FW_END:
    case PSU_CMD_FW_COMMIT:
    case PSU_CMD_FW_INFO:
    case PSU_CMD_FW_ERASE:
    case PSU_CMD_FW_ROLLBACK: return PSU_LEN_FW_CTRL;
    default:                  return 0u;
    }
}

uint16_t Psu_Crc16(const uint8_t *f, uint16_t n)
{
    return Crc16_Compute(&f[PSU_CRC_START], (uint32_t)(n - 4u - PSU_CRC_START));
}

bool Psu_FrameValid(const uint8_t *f, uint16_t n)
{
    if (n < 9u) return false;
    if (f[0] != PSU_HDR1 || f[1] != PSU_HDR2)           return false;
    if (f[n - 2u] != PSU_TAIL1 || f[n - 1u] != PSU_TAIL2) return false;

    const uint16_t rx = (uint16_t)(((uint16_t)f[n - 4u] << 8) | f[n - 3u]);
    return rx == Psu_Crc16(f, n);
}

void Psu_FrameBegin(uint8_t *f, uint16_t n, uint16_t cmd)
{
    memset(f, 0, n);
    f[0] = PSU_HDR1;
    f[1] = PSU_HDR2;
    f[2] = (uint8_t)(cmd >> 8);
    f[3] = (uint8_t)cmd;
    f[PSU_OFF_ADDR] = (uint8_t)PSU_ADDR;
    f[n - 2u] = PSU_TAIL1;
    f[n - 1u] = PSU_TAIL2;
}

void Psu_FrameSeal(uint8_t *f, uint16_t n)
{
    const uint16_t crc = Psu_Crc16(f, n);
    f[n - 4u] = (uint8_t)(crc >> 8);
    f[n - 3u] = (uint8_t)crc;
}

void Psu_RxInit(psu_rx_t *r, uint8_t *buf, uint16_t cap)
{
    r->buf = buf;
    r->cap = cap;
    r->idx = 0u;
    r->len = 0u;
}

void Psu_RxReset(psu_rx_t *r)
{
    r->idx = 0u;
}

uint16_t Psu_RxFeed(psu_rx_t *r, uint8_t c)
{
    for (;;) {
        switch (r->idx) {
        case 0:
            if (c == PSU_HDR1) { r->buf[0] = c; r->idx = 1u; }
            return 0u;

        case 1:
            if (c == PSU_HDR2)      { r->buf[1] = c; r->idx = 2u; }
            else if (c == PSU_HDR1) { r->idx = 1u; }       /* "AB AB CD" */
            else                    { r->idx = 0u; }
            return 0u;

        case 2:
            r->buf[2] = c;
            r->idx = 3u;
            return 0u;

        case 3: {
            r->buf[3] = c;
            const uint16_t len = Psu_RxFrameLen((uint16_t)(((uint16_t)r->buf[2] << 8) | c));
            if (len == 0u || len > r->cap) {
                r->idx = 0u;
                continue;            /* byte nay co the la AB cua khung ke tiep */
            }
            r->len = len;
            r->idx = 4u;
            return 0u;
        }

        default:
            r->buf[r->idx++] = c;
            if (r->idx < r->len) return 0u;

            r->idx = 0u;
            if (r->buf[r->len - 2u] == PSU_TAIL1 && r->buf[r->len - 1u] == PSU_TAIL2) return r->len;
            return 0u;
        }
    }
}
