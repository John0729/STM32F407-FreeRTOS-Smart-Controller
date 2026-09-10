#include "oled.h"
#include "main.h"

#include <string.h>

/* SPI1 是在 main.c 裡面建立的 */
extern SPI_HandleTypeDef hspi2;


/* =========================================================
 * OLED GPIO
 * =========================================================
 */


#define OLED_CS_PORT   GPIOE
#define OLED_CS_PIN    GPIO_PIN_7

#define OLED_DC_PORT   GPIOE
#define OLED_DC_PIN    GPIO_PIN_8

#define OLED_RES_PORT  GPIOE
#define OLED_RES_PIN   GPIO_PIN_9


/* =========================================================
 * Frame Buffer
 *
 * 128 x 64 pixel
 *
 * 128 * 64 / 8
 * = 1024 bytes
 * =========================================================
 */

static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];


/* =========================================================
 * Low-Level GPIO
 * =========================================================
 */

static void OLED_CS_Low(void)
{
    HAL_GPIO_WritePin(OLED_CS_PORT,
                      OLED_CS_PIN,
                      GPIO_PIN_RESET);
}


static void OLED_CS_High(void)
{
    HAL_GPIO_WritePin(OLED_CS_PORT,
                      OLED_CS_PIN,
                      GPIO_PIN_SET);
}


static void OLED_Reset(void)
{
    HAL_GPIO_WritePin(OLED_RES_PORT,
                      OLED_RES_PIN,
                      GPIO_PIN_RESET);

    HAL_Delay(10);

    HAL_GPIO_WritePin(OLED_RES_PORT,
                      OLED_RES_PIN,
                      GPIO_PIN_SET);

    HAL_Delay(10);
}


/* =========================================================
 * SPI Command
 *
 * DC = 0
 * =========================================================
 */

static void OLED_WriteCommand(uint8_t cmd)
{
    /* DC = 0 → Command */
    HAL_GPIO_WritePin(OLED_DC_PORT,
                      OLED_DC_PIN,
                      GPIO_PIN_RESET);

    OLED_CS_Low();

    HAL_SPI_Transmit(&hspi2,
                     &cmd,
                     1,
                     100);

    OLED_CS_High();
}


/* =========================================================
 * SPI Display Data
 *
 * DC = 1
 * =========================================================
 */

static void OLED_WriteData(uint8_t *data,
                           uint16_t length)
{
    /* DC = 1 → Display Data */
    HAL_GPIO_WritePin(OLED_DC_PORT,
                      OLED_DC_PIN,
                      GPIO_PIN_SET);

    OLED_CS_Low();

    HAL_SPI_Transmit(&hspi2,
                     data,
                     length,
                     100);

    OLED_CS_High();
}


/* =========================================================
 * SSD1306 Initialization
 * 128 x 64
 * =========================================================
 */

void OLED_Init(void)
{
    OLED_CS_High();

    OLED_Reset();

    /* Display OFF */
    OLED_WriteCommand(0xAE);

    /* Display clock */
    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0x80);

    /* Multiplex ratio = 63 → 64 rows */
    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);

    /* Display offset */
    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);

    /* Start line = 0 */
    OLED_WriteCommand(0x40);

    /* Charge pump */
    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);

    /* Horizontal addressing mode */
    OLED_WriteCommand(0x20);
    OLED_WriteCommand(0x00);

    /* Segment remap */
    OLED_WriteCommand(0xA1);

    /* COM scan direction */
    OLED_WriteCommand(0xC8);

    /* COM pin configuration */
    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);

    /* Contrast */
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0x7F);

    /* Pre-charge */
    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0xF1);

    /* VCOMH */
    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x40);

    /* Use display RAM */
    OLED_WriteCommand(0xA4);

    /* Normal display */
    OLED_WriteCommand(0xA6);

    /* Display ON */
    OLED_WriteCommand(0xAF);

    OLED_Clear();
    OLED_Update();
}


/* =========================================================
 * Clear framebuffer
 * =========================================================
 */

void OLED_Clear(void)
{
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}


/* =========================================================
 * Send framebuffer to OLED
 * =========================================================
 */

void OLED_Update(void)
{
    /*
     * Column address
     * 0 ~ 127
     */
    OLED_WriteCommand(0x21);
    OLED_WriteCommand(0);
    OLED_WriteCommand(127);

    /*
     * Page address
     * 0 ~ 7
     */
    OLED_WriteCommand(0x22);
    OLED_WriteCommand(0);
    OLED_WriteCommand(7);

    OLED_WriteData(oled_buffer, sizeof(oled_buffer));
}


/* =========================================================
 * 5 x 7 Font
 *
 * 每個字：
 *
 * 5 pixels 寬
 * 7 pixels 高
 *
 * + 1 pixel spacing
 * =========================================================
 */

static const uint8_t font_number[10][5] =
{
    /* 0 */
    {0x3E, 0x51, 0x49, 0x45, 0x3E},

    /* 1 */
    {0x00, 0x42, 0x7F, 0x40, 0x00},

    /* 2 */
    {0x42, 0x61, 0x51, 0x49, 0x46},

    /* 3 */
    {0x21, 0x41, 0x45, 0x4B, 0x31},

    /* 4 */
    {0x18, 0x14, 0x12, 0x7F, 0x10},

    /* 5 */
    {0x27, 0x45, 0x45, 0x45, 0x39},

    /* 6 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30},

    /* 7 */
    {0x01, 0x71, 0x09, 0x05, 0x03},

    /* 8 */
    {0x36, 0x49, 0x49, 0x49, 0x36},

    /* 9 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}
};


static const uint8_t font_uppercase[26][5] =
{
    /* A */
    {0x7E, 0x11, 0x11, 0x11, 0x7E},

    /* B */
    {0x7F, 0x49, 0x49, 0x49, 0x36},

    /* C */
    {0x3E, 0x41, 0x41, 0x41, 0x22},

    /* D */
    {0x7F, 0x41, 0x41, 0x22, 0x1C},

    /* E */
    {0x7F, 0x49, 0x49, 0x49, 0x41},

    /* F */
    {0x7F, 0x09, 0x09, 0x09, 0x01},

    /* G */
    {0x3E, 0x41, 0x49, 0x49, 0x7A},

    /* H */
    {0x7F, 0x08, 0x08, 0x08, 0x7F},

    /* I */
    {0x00, 0x41, 0x7F, 0x41, 0x00},

    /* J */
    {0x20, 0x40, 0x41, 0x3F, 0x01},

    /* K */
    {0x7F, 0x08, 0x14, 0x22, 0x41},

    /* L */
    {0x7F, 0x40, 0x40, 0x40, 0x40},

    /* M */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},

    /* N */
    {0x7F, 0x04, 0x08, 0x10, 0x7F},

    /* O */
    {0x3E, 0x41, 0x41, 0x41, 0x3E},

    /* P */
    {0x7F, 0x09, 0x09, 0x09, 0x06},

    /* Q */
    {0x3E, 0x41, 0x51, 0x21, 0x5E},

    /* R */
    {0x7F, 0x09, 0x19, 0x29, 0x46},

    /* S */
    {0x46, 0x49, 0x49, 0x49, 0x31},

    /* T */
    {0x01, 0x01, 0x7F, 0x01, 0x01},

    /* U */
    {0x3F, 0x40, 0x40, 0x40, 0x3F},

    /* V */
    {0x1F, 0x20, 0x40, 0x20, 0x1F},

    /* W */
    {0x7F, 0x20, 0x18, 0x20, 0x7F},

    /* X */
    {0x63, 0x14, 0x08, 0x14, 0x63},

    /* Y */
    {0x03, 0x04, 0x78, 0x04, 0x03},

    /* Z */
    {0x61, 0x51, 0x49, 0x45, 0x43}
};


/* =========================================================
 * Draw one character
 * =========================================================
 */

void OLED_DrawChar(uint8_t x,
                   uint8_t page,
                   char c)
{
    const uint8_t *font = NULL;

    uint8_t special_font[5] = {0};


    /* 超出螢幕 */
    if (x > 122 || page > 7)
    {
        return;
    }


    /* A ~ Z */
    if (c >= 'A' && c <= 'Z')
    {
        font = font_uppercase[c - 'A'];
    }

    /* 0 ~ 9 */
    else if (c >= '0' && c <= '9')
    {
        font = font_number[c - '0'];
    }

    /* Space */
    else if (c == ' ')
    {
        special_font[0] = 0x00;
        special_font[1] = 0x00;
        special_font[2] = 0x00;
        special_font[3] = 0x00;
        special_font[4] = 0x00;

        font = special_font;
    }

    /* : */
    else if (c == ':')
    {
        special_font[0] = 0x00;
        special_font[1] = 0x36;
        special_font[2] = 0x36;
        special_font[3] = 0x00;
        special_font[4] = 0x00;

        font = special_font;
    }

    /* % */
    else if (c == '%')
    {
        special_font[0] = 0x63;
        special_font[1] = 0x13;
        special_font[2] = 0x08;
        special_font[3] = 0x64;
        special_font[4] = 0x63;

        font = special_font;
    }

    /* - */
    else if (c == '-')
    {
        special_font[0] = 0x08;
        special_font[1] = 0x08;
        special_font[2] = 0x08;
        special_font[3] = 0x08;
        special_font[4] = 0x08;

        font = special_font;
    }
    else if (c == '.')
    {
        special_font[0] = 0x00;
        special_font[1] = 0x60;
        special_font[2] = 0x60;
        special_font[3] = 0x00;
        special_font[4] = 0x00;

        font = special_font;
    }
    else
    {
        return;
    }


    /*
     * 一個 byte = 垂直 8 pixels
     *
     * page * 128
     * 找到該 page 的起始位置
     */

    for (uint8_t i = 0; i < 5; i++)
    {
        oled_buffer[page * OLED_WIDTH + x + i] = font[i];
    }

    /* 字元間留一個 pixel */
    oled_buffer[page * OLED_WIDTH + x + 5] = 0x00;
}


/* =========================================================
 * Draw String
 * =========================================================
 */

void OLED_DrawString(uint8_t x,
                     uint8_t page,
                     const char *str)
{
    while (*str != '\0')
    {
        /*
         * 如果超過右邊界
         * 換下一行
         */
        if (x > 122)
        {
            x = 0;
            page++;

            if (page > 7)
            {
                return;
            }
        }

        OLED_DrawChar(x, page, *str);

        /*
         * 5 pixel 字體
         * + 1 pixel spacing
         */
        x += 6;

        str++;
    }
}
