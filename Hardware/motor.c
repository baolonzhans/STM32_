#include "stm32f10x.h"

// ===================== 引脚定义 =====================
// TB6612 电机驱动 —— 与 OV7670 引脚分开
//
// 左电机 (A通道):
//   PWMA  -> PA2  (TIM2_CH3)
//   AIN1  -> PA0
//   AIN2  -> PA12
//
// 右电机 (B通道):
//   PWMB  -> PA3  (TIM2_CH4)
//   BIN1  -> PA1
//   BIN2  -> PB2

// PWM 引脚
#define MOTOR_PWM_L_GPIO_PORT    GPIOA
#define MOTOR_PWM_L_GPIO_PIN     GPIO_Pin_2      // PA2  TIM2_CH3 左电机PWM
#define MOTOR_PWM_R_GPIO_PORT    GPIOA
#define MOTOR_PWM_R_GPIO_PIN     GPIO_Pin_3      // PA3  TIM2_CH4 右电机PWM

// 左电机方向 (A通道)
#define MOTOR_AIN1_GPIO_PORT     GPIOA
#define MOTOR_AIN1_GPIO_PIN      GPIO_Pin_0      // PA0  AIN1
#define MOTOR_AIN2_GPIO_PORT     GPIOA
#define MOTOR_AIN2_GPIO_PIN      GPIO_Pin_12     // PA12 AIN2

// 右电机方向 (B通道)
#define MOTOR_BIN1_GPIO_PORT     GPIOA
#define MOTOR_BIN1_GPIO_PIN      GPIO_Pin_1      // PA1  BIN1
#define MOTOR_BIN2_GPIO_PORT     GPIOB
#define MOTOR_BIN2_GPIO_PIN      GPIO_Pin_2      // PB2  BIN2

// ===================== PWM初始化 =====================
// TIM2: CH3->PA2, CH4->PA3
// 72MHz / 72 / 500 = 2kHz
void PWM_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    // 释放 JTAG (PA15/PB3 给摄像头用, 保留 SWD)
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    // PA2 PA3 -> TIM2_CH3 CH4 复用推挽
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin   = MOTOR_PWM_L_GPIO_PIN | MOTOR_PWM_R_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_PWM_L_GPIO_PORT, &GPIO_InitStructure);

    // A方向: PA0, PA12
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin  = MOTOR_AIN1_GPIO_PIN | MOTOR_AIN2_GPIO_PIN;
    GPIO_Init(MOTOR_AIN1_GPIO_PORT, &GPIO_InitStructure);

    // B方向: PA1
    GPIO_InitStructure.GPIO_Pin = MOTOR_BIN1_GPIO_PIN;
    GPIO_Init(MOTOR_BIN1_GPIO_PORT, &GPIO_InitStructure);

    // B方向: PB2
    GPIO_InitStructure.GPIO_Pin = MOTOR_BIN2_GPIO_PIN;
    GPIO_Init(MOTOR_BIN2_GPIO_PORT, &GPIO_InitStructure);

    // TIM2 时基
    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    TIM_InitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_InitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_InitStructure.TIM_Period            = 500 - 1;
    TIM_InitStructure.TIM_Prescaler         = 72 - 1;
    TIM_InitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_InitStructure);

    // CH3 CH4 PWM输出
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;

    TIM_OC3Init(TIM2, &TIM_OCInitStructure);   // CH3 -> PA2 左电机
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);   // CH4 -> PA3 右电机

    TIM_Cmd(TIM2, ENABLE);
}

// ===================== 方向控制 =====================
// left:  0=正转  1=反转
// right: 0=正转  1=反转
void dir_control(uint8_t left, uint8_t right)
{
    if (left == 0) {
        GPIO_WriteBit(MOTOR_AIN1_GPIO_PORT, MOTOR_AIN1_GPIO_PIN, Bit_SET);
        GPIO_WriteBit(MOTOR_AIN2_GPIO_PORT, MOTOR_AIN2_GPIO_PIN, Bit_RESET);
    } else {
        GPIO_WriteBit(MOTOR_AIN1_GPIO_PORT, MOTOR_AIN1_GPIO_PIN, Bit_RESET);
        GPIO_WriteBit(MOTOR_AIN2_GPIO_PORT, MOTOR_AIN2_GPIO_PIN, Bit_SET);
    }

    if (right == 0) {
        GPIO_WriteBit(MOTOR_BIN1_GPIO_PORT, MOTOR_BIN1_GPIO_PIN, Bit_SET);
        GPIO_WriteBit(MOTOR_BIN2_GPIO_PORT, MOTOR_BIN2_GPIO_PIN, Bit_RESET);
    } else {
        GPIO_WriteBit(MOTOR_BIN1_GPIO_PORT, MOTOR_BIN1_GPIO_PIN, Bit_RESET);
        GPIO_WriteBit(MOTOR_BIN2_GPIO_PORT, MOTOR_BIN2_GPIO_PIN, Bit_SET);
    }
}

// ===================== PWM输出 =====================
// array[0]=左方向(0正/1反) array[1]=左速度(0~100)
// array[2]=右方向(0正/1反) array[3]=右速度(0~100)
void pwm_out(uint8_t *array)
{
    dir_control(array[0], array[2]);
    TIM_SetCompare3(TIM2, (uint16_t)array[1] * 5);   // CH3 左电机  0~100 → 0~500 (ARR=499)
    TIM_SetCompare4(TIM2, (uint16_t)array[3] * 5);   // CH4 右电机
}

// ===================== 电机停止 =====================
void Motor_Stop(void)
{
    TIM_SetCompare3(TIM2, 0);
    TIM_SetCompare4(TIM2, 0);
    GPIO_WriteBit(MOTOR_AIN1_GPIO_PORT, MOTOR_AIN1_GPIO_PIN, Bit_RESET);
    GPIO_WriteBit(MOTOR_AIN2_GPIO_PORT, MOTOR_AIN2_GPIO_PIN, Bit_RESET);
    GPIO_WriteBit(MOTOR_BIN1_GPIO_PORT, MOTOR_BIN1_GPIO_PIN, Bit_RESET);
    GPIO_WriteBit(MOTOR_BIN2_GPIO_PORT, MOTOR_BIN2_GPIO_PIN, Bit_RESET);
}
