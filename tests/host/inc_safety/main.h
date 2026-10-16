#pragma once
/* Ban gia toi thieu cua main.h/HAL de bien dich safety.c tren host */
#include <stdint.h>
#include <stdbool.h>
typedef struct { volatile uint32_t BSRR; } GPIO_TypeDef;
extern GPIO_TypeDef fake_gpiob;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
#define PWR_EN_CTRL_Pin        ((uint16_t)0x0004u)
#define PWR_EN_CTRL_GPIO_Port  (&fake_gpiob)
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState st);
