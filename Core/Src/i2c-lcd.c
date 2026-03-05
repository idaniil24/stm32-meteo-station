#include "i2c-lcd.h"
extern I2C_HandleTypeDef hi2c1;

// 4 иконки по 8 байт = 32 байта
// Индекс 0: Термометр
// Индекс 1: Капля
// Индекс 2: Часы
// Индекс 3: Календарь
uint8_t custom_icons[32] = {
    0x04, 0x0A, 0x0A, 0x0E, 0x0E, 0x1F, 0x1F, 0x0E, // Термометр
    0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x11, 0x0E, // Капля
    0x0E, 0x11, 0x15, 0x15, 0x11, 0x0E, 0x00, 0x00, // Часы
    0x1F, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F, 0x00, // Календарь
};

void lcd_send_cmd(char cmd)
{
    char data_u = (cmd & 0xF0);
    char data_l = ((cmd << 4) & 0xF0);
    uint8_t data_t[4] = {
        data_u | 0x0C,
        data_u | 0x08,
        data_l | 0x0C,
        data_l | 0x08
    };
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, 100);
}

void lcd_send_data(char data)
{
    char data_u = (data & 0xF0);
    char data_l = ((data << 4) & 0xF0);
    uint8_t data_t[4] = {
        data_u | 0x0D,
        data_u | 0x09,
        data_l | 0x0D,
        data_l | 0x09
    };
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, 100);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_put_cur(int row, int col)
{
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
        case 2: col |= 0x94; break;
        case 3: col |= 0xD4; break;
    }
    lcd_send_cmd(col);
}

void lcd_make_custom(void)
{
    lcd_send_cmd(0x40); // Переходим в CGRAM
    for (int i = 0; i < 32; i++) {
        lcd_send_data(custom_icons[i]);
    }
    lcd_send_cmd(0x80); // Возвращаемся в DDRAM (строка 0, позиция 0)
}

void lcd_init(void)
{
    HAL_Delay(50);
    lcd_send_cmd(0x30); HAL_Delay(5);
    lcd_send_cmd(0x30); HAL_Delay(1);
    lcd_send_cmd(0x30); HAL_Delay(10);
    lcd_send_cmd(0x20); HAL_Delay(10);
    lcd_send_cmd(0x28); HAL_Delay(1);
    lcd_send_cmd(0x08); HAL_Delay(1);
    lcd_send_cmd(0x01); HAL_Delay(2);
    lcd_send_cmd(0x06); HAL_Delay(1);
    lcd_send_cmd(0x0C);
}

void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++);
}
