#ifndef OTA_SELECT_H_
#define OTA_SELECT_H_

#include "ota_image.h"
#include "cfg_record.h"

/* So lan boot toi da cho mot anh CHUA confirm. Lan boot thu (N+1) tro di anh bi LOAI. */
#define OTA_MAX_BOOT_ATTEMPTS   3u

/** Chon slot de khoi dong. Chi nhan slot thoa TAT CA:
 *    - anh hop le (CRC32 toan anh + vector table)
 *    - khong bi PC rollback (boot_count != CFG_BOOT_COUNT_FORCE_FAIL)
 *    - chua het luot: da confirm HOAC boot_count < OTA_MAX_BOOT_ATTEMPTS
 *  Slot het luot ma chua confirm KHONG BAO GIO duoc chon lai (ke ca anh van
 *  hop le) cho den khi duoc nap lai (OTA / recovery ghi lai boot record).
 *  Nhieu ung vien: install_seq lon hon thang, bang nhau thi version lon hon.
 *  @param tick  goi sau moi slot de feed watchdog (co the NULL)
 *  @retval OTA_SLOT_NONE neu khong con slot nao -> bootloader vao recovery */
ota_slot_t Ota_SelectSlot(void (*tick)(void));

/** Slot co the khoi dong khong (dieu kien nhu Ota_SelectSlot, cho 1 slot). */
bool Ota_SlotBootable(ota_slot_t sl);

/** Goi NGAY TRUOC khi nhay vao `sl` (da chon boi Ota_SelectSlot): neu anh chua
 *  confirm thi ghi boot_count+1. Neu ghi that bai (flash/cfg hong) thi loi
 *  bao ve trial mat tac dung -> uu tien slot kia NEU no da confirm va khoi dong
 *  duoc; khong co thi van boot `sl` (khong de thiet bi chet).
 *  @return slot se thuc su khoi dong */
ota_slot_t Ota_CommitTrial(ota_slot_t sl);

#endif /* OTA_SELECT_H_ */
