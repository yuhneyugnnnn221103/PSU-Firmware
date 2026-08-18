/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FT232_CTS_Pin GPIO_PIN_2
#define FT232_CTS_GPIO_Port GPIOA
#define FT232_RTS_Pin GPIO_PIN_3
#define FT232_RTS_GPIO_Port GPIOA
#define DCM_CUR_ALRT1_Pin GPIO_PIN_4
#define DCM_CUR_ALRT1_GPIO_Port GPIOA
#define DCM_CUR_ALRT1_EXTI_IRQn EXTI4_IRQn
#define DCM_CUR_ALRT4_Pin GPIO_PIN_5
#define DCM_CUR_ALRT4_GPIO_Port GPIOA
#define DCM_CUR_ALRT4_EXTI_IRQn EXTI9_5_IRQn
#define DCM_CUR_ALRT3_Pin GPIO_PIN_6
#define DCM_CUR_ALRT3_GPIO_Port GPIOA
#define DCM_CUR_ALRT3_EXTI_IRQn EXTI9_5_IRQn
#define DCM_CUR_ALRT2_Pin GPIO_PIN_7
#define DCM_CUR_ALRT2_GPIO_Port GPIOA
#define DCM_CUR_ALRT2_EXTI_IRQn EXTI9_5_IRQn
#define FT_SEC1_Pin GPIO_PIN_4
#define FT_SEC1_GPIO_Port GPIOC
#define FT_SEC2_Pin GPIO_PIN_5
#define FT_SEC2_GPIO_Port GPIOC
#define FT_SEC3_Pin GPIO_PIN_0
#define FT_SEC3_GPIO_Port GPIOB
#define FT_SEC4_Pin GPIO_PIN_1
#define FT_SEC4_GPIO_Port GPIOB
#define RS422_DE1_Pin GPIO_PIN_2
#define RS422_DE1_GPIO_Port GPIOB
#define RS422_RE1_Pin GPIO_PIN_10
#define RS422_RE1_GPIO_Port GPIOB
#define RS422_DE2_Pin GPIO_PIN_6
#define RS422_DE2_GPIO_Port GPIOC
#define RS422_RE2_Pin GPIO_PIN_9
#define RS422_RE2_GPIO_Port GPIOC
#define GR_LED_Pin GPIO_PIN_11
#define GR_LED_GPIO_Port GPIOC
#define RD_LED_Pin GPIO_PIN_12
#define RD_LED_GPIO_Port GPIOC
#define BL_LED_Pin GPIO_PIN_2
#define BL_LED_GPIO_Port GPIOD
#define PWR_EN_CTRL_Pin GPIO_PIN_9
#define PWR_EN_CTRL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
