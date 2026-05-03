#ifndef __LCD_DRIVER_H
#define __LCD_DRIVER_H

#include "stm32f10x.h"

#define LCD_CTRLB   GPIOB
#define LCD_CTRLC   GPIOC

#define LCD_CS        GPIO_Pin_13  // PC13
#define LCD_DC        GPIO_Pin_0   // PB0 数据/命令选择
#define LCD_RST       GPIO_Pin_1   // PB1 复位 (与ADC共用，拍照前后需快速切换)

#define LCD_CS_SET    LCD_CTRLC->BSRR=LCD_CS
#define LCD_DC_SET    LCD_CTRLB->BSRR=LCD_DC
#define LCD_RST_SET   LCD_CTRLB->BSRR=LCD_RST

#define LCD_CS_CLR    LCD_CTRLC->BRR=LCD_CS
#define LCD_DC_CLR    LCD_CTRLB->BRR=LCD_DC
#define LCD_RST_CLR   LCD_CTRLB->BRR=LCD_RST

#define LCD_W 128
#define LCD_H 160

extern u8 SendBuff[];

// ===================== 基础驱动 =====================
void DMA_Start(void);
void Lcd_Init(void);
void Lcd_Clear(u16 Color);
void Lcd_SetXY(u16 x, u16 y);
void Lcd_DrawPoint(u16 x, u16 y, u16 Data);
void Lcd_SetRegion(u16 x_start, u16 y_start, u16 x_end, u16 y_end);
void Lcd_WriteData_16Bit(u16 Data);

// ===================== 颜色常量 =====================
#define LCD_WHITE     0xFFFF
#define LCD_BLACK     0x0000
#define LCD_RED       0xF800
#define LCD_GREEN     0x07E0
#define LCD_BLUE      0x001F
#define LCD_YELLOW    0xFFE0
#define LCD_CYAN      0x07FF
#define LCD_DARKGREY  0x4208

// ===================== 文本/图形函数 =====================
void LCD_ShowChar(u16 x, u16 y, char ch, u16 color, u16 bg);
void LCD_ShowString(u16 x, u16 y, const char *str, u16 color, u16 bg);
void LCD_ShowNum(u16 x, u16 y, uint32_t num, uint8_t len, u16 color, u16 bg);
void LCD_ShowSignedNum(u16 x, u16 y, int32_t num, uint8_t len, u16 color, u16 bg);
void LCD_DrawMonoBMP(u16 x, u16 y, u16 w, u16 h, const uint8_t *bmp, u16 color, u16 bg);

// ===================== 局部刷新函数 =====================
void LCD_ClearRegion(u16 x, u16 y, u16 w, u16 h, u16 bg);
void LCD_RefreshNum(u16 x, u16 y, uint32_t val, uint8_t len, u16 color, u16 bg);
void LCD_RefreshSignedNum(u16 x, u16 y, int32_t val, uint8_t len, u16 color, u16 bg);
void LCD_RefreshString(u16 x, u16 y, const char *str, u16 max_w, u16 color, u16 bg);

// ===================== 图像显示 =====================
void LCD_ShowGrayImage(uint8_t *img, u16 w, u16 h);

// ===================== 界面绘制 =====================
#define TX(col)  ((u16)(col) * 8)
#define TY(row)  ((u16)(row) * 16)

// 界面绘制
void UI_IdleLabels(void);
void UI_IdleRefreshLCD(uint16_t photoCnt, uint8_t saved, uint8_t hasData);

void UI_ManualLabels(void);
void UI_ManualRefreshLCD(uint8_t left_spd, uint8_t right_spd,
                         uint8_t pitch, uint8_t yaw,
                         uint16_t photoCnt);

void UI_RecordLabels(void);
void UI_RecordRefreshLCD(uint8_t recording, uint16_t stepCnt, uint16_t photoCnt,
                         uint8_t left_dir, uint8_t left_spd,
                         uint8_t right_dir, uint8_t right_spd,
                         uint8_t pitch, uint8_t yaw, uint16_t adcCnt);

void UI_ReplayLabels(void);
void UI_ReplayRefreshLCD(uint8_t step_cur, uint16_t step_total,
                         uint16_t dur, uint16_t photo_cnt, uint16_t max_photos,
                         int16_t left, int16_t right,
                         uint8_t pitch, uint8_t yaw);

// 状态控制
void UI_SetRecording(uint8_t val);
void UI_SetPhotoPending(uint8_t val);

#endif
