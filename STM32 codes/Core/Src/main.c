/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
uint32_t rotation_count = 0;
uint8_t last_sensor_state = 1; // Assumes sensor is HIGH when idle
char display_buffer[32];       // Buffer to hold our text

// --- WEB TELEMETRY TIMERS ---
uint32_t telemetry_send_interval_ms = 30000; // Testing: Sends every 30 seconds
uint32_t last_telemetry_send_time = 0;       // Timestamp tracking last telemetry packet
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
void Send_SMS(void);
void Debug_SIM_OLED(void);
void GSM_Send_Telemetry(uint32_t count);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_LPUART1_UART_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */

  // Initialize the OLED screen
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(5, 10);
  ssd1306_WriteString("Meter Ready", Font_7x10, White);
  ssd1306_UpdateScreen();

  // Run the SIM module debug test on startup
  Debug_SIM_OLED();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // Read the current state of the Hall sensor (PA2)
      uint8_t current_sensor_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);

      // Check if sensor went from HIGH to LOW (Magnet detected)
      if (current_sensor_state == GPIO_PIN_RESET && last_sensor_state == GPIO_PIN_SET)
      {
          // 1. Increment the count
          rotation_count++;

          // 2. Format the text and push it to the OLED
          sprintf(display_buffer, "Count: %lu", rotation_count);
          ssd1306_Fill(Black);
          ssd1306_SetCursor(5, 10);
          ssd1306_WriteString(display_buffer, Font_7x10, White);
          ssd1306_UpdateScreen();

          // COMMENTED OUT: Prevents SMS spam on every single magnet pass!
          // Send_SMS();

          // 3. Turn ON the PA5 test LED
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

          // 4. Keep it on briefly (reduced from 1000ms so we don't miss fast water flow)
          HAL_Delay(100);

          // 5. Turn OFF the PA5 LED
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
      }

      // --- PERIODIC WEB TELEMETRY BLOCK ---
      uint32_t current_time = HAL_GetTick();
      if (current_time - last_telemetry_send_time >= telemetry_send_interval_ms)
      {
          // Let the user know the board is busy sending data
          ssd1306_Fill(Black);
          ssd1306_SetCursor(5, 10);
          ssd1306_WriteString("Sending Web...", Font_7x10, White);
          ssd1306_UpdateScreen();

          // Fire the web function
          GSM_Send_Telemetry(rotation_count);

          // Reset rotation count after transmission to avoid double billing
          rotation_count = 0;
          last_telemetry_send_time = HAL_GetTick(); // Update timer AFTER sending finishes

          // Clear screen back to normal
          ssd1306_Fill(Black);
          ssd1306_UpdateScreen();
      }

      // Save the current state for the next loop
      last_sensor_state = current_sensor_state;

      // Tiny delay to act as a "debounce" so a shaky magnet doesn't count twice
      HAL_Delay(10);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_LPUART1
                              |RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00000608;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 9600;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

  /*Configure GPIO pins : PA3 PA4 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

void GSM_Send_Telemetry(uint32_t count)
{
  char gsm_buffer[256];
  char json_payload[128];

  // Compute liters on the STM32 (450 rotations = 1 Liter)
  uint32_t liters_int = count / 450;
  uint32_t liters_frac = ((count % 450) * 10000) / 450;

  // Format the JSON payload
  sprintf(json_payload, "{\"client_id\":\"00000000-0000-0000-0000-000000000000\",\"liters_consumed\":%lu.%04lu}", liters_int, liters_frac);

  // --- GPRS SETUP (Required for SIM800L internet) ---
  char *cmd_sapbr1 = "AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_sapbr1, strlen(cmd_sapbr1), 1000);
  HAL_Delay(500);

  // Make sure this APN matches your local carrier (e.g., dialogbb, mobitel, hutch3g)
  char *cmd_sapbr2 = "AT+SAPBR=3,1,\"APN\",\"dialogbb\"\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_sapbr2, strlen(cmd_sapbr2), 1000);
  HAL_Delay(500);

  char *cmd_sapbr3 = "AT+SAPBR=1,1\r\n"; // Open the bearer
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_sapbr3, strlen(cmd_sapbr3), 3000);
  HAL_Delay(2000);

  // 1. Initialize HTTP Service
  char *cmd_init = "AT+HTTPINIT\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_init, strlen(cmd_init), 1000);
  HAL_Delay(500);

  // 2. Set HTTP parameters: CID
  char *cmd_cid = "AT+HTTPPARA=\"CID\",1\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_cid, strlen(cmd_cid), 1000);
  HAL_Delay(500);

  // 3. Set URL to the AWS Proxy endpoint (UNENCRYPTED HTTP!)
  sprintf(gsm_buffer, "AT+HTTPPARA=\"URL\",\"http://13.48.194.28/forward\"\r\n");
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)gsm_buffer, strlen(gsm_buffer), 1000);
  HAL_Delay(500);

  // 4. Set Content Type to JSON
  char *cmd_content = "AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_content, strlen(cmd_content), 1000);
  HAL_Delay(500);

  // 5. Send HTTP Data Size
  sprintf(gsm_buffer, "AT+HTTPDATA=%d,10000\r\n", strlen(json_payload));
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)gsm_buffer, strlen(gsm_buffer), 1000);
  HAL_Delay(2000); // Wait for module prompt (>)

  // 6. Send the actual payload
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)json_payload, strlen(json_payload), 5000);
  HAL_Delay(2000);

  // 7. Perform HTTP POST
  char *cmd_action = "AT+HTTPACTION=1\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_action, strlen(cmd_action), 1000);
  HAL_Delay(5000); // Wait for action response (+HTTPACTION: 1,200,...)

  // 8. Terminate HTTP Service
  char *cmd_term = "AT+HTTPTERM\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_term, strlen(cmd_term), 1000);
  HAL_Delay(500);

  // --- Close GPRS Bearer ---
  char *cmd_close = "AT+SAPBR=0,1\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_close, strlen(cmd_close), 1000);
  HAL_Delay(500);
}

void Debug_SIM_OLED(void)
{
  char rx_buffer[30] = {0};

  ssd1306_Fill(Black);
  ssd1306_SetCursor(0, 2);
  ssd1306_WriteString("Testing SIM...", Font_7x10, White);
  ssd1306_UpdateScreen();

  char *cmd_ate0 = "ATE0\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_ate0, strlen(cmd_ate0), 500);
  HAL_Delay(500);

  HAL_UART_Receive(&hlpuart1, (uint8_t*)rx_buffer, 30, 500);
  memset(rx_buffer, 0, 30);

  char *cmd_at = "AT\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_at, strlen(cmd_at), 500);
  HAL_UART_Receive(&hlpuart1, (uint8_t*)rx_buffer, 15, 2000);

  for(int i = 0; i < 30; i++) {
      if(rx_buffer[i] == '\r' || rx_buffer[i] == '\n') {
          rx_buffer[i] = ' ';
      }
  }

  if (strlen(rx_buffer) > 0)
  {
      ssd1306_SetCursor(0, 12);
      ssd1306_WriteString("Heard:", Font_7x10, White);
      ssd1306_SetCursor(0, 22);
      ssd1306_WriteString(rx_buffer, Font_7x10, White);
  }
  else
  {
      ssd1306_SetCursor(0, 12);
      ssd1306_WriteString("NO REPLY", Font_7x10, White);
  }
  ssd1306_UpdateScreen();

  HAL_Delay(3000);
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();
}

void Send_SMS(void)
{
  char *cmd_ate0 = "ATE0\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_ate0, strlen(cmd_ate0), 1000);
  HAL_Delay(500);

  char *cmd_text = "AT+CMGF=1\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_text, strlen(cmd_text), 1000);
  HAL_Delay(500);

  char *cmd_phone = "AT+CMGS=\"0760128947\"\r\n";
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd_phone, strlen(cmd_phone), 1000);
  HAL_Delay(1000);

  char payload[64];
  sprintf(payload, "Water Meter Alert! Magnet detected. Count: %lu\x1A", rotation_count);
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)payload, strlen(payload), 2000);
  HAL_Delay(4000);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
