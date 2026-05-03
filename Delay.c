#include "stm32f10x.h"
#include "Delay.h"
#include "LCD_Driver.h"
#include "motor.h"
#include "servo.h"
#include "bluetooth.h"

// ===================== SysTick 延时 =====================
void Delay_us(uint32_t xus)
{
    SysTick->LOAD = 72 * xus;
    SysTick->VAL  = 0x00;
    SysTick->CTRL = 0x00000005;
    while (!(SysTick->CTRL & 0x00010000));
    SysTick->CTRL = 0x00000004;
}

void Delay_ms(uint32_t xms)
{
    while (xms--) Delay_us(1000);
}

void Delay_s(uint32_t xs)
{
    while (xs--) Delay_ms(1000);
}

// ===================== TIM4 计时器 =====================
volatile uint8_t  g_tick_expired = 0;
static volatile uint16_t g_tick_ofs = 0;
static volatile uint8_t  g_tick_oneshot = 0;

void Tick_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    TIM_InitStructure.TIM_Prescaler         = (uint16_t)(36000 - 1);
    TIM_InitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_InitStructure.TIM_Period            = 0xFFFF;
    TIM_InitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_InitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_InitStructure);

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    g_tick_ofs = 0;
    g_tick_expired = 0;
    g_tick_oneshot = 0;
    TIM_Cmd(TIM4, ENABLE);
}

uint32_t g_tick_now(void)
{
    uint16_t ofs1, ofs2, cnt;
    do {
        ofs1 = g_tick_ofs;
        cnt  = TIM_GetCounter(TIM4);
        ofs2 = g_tick_ofs;
    } while (ofs1 != ofs2);
    return (((uint32_t)ofs1 << 16) | cnt) / 2;
}

void Tick_StartOneShot(uint16_t ms)
{
    TIM_Cmd(TIM4, DISABLE);
    TIM_SetCounter(TIM4, 0);
    g_tick_ofs = 0;
    TIM_SetAutoreload(TIM4, (uint32_t)ms * 2 - 1);
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    g_tick_expired = 0;
    g_tick_oneshot = 1;
    TIM_Cmd(TIM4, ENABLE);
}

uint8_t Tick_IsExpired(void)
{
    return g_tick_expired;
}

// ===================== 回放缓冲 =====================
volatile uint8_t  replay_step_ready = 0;
volatile uint8_t  replay_motor[6] = {0};
volatile uint16_t replay_duration = 0;

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        if (g_tick_oneshot) {
            if (replay_step_ready) {
                pwm_out((uint8_t*)replay_motor);
                SG90_SetAngle((uint8_t*)replay_motor);

                uint16_t dur = replay_duration;
                if (dur == 0) dur = 1;
                TIM_SetAutoreload(TIM4, (uint32_t)dur * 2 - 1);
                TIM_SetCounter(TIM4, 0);
                TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
                g_tick_expired = 0;
                TIM_Cmd(TIM4, ENABLE);

                replay_step_ready = 0;
            } else {
                g_tick_expired = 1;
                g_tick_oneshot = 0;
                TIM_Cmd(TIM4, DISABLE);
            }
        } else {
            g_tick_ofs++;
        }
    }
}

// ===================== 保存/恢复自由运行状态 =====================
static uint16_t saved_ofs;
static uint16_t saved_cnt;
static uint8_t  saved_en;

void Tick_SaveFreeRun(void)
{
    saved_ofs = g_tick_ofs;
    saved_cnt = TIM_GetCounter(TIM4);
    saved_en  = (TIM4->CR1 & TIM_CR1_CEN) ? 1 : 0;
}

void Tick_RestoreFreeRun(void)
{
    TIM_Cmd(TIM4, DISABLE);
    TIM_SetAutoreload(TIM4, 0xFFFF);
    TIM_SetCounter(TIM4, saved_cnt);
    g_tick_ofs     = saved_ofs;
    g_tick_oneshot = 0;
    g_tick_expired = 0;
    if (saved_en) TIM_Cmd(TIM4, ENABLE);
}

// ===================== 休眠 =====================
void Enter_Sleep(void)
{
    Lcd_Clear(LCD_BLACK);
    Motor_Stop();

    while (1)
    {
        process_received_data();

        if (model_flag_1 || model_flag_2 || model_flag_3 ||
            model_flag_PHOTO || model_flag_PHOTOData ||
            model_flag_ADC || model_flag_RETURN)
            break;

        __WFI();
    }

    Lcd_Clear(LCD_BLACK);
}
