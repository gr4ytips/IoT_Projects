#include "main.h"
#include "image_header.h"
#include <stdint.h>
#include <string.h>

UART_HandleTypeDef huart1;

typedef void (*pFunction)(void);

/* Provided by verify.c */
int secure_verify_app(const image_header_t *hdr);

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

static void dbg_puts(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

//static int app_vectors_are_sane(uint32_t app_base)
//{
//    uint32_t sp = *(volatile uint32_t *)(app_base + 0U);
//    uint32_t rv = *(volatile uint32_t *)(app_base + 4U);

//    if ((sp & 0x2FFE0000UL) != 0x20000000UL) return 0;
//    if ((rv & 1U) == 0U) return 0; /* Cortex-M reset vector must be Thumb */
//    if ((rv & ~1U) < APP_VECTOR_ADDR || (rv & ~1U) >= FLASH_END_ADDR) return 0;

//    return 1;
//}

static int app_vectors_are_sane(uint32_t app_base)
{
    uint32_t sp = *(volatile uint32_t *)(app_base + 0U);
    uint32_t rv = *(volatile uint32_t *)(app_base + 4U);

    /* STM32F411CE: RAM = 0x20000000 .. 0x2001FFFF
       Initial MSP is allowed to be exactly 0x20020000 (top of RAM). */
    if (sp < 0x20000000UL || sp > 0x20020000UL) return 0;
    if ((sp & 0x3U) != 0U) return 0;  /* word aligned */

    if ((rv & 1U) == 0U) return 0;    /* Cortex-M reset handler must be Thumb */
    if ((rv & ~1U) < APP_VECTOR_ADDR || (rv & ~1U) >= FLASH_END_ADDR) return 0;

    return 1;
}


static void jump_to_app(uint32_t app_base)
{
    uint32_t app_sp = *(volatile uint32_t *)(app_base + 0U);
    uint32_t app_reset = *(volatile uint32_t *)(app_base + 4U);
    pFunction app_entry = (pFunction)app_reset;
    uint32_t nvic_lines = (uint32_t)(sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint32_t i = 0; i < nvic_lines; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    HAL_UART_DeInit(&huart1);
    HAL_RCC_DeInit();
    HAL_DeInit();

    SCB->VTOR = app_base;
    __DSB();
    __ISB();

    __set_MSP(app_sp);
    app_entry();
}

int main(void)
{
    const image_header_t *hdr = (const image_header_t *)APP_HEADER_ADDR;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* Black Pill PC13 LED is usually active-low. Start OFF. */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    dbg_puts("\r\n[boot] secure boot starting\r\n");

    if (!app_vectors_are_sane(APP_VECTOR_ADDR)) {
        dbg_puts("[boot] invalid vectors\r\n");
    } else if (secure_verify_app(hdr)) {
        dbg_puts("[boot] signature valid\r\n");
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(100);
        jump_to_app(APP_VECTOR_ADDR);
    } else {
        dbg_puts("[boot] signature invalid\r\n");
    }

    dbg_puts("[boot] fail closed\r\n");
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(100);
    }
}

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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK
                                | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1
                                | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
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
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

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
    while (1) {
    }
}
