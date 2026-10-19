#ifndef CFG_STORE_H_
#define CFG_STORE_H_

#include <stdint.h>
#include <stdbool.h>
#include "cfg_record.h"

void CfgStore_Init(void);

bool CfgStore_ReadBoot(ota_slot_t slot, cfg_boot_payload_t *out);
bool CfgStore_WriteBoot(ota_slot_t slot, uint32_t boot_count, uint8_t confirmed);

/** Doc blob da luu. @return false neu chua co / cap qua nho. */
bool CfgStore_ReadBlob(uint8_t id, void *buf, uint16_t cap, uint16_t *len_out);

/** Ghi blob (<= CFG_BLOB_MAX byte). Mat dien giua chung: van doc duoc ban cu. */
bool CfgStore_WriteBlob(uint8_t id, const void *data, uint16_t len);

uint32_t CfgStore_AllocInstallSeq(void);

/** @brief true neu lan ghi ke tiep (boot record HOAC blob day du) co the PHAI XOA sector (khoi tao bank hoac compaction).
 *         Xoa sector chan CPU ~vai tram ms -> khong nen lam khi dau ra dang bat. */
bool CfgStore_NeedsErase(void);

#endif /* CFG_STORE_H_ */
