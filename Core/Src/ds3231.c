#include "ds3231.h"
#include <string.h>
#include <stdlib.h>

// ── Вспомогательные функции ──────────────────────────────────────────────────

static uint8_t bcd2dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t dec2bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// ── Парсинг __DATE__ ("Feb 22 2026") и __TIME__ ("14:35:00") ────────────────

static uint8_t parse_month(const char *str) {
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    for (uint8_t i = 0; i < 12; i++) {
        if (strncmp(str, months + i * 3, 3) == 0) return i + 1;
    }
    return 1;
}

static void parse_compile_time(DS3231_Time *t) {
    const char *date = __DATE__; // "Feb 22 2026"
    const char *time = __TIME__; // "14:35:00"

    t->month   = parse_month(date);
    t->day     = (uint8_t)atoi(date + 4);
    t->year    = (uint8_t)atoi(date + 9) % 100; // берём последние 2 цифры

    t->hours   = (uint8_t)atoi(time);
    t->minutes = (uint8_t)atoi(time + 3);
    t->seconds = (uint8_t)atoi(time + 6);
}

// ── Низкоуровневая запись/чтение регистров ───────────────────────────────────

static bool ds3231_write_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, buf, 2, 50) == HAL_OK;
}

static bool ds3231_read_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *val) {
    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, &reg, 1, 50) != HAL_OK)
        return false;
    return HAL_I2C_Master_Receive(hi2c, DS3231_ADDRESS, val, 1, 50) == HAL_OK;
}

static bool ds3231_set_time(I2C_HandleTypeDef *hi2c, DS3231_Time *t) {
    uint8_t buf[8] = {
        0x00,                   // начальный регистр
        dec2bcd(t->seconds),
        dec2bcd(t->minutes),
        dec2bcd(t->hours),
        0x01,                   // день недели — не используем
        dec2bcd(t->day),
        dec2bcd(t->month),
        dec2bcd(t->year)
    };
    return HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, buf, 8, 50) == HAL_OK;
}

// ── Публичные функции ────────────────────────────────────────────────────────

void DS3231_Begin(I2C_HandleTypeDef *hi2c) {
    uint8_t status_reg = 0;

    // Читаем регистр статуса (0x0F), проверяем бит OSF (бит 7)
    if (ds3231_read_reg(hi2c, 0x0F, &status_reg)) {
        if (status_reg & 0x80) {
            // OSF = 1: часы не были установлены или садилась батарейка
            DS3231_Time t;
            parse_compile_time(&t);
            ds3231_set_time(hi2c, &t);

            // Сбрасываем OSF бит
            status_reg &= ~0x80;
            ds3231_write_reg(hi2c, 0x0F, status_reg);
        }
        // OSF = 0: время уже установлено, ничего не делаем
    }
}

bool DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time *t) {
    uint8_t reg = 0x00;
    uint8_t buf[7];

    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, &reg, 1, 50) != HAL_OK)
        return false;
    if (HAL_I2C_Master_Receive(hi2c, DS3231_ADDRESS, buf, 7, 50) != HAL_OK)
        return false;

    t->seconds = bcd2dec(buf[0] & 0x7F);
    t->minutes = bcd2dec(buf[1] & 0x7F);
    t->hours   = bcd2dec(buf[2] & 0x3F);
    // buf[3] — день недели, пропускаем
    t->day     = bcd2dec(buf[4] & 0x3F);
    t->month   = bcd2dec(buf[5] & 0x1F);
    t->year    = bcd2dec(buf[6]);

    return true;
}
