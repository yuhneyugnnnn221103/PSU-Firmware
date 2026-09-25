#ifndef FW_UPDATE_H_
#define FW_UPDATE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FWU_IDLE = 0,       /* khong co phien OTA nao dang chay        */
    FWU_ERASING,        /* dang xoa sector, chia nho qua nhieu tick */
    FWU_RECEIVING,      /* dang nhan FW_DATA                        */
    FWU_VERIFIED        /* FW_END da xac nhan CRC dung, cho COMMIT  */
} fwu_state_t;

void FwUpdate_Init(void);

/** @brief Goi moi vong main loop. Chia nho viec xoa sector qua nhieu tick
 *         (xem FWU_ERASING) de khong block Safety_Task qua lau 1 lan. */
void FwUpdate_Task(uint32_t now_ms);

/* --- Xu ly tung lenh, goi tu rx_exec() trong comm.c ---
 * Tra ve ma trang thai TLM_FW_ACK_* (dinh nghia trong comm.h) de comm.c
 * dong khung FW_ACK gui lai PC. out_info duoc dien tuy lenh (vd last_seq
 * cho FW_DATA) - xem comm.h TLM_FW_ACK_OFF_INFO. */
uint8_t FwUpdate_Begin(uint32_t total_size, uint32_t crc32_total,
                       uint32_t version, uint32_t *out_info);
uint8_t FwUpdate_Data(uint16_t seq, uint16_t chunk_len,
                      const uint8_t *data, uint32_t *out_info);
uint8_t FwUpdate_End(uint32_t *out_info);

/** @brief Tra true neu da FWU_VERIFIED (FW_END thanh cong) VA duoc phep
 *         reset. KHONG tu reset trong ham nay - comm.c goi sau khi da
 *         gui xong FW_ACK, de PC chac chan nhan duoc ACK truoc khi mat
 *         ket noi do reset. */
bool FwUpdate_CanCommit(void);

/** @brief Thuc hien reset he thong. Goi SAU khi FW_ACK cho FW_COMMIT da
 *         duoc dua vao hang doi TX (khong nhat thiet da gui xong tren
 *         day - UART DMA se hoan tat truyen truoc khi HAL_NVIC_SystemReset
 *         thuc su cat nguon logic, nhung KHONG dam bao 100%; xem ghi chu
 *         trong fw_update.c ve do tre toi thieu truoc reset). */
void FwUpdate_Commit(void);

fwu_state_t FwUpdate_State(void);

/** @brief Co phien OTA dang chay khong (ERASING/RECEIVING/VERIFIED)?
 *
 *  Dung de TAM DUNG cac luong phat khac trong main loop. Ly do ky thuat:
 *  FW_ACK dung chung buffer aux voi telemetry/STATUS qua aux_acquire(),
 *  ma ham do tu choi cap buffer neu BAT KY cong nao dang ban. Telemetry
 *  phat moi 60 ms len ca 3 cong (~5 ms/khung) -> cu 60 ms lai co cua so
 *  ~5 ms khien fw_send_ack() that bai IM LANG, PC khong nhan ACK, phai
 *  cho het timeout roi gui lai chunk. Voi hang tram chunk, va cham tich
 *  luy lai gay cham hoac hong ca phien.
 *
 *  KHONG dung de tat Safety_Task() - do la lop bao ve, phai luon chay. */
static inline bool FwUpdate_IsBusy(void)
{
    const fwu_state_t st = FwUpdate_State();
    return (st != FWU_IDLE);
}

/** @brief Lay (va XOA) thong tin loi xoa sector gan nhat.
 *  @param out_err  nhan ma loi HAL (HAL_FLASH_GetError()) de PC chan doan.
 *  @retval true neu vua co mot lan xoa THAT BAI chua duoc bao cao.
 *
 *  Ly do can ham nay: khi xoa loi, FwUpdate_Task() dua state ve FWU_IDLE.
 *  Comm_FwPoll() chi phat ACK khi thay chuyen ERASING -> RECEIVING, nen
 *  chuyen ERASING -> IDLE se KHONG sinh ACK nao - PC cho het timeout roi
 *  bao "thiet bi treo", trong khi thuc te thiet bi van chay binh thuong
 *  va chi im lang nuot loi. */
bool FwUpdate_TakeEraseError(uint32_t *out_err);

bool FwUpdate_TakeTimeout(void);

/** @brief Version cua CHINH anh dang chay, doc tu header cua slot minh.
 *  @retval 0 neu header khong hop le (vd nap tay qua SWD ma quen header). */
uint32_t FwUpdate_RunningVersion(void);

bool FwUpdate_RequestRollBack(void);

char FwUpdate_RunningSlot(void);   /* 'A'/'B', '?' neu link sai */

#endif /* FW_UPDATE_H_ */
