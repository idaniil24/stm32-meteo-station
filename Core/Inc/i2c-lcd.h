#ifndef I2C_LCD_H
#define I2C_LCD_H

#include "stm32f4xx_hal.h"

#define SLAVE_ADDRESS_LCD 0x4E
#define DEGREE_SYMBOL     0xDF  // символ градуса в HD44780

void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_put_cur(int row, int col);
void lcd_clear(void);
void lcd_make_custom(void);

#endif
