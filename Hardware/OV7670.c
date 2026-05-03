#include "OV7670.h"
#include "SCCB.h"
#include "TIM2.h"
#include "LCD_Driver.h"
#include "Delay.h"
#include "flash.h"
#include "motor.h"

// ===================== OV7670 输出参数 =====================
#define OV7670_LINE_PIXELS  256
#define OV7670_LINES        144
#define OV7670_VALID_W      128

#define CROP_X0     ((OV7670_VALID_W - CAM_IMG_WIDTH) / 2)   // 4
#define CROP_X1     (CROP_X0 + CAM_IMG_WIDTH)                 // 124
#define CROP_Y0     ((OV7670_LINES - CAM_IMG_HEIGHT) / 2)    // 27
#define CROP_Y1     (CROP_Y0 + CAM_IMG_HEIGHT)                // 117

#define VSYNC_PIN   GPIO_Pin_15
#define HREF_PIN    GPIO_Pin_3
#define PCLK_PIN    GPIO_Pin_5
#define READ_PIXEL()  ((uint8_t)(GPIOB->IDR >> 8))

// ===================== 错误输出 =====================
void Error_Output(void)
{
    Lcd_Clear(LCD_RED);
    while(1);
}

// ===================== SCCB 寄存器配置 =====================
static void OV7670_RegExample(void)
{
    SCCB_WriteReg(0x3a, 0x04);
    SCCB_WriteReg(0x40, 0xd0);
    SCCB_WriteReg(0x12, 0x0C);
    SCCB_WriteReg(0x32, 0x80);
    SCCB_WriteReg(0x17, 0x16);
    SCCB_WriteReg(0x18, 0x04);
    SCCB_WriteReg(0x19, 0x02);
    SCCB_WriteReg(0x1a, 0x7b);
    SCCB_WriteReg(0x03, 0x06);
    SCCB_WriteReg(0x0c, 0x00);
    SCCB_WriteReg(0x15, 0x00);
    SCCB_WriteReg(0x3e, 0x00);
    SCCB_WriteReg(0x70, 0x3a);
    SCCB_WriteReg(0x71, 0x35);
    SCCB_WriteReg(0x72, 0x11);
    SCCB_WriteReg(0x73, 0x00);
    SCCB_WriteReg(0xa2, 0x02);
    SCCB_WriteReg(0x11, 0x00);
    SCCB_WriteReg(0x7a, 0x20);
    SCCB_WriteReg(0x7b, 0x1c);
    SCCB_WriteReg(0x7c, 0x28);
    SCCB_WriteReg(0x7d, 0x3c);
    SCCB_WriteReg(0x7e, 0x55);
    SCCB_WriteReg(0x7f, 0x68);
    SCCB_WriteReg(0x80, 0x76);
    SCCB_WriteReg(0x81, 0x80);
    SCCB_WriteReg(0x82, 0x88);
    SCCB_WriteReg(0x83, 0x8f);
    SCCB_WriteReg(0x84, 0x96);
    SCCB_WriteReg(0x85, 0xa3);
    SCCB_WriteReg(0x86, 0xaf);
    SCCB_WriteReg(0x87, 0xc4);
    SCCB_WriteReg(0x88, 0xd7);
    SCCB_WriteReg(0x89, 0xe8);
    SCCB_WriteReg(0x13, 0xe0);
    SCCB_WriteReg(0x00, 0x00);
    SCCB_WriteReg(0x10, 0x00);
    SCCB_WriteReg(0x0d, 0x00);
    SCCB_WriteReg(0x14, 0x28);
    SCCB_WriteReg(0xa5, 0x05);
    SCCB_WriteReg(0xab, 0x07);
    SCCB_WriteReg(0x24, 0x75);
    SCCB_WriteReg(0x25, 0x63);
    SCCB_WriteReg(0x26, 0xA5);
    SCCB_WriteReg(0x9f, 0x78);
    SCCB_WriteReg(0xa0, 0x68);
    SCCB_WriteReg(0xa1, 0x03);
    SCCB_WriteReg(0xa6, 0xdf);
    SCCB_WriteReg(0xa7, 0xdf);
    SCCB_WriteReg(0xa8, 0xf0);
    SCCB_WriteReg(0xa9, 0x90);
    SCCB_WriteReg(0xaa, 0x94);
    SCCB_WriteReg(0x13, 0xe5);
    SCCB_WriteReg(0x0e, 0x61);
    SCCB_WriteReg(0x0f, 0x4b);
    SCCB_WriteReg(0x16, 0x02);
    SCCB_WriteReg(0x1e, 0x37);
    SCCB_WriteReg(0x21, 0x02);
    SCCB_WriteReg(0x22, 0x91);
    SCCB_WriteReg(0x29, 0x07);
    SCCB_WriteReg(0x33, 0x0b);
    SCCB_WriteReg(0x35, 0x0b);
    SCCB_WriteReg(0x37, 0x1d);
    SCCB_WriteReg(0x38, 0x71);
    SCCB_WriteReg(0x39, 0x2a);
    SCCB_WriteReg(0x3c, 0x78);
    SCCB_WriteReg(0x4d, 0x40);
    SCCB_WriteReg(0x4e, 0x20);
    SCCB_WriteReg(0x69, 0x00);
    SCCB_WriteReg(0x6b, 0x60);
    SCCB_WriteReg(0x74, 0x19);
    SCCB_WriteReg(0x8d, 0x4f);
    SCCB_WriteReg(0x8e, 0x00);
    SCCB_WriteReg(0x8f, 0x00);
    SCCB_WriteReg(0x90, 0x00);
    SCCB_WriteReg(0x91, 0x00);
    SCCB_WriteReg(0x92, 0x00);
    SCCB_WriteReg(0x96, 0x00);
    SCCB_WriteReg(0x9a, 0x80);
    SCCB_WriteReg(0xb0, 0x84);
    SCCB_WriteReg(0xb1, 0x0c);
    SCCB_WriteReg(0xb2, 0x0e);
    SCCB_WriteReg(0xb3, 0x82);
    SCCB_WriteReg(0xb8, 0x0a);
    SCCB_WriteReg(0x43, 0x14);
    SCCB_WriteReg(0x44, 0xf0);
    SCCB_WriteReg(0x45, 0x34);
    SCCB_WriteReg(0x46, 0x58);
    SCCB_WriteReg(0x47, 0x28);
    SCCB_WriteReg(0x48, 0x3a);
    SCCB_WriteReg(0x59, 0x88);
    SCCB_WriteReg(0x5a, 0x88);
    SCCB_WriteReg(0x5b, 0x44);
    SCCB_WriteReg(0x5c, 0x67);
    SCCB_WriteReg(0x5d, 0x49);
    SCCB_WriteReg(0x5e, 0x0e);
    SCCB_WriteReg(0x64, 0x04);
    SCCB_WriteReg(0x65, 0x20);
    SCCB_WriteReg(0x66, 0x05);
    SCCB_WriteReg(0x94, 0x04);
    SCCB_WriteReg(0x95, 0x08);
    SCCB_WriteReg(0x6c, 0x0a);
    SCCB_WriteReg(0x6d, 0x55);
    SCCB_WriteReg(0x4f, 0x80);
    SCCB_WriteReg(0x50, 0x80);
    SCCB_WriteReg(0x51, 0x00);
    SCCB_WriteReg(0x52, 0x22);
    SCCB_WriteReg(0x53, 0x5e);
    SCCB_WriteReg(0x54, 0x80);
    SCCB_WriteReg(0x09, 0x03);
    SCCB_WriteReg(0x6e, 0x11);
    SCCB_WriteReg(0x6f, 0x9f);
    SCCB_WriteReg(0x55, 0x00);
    SCCB_WriteReg(0x56, 0x40);
    SCCB_WriteReg(0x57, 0x40);
    SCCB_WriteReg(0x6a, 0x40);
    SCCB_WriteReg(0x01, 0x40);
    SCCB_WriteReg(0x02, 0x40);
    SCCB_WriteReg(0x13, 0xe7);
    SCCB_WriteReg(0x15, 0x00);
    SCCB_WriteReg(0x58, 0x9e);
    SCCB_WriteReg(0x41, 0x08);
    SCCB_WriteReg(0x3f, 0x00);
    SCCB_WriteReg(0x75, 0x05);
    SCCB_WriteReg(0x76, 0xe1);
    SCCB_WriteReg(0x4c, 0x00);
    SCCB_WriteReg(0x77, 0x01);
    SCCB_WriteReg(0x3d, 0xc2);
    SCCB_WriteReg(0x4b, 0x09);
    SCCB_WriteReg(0xc9, 0x60);
    SCCB_WriteReg(0x41, 0x38);
    SCCB_WriteReg(0x34, 0x11);
    SCCB_WriteReg(0x3b, 0x02);
    SCCB_WriteReg(0xa4, 0x89);
    SCCB_WriteReg(0x96, 0x00);
    SCCB_WriteReg(0x97, 0x30);
    SCCB_WriteReg(0x98, 0x20);
    SCCB_WriteReg(0x99, 0x30);
    SCCB_WriteReg(0x9a, 0x84);
    SCCB_WriteReg(0x9b, 0x29);
    SCCB_WriteReg(0x9c, 0x03);
    SCCB_WriteReg(0x9d, 0x4c);
    SCCB_WriteReg(0x9e, 0x3f);
    SCCB_WriteReg(0x78, 0x04);
    SCCB_WriteReg(0x79, 0x01);
    SCCB_WriteReg(0xc8, 0xf0);
    SCCB_WriteReg(0x79, 0x0f);
    SCCB_WriteReg(0xc8, 0x00);
    SCCB_WriteReg(0x79, 0x10);
    SCCB_WriteReg(0xc8, 0x7e);
    SCCB_WriteReg(0x79, 0x0a);
    SCCB_WriteReg(0xc8, 0x80);
    SCCB_WriteReg(0x79, 0x0b);
    SCCB_WriteReg(0xc8, 0x01);
    SCCB_WriteReg(0x79, 0x0c);
    SCCB_WriteReg(0xc8, 0x0f);
    SCCB_WriteReg(0x79, 0x0d);
    SCCB_WriteReg(0xc8, 0x20);
    SCCB_WriteReg(0x79, 0x09);
    SCCB_WriteReg(0xc8, 0x80);
    SCCB_WriteReg(0x79, 0x02);
    SCCB_WriteReg(0xc8, 0xc0);
    SCCB_WriteReg(0x79, 0x03);
    SCCB_WriteReg(0xc8, 0x40);
    SCCB_WriteReg(0x79, 0x05);
    SCCB_WriteReg(0xc8, 0x30);
    SCCB_WriteReg(0x79, 0x26);
    SCCB_WriteReg(0x09, 0x00);
}

static void OV7670_RST(void)
{
    SCCB_WriteReg(0x12, 0x80);
    Delay_ms(20);
}

static void OV7670_Set_PCLK_Divider(uint8_t divider)
{
    SCCB_WriteReg(0x11, divider);
    Delay_ms(20);
}

static void DataPort_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                                  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
                                  GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = VSYNC_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = HREF_PIN | PCLK_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

// ===================== 初始化 =====================
void OV7670_Init(void)
{
    // 顺序与参考一致：先 SCCB，再 GPIO，再 MCLK
    SCCB_Init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    DataPort_Init();

    TIM3_MCLK_Init(3, 1);
    OV7670_RST();
    OV7670_RegExample();
    OV7670_Set_PCLK_Divider(0x05);
}

// ===================== 拍照（带重试）=====================
int8_t OV7670_Capture(uint8_t *gray_buf)
{
    uint32_t timeout;

    timeout = 8000000;
    while (!(GPIOA->IDR & VSYNC_PIN)) { if (--timeout == 0) return -1; }

    for (uint16_t line = 0; line < OV7670_LINES; line++)
    {
        timeout = 8000000;
        while (!(GPIOB->IDR & HREF_PIN)) { if (--timeout == 0) return -1; }

        for (uint16_t px = 0; px < OV7670_LINE_PIXELS; px++)
        {
            timeout = 8000000;
            while (!(GPIOB->IDR & PCLK_PIN)) { if (--timeout == 0) return -1; }

            if ((px & 1) == 0) {
                uint16_t y_px = px >> 1;
                if (line >= CROP_Y0 && line < CROP_Y1 &&
                    y_px >= CROP_X0 && y_px < CROP_X1)
                {
                    uint16_t oy = line - CROP_Y0;
                    uint16_t ox = y_px - CROP_X0;
                    gray_buf[oy * CAM_IMG_WIDTH + ox] = READ_PIXEL();
                }
            }
        }

        timeout = 8000000;
        while (GPIOB->IDR & HREF_PIN) { if (--timeout == 0) return -1; }
    }

    return 0;
}

// ===================== 拍照 + 存图 + LCD 显示 =====================
void OV7670_DoPhoto(uint8_t *imgBuf, uint8_t *photoCnt)
{
    static uint8_t cam_inited = 0;

    Motor_Stop();

    if (*photoCnt >= MAX_PHOTOS) {
        Lcd_Clear(LCD_BLACK);
        LCD_ShowString(TX(0), TY(0), "FULL!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(TX(0), TY(2), "10/10 photos", LCD_WHITE, LCD_BLACK);
        Delay_ms(1000);
        return;
    }

    if (!cam_inited) {
        OV7670_Init();
        cam_inited = 1;
    }

    if (OV7670_Capture(imgBuf) == 0) {
        uint32_t addr = FLASH_PHOTO_BASE + (uint32_t)(*photoCnt) * PHOTO_SIZE;
        Flash_EraseSector(addr);
        Flash_Write(addr, imgBuf, CAM_IMG_SIZE);
        (*photoCnt)++;

        LCD_ShowGrayImage(imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
        Delay_ms(3000);

        Lcd_Clear(LCD_BLACK);
        LCD_ShowString(TX(0), TY(0), "SAVED!", LCD_GREEN, LCD_BLACK);
        LCD_ShowString(TX(0), TY(2), "PIC:", LCD_WHITE, LCD_BLACK);
        LCD_ShowNum(TX(4), TY(2), *photoCnt, 2, LCD_WHITE, LCD_BLACK);
        Delay_ms(500);
    } else {
        LCD_RefreshString(TX(0), TY(7), "Capture FAIL!", 13 * 8, LCD_RED, LCD_BLACK);
        Delay_ms(300);
        LCD_ClearRegion(TX(0), TY(7), 13 * 8, 16, LCD_BLACK);
    }
}
