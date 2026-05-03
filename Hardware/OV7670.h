#ifndef __OV7670_H
#define __OV7670_H

#include "stm32f10x.h"

#define CAM_IMG_WIDTH   120
#define CAM_IMG_HEIGHT  90
#define CAM_IMG_SIZE    (CAM_IMG_WIDTH * CAM_IMG_HEIGHT)

#define FLASH_PHOTO_BASE  0x020000
#define FLASH_PHOTO_LIMIT (FLASH_PHOTO_BASE + 0x20000)
#define PHOTO_SIZE        10800
#define MAX_PHOTOS        10

void    OV7670_Init(void);
int8_t  OV7670_Capture(uint8_t *gray_buf);

// 拍照 + 存图 + LCD 显示
void    OV7670_DoPhoto(uint8_t *imgBuf, uint8_t *photoCnt);

#endif
