#include "main.h"
#include "cmsis_os.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

/* Private variables */
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;

/* FreeRTOS handles */
osThreadId_t doorTaskHandle;
osThreadId_t uartRxTaskHandle;
osThreadId_t uartTxTaskHandle;
osMessageQueueId_t txQueueHandle;
osSemaphoreId_t doorStateSemHandle;

/* Global variables */
typedef enum {
    DOOR_LOCKED,
    DOOR_UNLOCKED
} DoorState_t;

static DoorState_t doorState = DOOR_LOCKED;
static uint8_t rxBuffer[256];
static uint8_t rxChar;
static uint32_t rxIndex = 0;

/* Function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void DoorTask(void *argument);
static void UartRxTask(void *argument);
static void UartTxTask(void *argument);
static void SendDoorEvent(const char* event);
static void ProcessJsonMessage(const char* message);
static void ToggleDoorState(void);

int main(void) {
    /* Reset peripherals, Initialize Flash and Systick */
    HAL_Init();
    
    /* Configure system clock */
    SystemClock_Config();
    
    /* Initialize peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    
    /* Initialize kernel */
    osKernelInitialize();
    
    /* Create semaphore for door state protection */
    doorStateSemHandle = osSemaphoreNew(1, 1, NULL);
    
    /* Create message queue for UART TX */
    txQueueHandle = osMessageQueueNew(10, 256, NULL);
    
    /* Create tasks */
    const osThreadAttr_t doorTask_attributes = {
        .name = "doorTask",
        .stack_size = 512 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    doorTaskHandle = osThreadNew(DoorTask, NULL, &doorTask_attributes);
    
    const osThreadAttr_t uartRxTask_attributes = {
        .name = "uartRxTask",
        .stack_size = 512 * 4,
        .priority = (osPriority_t) osPriorityAboveNormal,
    };
    uartRxTaskHandle = osThreadNew(UartRxTask, NULL, &uartRxTask_attributes);
    
    const osThreadAttr_t uartTxTask_attributes = {
        .name = "uartTxTask",
        .stack_size = 512 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    uartTxTaskHandle = osThreadNew(UartTxTask, NULL, &uartTxTask_attributes);
    
    /* Start scheduler */
    osKernelStart();
    
    /* Should never reach here */
    while (1) {}
}

/* Door control task */
static void DoorTask(void *argument) {
    uint32_t lastBlinkTime = 0;
    uint8_t ledState = 0;
    
    /* Send initial state */
    SendDoorEvent("door_locked");
    
    /* Turn on LED (door locked) */
    HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
    
    for (;;) {
        /* Get current door state */
        osSemaphoreAcquire(doorStateSemHandle, osWaitForever);
        DoorState_t currentState = doorState;
        osSemaphoreRelease(doorStateSemHandle);
        
        if (currentState == DOOR_LOCKED) {
            /* LED continuously ON */
            HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
        } else {
            /* LED blinking every 500ms */
            uint32_t currentTime = osKernelGetTickCount();
            if (currentTime - lastBlinkTime >= 500) {
                ledState = !ledState;
                HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, ledState ? GPIO_PIN_SET : GPIO_PIN_RESET);
                lastBlinkTime = currentTime;
            }
        }
        
        osDelay(10);
    }
}

/* UART receive task */
static void UartRxTask(void *argument) {
    /* Start UART reception */
    HAL_UART_Receive_IT(&huart1, &rxChar, 1);
    
    for (;;) {
        /* Wait for signal from UART interrupt */
        uint32_t flags = osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
        
        if (flags & 0x01) {
            /* Process received character */
            if (rxChar == '\n' || rxChar == '\r') {
                if (rxIndex > 0) {
                    rxBuffer[rxIndex] = '\0';
                    ProcessJsonMessage((char*)rxBuffer);
                    rxIndex = 0;
                }
            } else if (rxIndex < sizeof(rxBuffer) - 1) {
                rxBuffer[rxIndex++] = rxChar;
            }
            
            /* Restart reception */
            HAL_UART_Receive_IT(&huart1, &rxChar, 1);
        }
    }
}

/* UART transmit task */
static void UartTxTask(void *argument) {
    char txBuffer[256];
    
    for (;;) {
        /* Wait for message in queue */
        if (osMessageQueueGet(txQueueHandle, txBuffer, NULL, osWaitForever) == osOK) {
            /* Transmit message */
            HAL_UART_Transmit(&huart1, (uint8_t*)txBuffer, strlen(txBuffer), 100);
            HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
        }
    }
}

/* Send door event */
static void SendDoorEvent(const char* event) {
    char message[256];
    
    /* Create JSON message */
    snprintf(message, sizeof(message), 
        "{\"source\":\"stm32\",\"event\":\"%s\",\"timestamp\":\"2025-06-08T%02d:%02d:%02dZ\"}", 
        event,
        (int)(osKernelGetTickCount() / 3600000) % 24,
        (int)(osKernelGetTickCount() / 60000) % 60,
        (int)(osKernelGetTickCount() / 1000) % 60);
    
    /* Send to queue */
    osMessageQueuePut(txQueueHandle, message, 0, 0);
}

/* Process JSON message */
static void ProcessJsonMessage(const char* message) {
    /* Simple JSON parsing for command */
    if (strstr(message, "\"command\":\"lock\"") != NULL) {
        osSemaphoreAcquire(doorStateSemHandle, osWaitForever);
        if (doorState != DOOR_LOCKED) {
            doorState = DOOR_LOCKED;
            osSemaphoreRelease(doorStateSemHandle);
            SendDoorEvent("door_locked");
        } else {
            osSemaphoreRelease(doorStateSemHandle);
        }
        
        /* Send acknowledgment */
        char ack[] = "{\"type\":\"ack\",\"status\":\"ok\"}";
        osMessageQueuePut(txQueueHandle, ack, 0, 0);
    }
    else if (strstr(message, "\"command\":\"unlock\"") != NULL) {
        osSemaphoreAcquire(doorStateSemHandle, osWaitForever);
        if (doorState != DOOR_UNLOCKED) {
            doorState = DOOR_UNLOCKED;
            osSemaphoreRelease(doorStateSemHandle);
            SendDoorEvent("door_unlocked");
        } else {
            osSemaphoreRelease(doorStateSemHandle);
        }
        
        /* Send acknowledgment */
        char ack[] = "{\"type\":\"ack\",\"status\":\"ok\"}";
        osMessageQueuePut(txQueueHandle, ack, 0, 0);
    }
}

/* Toggle door state */
static void ToggleDoorState(void) {
    osSemaphoreAcquire(doorStateSemHandle, osWaitForever);
    
    if (doorState == DOOR_LOCKED) {
        doorState = DOOR_UNLOCKED;
        osSemaphoreRelease(doorStateSemHandle);
        SendDoorEvent("door_unlocked");
    } else {
        doorState = DOOR_LOCKED;
        osSemaphoreRelease(doorStateSemHandle);
        SendDoorEvent("door_locked");
    }
}

/* GPIO Initialization */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    
    /* Configure LD1 (PI1) */
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
    
    /* Configure User Button B1 (PI11) */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
    
    /* Enable and set EXTI line 11 Interrupt */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USART1 Initialization */
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    
    /* Enable UART interrupt */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* TIM2 Initialization */
static void MX_TIM2_Init(void) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 10000;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
    
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
}

/* System Clock Configuration */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    /* Initializes the RCC Oscillators */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 216;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    /* Activate the Over-Drive mode */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }
    
    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) {
        Error_Handler();
    }
}

/* EXTI interrupt handler for user button */
void EXTI15_10_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_11) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
        
        /* Toggle door state from ISR */
        ToggleDoorState();
    }
}

/* USART1 interrupt handler */
void USART1_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET) {
        /* Signal the RX task */
        osThreadFlagsSet(uartRxTaskHandle, 0x01);
    }
    
    HAL_UART_IRQHandler(&huart1);
}

/* Error handler */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        /* Toggle LED to indicate error */
        HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
        HAL_Delay(200);
    }
}