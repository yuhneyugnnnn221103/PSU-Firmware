/**
 ******************************************************************************
 * @file    cfg_store.h
 * @brief   Doc/ghi ban ghi trong sector Config (128 KB, xem ota_map.h).
 *          Dung CHUNG cho ca bootloader (trang thai boot cua tung slot) va
 *          ung dung chinh (nguong OV/OC/UV + tu xac nhan "confirmed" sau
 *          khi chay on dinh).
 *
 * CO CHE: log noi tiep (append-only) trong toan bo sector, moi ban ghi
 * CFG_REC_SIZE byte, danh dau bang "seq" tang dan. Doc = quet toan sector,
 * lay ban ghi seq lon nhat CON HOP LE (CRC dung) cho tung (type, slot).
 * Ghi = them ban ghi MOI vao vi tri trong ke tiep - KHONG bao gio ghi de
 * len ban ghi cu, tranh vi pham gioi han "ghi 1 lan/flash-word".
 *
 * Khi sector day (het cho): NEN (compact) - doc lai ban ghi moi nhat cua
 * TUNG stream (Limits, Boot-A, Boot-B), xoa ca sector, ghi lai 3 ban ghi
 * do vao dau sector roi tiep tuc nhu binh thuong. Duong nay CHI chay sau
 * ~2048 lan ghi (xem CFG_SLOTS_PER_SECTOR trong ota_map.h) - rat hiem
 * trong doi thuc te cua thiet bi, nhung PHAI dung vi day la duong duy
 * nhat giai phong khong gian.
 ******************************************************************************
 */

#ifndef CFG_STORE_H_
#define CFG_STORE_H_

#include <stdint.h>
#include <stdbool.h>
#include "cfg_record.h"

void CfgStore_Init(void);

bool CfgStore_ReadBoot(ota_slot_t slot, cfg_boot_payload_t *out);
bool CfgStore_WriteBoot(ota_slot_t slot, uint32_t boot_count, uint8_t confirmed);

bool CfgStore_ReadLimits(cfg_limits_payload_t *out);
bool CfgStore_WriteLimits(const cfg_limits_payload_t *in);

uint32_t CfgStore_AllocInstallSeq(void);

#endif /* CFG_STORE_H_ */
