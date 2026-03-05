/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Метеостанция (AHT21 + DS3231 + LCD 2004)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "i2c-lcd.h"
#include "ds3231.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ── Переферия ───────────────────────────────────────────────────────────── */
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart2;

/* ── Константы ───────────────────────────────────────────────────────────── */
#define AHT21_ADDRESS        (0x38 << 1)
#define SENSOR_POLL_INTERVAL  30000U
#define I2C_TIMEOUT           50U

/* ── Состояние приложения ────────────────────────────────────────────────── */
static float       temperature      = 0.0f;
static float       humidity         = 0.0f;
static bool        sensor_ok        = false;
static bool        display_dirty    = true;
static uint32_t    last_sensor_time = 0;
static DS3231_Time rtc_time         = {0};

/* ── Прототипы ───────────────────────────────────────────────────────────── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static bool AHT21_Read(float *temp, float *hum);
static void Display_Update(void);

/* ── AHT21 ───────────────────────────────────────────────────────────────── */
static bool AHT21_Read(float *temp, float *hum)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    uint8_t data[6];

    if (HAL_I2C_Master_Transmit(&hi2c1, AHT21_ADDRESS, cmd, 3, I2C_TIMEOUT) != HAL_OK)
        return false;

    HAL_Delay(80);

    if (HAL_I2C_Master_Receive(&hi2c1, AHT21_ADDRESS, data, 6, I2C_TIMEOUT) != HAL_OK)
        return false;

    if (data[0] & 0x80)
        return false;

    uint32_t hum_raw  = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    *hum  = (float)hum_raw  * 100.0f / 1048576.0f;
    *temp = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;

    return true;
}

/* ── LCD ─────────────────────────────────────────────────────────────────── */
static void Display_Update(void)
{
    if (!display_dirty) return;
    display_dirty = false;

    char buf[25];

    // Строка 0: Время
    lcd_put_cur(0, 0);
    lcd_send_data(2); // иконка часов
    snprintf(buf, sizeof(buf), " TIME: %02d:%02d:%02d     ",
             rtc_time.hours, rtc_time.minutes, rtc_time.seconds);
    lcd_send_string(buf);

    // Строка 1: Дата
    lcd_put_cur(1, 0);
    lcd_send_data(3); // иконка календаря
    snprintf(buf, sizeof(buf), " DATE: %02d.%02d.%02d     ",
             rtc_time.day, rtc_time.month, rtc_time.year);
    lcd_send_string(buf);

    // Строка 2: Температура
    lcd_put_cur(2, 0);
    lcd_send_data(0); // иконка термометра
    snprintf(buf, sizeof(buf), " TEMP: %5.1f %cC    ", temperature, DEGREE_SYMBOL);
    lcd_send_string(buf);

    // Строка 3: Влажность
    lcd_put_cur(3, 0);
    lcd_send_data(1); // иконка капли
    if (sensor_ok) {
        snprintf(buf, sizeof(buf), " HUMI: %5.1f %%     ", humidity);
    } else {
        snprintf(buf, sizeof(buf), " SENS ERROR         ");
    }
    lcd_send_string(buf);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    HAL_Delay(200);

    // Инициализация LCD
    lcd_init();
    lcd_make_custom();
    lcd_clear();

    // Заставка
    lcd_put_cur(0, 0);
    lcd_send_string("   SYSTEM STARTING  ");
    HAL_Delay(1000);
    lcd_clear();

    // Инициализация RTC — авто-установка времени если нужно
    DS3231_Begin(&hi2c1);

    // Первый опрос датчиков
    sensor_ok = AHT21_Read(&temperature, &humidity);
    DS3231_GetTime(&hi2c1, &rtc_time);
    display_dirty = true;
    Display_Update();

    last_sensor_time = HAL_GetTick();

    /* ── Главный цикл ──────────────────────────────────────────────────── */
    while (1)
    {
        // Читаем RTC каждую секунду
        static uint32_t last_rtc_time = 0;
        if (HAL_GetTick() - last_rtc_time >= 1000)
        {
            DS3231_GetTime(&hi2c1, &rtc_time);
            last_rtc_time = HAL_GetTick();
            display_dirty = true;
        }

        // Читаем AHT21 каждые 30 секунд
        if (HAL_GetTick() - last_sensor_time >= SENSOR_POLL_INTERVAL)
        {
            bool new_ok = AHT21_Read(&temperature, &humidity);

            if (new_ok) {
                char uart_buf[32];
                snprintf(uart_buf, sizeof(uart_buf), "T%.1fH%.1f\n", temperature, humidity);
                HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), 100);
            }

            if (new_ok != sensor_ok) display_dirty = true;
            sensor_ok = new_ok;

            last_sensor_time = HAL_GetTick();
        }

        Display_Update();
        HAL_Delay(100);
    }
}

/* ── Конфигурация периферии ──────────────────────────────────────────────── */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 25;
    RCC_OscInitStruct.PLL.PLLN       = 168;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
