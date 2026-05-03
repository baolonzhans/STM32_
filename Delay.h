#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

// SysTick 延时
void Delay_us(uint32_t xus);
void Delay_ms(uint32_t xms);
void Delay_s(uint32_t xs);

// TIM4 计时器
void     Tick_Init(void);
uint32_t g_tick_now(void);
void     Tick_StartOneShot(uint16_t ms);
uint8_t  Tick_IsExpired(void);
void     Tick_SaveFreeRun(void);
void     Tick_RestoreFreeRun(void);

// 回放缓冲 (中断直接输出用)
extern volatile uint8_t  replay_step_ready;
extern volatile uint8_t  replay_motor[6];
extern volatile uint16_t replay_duration;
extern volatile uint8_t  g_tick_expired;

// 休眠
void Enter_Sleep(void);

#endif
