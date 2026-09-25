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
