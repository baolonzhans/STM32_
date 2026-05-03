#ifndef __SCCB_H
#define __SCCB_H

#include "stm32f10x.h"                  // Device header

void SCCB_Init(void);
void SCCB_WriteReg(uint8_t reg_addr, uint8_t data);
uint8_t SCCB_ReadReg(uint8_t reg_addr);

#endif
