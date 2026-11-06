#ifndef FW_UPDATE_H_
#define FW_UPDATE_H_

#include <stdint.h>
#include <stdbool.h>

/* App KHONG nap firmware. Viec nap (ERASE/BEGIN/DATA/END/COMMIT) chi do bootloader/recovery lam; app chi:
 *   - tra loi INFO (version, trang thai trial cua chinh minh);
 *   - nhan ENTER_REC: danh dau "vao recovery" trong cfg roi reset (bootloader o lai recovery cho PC nap). */

void FwUpdate_Init(void);

/** Goi moi vong main loop: thuc hien reset da hen sau ENTER_REC. */
void FwUpdate_Task(uint32_t now_ms);

/** Yeu cau vao recovery.
 *  @return OTAP_ST_OK (se reset ~50 ms sau, de ACK kip gui) | REFUSED (con cum khong OFF) |
 *          WRONG_SLOT (khong biet dang chay vung app nao: link sai) | PROG_FAIL (khong ghi duoc cfg) */
uint8_t FwUpdate_RequestEnterRecovery(void);

/** Version cua CHINH anh dang chay, doc tu header. 0 neu header khong hop le (vd nap tay qua SWD, khong header). */
uint32_t FwUpdate_RunningVersion(void);

/** Trang thai trial de bao trong INFO: bit7 = da confirm, bit6..0 = boot_count (127 = bi danh dau/lon hon). 0 neu khong co ban ghi. */
uint8_t FwUpdate_RunningState(void);

/** Chuoi version (32 byte ASCII, dem 00) trong header anh dang chay; rong neu khong co. */
void FwUpdate_VersionStr(char out[32]);

char FwUpdate_RunningSlot(void);   /* 'A' (app), '?' neu link sai */

#endif /* FW_UPDATE_H_ */
