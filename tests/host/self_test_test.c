/* Test tu kiem tra truoc khi confirm (self_test.c that). */
#include <stdio.h>
#include <ina228_cfg.h>
#include "self_test.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

int main(void)
{
    selftest_in_t good = { INA228_CH_COUNT, SAFE_OFF, false };
    selftest_in_t none = { 0, SAFE_OFF, false };
    selftest_in_t boot = { INA228_CH_COUNT, SAFE_BOOT, false };

    SelfTest_Reset();
    CHECK(!SelfTest_Update(0, &good), "t=0 chua du cua so");
    CHECK(!SelfTest_Update(SELFTEST_WINDOW_MS - 1, &good), "chua du cua so");
    CHECK(SelfTest_Update(SELFTEST_WINDOW_MS, &good), "du cua so lien tuc -> confirm");

    /* dieu kien sai giua chung -> dem lai tu dau */
    SelfTest_Reset();
    SelfTest_Update(0, &good);
    SelfTest_Update(SELFTEST_WINDOW_MS / 2, &none);                      /* mat cam bien */
    CHECK(!SelfTest_Update(SELFTEST_WINDOW_MS, &good), "cua so phai dem lai sau khi sai");
    CHECK(!SelfTest_Update(SELFTEST_WINDOW_MS + SELFTEST_WINDOW_MS - 1, &good), "chua du cua so moi");
    CHECK(SelfTest_Update(2 * SELFTEST_WINDOW_MS, &good), "du cua so moi");

    /* khong cam bien nao -> khong bao gio confirm, du troi qua bao lau */
    SelfTest_Reset();
    for (uint32_t t = 0; t <= 5 * SELFTEST_WINDOW_MS; t += 500) CHECK(!SelfTest_Update(t, &none), "khong cam bien khong duoc confirm");

    /* may trang thai safety chua chay -> khong confirm */
    SelfTest_Reset();
    for (uint32_t t = 0; t <= 3 * SELFTEST_WINDOW_MS; t += 500) CHECK(!SelfTest_Update(t, &boot), "SAFE_BOOT khong duoc confirm");

    /* tran so thoi gian (uint32 quay vong) */
    SelfTest_Reset();
    uint32_t t0 = 0xFFFFFFFFu - 4000u;
    SelfTest_Update(t0, &good);
    CHECK(!SelfTest_Update(t0 + 9000u, &good), "quay vong: chua du");
    CHECK(SelfTest_Update(t0 + SELFTEST_WINDOW_MS, &good), "quay vong: du");

    /* trip khong phai tieu chi that bai */
    selftest_in_t tripped = { INA228_CH_COUNT, SAFE_TRIPPED, false };
    SelfTest_Reset(); SelfTest_Update(0, &tripped);
    CHECK(SelfTest_Update(SELFTEST_WINDOW_MS, &tripped), "TRIPPED van duoc tinh la firmware chay tot");

    printf("self_test tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
