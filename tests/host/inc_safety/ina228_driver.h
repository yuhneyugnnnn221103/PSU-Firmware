#pragma once
/* Ban gia: chi cac truong safety.c dung. Hang so lay tu BSP/ina228_cfg.h that. */
#include <ina228_cfg.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint16_t diag_sticky; bool alert_new; bool cfg_ok; uint32_t last_ok_ms; } ina228_dev_t;
ina228_dev_t *ina228_get_dev(uint8_t idx);
static inline bool ina228_is_fresh(const ina228_dev_t *d, uint32_t now_ms) { return (uint32_t)(now_ms - d->last_ok_ms) < INA228_STALE_MS; }
