#ifndef SELF_TEST_H_
#define SELF_TEST_H_

#include <stdint.h>
#include <stdbool.h>
#include <safety.h>

/* Tu kiem tra truoc khi xac nhan (confirm) anh moi. Anh chi duoc confirm khi
 * TAT CA dieu kien duoi day dung LIEN TUC trong SELFTEST_WINDOW_MS. Dieu kien
 * sai o bat ky thoi diem nao -> dong ho dem lai tu dau.
 *
 * Khong dung loi tai/trip lam tieu chi: trip la bao ve dung thiet ke, khong phai
 * firmware hong. Tieu chi chi cho thay firmware CHAY DUOC: main loop song, doc duoc
 * cam bien, may trang thai safety da khoi dong. */

#ifndef SELFTEST_WINDOW_MS
#define SELFTEST_WINDOW_MS      10000u
#endif

/* So kenh INA228 toi thieu phai "tuoi" (du lieu cap nhat) va cau hinh dung. Mac dinh 1:
 * mot cam bien hong phan cung khong lam anh MOI khong the confirm. Dat bang
 * INA228_CH_COUNT neu muon nghiem ngat. */
#ifndef SELFTEST_MIN_CH
#define SELFTEST_MIN_CH         1u
#endif

/* 1 = yeu cau da nhan it nhat 1 khung hop le tu PC gan day. Mac dinh 0: thiet bi
 * chay doc lap khong co PC van phai confirm duoc. */
#ifndef SELFTEST_NEED_PC_LINK
#define SELFTEST_NEED_PC_LINK   0
#endif

typedef struct {
    uint8_t      ch_ok;      /* so kenh fresh && cfg_ok                  */
    safe_state_t safety;     /* trang thai may safety                    */
    bool         pc_link;    /* Comm_PcLinkAlive()                       */
} selftest_in_t;

void SelfTest_Reset(void);

/** Goi dinh ky (moi vong main loop). @return true khi du dieu kien lien tuc du cua so. */
bool SelfTest_Update(uint32_t now_ms, const selftest_in_t *in);

#endif /* SELF_TEST_H_ */
