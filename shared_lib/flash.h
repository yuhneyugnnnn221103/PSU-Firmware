#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>
#include <stdbool.h>

#include "stm32h7xx_hal.h"

typedef struct {
    bool     ok;
    uint32_t hal_error;   /* HAL_FLASH_GetError(), chỉ có ý nghĩa khi ok == false */
} flash_op_result_t;

flash_op_result_t Flash_ProgramWords(uint32_t addr, const uint8_t *data, uint32_t len);

flash_op_result_t Flash_EraseSector(uint32_t sector_num);

#endif /* FLASH_H_ */
