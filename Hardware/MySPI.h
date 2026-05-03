#ifndef __MYSPI_H
#define __MYSPI_H

#include "stm32f10x.h"

void MySPI_Init(void);
void MySPI_Start(void);
void MySPI_Stop(void);
void MySPI_SwapByte(uint8_t ByteSend);
void MySPI_W_SS(uint8_t BitValue);

// SPI 分频器切换（LCD/Flash 共用 SPI1 时切换速度）
void SPI_SetFlashSpeed(void);   // 切到 18MHz
void SPI_SetLCDSpeed(void);     // 切到 36MHz

#endif
