/* Test khung PSU: CRC, bo tach khung, do dai theo CMD. */
#include <stdio.h>
#include <string.h>
#include "psu_frame.h"
#include "crc16_ccitt.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

static uint8_t rxbuf[PSU_LEN_MAX];
static psu_rx_t rx;
static uint16_t feed_all(const uint8_t *d, uint16_t n)   /* tra ve do dai khung cuoi cung hoan tat */
{
    uint16_t got = 0;
    for (uint16_t i = 0; i < n; i++) { uint16_t l = Psu_RxFeed(&rx, d[i]); if (l) got = l; }
    return got;
}

int main(void)
{
    /* 1. CRC khop vector chuan CRC16-CCITT-FALSE ("123456789" = 0x29B1) va pham vi theo macro */
    const uint8_t v[] = "123456789";
    CHECK(Crc16_Compute(v, 9) == 0x29B1, "CRC16-CCITT-FALSE vector chuan");

    uint8_t f[PSU_LEN_CONTROL];
    Psu_FrameBegin(f, sizeof f, PSU_CMD_CONTROL);
    f[5] = 0x0F; f[6] = 0x01;
    Psu_FrameSeal(f, sizeof f);
    CHECK(f[0]==0xAB && f[1]==0xCD && f[2]==0x01 && f[3]==0x01 && f[4]==PSU_ADDR, "header/cmd/addr");
    CHECK(f[11]==0xE1 && f[12]==0xE2, "tail");
    uint16_t crc = (uint16_t)((f[9] << 8) | f[10]);
#if PSU_CRC_INCLUDE_CMD
    CHECK(crc == Crc16_Compute(&f[2], 7), "CRC phu CMD..payload (byte 3..9 theo tai lieu)");
#else
    CHECK(crc == Crc16_Compute(&f[4], 5), "CRC phu ADDR..payload");
#endif
    CHECK(Psu_FrameValid(f, sizeof f), "khung hop le");
    f[6] ^= 1; CHECK(!Psu_FrameValid(f, sizeof f), "doi 1 byte payload -> CRC sai"); f[6] ^= 1;
#if PSU_CRC_INCLUDE_CMD
    f[3] ^= 1; CHECK(!Psu_FrameValid(f, sizeof f), "doi CMD -> CRC sai"); f[3] ^= 1;
#endif   /* CRC tu ADDR: loi CMD khong bi CRC bat (chi bi chan boi bang do dai/CMD hop le) */

    /* 2. Do dai theo CMD khop tai lieu */
    CHECK(Psu_RxFrameLen(0x0101)==13 && Psu_RxFrameLen(0x0202)==13 && Psu_RxFrameLen(0x0303)==13, "13 byte");
    CHECK(Psu_RxFrameLen(0x0404)==915, "04 04 = 915");
    CHECK(Psu_RxFrameLen(0x9090)==21 && Psu_RxFrameLen(0x9191)==269, "BEGIN/DATA");
    CHECK(Psu_RxFrameLen(0x9292)==9 && Psu_RxFrameLen(0x9393)==9 && Psu_RxFrameLen(0x9595)==9 && Psu_RxFrameLen(0x9696)==9 && Psu_RxFrameLen(0x9797)==9, "ctrl 9");
    CHECK(Psu_RxFrameLen(0x8181)==0 && Psu_RxFrameLen(0x8282)==0 && Psu_RxFrameLen(0x9494)==0, "khung PSU->PC khong phai lenh nhan");
    CHECK(Psu_RxFrameLen(0x0102)==0 && Psu_RxFrameLen(0x0001)==0, "CMD 2 byte khac nhau khong hop le");

    /* 3. Bo tach khung: nhieu chuan, "AB AB CD", khung dang do, 2 khung lien tiep */
    Psu_RxInit(&rx, rxbuf, sizeof rxbuf);
    uint8_t stream[64]; uint16_t n = 0;
    stream[n++] = 0x00; stream[n++] = 0xAB; stream[n++] = 0x12;     /* rac */
    stream[n++] = 0xAB;                                               /* AB AB CD ... */
    memcpy(&stream[n], f, sizeof f); n += sizeof f;
    CHECK(feed_all(stream, n) == 13 && Psu_FrameValid(rxbuf, 13), "tach khung giua rac, AB AB CD");

    uint8_t two[26]; memcpy(two, f, 13); Psu_FrameBegin(f, 13, PSU_CMD_MON_REQ); Psu_FrameSeal(f, 13); memcpy(two+13, f, 13);
    int count = 0; for (int i = 0; i < 26; i++) if (Psu_RxFeed(&rx, two[i])) count++;
    CHECK(count == 2, "2 khung lien tiep (=%d)", count);

    /* khung dang do roi khung dung ngay sau: AB CD 01 01 .. (thieu) roi khung day du -> khung sau van tach duoc? */
    Psu_RxReset(&rx);
    uint8_t half[6] = {0xAB,0xCD,0x03,0x03,0x01,0x00};
    for (int i = 0; i < 6; i++) Psu_RxFeed(&rx, half[i]);
    Psu_RxReset(&rx);                                  /* nguoi goi resync khi im lang lau */
    Psu_FrameBegin(f, 13, PSU_CMD_CFG_QUERY); Psu_FrameSeal(f, 13);
    CHECK(feed_all(f, 13) == 13, "sau resync tach duoc khung moi");

    /* CMD la -> bo, byte do co the la AB cua khung ke tiep */
    Psu_RxReset(&rx);
    uint8_t seq[32]; n = 0; seq[n++]=0xAB; seq[n++]=0xCD; seq[n++]=0x77; seq[n++]=0xAB;   /* CMD 77 AB khong hop le, AB bat dau khung moi */
    memcpy(&seq[n], &f[1], 12); n += 12;                                                  /* f[1..12] sau AB */
    CHECK(feed_all(seq, n) == 13 && Psu_FrameValid(rxbuf, 13), "CMD la, byte cuoi la AB bat dau khung moi");

    /* Khung dai qua cap bi bo */
    uint8_t small[16]; psu_rx_t r2; Psu_RxInit(&r2, small, sizeof small);
    uint8_t big[PSU_LEN_CFG_WRITE]; Psu_FrameBegin(big, sizeof big, PSU_CMD_CFG_WRITE); Psu_FrameSeal(big, sizeof big);
    int got = 0; for (unsigned i = 0; i < sizeof big; i++) if (Psu_RxFeed(&r2, big[i])) got++;
    CHECK(got == 0, "khung 915 qua cap 16 bi bo");

    /* Khung 915 va 269 day du */
    Psu_RxReset(&rx);
    CHECK(feed_all(big, sizeof big) == 915 && Psu_FrameValid(rxbuf, 915), "khung 915");
    uint8_t d269[PSU_LEN_FW_DATA]; Psu_FrameBegin(d269, sizeof d269, PSU_CMD_FW_DATA); Psu_FrameSeal(d269, sizeof d269);
    CHECK(feed_all(d269, sizeof d269) == 269 && Psu_FrameValid(rxbuf, 269), "khung DATA 269");

    /* Duoi sai -> khong bao khung */
    d269[268] ^= 1; Psu_RxReset(&rx);
    CHECK(feed_all(d269, sizeof d269) == 0, "duoi sai khong duoc bao khung");

    printf("psu_frame tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
