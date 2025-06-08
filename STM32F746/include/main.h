#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes */
#include "stm32f7xx_hal.h"

/* Private defines */
#define LD1_Pin GPIO_PIN_1
#define LD1_GPIO_Port GPIOI
#define USER_Btn_Pin GPIO_PIN_11
#define USER_Btn_GPIO_Port GPIOI
#define USER_Btn_EXTI_IRQn EXTI15_10_IRQn

/* Exported functions prototypes */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/* File: stm32f7xx_hal_msp.c */
#include "main.h"

void HAL_MspInit(void) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    
    /* System interrupt init*/
    /* PendSV_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
    
    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if(huart->Instance==USART1) {
        /* Peripheral clock enable */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        
        /* USART1 GPIO Configuration
           PA9     ------> USART1_TX
           PB7     ------> USART1_RX */
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* huart) {
    if(huart->Instance==USART1) {
        /* Peripheral clock disable */
        __HAL_RCC_USART1_CLK_DISABLE();
        
        /* USART1 GPIO Configuration */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9);
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base) {
    if(htim_base->Instance==TIM2) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base) {
    if(htim_base->Instance==TIM2) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM2_CLK_DISABLE();
    }
}