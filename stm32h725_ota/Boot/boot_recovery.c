#include <string.h>

#include "boot_recovery.h"
#include "ota_proto.h"
#include "ota_image.h"
#include "cfg_store.h"
#include "crc32_sw.h"
#include "flash.h"
#include "flash_guard.h"

/* ==========================================================================
 * Cuu ho khi ca 2 slot deu hong. Trinh tu: INFO -> ERASE (xoa CA HAI slot, ACK khi
 * xong) -> BEGIN -> DATA... -> END -> COMMIT. Slot dich lay tu Reset_Handler o chunk 0.
 * Mat dien o bat ky buoc nao -> van khong co anh hop le -> lai vao cuu ho.
 * Anh chi hop le khi header (1 flash-word, ghi CUOI) + CRC32 khop.
 * Khung/CRC/dia chi: psu_frame.h. Dia chi khac PSU_ADDR bi bo qua.
 * ========================================================================== */

typedef enum { R_IDLE = 0, R_ERASED, R_RECEIVING, R_VERIFIED } rstate_t;

static struct {
    rstate_t   st;
    ota_slot_t slot;
    uint32_t   size, crc, version;
    uint32_t   run_crc, rx_bytes;
    int32_t    last_seq;
} r = { R_IDLE, OTA_SLOT_NONE, 0, 0, 0, 0, 0, -1 };

static uint8_t   rx_buf[REC_PORTS][PSU_LEN_FW_DATA];
static psu_rx_t  rx[REC_PORTS];
static bool      rx_ready;

static void rx_init_once(void)
{
    if (rx_ready) return;
    for (uint8_t p = 0; p < REC_PORTS; p++) Psu_RxInit(&rx[p], rx_buf[p], PSU_LEN_FW_DATA);
    rx_ready = true;
}

/* ==========================================================================
 * TRA LOI
 * ========================================================================== */
static void send_ack(uint8_t port, uint16_t cmd, uint8_t status, uint32_t info)
{
    uint8_t f[PSU_LEN_FW_ACK];
    Psu_FrameBegin(f, sizeof(f), PSU_CMD_FW_ACK);

    f[OTAP_ACK_OFF_CMD]    = OTAP_ACKCMD(cmd);
    f[OTAP_ACK_OFF_STATUS] = status;
    f[OTAP_ACK_OFF_INFO + 0] = (uint8_t)(info >> 24);
    f[OTAP_ACK_OFF_INFO + 1] = (uint8_t)(info >> 16);
    f[OTAP_ACK_OFF_INFO + 2] = (uint8_t)(info >> 8);
    f[OTAP_ACK_OFF_INFO + 3] = (uint8_t)info;
    f[OTAP_ACK_OFF_SLOT]   = (uint8_t)OTAP_SLOT_RECOVERY;
    f[OTAP_ACK_OFF_STATE]  = 0u;     /* recovery: khong co anh dang chay */

    Psu_FrameSeal(f, sizeof(f));
    RecHw_Tx(port, f, sizeof(f));
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

static void cmd_erase(uint8_t p)
{
    r.st = R_IDLE;
    r.slot = OTA_SLOT_NONE;

    for (ota_slot_t s = OTA_SLOT_A; s < OTA_SLOT_COUNT; s++) {
        for (uint8_t k = 0; k < OTA_SLOT_SECTORS; k++) {
            const flash_op_result_t e = Flash_EraseSector((uint32_t)g_ota_slots[s].sector_first + k);
            if (!e.ok) {
                send_ack(p, PSU_CMD_FW_ERASE, OTAP_ST_ERASE_FAIL, e.hal_error);
                return;
            }
        }
    }

    r.st = R_ERASED;
    send_ack(p, PSU_CMD_FW_ERASE, OTAP_ST_OK, 1u);      /* info = 1: da xoa xong */
}

static void cmd_begin(uint8_t p, const uint8_t *f)
{
    if (r.st != R_ERASED) {
        send_ack(p, PSU_CMD_FW_BEGIN, OTAP_ST_NOT_ERASED, 0u);
        return;
    }

    const uint32_t size = be32(&f[OTAP_BEGIN_OFF_SIZE]);
    if (size == 0u || size > OTA_IMG_MAX_SIZE) {
        send_ack(p, PSU_CMD_FW_BEGIN, OTAP_ST_BAD_SIZE, OTA_IMG_MAX_SIZE);
        return;
    }

    r.slot = OTA_SLOT_NONE;
    r.size = size;
    r.crc = be32(&f[OTAP_BEGIN_OFF_CRC32]);
    r.version = be32(&f[OTAP_BEGIN_OFF_VERSION]);
    r.run_crc = CRC32_INIT;
    r.rx_bytes = 0u;
    r.last_seq = -1;
    r.st = R_RECEIVING;
    send_ack(p, PSU_CMD_FW_BEGIN, OTAP_ST_OK, 0u);
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
        send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_BUSY, 0u);
        return;
    }
    if ((int32_t)seq != r.last_seq + 1) {
        send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_BAD_SEQ, (uint32_t)(r.last_seq < 0 ? 0 : r.last_seq));
        return;
    }

    const uint32_t off = (uint32_t)seq * OTAP_CHUNK_MAX;
    if (len == 0u || len > OTAP_CHUNK_MAX || (off + len) > r.size ||
        (len < OTAP_CHUNK_MAX && (off + len) != r.size)) {
        send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_BAD_SIZE, 0u);
        return;
    }

    if (seq == 0u) {
        const uint32_t reset_handler = (len >= 8u)
            ? ((uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24))
            : 0u;
        r.slot = slot_of_entry(reset_handler);
        if (r.slot == OTA_SLOT_NONE) {
            r.st = R_ERASED;       /* chua ghi gi -> van sach, co the BEGIN lai */
            send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_WRONG_SLOT, reset_handler);
            return;
        }
    }

    static uint8_t buf[OTAP_CHUNK_MAX] __attribute__((aligned(32)));
    memcpy(buf, data, len);
    memset(buf + len, 0xFF, OTAP_CHUNK_MAX - len);

    const uint32_t addr = Ota_CodeBase(r.slot) + off;
    if (!prog_chunk(addr, buf)) {
        r.st = R_IDLE;             /* flash co the da ghi do -> phai ERASE lai */
        send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_PROG_FAIL, addr);
        return;
    }

    r.run_crc   = Crc32_Update(r.run_crc, data, len);
    r.rx_bytes += len;
    r.last_seq  = (int32_t)seq;
    send_ack(p, PSU_CMD_FW_DATA, OTAP_ST_OK, (uint32_t)seq);
}

static void cmd_end(uint8_t p)
{
    if (r.st != R_RECEIVING || r.slot == OTA_SLOT_NONE) {
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_BUSY, 0u);
        return;
    }

    const uint32_t final_crc = Crc32_Finalize(r.run_crc);
    if (r.rx_bytes != r.size || final_crc != r.crc) {
        r.st = R_IDLE;
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_CRC_FAIL, final_crc);
        return;
    }

    const uint32_t seq = CfgStore_AllocInstallSeq();
    if (seq == 0u) {
        r.st = R_IDLE;
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_REFUSED, 0u);
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
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_PROG_FAIL, w.hal_error);
        return;
    }

    if (!Ota_ImageValid(r.slot) || !Ota_VectorOk(r.slot)) {
        r.st = R_IDLE;
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_CRC_FAIL, 0u);
        return;
    }

    if (!CfgStore_WriteBoot(r.slot, 0u, 0u)) {
        r.st = R_IDLE;
        send_ack(p, PSU_CMD_FW_END, OTAP_ST_REFUSED, 1u);
        return;
    }

    r.st = R_VERIFIED;
    send_ack(p, PSU_CMD_FW_END, OTAP_ST_OK, 1u);
}

static void cmd_commit(uint8_t p)
{
    if (r.st != R_VERIFIED) {
        send_ack(p, PSU_CMD_FW_COMMIT, OTAP_ST_NO_COMMIT, 0u);
        return;
    }
    send_ack(p, PSU_CMD_FW_COMMIT, OTAP_ST_OK, 0u);
    RecHw_ResetSystem();
}

static void exec(uint8_t p, const uint8_t *f, uint16_t n)
{
    if (!Psu_FrameValid(f, n))      return;
    if (Psu_Addr(f) != PSU_ADDR)    return;

    switch (Psu_Cmd(f)) {
    case PSU_CMD_FW_ERASE:   cmd_erase(p);     break;
    case PSU_CMD_FW_BEGIN:   cmd_begin(p, f);  break;
    case PSU_CMD_FW_DATA:    cmd_data(p, f);   break;
    case PSU_CMD_FW_END:     cmd_end(p);       break;
    case PSU_CMD_FW_COMMIT:  cmd_commit(p);    break;
    case PSU_CMD_FW_INFO:    send_ack(p, PSU_CMD_FW_INFO, OTAP_ST_OK, 0u); break;
    default: break;          /* lenh khac (ke ca ROLLBACK, giam sat) khong co o recovery */
    }
}

/* ==========================================================================
 * NHAN BYTE
 * ========================================================================== */
void Boot_RecoveryResync(uint8_t port)
{
    rx_init_once();
    if (port < REC_PORTS) Psu_RxReset(&rx[port]);
}

void Boot_RecoveryFeed(uint8_t p, uint8_t c)
{
    rx_init_once();
    if (p >= REC_PORTS) return;

    const uint16_t n = Psu_RxFeed(&rx[p], c);
    if (n != 0u) exec(p, rx[p].buf, n);
}
