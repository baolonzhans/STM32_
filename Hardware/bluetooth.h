#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f10x.h"

// ===================== 缓冲区定义 =====================
#define DMA_BUF_SIZE    128

// ===================== 全局变量声明 =====================
extern uint8_t  dma_buf[2][DMA_BUF_SIZE];
extern uint8_t  ready_buf;
extern uint16_t frame_len;
extern volatile uint8_t new_frame_flag;

extern int8_t   pwm_data[4];       // [X, Y, servo1, servo2]

// 模式标志位
extern volatile uint8_t model_flag_1;
extern volatile uint8_t model_flag_2;
extern volatile uint8_t model_flag_3;
extern volatile uint8_t model_flag_ADC;
extern volatile uint8_t model_flag_PHOTO;
extern volatile uint8_t model_flag_PHOTOData;
extern volatile uint8_t model_flag_RETURN;

// ===================== 函数声明 =====================
void Bluetooth_Init(void);
void USART1_SendByte(uint8_t dat);
void USART1_SendArray(uint8_t *array, uint32_t len);
void process_received_data(void);
void joystick_to_motor_control(uint8_t *output);
void Send_Image(uint8_t *img_buf, uint16_t width, uint16_t height);

#endif
