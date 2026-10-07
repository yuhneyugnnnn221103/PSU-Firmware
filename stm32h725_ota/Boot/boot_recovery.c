#include <string.h>

#include "boot_recovery.h"
#include "ota_proto.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "crc16_ccitt.h"
#include "crc32_sw.h"
#include "flash.h"
#include "flash_guard.h"

/* ==========================================================================
 * Cuu ho khi ca 2 slot deu hong. Ca 2 slot deu vo dung nen BEGIN xoa CA HAI;
 * slot dich duoc xac dinh tu Reset_Handler o chunk 0 (anh phai duoc link dung
 * cho slot do - nhu fw_update.c cua app).
 * Mat dien o bat ky buoc nao -> van khong co anh hop le -> lai vao cuu ho.
 * Anh chi duoc coi la hop le khi header (1 flash-word, ghi CUOI) + CRC32 khop.
 * ========================================================================== */

typedef enum { R_IDLE = 0, R_RECEIVING, R_VERIFIED } rstate_t;

static struct {
    rstate_t   st;
    ota_slot_t slot;
    uint32_t   size, crc, version;
    uint32_t   run_crc, rx_bytes;
    int32_t    last_seq;
} r = { R_IDLE, OTA_SLOT_NONE, 0, 0, 0, 0, 0, -1 };

static struct {
    uint8_t  frm[OTAP_RX_MAX];
    uint16_t idx, len;
} rx[REC_PORTS];

/* ==========================================================================
 * TRA LOI
 * ========================================================================== */
static void put_be32(uint8_t *d, uint32_t v)
{
    d[0] = (uint8_t)(v >> 24); d[1] = (uint8_t)(v >> 16);
    d[2] = (uint8_t)(v >> 8);  d[3] = (uint8_t)v;
}

static void send_ack(uint8_t port, uint8_t cmd, uint8_t status, uint32_t info)
{
    uint8_t f[OTAP_ACK_SZ];
    memset(f, 0, sizeof(f));

    f[0] = OTAP_HDR1;  f[1] = OTAP_HDR2;
    f[2] = OTAP_CMD_FW_ACK;
    f[3] = OTAP_BOARD_ADDR;
    f[OTAP_ACK_OFF_CMD]    = cmd;
    f[OTAP_ACK_OFF_STATUS] = status;
    put_be32(&f[OTAP_ACK_OFF_INFO], info);
    f[OTAP_ACK_OFF_SLOT]   = (uint8_t)OTAP_SLOT_RECOVERY;

    const uint16_t crc = Crc16_Compute(&f[2], OTAP_ACK_SZ - 4u - 2u);
    f[OTAP_ACK_SZ - 4u] = (uint8_t)(crc >> 8);
    f[OTAP_ACK_SZ - 3u] = (uint8_t)crc;
    f[OTAP_ACK_SZ - 2u] = OTAP_TAIL1;
    f[OTAP_ACK_SZ - 1u] = OTAP_TAIL2;

    RecHw_Tx(port, f, OTAP_ACK_SZ);
}

/* ==========================================================================
 * LENH
 * ========================================================================== */
static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static uint16_t be16(const uint8_t *b)
{
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static ota_slot_t slot_of_entry(uint32_t entry)
{
    for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
        const uint32_t lo = Ota_CodeBase(s);
        const uint32_t hi = g_ota_slots[s].base + OTA_SLOT_SIZE;
        if (entry >= lo && entry < hi) return s;
    }
    return OTA_SLOT_NONE;
}

static void cmd_begin(uint8_t p, const uint8_t *f)
{
    const uint32_t size = be32(&f[OTAP_BEGIN_OFF_SIZE]);

    if (size == 0u || size > OTA_IMG_MAX_SIZE) {
        send_ack(p, OTAP_CMD_FW_BEGIN, OTAP_ST_BAD_SIZE, OTA_IMG_MAX_SIZE);
        return;
    }

    r.st = R_IDLE;
    r.slot = OTA_SLOT_NONE;
    r.size = size;
    r.crc = be32(&f[OTAP_BEGIN_OFF_CRC32]);
    r.version = be32(&f[OTAP_BEGIN_OFF_VERSION]);
    r.run_crc = CRC32_INIT;
    r.rx_bytes = 0u;
    r.last_seq = -1;

    for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
        for (uint8_t k = 0; k < OTA_SLOT_SECTORS; k++) {
            const flash_op_result_t e = Flash_EraseSector((uint32_t)g_ota_slots[s].sector_first + k);
            if (!e.ok) {
                send_ack(p, OTAP_CMD_FW_BEGIN, OTAP_ST_ERASE_FAIL, e.hal_error);
                return;
            }
        }
    }

    r.st = R_RECEIVING;
    send_ack(p, OTAP_CMD_FW_BEGIN, OTAP_ST_OK, 1u);
}

/* Ghi 1 chunk day du 256 byte roi doc lai so sanh (ECC/ghi hong) */
static bool prog_chunk(uint32_t addr, const uint8_t *buf)
{
    if (!Flash_ProgramWords(addr, buf, OTAP_CHUNK_MAX).ok) return false;

    uint32_t chk[OTAP_CHUNK_MAX / 4u];
    FlashGuard_Begin(addr, addr + OTAP_CHUNK_MAX);
    FlashGuard_CopyWords(chk, addr, OTAP_CHUNK_MAX);
    if (!FlashGuard_End()) return false;

    return memcmp(chk, buf, OTAP_CHUNK_MAX) == 0;
}

static void cmd_data(uint8_t p, const uint8_t *f)
{
    const uint16_t seq = be16(&f[OTAP_DATA_OFF_SEQ]);
    const uint16_t len = be16(&f[OTAP_DATA_OFF_LEN]);
    const uint8_t *data = &f[OTAP_DATA_OFF_PAYLOAD];

    if (r.st != R_RECEIVING) {
        send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_BUSY, 0u);
        return;
    }
    if ((int32_t)seq != r.last_seq + 1) {
        send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_BAD_SEQ, (uint32_t)(r.last_seq < 0 ? 0 : r.last_seq));
        return;
    }

    const uint32_t off = (uint32_t)seq * OTAP_CHUNK_MAX;
    if (len == 0u || len > OTAP_CHUNK_MAX || (off + len) > r.size ||
        (len < OTAP_CHUNK_MAX && (off + len) != r.size)) {
        send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_BAD_SIZE, 0u);
        return;
    }

    if (seq == 0u) {
        const uint32_t reset_handler = (len >= 8u)
            ? ((uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24))
            : 0u;
        r.slot = slot_of_entry(reset_handler);
        if (r.slot == OTA_SLOT_NONE) {
            r.st = R_IDLE;
            send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_WRONG_SLOT, reset_handler);
            return;
        }
    }

    static uint8_t buf[OTAP_CHUNK_MAX] __attribute__((aligned(32)));
    memcpy(buf, data, len);
    memset(buf + len, 0xFF, OTAP_CHUNK_MAX - len);

    const uint32_t addr = Ota_CodeBase(r.slot) + off;
    if (!prog_chunk(addr, buf)) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_PROG_FAIL, addr);
        return;
    }

    r.run_crc   = Crc32_Update(r.run_crc, data, len);
    r.rx_bytes += len;
    r.last_seq  = (int32_t)seq;
    send_ack(p, OTAP_CMD_FW_DATA, OTAP_ST_OK, (uint32_t)seq);
}

static void cmd_end(uint8_t p)
{
    if (r.st != R_RECEIVING || r.slot == OTA_SLOT_NONE) {
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_BUSY, 0u);
        return;
    }

    const uint32_t final_crc = Crc32_Finalize(r.run_crc);
    if (r.rx_bytes != r.size || final_crc != r.crc) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_CRC_FAIL, final_crc);
        return;
    }

    const uint32_t seq = CfgStore_AllocInstallSeq();
    if (seq == 0u) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_REFUSED, 0u);
        return;
    }

    /* Header ghi CUOI CUNG, nam tron trong 1 flash-word dau: hop le chi khi ghi tron ven */
    static ota_image_header_t hdr __attribute__((aligned(32)));
    memset(&hdr, 0xFF, sizeof(hdr));
    hdr.magic       = OTA_IMG_MAGIC;
    hdr.version     = r.version;
    hdr.size        = r.size;
    hdr.crc32       = r.crc;
    hdr.install_seq = seq;

    const flash_op_result_t w = Flash_ProgramWords(g_ota_slots[r.slot].base,
                                                   (const uint8_t *)&hdr, OTA_HEADER_SIZE);
    if (!w.ok) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_PROG_FAIL, w.hal_error);
        return;
    }

    if (!Ota_ImageValid(r.slot) || !Ota_VectorOk(r.slot)) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_CRC_FAIL, 0u);
        return;
    }

    if (!CfgStore_WriteBoot(r.slot, 0u, 0u)) {
        r.st = R_IDLE;
        send_ack(p, OTAP_CMD_FW_END, OTAP_ST_REFUSED, 1u);
        return;
    }

    r.st = R_VERIFIED;
    send_ack(p, OTAP_CMD_FW_END, OTAP_ST_OK, 1u);
}

static void cmd_commit(uint8_t p)
{
    if (r.st != R_VERIFIED) {
        send_ack(p, OTAP_CMD_FW_COMMIT, OTAP_ST_NO_COMMIT, 0u);
        return;
    }
    send_ack(p, OTAP_CMD_FW_COMMIT, OTAP_ST_OK, 0u);
    RecHw_ResetSystem();
}

static void exec(uint8_t p, const uint8_t *f, uint16_t n)
{
    const uint16_t crc_off = (uint16_t)(n - 4u);
    const uint16_t crc_rx  = be16(&f[crc_off]);
    if (crc_rx != Crc16_Compute(&f[2], (uint32_t)(crc_off - 2u))) return;

    /* Giong app: khong loc theo ADDR (da co mach rieng cho tung cong) */
    switch (f[2]) {
    case OTAP_CMD_FW_BEGIN:  cmd_begin(p, f);  break;
    case OTAP_CMD_FW_DATA:   cmd_data(p, f);   break;
    case OTAP_CMD_FW_END:    cmd_end(p);       break;
    case OTAP_CMD_FW_COMMIT: cmd_commit(p);    break;
    case OTAP_CMD_FW_INFO:   send_ack(p, OTAP_CMD_FW_INFO, OTAP_ST_OK, 0u); break;
    default: break;
    }
}

/* ==========================================================================
 * TACH KHUNG (bo cuc giong comm.c rx_feed)
 * ========================================================================== */
static uint16_t frame_len(uint8_t cmd)
{
    switch (cmd) {
    case OTAP_CMD_FW_BEGIN:  return OTAP_BEGIN_SZ;
    case OTAP_CMD_FW_DATA:   return OTAP_DATA_FRAME_SZ;
    case OTAP_CMD_FW_END:
    case OTAP_CMD_FW_COMMIT:
    case OTAP_CMD_FW_INFO:   return OTAP_CTRL_SZ;
    default:                 return 0u;
    }
}

void Boot_RecoveryResync(uint8_t port)
{
    if (port < REC_PORTS) rx[port].idx = 0u;
}

void Boot_RecoveryFeed(uint8_t p, uint8_t c)
{
    if (p >= REC_PORTS) return;
    uint16_t k = rx[p].idx;

    switch (k) {
    case 0:
        if (c == OTAP_HDR1) { rx[p].frm[0] = c; rx[p].idx = 1u; }
        return;
    case 1:
        if (c == OTAP_HDR2) { rx[p].frm[1] = c; rx[p].idx = 2u; }
        else if (c != OTAP_HDR1) { rx[p].idx = 0u; }
        return;
    case 2:
        rx[p].len = frame_len(c);
        if (rx[p].len == 0u) { rx[p].idx = 0u; return; }
        rx[p].frm[2] = c;
        rx[p].idx = 3u;
        return;
    default:
        if (k >= OTAP_RX_MAX) { rx[p].idx = 0u; return; }
        rx[p].frm[k] = c;
        rx[p].idx = (uint16_t)(k + 1u);
        break;
    }

    const uint16_t n = rx[p].len;
    if (rx[p].idx < n) return;

    rx[p].idx = 0u;
    if (rx[p].frm[n - 2u] == OTAP_TAIL1 && rx[p].frm[n - 1u] == OTAP_TAIL2) {
        exec(p, rx[p].frm, n);
    }
}
