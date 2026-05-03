#include "LCD_Driver.h"
#include "Delay.h"
#include "MySPI.h"
#include "LCDFont.h"

#define ST7735_MADCTL     0x36
#define ST7735_MADCTL_MX  0x40
#define ST7735_MADCTL_MY  0x80
#define ST7735_MADCTL_MV  0x20
#define ST7735_MADCTL_RGB 0x00

u32 DMA1_MEM_LEN;
u8 SendBuff[1280];

// ===================== 界面状态变量 =====================
static uint8_t prev_recording    = 0xFF;
static uint8_t prev_photo_pending = 0xFF;

// ===================== 基础驱动 =====================

void Lcd_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
    MySPI_Init();
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void DMA_Start(void)
{
    u16 i;
    SPI_SetLCDSpeed();      // LCD: 36MHz
    LCD_CS_CLR;
    LCD_DC_SET;
    for (i = 0; i < DMA1_MEM_LEN; i++)
        MySPI_SwapByte(SendBuff[i]);
    LCD_CS_SET;
}

void Lcd_WriteIndex(u8 Index)
{
    SPI_SetLCDSpeed();      // LCD: 36MHz
    LCD_CS_CLR;
    LCD_DC_CLR;
    MySPI_SwapByte(Index);
    LCD_CS_SET;
}

void Lcd_WriteData(u8 Data)
{
    SPI_SetLCDSpeed();      // LCD: 36MHz
    LCD_CS_CLR;
    LCD_DC_SET;
    MySPI_SwapByte(Data);
    LCD_CS_SET;
}

void Lcd_WriteData_16Bit(u16 Data)
{
    SPI_SetLCDSpeed();      // LCD: 36MHz
    LCD_CS_CLR;
    LCD_DC_SET;
    MySPI_SwapByte(Data >> 8);
    MySPI_SwapByte(Data);
    LCD_CS_SET;
}

void Lcd_Reset(void)
{
    LCD_RST_CLR;
    Delay_ms(50);
    LCD_RST_SET;
    Delay_ms(50);
}

void ST7735_SetRotation(uint8_t rotation)
{
    uint8_t madctl = 0;
    switch (rotation) {
        case 0: madctl = ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_RGB; break;
        case 1: madctl = ST7735_MADCTL_MY | ST7735_MADCTL_MV | ST7735_MADCTL_RGB; break;
        case 2: madctl = ST7735_MADCTL_RGB; break;
        case 3: madctl = ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_RGB; break;
    }
    Lcd_WriteIndex(ST7735_MADCTL);
    Lcd_WriteData(madctl);
}

void Lcd_Init(void)
{
    Lcd_GPIO_Init();
    Lcd_Reset();

    Lcd_WriteIndex(0x11);
    Delay_ms(100);
    Lcd_WriteIndex(0xB1);
    Lcd_WriteData(0x00); Lcd_WriteData(0x08); Lcd_WriteData(0x05);
    Lcd_WriteIndex(0xB2);
    Lcd_WriteData(0x05); Lcd_WriteData(0x3A); Lcd_WriteData(0x3A);
    Lcd_WriteIndex(0xB3);
    Lcd_WriteData(0x05); Lcd_WriteData(0x3A); Lcd_WriteData(0x3A);
    Lcd_WriteData(0x05); Lcd_WriteData(0x3A); Lcd_WriteData(0x3A);
    Lcd_WriteIndex(0xB4);
    Lcd_WriteData(0x03);
    Lcd_WriteIndex(0xC0);
    Lcd_WriteData(0x62); Lcd_WriteData(0x02); Lcd_WriteData(0x04);
    Lcd_WriteIndex(0xC1);
    Lcd_WriteData(0xC0);
    Lcd_WriteIndex(0xC2);
    Lcd_WriteData(0x0D); Lcd_WriteData(0x00);
    Lcd_WriteIndex(0xC3);
    Lcd_WriteData(0x8D); Lcd_WriteData(0x6A);
    Lcd_WriteIndex(0xC4);
    Lcd_WriteData(0x8D); Lcd_WriteData(0xEE);
    Lcd_WriteIndex(0xC5);
    Lcd_WriteData(0x08);
    Lcd_WriteIndex(0xE0);
    Lcd_WriteData(0x03); Lcd_WriteData(0x1B); Lcd_WriteData(0x12); Lcd_WriteData(0x11);
    Lcd_WriteData(0x3F); Lcd_WriteData(0x3A); Lcd_WriteData(0x32); Lcd_WriteData(0x34);
    Lcd_WriteData(0x2F); Lcd_WriteData(0x2B); Lcd_WriteData(0x30); Lcd_WriteData(0x3A);
    Lcd_WriteData(0x00); Lcd_WriteData(0x01); Lcd_WriteData(0x02); Lcd_WriteData(0x05);
    Lcd_WriteIndex(0xE1);
    Lcd_WriteData(0x03); Lcd_WriteData(0x1B); Lcd_WriteData(0x12); Lcd_WriteData(0x11);
    Lcd_WriteData(0x32); Lcd_WriteData(0x2F); Lcd_WriteData(0x2A); Lcd_WriteData(0x2F);
    Lcd_WriteData(0x2E); Lcd_WriteData(0x2C); Lcd_WriteData(0x35); Lcd_WriteData(0x3F);
    Lcd_WriteData(0x00); Lcd_WriteData(0x00); Lcd_WriteData(0x01); Lcd_WriteData(0x05);
    Lcd_WriteIndex(0x3A);
    Lcd_WriteData(0x05);
    ST7735_SetRotation(0);
    Lcd_WriteIndex(0x29);
    Lcd_WriteIndex(0x2C);
    Lcd_WriteIndex(0x29);

    Lcd_Clear(LCD_RED);   Delay_ms(200);
    Lcd_Clear(LCD_GREEN); Delay_ms(200);
    Lcd_Clear(LCD_BLUE);  Delay_ms(200);
    Lcd_Clear(LCD_BLACK);
}

void Lcd_SetRegion(u16 x_start, u16 y_start, u16 x_end, u16 y_end)
{
    Lcd_WriteIndex(0x2a);
    Lcd_WriteData(x_start >> 8); Lcd_WriteData(x_start);
    Lcd_WriteData(x_end >> 8);   Lcd_WriteData(x_end);
    Lcd_WriteIndex(0x2b);
    Lcd_WriteData(y_start >> 8); Lcd_WriteData(y_start);
    Lcd_WriteData(y_end >> 8);   Lcd_WriteData(y_end);
    Lcd_WriteIndex(0x2C);
}

void Lcd_SetXY(u16 x, u16 y)
{
    Lcd_SetRegion(x, y, x, y);
}

void Lcd_DrawPoint(u16 x, u16 y, u16 Data)
{
    Lcd_SetRegion(x, y, x, y);
    Lcd_WriteData_16Bit(Data);
}

void Lcd_Clear(u16 Color)
{
    unsigned int i, j;
    DMA1_MEM_LEN = 2 * LCD_W;
    Lcd_SetRegion(0, 0, LCD_W - 1, LCD_H - 1);
    for (i = 0; i < 2 * LCD_W; i += 2) {
        SendBuff[i]     = Color >> 8;
        SendBuff[i + 1] = Color;
    }
    for (j = 0; j < LCD_H; j++)
        DMA_Start();
}

// ===================== 文本/图形函数 =====================

void LCD_ShowChar(u16 x, u16 y, char ch, u16 color, u16 bg)
{
    if (x > LCD_W - 8 || y > LCD_H - 16) return;
    const unsigned char *p = &ascii_1608[ch - ' '][0];
    Lcd_SetRegion(x, y, x + 7, y + 15);
    for (u8 row = 0; row < 16; row++) {
        unsigned char bits = p[row];
        for (u8 col = 0; col < 8; col++) {
            Lcd_WriteData_16Bit((bits & (0x01 << col)) ? color : bg);
        }
    }
}

void LCD_ShowString(u16 x, u16 y, const char *str, u16 color, u16 bg)
{
    while (*str) {
        if (x > LCD_W - 8) { x = 0; y += 16; }
        if (y > LCD_H - 16) break;
        LCD_ShowChar(x, y, *str, color, bg);
        x += 8;
        str++;
    }
}

void LCD_ShowNum(u16 x, u16 y, uint32_t num, uint8_t len, u16 color, u16 bg)
{
    uint8_t started = 0;
    for (int8_t i = len - 1; i >= 0; i--) {
        uint32_t div = 1;
        for (uint8_t j = 0; j < i; j++) div *= 10;
        uint8_t digit = (num / div) % 10;
        if (digit || started || i == 0) {
            LCD_ShowChar(x, y, '0' + digit, color, bg);
            started = 1;
        } else {
            LCD_ShowChar(x, y, ' ', color, bg);
        }
        x += 8;
    }
}

void LCD_ShowSignedNum(u16 x, u16 y, int32_t num, uint8_t len, u16 color, u16 bg)
{
    if (num < 0) {
        LCD_ShowChar(x, y, '-', color, bg);
        x += 8;
        LCD_ShowNum(x, y, (uint32_t)(-num), len - 1, color, bg);
    } else {
        LCD_ShowChar(x, y, ' ', color, bg);
        x += 8;
        LCD_ShowNum(x, y, (uint32_t)num, len - 1, color, bg);
    }
}

void LCD_DrawMonoBMP(u16 x, u16 y, u16 w, u16 h, const uint8_t *bmp, u16 color, u16 bg)
{
    u16 byte_w = (w + 7) / 8;
    for (u16 row = 0; row < h; row++) {
        for (u16 col = 0; col < w; col++) {
            uint8_t bit = bmp[row * byte_w + col / 8] & (0x01 << (col % 8));
            Lcd_DrawPoint(x + col, y + row, bit ? color : bg);
        }
    }
}


// ===================== 局部刷新函数 =====================

void LCD_ClearRegion(u16 x, u16 y, u16 w, u16 h, u16 bg)
{
    Lcd_SetRegion(x, y, x + w - 1, y + h - 1);
    for (u16 i = 0; i < w * h; i++) {
        Lcd_WriteData_16Bit(bg);
    }
}

void LCD_RefreshNum(u16 x, u16 y, uint32_t val, uint8_t len, u16 color, u16 bg)
{
    LCD_ClearRegion(x, y, len * 8, 16, bg);
    LCD_ShowNum(x, y, val, len, color, bg);
}

void LCD_RefreshSignedNum(u16 x, u16 y, int32_t val, uint8_t len, u16 color, u16 bg)
{
    LCD_ClearRegion(x, y, len * 8, 16, bg);
    LCD_ShowSignedNum(x, y, val, len, color, bg);
}

void LCD_RefreshString(u16 x, u16 y, const char *str, u16 max_w, u16 color, u16 bg)
{
    LCD_ClearRegion(x, y, max_w, 16, bg);
    LCD_ShowString(x, y, str, color, bg);
}

// ===================== 灰度图像显示（逐行 DMA）=====================

void LCD_ShowGrayImage(uint8_t *img, u16 w, u16 h)
{
    u16 x_off = (LCD_W - w) / 2;
    u16 y_off = (LCD_H - h) / 2;

    Lcd_Clear(LCD_BLACK);
    DMA1_MEM_LEN = 2 * w;

    for (u16 row = 0; row < h; row++) {
        for (u16 col = 0; col < w; col++) {
            u16 src_col = w - 1 - col;
            uint8_t gray = img[row * w + src_col];
            SendBuff[col * 2]     = (gray & 0xF8) | (gray >> 5);
            SendBuff[col * 2 + 1] = ((gray & 0x1C) << 3) | (gray >> 3);
        }
        Lcd_SetRegion(x_off, y_off + row, x_off + w - 1, y_off + row);
        DMA_Start();
    }
}

// ===================== 彩色条形图（渐变色块）=====================
static u16 Bar_Color(uint8_t pos, uint8_t width)
{
    // 绿 → 黄 → 红 渐变
    uint8_t r, g;
    uint16_t t = (uint16_t)pos * 510 / width;
    if (t <= 255) { g = 31; r = (uint8_t)(t * 31 / 255); }
    else          { r = 31; g = (uint8_t)((510 - t) * 31 / 255); }
    return (u16)((r << 11) | (g << 5) | 0x00);
}

static void UI_Bar(u16 x, u16 y, uint8_t val, uint8_t width, u16 bg)
{
    u8 i, row, col;
    if (val > 100) val = 100;
    uint8_t filled = (uint8_t)((uint16_t)val * width / 100);
    u16 dark = 0x2104;  // 深灰

    for (i = 0; i < width; i++) {
        u16 color = (i < filled) ? Bar_Color(i, width) : dark;
        Lcd_SetRegion(x + i * 8, y, x + i * 8 + 7, y + 15);
        for (row = 0; row < 16; row++) {
            for (col = 0; col < 8; col++) {
                Lcd_WriteData_16Bit(color);
            }
        }
    }
}

// ===================== IDLE 界面 =====================
//  0: IDLE
//  1: [a] Manual
//  2: [b] Record
//  3: [c] Replay
//  4: [d] SendPic
//  5:
//  6: PIC: XX / 10
//  7: SAV:Y DAT:Y

void UI_IdleLabels(void)
{
    Lcd_Clear(LCD_BLACK);
    LCD_ShowString(TX(0), TY(0), "IDLE", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(1), "[a] Manual", LCD_GREEN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(2), "[b] Record", LCD_YELLOW, LCD_BLACK);
    LCD_ShowString(TX(0), TY(3), "[c] Replay", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(4), "[d] SendPic", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(0), TY(6), "PIC:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(9), TY(6), "/10", LCD_DARKGREY, LCD_BLACK);
    LCD_ShowString(TX(0), TY(7), "SAV:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(7), TY(7), "DAT:", LCD_WHITE, LCD_BLACK);
}

void UI_IdleRefreshLCD(uint16_t photoCnt, uint8_t saved, uint8_t hasData)
{
    LCD_RefreshNum(TX(5), TY(6), photoCnt, 2, LCD_YELLOW, LCD_BLACK);
    LCD_ShowString(TX(4), TY(7), saved ? "Y" : "N",
                   saved ? LCD_GREEN : LCD_RED, LCD_BLACK);
    LCD_ShowString(TX(11), TY(7), hasData ? "Y" : "N",
                   hasData ? LCD_GREEN : LCD_RED, LCD_BLACK);
}

// ===================== MANUAL 界面 =====================
//  0: MANUAL
//  1: L XXX    R XXX
//  2: bar[12]       bar[12]  (col 2~13, col 2~13)
//  3: PIC: XX/10
//  4: PT:XXX  YW:XXX
//  5: ADC: ----

void UI_ManualLabels(void)
{
    Lcd_Clear(LCD_BLACK);
    LCD_ShowString(TX(0), TY(0), "MANUAL", LCD_GREEN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(1), "L", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(8), TY(1), "R", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(3), "PIC:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(7), TY(3), "/10", LCD_DARKGREY, LCD_BLACK);
    LCD_ShowString(TX(0), TY(4), "PT:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(7), TY(4), "YW:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(5), "ADC:", LCD_WHITE, LCD_BLACK);
}

void UI_ManualRefreshLCD(uint8_t left_spd, uint8_t right_spd,
                         uint8_t pitch, uint8_t yaw,
                         uint16_t photoCnt)
{
    LCD_RefreshNum(TX(2), TY(1), left_spd, 3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(1), right_spd, 3, LCD_WHITE, LCD_BLACK);
    UI_Bar(TX(2), TY(2), left_spd, 12, 0x2104);
    LCD_RefreshNum(TX(5), TY(3), photoCnt, 2, LCD_YELLOW, LCD_BLACK);
    LCD_RefreshNum(TX(3), TY(4), pitch, 3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(4), yaw, 3, LCD_WHITE, LCD_BLACK);
}

// ===================== RECORD 界面 =====================
//  0: REC  ST:XXXX
//  1: PH:XX  ADC:XXXX
//  2: L F XXX  R B XXX
//  3: bar[12]
//  4: PT:XXX YW:XXX
//  5:
//  6: [e]Photo [h]Stop

void UI_RecordLabels(void)
{
    Lcd_Clear(LCD_BLACK);
    LCD_ShowString(TX(0), TY(0), "REC", LCD_RED, LCD_BLACK);
    LCD_ShowString(TX(5), TY(0), "ST:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(0), TY(1), "PH:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(6), TY(1), "ADC:", LCD_WHITE, LCD_BLACK);
    LCD_ShowString(TX(0), TY(2), "L", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(8), TY(2), "R", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(4), "PT:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(7), TY(4), "YW:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0), TY(6), "[e]Photo [h]Stop", LCD_DARKGREY, LCD_BLACK);
    prev_recording     = 0xFF;
    prev_photo_pending = 0xFF;
}

void UI_RecordRefreshLCD(uint8_t recording, uint16_t stepCnt, uint16_t photoCnt,
                         uint8_t left_dir, uint8_t left_spd,
                         uint8_t right_dir, uint8_t right_spd,
                         uint8_t pitch, uint8_t yaw, uint16_t adcCnt)
{
    if (recording != prev_recording) {
        LCD_RefreshString(TX(0), TY(0), recording ? "REC" : "---",
                          3 * 8, recording ? LCD_RED : LCD_DARKGREY, LCD_BLACK);
        prev_recording = recording;
    }
    LCD_RefreshNum(TX(8), TY(0), stepCnt, 4, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(3), TY(1), photoCnt, 2, LCD_YELLOW, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(1), adcCnt, 4, LCD_WHITE, LCD_BLACK);

    LCD_ShowChar(TX(2), TY(2), left_dir ? 'B' : 'F',
                 left_dir ? LCD_RED : LCD_GREEN, LCD_BLACK);
    LCD_RefreshNum(TX(4), TY(2), left_spd, 3, LCD_WHITE, LCD_BLACK);
    LCD_ShowChar(TX(10), TY(2), right_dir ? 'B' : 'F',
                 right_dir ? LCD_RED : LCD_GREEN, LCD_BLACK);
    LCD_RefreshNum(TX(12), TY(2), right_spd, 3, LCD_WHITE, LCD_BLACK);
    UI_Bar(TX(2), TY(3), left_spd, 12, 0x2104);

    LCD_RefreshNum(TX(3), TY(4), pitch, 3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(4), yaw, 3, LCD_WHITE, LCD_BLACK);
}

// ===================== REPLAY 界面 =====================
//  0: S001/050 T1234
//  1: PH:XX/10
//  2: L F +050 R B -030
//  3: bar[7]    bar[7]
//  4: PT:XXX YW:XXX
//  5:
//  6: [h] Exit

void UI_ReplayLabels(void)
{
    Lcd_Clear(LCD_BLACK);
    LCD_ShowString(TX(0),  TY(0), "S", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(4),  TY(0), "/", LCD_DARKGREY, LCD_BLACK);
    LCD_ShowString(TX(9),  TY(0), "T", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0),  TY(1), "PH:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0),  TY(4), "PT:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(7),  TY(4), "YW:", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(TX(0),  TY(6), "[h] Exit", LCD_DARKGREY, LCD_BLACK);
}

void UI_ReplayRefreshLCD(uint8_t step_cur, uint16_t step_total,
                         uint16_t dur, uint16_t photo_cnt, uint16_t max_photos,
                         int16_t left, int16_t right,
                         uint8_t pitch, uint8_t yaw)
{
    LCD_RefreshNum(TX(1),  TY(0), step_cur,   3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(5),  TY(0), step_total, 3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(0), dur,        4, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(3),  TY(1), photo_cnt,  2, LCD_YELLOW, LCD_BLACK);
    LCD_ShowChar(TX(5),    TY(1), '/', LCD_DARKGREY, LCD_BLACK);
    LCD_RefreshNum(TX(6),  TY(1), max_photos, 2, LCD_RED, LCD_BLACK);

    {
        uint8_t l_dir = (left < 0) ? 1 : 0;
        uint8_t l_spd = (uint8_t)(left < 0 ? -left : left);
        uint8_t r_dir = (right < 0) ? 1 : 0;
        uint8_t r_spd = (uint8_t)(right < 0 ? -right : right);
        LCD_ShowChar(TX(0), TY(2), 'L',
                     l_dir ? LCD_RED : LCD_GREEN, LCD_BLACK);
        LCD_RefreshSignedNum(TX(2), TY(2), left, 4, LCD_WHITE, LCD_BLACK);
        LCD_ShowChar(TX(8), TY(2), 'R',
                     r_dir ? LCD_RED : LCD_GREEN, LCD_BLACK);
        LCD_RefreshSignedNum(TX(10), TY(2), right, 4, LCD_WHITE, LCD_BLACK);
        // 统一彩色条：左右取最大值
        uint8_t max_spd = (l_spd > r_spd) ? l_spd : r_spd;
        UI_Bar(TX(2), TY(3), max_spd, 12, 0x2104);
    }

    LCD_RefreshNum(TX(3),  TY(4), pitch, 3, LCD_WHITE, LCD_BLACK);
    LCD_RefreshNum(TX(10), TY(4), yaw,   3, LCD_WHITE, LCD_BLACK);
}

// ===================== 状态控制 =====================

void UI_SetRecording(uint8_t val)
{
    if (val != prev_recording) {
        LCD_RefreshString(TX(0), TY(0), val ? "REC" : "---",
                          3 * 8, val ? LCD_RED : LCD_DARKGREY, LCD_BLACK);
        prev_recording = val;
    }
}

void UI_SetPhotoPending(uint8_t val)
{
    if (val != prev_photo_pending) {
        if (val) {
            LCD_ShowString(TX(0), TY(0), "CAM", LCD_YELLOW, LCD_BLACK);
        } else {
            LCD_RefreshString(TX(0), TY(0), "REC", 3 * 8, LCD_RED, LCD_BLACK);
        }
        prev_photo_pending = val;
    }
}
