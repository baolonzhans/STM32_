#include "stm32f10x.h"
#include "Delay.h"
#include "OV7670.h"

// 引脚 (PB6/PB7)
#define SCCB_SCL_PIN   GPIO_Pin_6
#define SCCB_SDA_PIN   GPIO_Pin_7
#define SCCB_PORT      GPIOB
#define SCCB_SCL_HIGH()   GPIO_SetBits(SCCB_PORT, SCCB_SCL_PIN)
#define SCCB_SCL_LOW()    GPIO_ResetBits(SCCB_PORT, SCCB_SCL_PIN)
#define SCCB_SDA_HIGH()   GPIO_SetBits(SCCB_PORT, SCCB_SDA_PIN)
#define SCCB_SDA_LOW()    GPIO_ResetBits(SCCB_PORT, SCCB_SDA_PIN)
#define SCCB_SDA_READ()   GPIO_ReadInputDataBit(SCCB_PORT, SCCB_SDA_PIN)

#define OV7670_I2C_ADDR 0x42

#define SCCB_SDA_OUT() \
    do { \
        GPIO_InitTypeDef GPIO_InitStructure; \
        GPIO_InitStructure.GPIO_Pin = SCCB_SDA_PIN; \
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; \
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; \
        GPIO_Init(SCCB_PORT, &GPIO_InitStructure); \
    } while(0)

#define SCCB_SDA_IN() \
    do { \
        GPIO_InitTypeDef GPIO_InitStructure; \
        GPIO_InitStructure.GPIO_Pin = SCCB_SDA_PIN; \
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; \
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; \
        GPIO_Init(SCCB_PORT, &GPIO_InitStructure); \
    } while(0)

void SCCB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = SCCB_SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SCCB_PORT, &GPIO_InitStructure);
    SCCB_SCL_HIGH();

    SCCB_SDA_OUT();
    SCCB_SDA_HIGH();
}

void SCCB_Start(void)
{
    SCCB_SDA_OUT();
    SCCB_SDA_HIGH();
    SCCB_SCL_HIGH();
    SCCB_SDA_LOW();
    SCCB_SCL_LOW();
}

void SCCB_Stop(void)
{
    SCCB_SDA_OUT();
    SCCB_SDA_LOW();
    SCCB_SCL_HIGH();
    SCCB_SDA_HIGH();
}

uint8_t SCCB_WriteByte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (data & 0x80) SCCB_SDA_HIGH();
        else             SCCB_SDA_LOW();
        data <<= 1;
        Delay_us(5);
        SCCB_SCL_HIGH();
        Delay_us(5);
        SCCB_SCL_LOW();
        Delay_us(5);
    }

    SCCB_SDA_IN();
    Delay_us(5);
    SCCB_SCL_HIGH();
    Delay_us(5);
    uint8_t ack = SCCB_SDA_READ();
    SCCB_SCL_LOW();
    SCCB_SDA_OUT();

    return ack;
}

uint8_t SCCB_ReadByte(void)
{
    uint8_t i, data = 0;
    SCCB_SDA_IN();

    for (i = 0; i < 8; i++) {
        data <<= 1;
        SCCB_SCL_HIGH();
        Delay_us(5);
        if (SCCB_SDA_READ()) data |= 0x01;
        SCCB_SCL_LOW();
        Delay_us(5);
    }

    SCCB_SDA_OUT();
    SCCB_SDA_HIGH();
    Delay_us(5);
    SCCB_SCL_HIGH();
    Delay_us(5);
    SCCB_SCL_LOW();

    return data;
}

void SCCB_WriteReg(uint8_t reg_addr, uint8_t data)
{
    uint8_t res = 0;
    SCCB_Start();
    res |= SCCB_WriteByte(OV7670_I2C_ADDR);
    res |= SCCB_WriteByte(reg_addr);
    res |= SCCB_WriteByte(data);
    SCCB_Stop();
    if (res) Error_Output();
}

uint8_t SCCB_ReadReg(uint8_t reg_addr)
{
    uint8_t data = 0xFF;
    SCCB_Start();
    if (SCCB_WriteByte(OV7670_I2C_ADDR)) { SCCB_Stop(); return 0xFF; }
    if (SCCB_WriteByte(reg_addr))         { SCCB_Stop(); return 0xFF; }
    SCCB_Stop();

    SCCB_Start();
    if (SCCB_WriteByte(OV7670_I2C_ADDR | 0x01)) { SCCB_Stop(); return 0xFF; }
    data = SCCB_ReadByte();
    SCCB_Stop();

    return data;
}
