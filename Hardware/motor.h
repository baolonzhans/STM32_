#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

void PWM_Init(void);
void dir_control(uint8_t left, uint8_t right);
void pwm_out(uint8_t *array);
void Motor_Stop(void);

#endif
