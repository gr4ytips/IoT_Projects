/**
  * @file           : main.c
  * @brief          : Secure Boot Target Payload
  * @author         : gr4ytips
  */

#include "main.h"
#include <string.h>

UART_HandleTypeDef huart1;

/* ==================================================================== */
/* BACKDOOR VARIABLES                                                   */
/* ==================================================================== */
uint8_t rx_buffer[10];
const char *secret = "REBOOT";
volatile uint8_t trigger_reset = 0; /* Safe execution flag */

/* ==================================================================== */
/* FUNCTION PROTOTYPES                                                  */
/* ==================================================================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

static void app_puts(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

/* ==================================================================== */
/* INTERRUPT SERVICE ROUTINES (IRQ-Safe Backdoor Logic)                 */
/* ==================================================================== */

/* Callback fires when 6 bytes are received */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* Check if the received 6 bytes match the secret command */
        if (strncmp((char*)rx_buffer, secret, 6) == 0)
        {
            trigger_reset = 1;
        }
        /* Re-arm the UART interrupt to listen for the next command */
        HAL_UART_Receive_IT(&huart1, rx_buffer, 6);
    }
}

/* Callback fires if a UART error occurs */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* Clear the error state and forcefully re-arm the listener */
        HAL_UART_Receive_IT(&huart1, rx_buffer, 6);
    }
}

/* ==================================================================== */
/* MAIN ROUTINE                                                         */
/* ==================================================================== */
int main(void)
{
    HAL_Init();

    /* We inherited a locked CPU from the bootloader. Unlock it! */
    __enable_irq();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    app_puts("\r\n=================================\r\n");
    app_puts("[app] Backdoor_App Boot Successful\r\n");
    app_puts("=================================\r\n");

    /* Arm the UART Backdoor listener for the first time */
    HAL_UART_Receive_IT(&huart1, rx_buffer, 6);

    while (1)
    {
        /* Check if the backdoor flag was set by the interrupt */
        if (trigger_reset)
        {
            app_puts("\r\n[!] BACKDOOR TRIGGERED: SYSTEM RESET...\r\n");
            HAL_Delay(50); /* Safe to delay here to flush UART buffer */
            NVIC_SystemReset(); /* Force hardware reset */
        }

        /* Normal application behavior (Slow Blink) */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        app_puts("[app] tick...\r\n");
        HAL_Delay(1000);
    }
}

/* ==================================================================== */
/* PERIPHERAL CONFIGURATION CODE                                        */
/* ==================================================================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 200;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

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
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Turn off LED initially */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
