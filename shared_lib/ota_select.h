#ifndef OTA_SELECT_H_
#define OTA_SELECT_H_

#include "ota_image.h"
#include "cfg_record.h"

/* So lan boot toi da cho mot anh CHUA confirm. Lan boot thu (N+1) tro di anh bi LOAI. */
#define OTA_MAX_BOOT_ATTEMPTS   3u

/** App co the khoi dong khong? Thoa TAT CA:
 *    - anh hop le (CRC32 toan anh + vector table)
 *    - khong bi danh dau hong (boot_count != CFG_BOOT_COUNT_FORCE_FAIL; app dung co nay khi
 *      nhan lenh ENTER_RECOVERY)
 *    - chua het luot: da confirm HOAC boot_count < OTA_MAX_BOOT_ATTEMPTS
 *  App het luot ma chua confirm KHONG BAO GIO duoc chon lai (ke ca anh van hop le) cho den khi
 *  duoc nap lai (recovery ghi lai boot record). */
bool Ota_SlotBootable(ota_slot_t sl);

/** Tra ve OTA_SLOT_APP neu app khoi dong duoc, nguoc lai OTA_SLOT_NONE -> bootloader vao recovery.
 *  @param tick  goi sau khi kiem tra xong de feed watchdog (co the NULL) */
ota_slot_t Ota_SelectSlot(void (*tick)(void));

/** Goi NGAY TRUOC khi nhay vao `sl`: neu anh chua confirm thi ghi boot_count+1.
 *  @return false neu khong ghi duoc bo dem (cfg/flash hong): trial khong con bao ve, nhung van
 *          khoi dong de thiet bi khong chet. */
bool Ota_CommitTrial(ota_slot_t sl);

#endif /* OTA_SELECT_H_ */
