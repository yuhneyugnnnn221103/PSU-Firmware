/* Sinh docs/PSU_FRAME_EXAMPLES.md: khung mau (hex) voi CRC do CHINH code firmware tinh, de phia PC doi chieu.
 * Chay: make -C tests/host examples */
#include <stdio.h>
#include <string.h>
#include "psu_frame.h"
#include "ota_proto.h"
#include "psu_monitor.h"
#include "psu_trip.h"
#include "crc16_ccitt.h"
#include "crc32_sw.h"

static void hexdump(const uint8_t *f, uint16_t n)
{
    printf("```\n");
    for (uint16_t i = 0; i < n; i++) {
        printf("%02X%c", f[i], ((i + 1) % 32 == 0 || i + 1 == n) ? '\n' : ' ');
    }
    printf("```\n\n");
}

static void show(const char *title, const char *note, uint8_t *f, uint16_t n)
{
    printf("### %s (%u byte)\n%s\n\n", title, n, note);
    hexdump(f, n);
}

static void ctrl(uint8_t *f, uint16_t cmd) { Psu_FrameBegin(f, PSU_LEN_FW_CTRL, cmd); Psu_FrameSeal(f, PSU_LEN_FW_CTRL); }

static void ack(uint8_t *f, uint16_t cmd, uint8_t st, uint32_t info, char slot, uint8_t state)
{
    Psu_FrameBegin(f, PSU_LEN_FW_ACK, PSU_CMD_FW_ACK);
    f[OTAP_ACK_OFF_CMD] = OTAP_ACKCMD(cmd); f[OTAP_ACK_OFF_STATUS] = st;
    f[OTAP_ACK_OFF_INFO] = (uint8_t)(info >> 24); f[OTAP_ACK_OFF_INFO + 1] = (uint8_t)(info >> 16);
    f[OTAP_ACK_OFF_INFO + 2] = (uint8_t)(info >> 8); f[OTAP_ACK_OFF_INFO + 3] = (uint8_t)info;
    f[OTAP_ACK_OFF_SLOT] = (uint8_t)slot; f[OTAP_ACK_OFF_STATE] = state;
    Psu_FrameSeal(f, PSU_LEN_FW_ACK);
}

int main(void)
{
    static uint8_t f[PSU_LEN_MAX];

    printf("# Khung mau PSU (do firmware sinh, `make -C tests/host examples`)\n\n");
    printf("Dia chi PSU = %u. Khung: `AB CD | CMD(2) | ADDR | payload | CRC16 | E1 E2`. "
           "CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF) tinh tu byte ADDR den het payload (bo AB CD, CMD, CRC, E1 E2). "
           "Moi so nhieu byte la big-endian.\n\n", (unsigned)PSU_ADDR);
    printf("Vector kiem tra CRC: CRC16-CCITT-FALSE(\"123456789\") = 0x%04X; CRC32(\"123456789\") = 0x%08X.\n\n",
           Crc16_Compute((const uint8_t *)"123456789", 9), Crc32_Compute((const uint8_t *)"123456789", 9));

    printf("## PC -> PSU\n\n");

    Psu_FrameBegin(f, PSU_LEN_MON_REQ, PSU_CMD_MON_REQ); Psu_FrameSeal(f, PSU_LEN_MON_REQ);
    show("02 02 Hoi giam sat", "`[5..8]` du phong = 00.", f, PSU_LEN_MON_REQ);

    Psu_FrameBegin(f, PSU_LEN_CFG_QUERY, PSU_CMD_CFG_QUERY); Psu_FrameSeal(f, PSU_LEN_CFG_QUERY);
    show("03 03 Hoi cau hinh", "`[5..8]` du phong = 00.", f, PSU_LEN_CFG_QUERY);

    Psu_FrameBegin(f, PSU_LEN_CONTROL, PSU_CMD_CONTROL); f[5] = 0x01; f[6] = 0x00; Psu_FrameSeal(f, PSU_LEN_CONTROL);
    show("01 01 Bat cum 1", "`[5]` Mask (bit0 = cum 1), `[6]` ClearTrip = 00. Khong co ACK.", f, PSU_LEN_CONTROL);

    Psu_FrameBegin(f, PSU_LEN_CONTROL, PSU_CMD_CONTROL); f[5] = 0x00; f[6] = 0x00; Psu_FrameSeal(f, PSU_LEN_CONTROL);
    show("01 01 Tat moi cum", "Mask = 00.", f, PSU_LEN_CONTROL);

    Psu_FrameBegin(f, PSU_LEN_CONTROL, PSU_CMD_CONTROL); f[5] = 0x00; f[6] = 0x01; Psu_FrameSeal(f, PSU_LEN_CONTROL);
    show("01 01 ClearTrip", "`[6]` = 01: xoa trip code, moi cum TRIPPED ve OFF (toan PSU).", f, PSU_LEN_CONTROL);

    ctrl(f, PSU_CMD_FW_INFO);       show("95 95 INFO", "Chay o app va recovery.", f, PSU_LEN_FW_CTRL);
    ctrl(f, PSU_CMD_FW_ENTER_REC);  show("98 98 ENTER_REC", "App: ghi co, ACK, reset sau ~50 ms (chi khi moi cum OFF).", f, PSU_LEN_FW_CTRL);
    ctrl(f, PSU_CMD_FW_ERASE);      show("96 96 ERASE", "Chi recovery. Mot ACK khi xoa xong (timeout PC >= 30 s).", f, PSU_LEN_FW_CTRL);

    Psu_FrameBegin(f, PSU_LEN_FW_BEGIN, PSU_CMD_FW_BEGIN);
    f[5] = 0x00; f[6] = 0x00; f[7] = 0x10; f[8] = 0x00;           /* size 0x1000 */
    f[9] = 0xDE; f[10] = 0xAD; f[11] = 0xBE; f[12] = 0xEF;        /* crc32 toan file (vi du) */
    Psu_FrameSeal(f, PSU_LEN_FW_BEGIN);
    show("90 90 BEGIN", "`[5..8]` size = 0x00001000, `[9..12]` CRC32 cua file (vi du DEADBEEF), `[13..16]` so version cu = 0 (khong dung).",
         f, PSU_LEN_FW_BEGIN);

    Psu_FrameBegin(f, PSU_LEN_VERSION, PSU_CMD_FW_SET_VERSION);
    memcpy(&f[OTAP_VER_OFF_STR], "PSU_v1.2.3_abc123", 17);
    Psu_FrameSeal(f, PSU_LEN_VERSION);
    show("9A 9A SET_VERSION", "Chi recovery, giua ERASE va END. `[5..36]` chuoi ASCII 32 byte (ten file bin bo duoi), dem 00.", f, PSU_LEN_VERSION);

    Psu_FrameBegin(f, PSU_LEN_FW_DATA, PSU_CMD_FW_DATA);
    f[OTAP_DATA_OFF_SEQ] = 0x00; f[OTAP_DATA_OFF_SEQ + 1] = 0x00;
    f[OTAP_DATA_OFF_LEN] = 0x01; f[OTAP_DATA_OFF_LEN + 1] = 0x00;
    for (int i = 0; i < OTAP_CHUNK_MAX; i++) f[OTAP_DATA_OFF_PAYLOAD + i] = (uint8_t)i;
    Psu_FrameSeal(f, PSU_LEN_FW_DATA);
    show("91 91 DATA (seq 0)", "`[5..6]` seq = 0, `[7..8]` len = 0x0100, `[9..264]` 256 byte (vi du 00..FF). Chunk cuoi: len < 256, dem FF.",
         f, PSU_LEN_FW_DATA);

    ctrl(f, PSU_CMD_FW_END);        show("92 92 END", "Kiem tong byte + CRC32, ghi header.", f, PSU_LEN_FW_CTRL);
    ctrl(f, PSU_CMD_FW_COMMIT);     show("93 93 COMMIT", "Sau END thanh cong: ACK roi reset.", f, PSU_LEN_FW_CTRL);
    ctrl(f, PSU_CMD_FW_GET_VERSION);show("99 99 GET_VERSION", "Chay o app va recovery.", f, PSU_LEN_FW_CTRL);

    printf("## PSU -> PC\n\n");

    ack(f, PSU_CMD_FW_INFO, OTAP_ST_OK, 0u, 'A', OTAP_STATE_CONFIRMED);
    show("94 94 ACK cho INFO (app, da confirm)", "`[5]`=95, `[6]` status 0, `[7..10]` so version cu = 0, `[11]` slot `A`, `[12]` 0x80 = confirmed, boot_count 0.",
         f, PSU_LEN_FW_ACK);

    ack(f, PSU_CMD_FW_INFO, OTAP_ST_OK, 0u, 'R', 0u);
    show("94 94 ACK cho INFO (recovery)", "slot `R`.", f, PSU_LEN_FW_ACK);

    ack(f, PSU_CMD_FW_ERASE, OTAP_ST_OK, 1u, 'R', 0u);
    show("94 94 ACK cho ERASE (xong)", "`[5]`=96, status 0, info = 1.", f, PSU_LEN_FW_ACK);

    ack(f, PSU_CMD_FW_DATA, OTAP_ST_OK, 0u, 'R', 0u);
    show("94 94 ACK cho DATA seq 0", "`[5]`=91, info = seq vua ghi.", f, PSU_LEN_FW_ACK);

    ack(f, PSU_CMD_FW_BEGIN, OTAP_ST_NOT_ERASED, 0u, 'R', 0u);
    show("94 94 ACK loi: BEGIN khi chua ERASE", "status 11 (NotErased).", f, PSU_LEN_FW_ACK);

    ack(f, PSU_CMD_FW_ERASE, OTAP_ST_NOT_RECOVERY, 0u, 'A', 0u);
    show("94 94 ACK cua app khi nhan lenh nap", "status 12 (NotRecovery): phai ENTER_REC truoc.", f, PSU_LEN_FW_ACK);

    Psu_FrameBegin(f, PSU_LEN_VERSION, PSU_CMD_FW_VERSION);
    memcpy(&f[OTAP_VER_OFF_STR], "PSU_v1.2.3_abc123", 17);
    Psu_FrameSeal(f, PSU_LEN_VERSION);
    show("A9 A9 Tra loi GET_VERSION", "`[5..36]` chuoi ASCII, dem 00 (rong = chua dat).", f, PSU_LEN_VERSION);

    psu_mon_t m; memset(&m, 0, sizeof m);
    uint8_t trip[PSU_TRIP_BYTES] = {0};
    m.trip = trip;
    m.safe_state[0] = 1; m.safe_state[1] = 1; m.safe_state[2] = 1; m.safe_state[3] = 1;   /* OFF */
    m.slot = 'A'; m.version = 0u;
    m.cur24[0] = 0x00123456u; m.temp[0] = 0x0A00; m.fresh_mask = 0x01;
    m.last_cmd = 0x01; m.last_result = PSU_RES_OK; m.last_count = 1;
    m.diag[0] = 5; m.diag[5] = 5; m.uptime_s = 60;
    PsuMon_Build(f, &m);
    show("81 81 Giam sat (vi du: cum 1 OFF, DCM1 co du lieu, 5 khung da nhan)",
         "Byte 5..7 = I_OUT_ADC1 (00123456), 8..10 = TEMP_ADC1 (000A00). Byte 228..237 = TRIP1..10. "
         "Vung du phong tu byte 238 (xem PSU_PROTOCOL.md muc 3). Truong khong co du lieu = 00.", f, PSU_LEN_MONITOR);

    return 0;
}
