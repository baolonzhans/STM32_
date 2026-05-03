#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f10x.h"

void     Flash_Init(void);
void     Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void     Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
void     Flash_EraseSector(uint32_t addr);
uint32_t Flash_ReadJEDEC(void);

// ===================== 状态存储 =====================
#define STATE_MAGIC  0x524F424F

typedef struct {
    uint32_t magic;
    uint16_t motorRecCnt;
    uint16_t photoCnt;
    uint16_t recPhotoCnt;
    uint32_t loopCnt;
    uint8_t  reserved[4];
} SavedState;

extern uint16_t g_recPhotoCnt;     // 录制阶段记录的拍照次数

void    State_Save(uint16_t motorRecCnt, uint16_t recPhotoCnt, uint16_t photoCnt, uint16_t loopCnt);
uint8_t State_Load(SavedState *st);
void    State_Clear(void);
void    Flash_ScanPhotoFlags(void); // 扫描录制数据中的拍照标志

#endif
