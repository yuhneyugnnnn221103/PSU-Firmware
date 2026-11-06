#pragma once
/* Stub toi thieu CHI de kiem tra cu phap (gcc -fsyntax-only) cac file App tren host. KHONG phai HAL that. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { volatile uint32_t BSRR, IDR, ODR; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
#define GPIO_PIN_0  0x0001u
#define GPIO_PIN_1  0x0002u
#define GPIO_PIN_2  0x0004u
#define GPIO_PIN_3  0x0008u
#define GPIO_PIN_4  0x0010u
#define GPIO_PIN_5  0x0020u
#define GPIO_PIN_6  0x0040u
#define GPIO_PIN_7  0x0080u
#define GPIO_PIN_8  0x0100u
#define GPIO_PIN_9  0x0200u
#define GPIO_PIN_10 0x0400u
#define GPIO_PIN_11 0x0800u
#define GPIO_PIN_12 0x1000u
#define GPIO_PIN_13 0x2000u
#define GPIO_PIN_14 0x4000u
#define GPIO_PIN_15 0x8000u
#define GPIOA ((GPIO_TypeDef *)0x58020000)
#define GPIOB ((GPIO_TypeDef *)0x58020400)
#define GPIOC ((GPIO_TypeDef *)0x58020800)
#define GPIOD ((GPIO_TypeDef *)0x58020C00)
#define GPIOE ((GPIO_TypeDef *)0x58021000)
typedef struct { void *Instance; uint32_t ErrorCode; void *hdmarx; void *hdmatx; uint32_t RxState, gState; } UART_HandleTypeDef;
#define HAL_UART_STATE_READY 0x20u
#define HAL_UART_STATE_BUSY_RX 0x22u
typedef struct { void *Instance; } I2C_HandleTypeDef;
typedef struct { void *Instance; } DMA_HandleTypeDef;
#define HAL_UART_ERROR_NONE 0u
#define HAL_UART_ERROR_PE 1u
#define HAL_UART_ERROR_NE 2u
#define HAL_UART_ERROR_FE 4u
#define HAL_UART_ERROR_ORE 8u
#define HAL_UART_ERROR_DMA 16u
extern UART_HandleTypeDef huart1, huart4, huart5;
extern I2C_HandleTypeDef hi2c1;
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef *p, uint16_t pin);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
HAL_StatusTypeDef HAL_UART_AbortTransmit_IT(UART_HandleTypeDef *h);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h);
HAL_StatusTypeDef HAL_UART_DMAStop(UART_HandleTypeDef *h);
uint32_t HAL_UART_GetError(UART_HandleTypeDef *h);
uint32_t __HAL_DMA_GET_COUNTER_STUB(void *h);
#define __HAL_DMA_GET_COUNTER(h) __HAL_DMA_GET_COUNTER_STUB(h)
#define __HAL_UART_CLEAR_OREFLAG(h) ((void)(h))
#define __HAL_UART_CLEAR_FEFLAG(h)  ((void)(h))
#define __HAL_UART_CLEAR_NEFLAG(h)  ((void)(h))
#define __HAL_UART_CLEAR_PEFLAG(h)  ((void)(h))
#define __enable_irq() ((void)0)
#define __disable_irq() ((void)0)
#define __NOP() ((void)0)
#define __DSB() ((void)0)
#define __ISB() ((void)0)
void NVIC_SystemReset(void);
