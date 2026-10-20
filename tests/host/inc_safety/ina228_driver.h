#pragma once
/* Ban gia: chi cac truong safety.c dung. Hang so lay tu BSP/ina228_cfg.h that. */
#include <ina228_cfg.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t timestamp_ms; } ina228_meas_t;
typedef struct { uint32_t vbus_reg24, current_reg24; int16_t dietemp_raw; } ina228_raw_t;
typedef struct { uint16_t diag_sticky; bool alert_new; bool cfg_ok; ina228_meas_t meas; ina228_raw_t raw; } ina228_dev_t;
ina228_dev_t *ina228_get_dev(uint8_t idx);
static inline bool ina228_is_fresh(const ina228_dev_t *d, uint32_t now_ms) { return (uint32_t)(now_ms - d->meas.timestamp_ms) < INA228_STALE_MS; }
