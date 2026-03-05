#ifndef DS3231_H
#define DS3231_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

#define DS3231_ADDRESS (0x68 << 1)

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year; // 2 цифры: 26 = 2026
} DS3231_Time;

void     DS3231_Begin(I2C_HandleTypeDef *hi2c);  // Инициализация с авто-установкой времени
bool     DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time *t);

#endif
