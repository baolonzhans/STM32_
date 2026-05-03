#include "stm32f10x.h"
#include <string.h>
#include "flash.h"

// ===================== 引脚定义 =====================
#define FLASH_CS_GPIO_PORT      GPIOA
#define FLASH_CS_GPIO_PIN       GPIO_Pin_4
#define FLASH_SCK_GPIO_PORT     GPIOA
#define FLASH_SCK_GPIO_PIN      GPIO_Pin_5
#define FLASH_MISO_GPIO_PORT    GPIOA
#define FLASH_MISO_GPIO_PIN     GPIO_Pin_6
#define FLASH_MOSI_GPIO_PORT    GPIOA
#define FLASH_MOSI_GPIO_PIN     GPIO_Pin_7

#define CMD_READ    0x03
#define CMD_PP      0x02
#define CMD_WREN    0x06
#define CMD_SE      0x20
#define CMD_JEDEC   0x9F
#define CMD_RDSR    0x05

#define CS_L()  GPIO_ResetBits(FLASH_CS_GPIO_PORT, FLASH_CS_GPIO_PIN)
#define CS_H()  GPIO_SetBits(FLASH_CS_GPIO_PORT, FLASH_CS_GPIO_PIN)

#define FLASH_STATE_BASE  0x060000

// 录制阶段记录的拍照次数
uint16_t g_recPhotoCnt = 0;

// ===================== SPI 读写 =====================
static uint8_t SPI_RW(uint8_t d)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, d);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

static void Flash_WaitBusy(void)
{
    CS_L();
    SPI_RW(CMD_RDSR);
    while (SPI_RW(0xFF) & 0x01);
    CS_H();
}

// ===================== 初始化 =====================
void Flash_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = FLASH_SCK_GPIO_PIN | FLASH_MOSI_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLASH_SCK_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = FLASH_MISO_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FLASH_MISO_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = FLASH_CS_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FLASH_CS_GPIO_PORT, &GPIO_InitStructure);
    CS_H();

    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

// ===================== JEDEC ID =====================
uint32_t Flash_ReadJEDEC(void)
{
    uint32_t id;
    SPI_SetFlashSpeed();    // Flash: 18MHz
    CS_L();
    SPI_RW(CMD_JEDEC);
    id  = (uint32_t)SPI_RW(0xFF) << 16;
    id |= (uint32_t)SPI_RW(0xFF) << 8;
    id |= (uint32_t)SPI_RW(0xFF);
    CS_H();
    return id;
}

// ===================== 读数据 =====================
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;
    SPI_SetFlashSpeed();    // Flash: 18MHz
    CS_L();
    SPI_RW(CMD_READ);
    SPI_RW((addr >> 16) & 0xFF);
    SPI_RW((addr >>  8) & 0xFF);
    SPI_RW( addr        & 0xFF);
    for (i = 0; i < len; i++) buf[i] = SPI_RW(0xFF);
    CS_H();
}

// ===================== 页编程 =====================
static void Flash_WritePage(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    if (len > 256) len = 256;
    CS_L(); SPI_RW(CMD_WREN); CS_H();
    CS_L();
    SPI_RW(CMD_PP);
    SPI_RW((addr >> 16) & 0xFF);
    SPI_RW((addr >>  8) & 0xFF);
    SPI_RW( addr        & 0xFF);
    for (i = 0; i < len; i++) SPI_RW(buf[i]);
    CS_H();
    Flash_WaitBusy();
}

// ===================== 任意长度写入 =====================
void Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    SPI_SetFlashSpeed();    // Flash: 18MHz
    while (len > 0) {
        uint16_t rem = 256 - (addr & 0xFF);
        uint16_t w   = (len < rem) ? (uint16_t)len : rem;
        Flash_WritePage(addr, buf, w);
        addr += w; buf += w; len -= w;
    }
}

// ===================== 扇区擦除 =====================
void Flash_EraseSector(uint32_t addr)
{
    SPI_SetFlashSpeed();    // Flash: 18MHz
    CS_L(); SPI_RW(CMD_WREN); CS_H();
    CS_L();
    SPI_RW(CMD_SE);
    SPI_RW((addr >> 16) & 0xFF);
    SPI_RW((addr >>  8) & 0xFF);
    SPI_RW( addr        & 0xFF);
    CS_H();
    Flash_WaitBusy();
}

// ===================== 状态存储 =====================

void State_Save(uint16_t motorRecCnt, uint16_t recPhotoCnt, uint16_t photoCnt, uint16_t loopCnt)
{
    SavedState st;
    st.magic       = STATE_MAGIC;
    st.motorRecCnt = motorRecCnt;
    st.photoCnt    = photoCnt;
    st.recPhotoCnt = recPhotoCnt;
    st.loopCnt     = loopCnt;
    memset(st.reserved, 0, sizeof(st.reserved));

    Flash_EraseSector(FLASH_STATE_BASE);
    Flash_Write(FLASH_STATE_BASE, (uint8_t*)&st, sizeof(st));
}

uint8_t State_Load(SavedState *st)
{
    Flash_Read(FLASH_STATE_BASE, (uint8_t*)st, sizeof(SavedState));
    return (st->magic == STATE_MAGIC);
}

void State_Clear(void)
{
    Flash_EraseSector(FLASH_STATE_BASE);
}

// ===================== 扫描录制数据中的拍照标志 =====================
// 录制数据起始地址 0x000000，每条记录 10 字节，byte[8] & 0x01 为拍照标志
#define REC_FLAG_PHOTO   0x01
#define FLASH_MOTOR_BASE 0x000000
#define REC_FRAME_SIZE   10

void Flash_ScanPhotoFlags(void)
{
    uint16_t count = 0;
    uint32_t addr = FLASH_MOTOR_BASE;
    uint8_t buf[REC_FRAME_SIZE];

    // 最多扫描 FLASH_MOTOR_LIMIT (64KB) 范围
    while (addr < 0x10000) {
        Flash_Read(addr, buf, REC_FRAME_SIZE);
        // 检查是否为有效记录（非全0xFF = 已写入）
        if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF)
            break;
        if (buf[8] & REC_FLAG_PHOTO)
            count++;
        addr += REC_FRAME_SIZE;
    }
    g_recPhotoCnt = count;
}
