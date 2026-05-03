#include "stm32f10x.h"
#include "bluetooth.h"
#include "Delay.h"

// ===================== 全局变量定义 =====================
uint8_t  dma_buf[2][DMA_BUF_SIZE] = {0};
uint8_t  ready_buf = 0;
uint16_t frame_len = 0;
volatile uint8_t new_frame_flag = 0;

int8_t   pwm_data[4] = {0};

volatile uint8_t model_flag_1 = 0;
volatile uint8_t model_flag_2 = 0;
volatile uint8_t model_flag_3 = 0;
volatile uint8_t model_flag_ADC = 0;
volatile uint8_t model_flag_PHOTO = 0;
volatile uint8_t model_flag_PHOTOData = 0;
volatile uint8_t model_flag_RETURN = 0;

static volatile uint8_t rx_buf_idx = 0;     // 当前写入的缓冲区
static volatile uint16_t rx_pos = 0;        // 当前写入位置

// ===================== USART1 + DMA 初始化 =====================
// HC-04 蓝牙: PA9(TX) PA10(RX), 9600, DMA接收 + 空闲中断
void Bluetooth_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // PA9 TX: 复用推挽
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10 RX: 浮空输入
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART1: 9600, 8N1
    USART_InitStructure.USART_BaudRate            = 9600;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    // 使能空闲中断
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    // NVIC
    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // DMA1 Channel5: USART1_RX
    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)dma_buf[0];
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    DMA_Cmd(DMA1_Channel5, ENABLE);

    USART_Cmd(USART1, ENABLE);

    rx_buf_idx = 0;
    rx_pos = 0;
}

// ===================== USART1 空闲中断处理 =====================
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
    {
        // 读 SR 和 DR 清除空闲标志
        volatile uint32_t tmp;
        tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp;

        // 计算本次接收长度
        uint16_t dma_cnt = DMA_GetCurrDataCounter(DMA1_Channel5);
        uint16_t recv_len = DMA_BUF_SIZE - dma_cnt;

        // 先关 USART 接收 + DMA，防止切换期间丢字节
        USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);
        DMA_Cmd(DMA1_Channel5, DISABLE);

        if (recv_len > 0 && recv_len <= DMA_BUF_SIZE && !new_frame_flag)
        {
            // 有效数据：标记就绪，切换到另一个缓冲区
            ready_buf = rx_buf_idx;
            frame_len = recv_len;
            new_frame_flag = 1;

            rx_buf_idx ^= 1;
            DMA1_Channel5->CMAR = (uint32_t)dma_buf[rx_buf_idx];
        }
        // 无有效数据时保持原缓冲区，不切换

        // 重启 DMA + USART 接收
        DMA_SetCurrDataCounter(DMA1_Channel5, DMA_BUF_SIZE);
        DMA_Cmd(DMA1_Channel5, ENABLE);
        USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    }
}

// ===================== 发送一个字节 =====================
void USART1_SendByte(uint8_t dat)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, dat);
}

// ===================== 发送数组 =====================
void USART1_SendArray(uint8_t *array, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++)
    {
        USART1_SendByte(array[i]);
    }
}

// ===================== 解析数字 [num,num,num,num] =====================
static void serial_parse_numbers(uint8_t *buf, uint16_t len, int8_t *data)
{
    uint8_t *p = buf;
    uint8_t count = 0;
    int16_t val;
    uint8_t neg;

    for (uint8_t i = 0; i < 4; i++) data[i] = 0;

    while (p < buf + len && *p != '[') p++;
    if (p >= buf + len) return;
    p++;

    while (p < buf + len && count < 4)
    {
        while (p < buf + len && !((*p >= '0' && *p <= '9') || *p == '-')) p++;
        if (p >= buf + len || *p == ']') break;

        neg = (*p == '-') ? 1 : 0;
        if (neg) p++;
        val = 0;
        while (p < buf + len && *p >= '0' && *p <= '9')
        {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (neg) val = -val;
        data[count++] = (int8_t)val;
    }
}

// ===================== 数据处理 =====================
void process_received_data(void)
{
    if (!new_frame_flag) return;

    new_frame_flag = 0;

    uint8_t *buf = dma_buf[ready_buf];
    uint16_t len = frame_len;

    // model 指令
    if (len >= 6 && buf[0] == 'm' && buf[1] == 'o' && buf[2] == 'd' &&
        buf[3] == 'e' && buf[4] == 'l' && buf[5] == '.')
    {
        if (len > 6)
        {
            char letter = buf[6];
            switch (letter)
            {
                case 'a': case 'A': model_flag_1 = 1; break;
                case 'b': case 'B': model_flag_2 = 1; break;
                case 'c': case 'C': model_flag_3 = 1; break;
                case 'd': case 'D': model_flag_ADC = 1; break;
                case 'e': case 'E': model_flag_PHOTO = 1; break;
                case 'f': case 'F': model_flag_PHOTOData = 1; break;
                case 'h': case 'H': model_flag_RETURN = 1; break;
                default: break;
            }
        }
    }
    else
    {
        // 摇杆数据
        serial_parse_numbers(buf, len, pwm_data);
    }
}

// ===================== 摇杆 -> 电机+舵机 =====================
void joystick_to_motor_control(uint8_t *output)
{
    // 1. 电机控制: pwm_data[0]=X, pwm_data[1]=Y
    int16_t left  = pwm_data[1] + pwm_data[0];
    int16_t right = pwm_data[1] - pwm_data[0];

    if (left > 100)  left = 100;
    if (left < -100) left = -100;
    if (right > 100)  right = 100;
    if (right < -100) right = -100;

    if (left >= 0) {
        output[0] = 0;
        output[1] = (uint8_t)left;
    } else {
        output[0] = 1;
        output[1] = (uint8_t)(-left);
    }

    if (right >= 0) {
        output[2] = 0;
        output[3] = (uint8_t)right;
    } else {
        output[2] = 1;
        output[3] = (uint8_t)(-right);
    }

    // 2. 舵机角度: pwm_data[2]=servo1, pwm_data[3]=servo2
    //    -100~100 -> 0~180°
    int16_t servo1 = pwm_data[2];
    int16_t servo2 = pwm_data[3];

    if (servo1 < -100) servo1 = -100;
    if (servo1 > 100)  servo1 = 100;
    if (servo2 < -100) servo2 = -100;
    if (servo2 > 100)  servo2 = 100;

    output[4] = (uint8_t)((servo1 + 100) * 180 / 200);
    output[5] = (uint8_t)((servo2 + 100) * 180 / 200);
}

// ===================== 发送灰度图像 =====================
#define IMG_PKG   128

void Send_Image(uint8_t *img_buf, uint16_t width, uint16_t height)
{
    uint32_t img_total = (uint32_t)width * height;

    USART1_SendByte(0xAA);
    USART1_SendByte(0xBB);

    uint32_t i;
    for (i = 0; i < img_total; i += IMG_PKG)
    {
        uint16_t len = img_total - i;
        if (len > IMG_PKG) len = IMG_PKG;
        USART1_SendArray(&img_buf[i], len);
        Delay_ms(8);
    }

    USART1_SendByte(0xCC);
    USART1_SendByte(0xDD);
}
