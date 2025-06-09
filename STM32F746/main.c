/*
* Sir Locks-A-Lot - STM32F746 Discovery Board
* Version: 1.0
*
* Filename: main.c
* 
* Description:
* Door control program for STM32F746 Discovery board
* Communicates with Raspberry Pi via USB serial (USART1)
* Uses pushbutton B1 to toggle door state
* Sends JSON messages when button is pressed
*
* Hardware:
* - STM32F746G-DISCO board
* - USB connection to Raspberry Pi
* - User button B1 (PI11)
* - User LED LD1 (PI1) - Green LED indicates unlocked, off indicates locked
*
* JSON Format:
* {"source":"stm32","event":"lock|unlock","timestamp":"YYYY-MM-DD HH:MM:SS"}
*/

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Private variables */
UART_HandleTypeDef huart1;
RTC_HandleTypeDef hrtc;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
void Error_Handler(void);

/* Application variables */
typedef enum {
    DOOR_LOCKED = 0,
    DOOR_UNLOCKED = 1
} DoorState_t;

static DoorState_t door_state = DOOR_LOCKED;
static bool button_pressed = false;
static uint32_t last_button_time = 0;
static char rx_buffer[256];
static char tx_buffer[256];

/* Function to get current timestamp */
void get_timestamp(char* timestamp_str) {
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    
    sprintf(timestamp_str, "20%02d-%02d-%02d %02d:%02d:%02d", 
            sDate.Year, sDate.Month, sDate.Date,
            sTime.Hours, sTime.Minutes, sTime.Seconds);
}

/* Create JSON message */
void create_json_message(const char* event, char* json_str) {
    char timestamp[32];
    get_timestamp(timestamp);
    
    sprintf(json_str, "{\"source\":\"stm32\",\"event\":\"%s\",\"timestamp\":\"%s\"}", 
            event, timestamp);
}

/* Parse JSON value */
int parse_json_value(const char* json, const char* key, char* value, int max_len) {
    char search_key[64];
    sprintf(search_key, "\"%s\":\"", key);
    
    char* pos = strstr(json, search_key);
    if (pos == NULL) return 0;
    
    pos += strlen(search_key);
    char* end_pos = strchr(pos, '\"');
    if (end_pos == NULL) return 0;
    
    int len = end_pos - pos;
    if (len >= max_len) len = max_len - 1;
    
    strncpy(value, pos, len);
    value[len] = '\0';
    
    return 1;
}

/* Update door state and LED */
void update_door_state(DoorState_t new_state) {
    door_state = new_state;
    
    if (door_state == DOOR_UNLOCKED) {
        HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_SET);  // LED ON (unlocked)
    } else {
        HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_RESET); // LED OFF (locked)
    }
}

/* Send message to Raspberry Pi */
void send_to_raspberry_pi(const char* message) {
    HAL_UART_Transmit(&huart1, (uint8_t*)message, strlen(message), 1000);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\n", 1, 1000);
    
    printf("Sent: %s\n", message);
}

/* Process received message from Raspberry Pi */
void process_received_message(const char* message) {
    char source[32], event[32], timestamp[64];
    
    if (parse_json_value(message, "source", source, sizeof(source)) &&
        parse_json_value(message, "event", event, sizeof(event)) &&
        parse_json_value(message, "timestamp", timestamp, sizeof(timestamp))) {
        
        printf("Received from %s: %s at %s\n", source, event, timestamp);
        
        // Process commands from laptop or raspberry pi
        if (strcmp(event, "lock") == 0) {
            update_door_state(DOOR_LOCKED);
            printf("Door locked remotely\n");
        } else if (strcmp(event, "unlock") == 0) {
            update_door_state(DOOR_UNLOCKED);
            printf("Door unlocked remotely\n");
        } else if (strcmp(event, "status_request") == 0) {
            // Send current status
            const char* status_event = (door_state == DOOR_LOCKED) ? "lock" : "unlock";
            create_json_message(status_event, tx_buffer);
            send_to_raspberry_pi(tx_buffer);
        }
    } else {
        printf("Malformed JSON received: %s\n", message);
    }
}

/* Handle button press */
void handle_button_press() {
    uint32_t current_time = HAL_GetTick();
    
    // Debounce - ignore presses within 500ms
    if (current_time - last_button_time < 500) {
        return;
    }
    
    last_button_time = current_time;
    
    // Toggle door state
    if (door_state == DOOR_LOCKED) {
        update_door_state(DOOR_UNLOCKED);
        create_json_message("unlock", tx_buffer);
        printf("Door unlocked by button press\n");
    } else {
        update_door_state(DOOR_LOCKED);
        create_json_message("lock", tx_buffer);
        printf("Door locked by button press\n");
    }
    
    // Send message to Raspberry Pi
    send_to_raspberry_pi(tx_buffer);
}

/* Check for received data from UART */
void check_uart_reception() {
    static int rx_index = 0;
    uint8_t received_char;
    
    if (HAL_UART_Receive(&huart1, &received_char, 1, 0) == HAL_OK) {
        if (received_char == '\n' || received_char == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                process_received_message(rx_buffer);
                rx_index = 0;
            }
        } else if (rx_index < sizeof(rx_buffer) - 1) {
            rx_buffer[rx_index++] = received_char;
        } else {
            // Buffer overflow - reset
            rx_index = 0;
        }
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* MCU Configuration */
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_RTC_Init();
    
    /* Initialize door state */
    update_door_state(DOOR_LOCKED);
    
    printf("STM32F746 Door Control System Started\n");
    printf("Press B1 button to toggle door state\n");
    
    /* Send initial status */
    create_json_message("lock", tx_buffer);
    send_to_raspberry_pi(tx_buffer);
    
    /* Infinite loop */
    while (1)
    {
        /* Check button press */
        if (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_11) == GPIO_PIN_RESET) {
            if (!button_pressed) {
                button_pressed = true;
                handle_button_press();
            }
        } else {
            button_pressed = false;
        }
        
        /* Check for UART reception */
        check_uart_reception();
        
        /* Small delay */
        HAL_Delay(10);
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Configure LSE Drive Capability */
    HAL_PWR_EnableBkUpAccess();
    /** Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE
                                      |RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 432;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    /** Activate the Over-Drive mode */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    {
        Error_Handler();
    }
    /** Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
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
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    /** Initialize RTC Only */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN Check_RTC_BKUP */
    /* Check if data stored in backup register is correct */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2)
    {
    /* USER CODE END Check_RTC_BKUP */
        /** Initialize RTC and set the Time and Date */
        sTime.Hours = 12;
        sTime.Minutes = 0;
        sTime.Seconds = 0;
        sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        sTime.StoreOperation = RTC_STOREOPERATION_RESET;
        if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
        {
            Error_Handler();
        }
        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Month = RTC_MONTH_JUNE;
        sDate.Date = 9;
        sDate.Year = 25;

        if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
        {
            Error_Handler();
        }
        /** Enable the write protection for RTC registers */
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
    }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_RESET);

    /*Configure GPIO pin : PI11 (User Button B1) */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    /*Configure GPIO pin : PI1 (User LED LD1) */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    /*Configure GPIO pins : PA9 PA10 (USART1 TX/RX) */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */