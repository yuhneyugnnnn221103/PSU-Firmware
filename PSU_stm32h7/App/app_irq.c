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
	if (pin == DCM_CUR_ALRT1_Pin || pin == DCM_CUR_ALRT2_Pin ||
		pin == DCM_CUR_ALRT3_Pin || pin == DCM_CUR_ALRT4_Pin) {

		Safety_FastTrip();
		ina228_alert_isr(pin);
	}
}
