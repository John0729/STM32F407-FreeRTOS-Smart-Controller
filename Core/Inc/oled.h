#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  64

void OLED_Init(void);

void OLED_Clear(void);

void OLED_Update(void);

void OLED_DrawChar(uint8_t x, uint8_t page,char c);

void OLED_DrawString(uint8_t x, uint8_t page, const char *str);

#endif
