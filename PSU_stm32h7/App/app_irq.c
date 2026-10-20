#include "main.h"
#include "ina228_driver.h"
#include "safety.h"

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	ina228_i2c_rx_complete(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	ina228_i2c_error(hi2c);
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
	/* INA1..3 = DCM1..3 cua cum 1. ALERT4 (INA4) bo qua hoan toan. */
	int8_t ina = -1;
	if      (pin == DCM_CUR_ALRT1_Pin) ina = 0;
	else if (pin == DCM_CUR_ALRT2_Pin) ina = 1;
	else if (pin == DCM_CUR_ALRT3_Pin) ina = 2;
	if (ina < 0) return;

	Safety_FastTrip((uint8_t)ina);
	ina228_alert_isr(pin);
}
