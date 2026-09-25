#include "flash.h"
#include "iwdg_hw.h"

flash_op_result_t Flash_ProgramWords(uint32_t addr, const uint8_t *data, uint32_t len)
{
	flash_op_result_t r = {0};

	HAL_FLASH_Unlock();

	HAL_StatusTypeDef st = HAL_OK;
	for (uint32_t w = 0; w < (len / 32u); w++) {
		const uint32_t word_address = addr + 32u * w;
		const uint32_t data_address = (uint32_t)(data + 32u * w);

		st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, word_address, data_address);

		if (st != HAL_OK) break;
	}

	if (st == HAL_OK) {
		r.ok = true;
	}
	else {
		r.hal_error = HAL_FLASH_GetError();
	}

	HAL_FLASH_Lock();
	return r;
}

flash_op_result_t Flash_EraseSector(uint32_t sector_num)
{
	flash_op_result_t r = {0};

	FLASH_EraseInitTypeDef e = {0};
	e.TypeErase 	= FLASH_TYPEERASE_SECTORS;
	e.Banks			= FLASH_BANK_1;
	e.NbSectors		= 1u;
	e.VoltageRange 	= FLASH_VOLTAGE_RANGE_3;
	e.Sector		= sector_num;

	uint32_t sector_error = 0xFFFFFFFFu;

	HAL_FLASH_Unlock();

	IWDG_Refresh();
	const HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&e, &sector_error);
	IWDG_Refresh();

	if (st == HAL_OK && sector_error == 0xFFFFFFFFu) {
		r.ok = true;
	}
	else {
		r.hal_error = HAL_FLASH_GetError();
	}

	HAL_FLASH_Lock();
	return r;
}
