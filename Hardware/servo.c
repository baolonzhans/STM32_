#include "stm32f10x.h"

// ===================== 引脚定义 =====================
#define SERVO_PITCH_GPIO_PORT   GPIOA
#define SERVO_PITCH_GPIO_PIN    GPIO_Pin_8      // PA8  TIM1_CH1 俯仰
#define SERVO_YAW_GPIO_PORT     GPIOA
#define SERVO_YAW_GPIO_PIN      GPIO_Pin_11     // PA11 TIM1_CH4 偏航

// ===================== 舵机初始化 =====================
// TIM1 高级定时器, 50Hz, 脉宽0.5ms~2.5ms -> 0~180度
void Servo_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = SERVO_PITCH_GPIO_PIN | SERVO_YAW_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 时基: 72MHz / 72 / 20000 = 50Hz
    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    TIM_InitStructure.TIM_Prescaler         = 72 - 1;
    TIM_InitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_InitStructure.TIM_Period            = 20000 - 1;
    TIM_InitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_InitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_InitStructure);

    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 1500;     // 中间位置1.5ms

    TIM_OC1Init(TIM1, &TIM_OCInitStructure);        // CH1 PA8 俯仰
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);        // CH4 PA11 偏航

    TIM_CtrlPWMOutputs(TIM1, ENABLE);   // 高级定时器必须
    TIM_Cmd(TIM1, ENABLE);
}

// ===================== 设置舵机角度 =====================
// angle: 0~180 度
void Servo_SetPitch(uint16_t angle)
{
    if (angle > 180) angle = 180;
    uint16_t pulse = 500 + (uint32_t)angle * 2000 / 180;   // 500~2500us
    TIM_SetCompare1(TIM1, pulse);
}

void Servo_SetYaw(uint16_t angle)
{
    if (angle > 180) angle = 180;
    uint16_t pulse = 500 + (uint32_t)angle * 2000 / 180;
    TIM_SetCompare4(TIM1, pulse);
}

// ===================== 舵机关闭 =====================
void Servo_Disable(void)
{
    TIM_CtrlPWMOutputs(TIM1, DISABLE);
    TIM_Cmd(TIM1, DISABLE);
}
void SG90_Init(void)
{
    Servo_Init();
}

// motorCmd[6]: [左Dir, 左Spd, 右Dir, 右Spd, 俯仰, 偏航]
void SG90_SetAngle(uint8_t *array)
{
    Servo_SetPitch(array[4]);  // 俯仰
    Servo_SetYaw(array[5]);    // 偏航
}
