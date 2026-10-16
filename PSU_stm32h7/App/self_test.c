#include "self_test.h"

static bool     s_running;      /* dang trong mot cua so thoa dieu kien */
static uint32_t s_since_ms;

static bool conditions_ok(const selftest_in_t *in)
{
    if (in->ch_ok < SELFTEST_MIN_CH) return false;
    if (in->safety == SAFE_BOOT)     return false;     /* may trang thai safety chua chay xong */
#if SELFTEST_NEED_PC_LINK
    if (!in->pc_link)                return false;
#endif
    return true;
}

void SelfTest_Reset(void)
{
    s_running = false;
    s_since_ms = 0u;
}

bool SelfTest_Update(uint32_t now_ms, const selftest_in_t *in)
{
    if (!conditions_ok(in)) {
        s_running = false;
        return false;
    }
    if (!s_running) {
        s_running = true;
        s_since_ms = now_ms;
    }
    return (uint32_t)(now_ms - s_since_ms) >= SELFTEST_WINDOW_MS;
}
