#include "MySPI.h"

// ===================== CS 引脚定义 =====================
// LCD CS = PC13
#define LCD_CS_PORT   GPIOC
#define LCD_CS_PIN    GPIO_Pin_13

// Flash CS = PA4
#define FLASH_CS_PORT  GPIOA
#define FLASH_CS_PIN   GPIO_Pin_4

// ===================== CS 控制 =====================
void MySPI_W_SS(uint8_t BitValue)
{
	GPIO_WriteBit(LCD_CS_PORT, LCD_CS_PIN, (BitAction)BitValue);
}

// Flash CS 控制（内部使用）
static void Flash_CS_L(void) { GPIO_ResetBits(FLASH_CS_PORT, FLASH_CS_PIN); }
static void Flash_CS_H(void) { GPIO_SetBits(FLASH_CS_PORT, FLASH_CS_PIN); }

// ===================== SPI1 初始化（LCD + Flash 共用）=====================
// PA5=SCK, PA6=MISO, PA7=MOSI
// LCD:  PC13=CS, 36MHz (prescaler_2)
// Flash: PA4=CS,  18MHz (prescaler_4)
void MySPI_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	// SCK + MOSI: 复用推挽
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// MISO: 上拉输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// LCD CS (PC13): 推挽输出，默认高电平
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = LCD_CS_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure);
	MySPI_W_SS(1);

	// Flash CS (PA4): 推挽输出，默认高电平
	GPIO_InitStructure.GPIO_Pin = FLASH_CS_PIN;
	GPIO_Init(FLASH_CS_PORT, &GPIO_InitStructure);
	Flash_CS_H();

	// SPI1 默认配置（LCD 速度 36MHz）
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;  // 72/2=36MHz
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);
}

// ===================== 分频器切换 =====================
// 切换到 Flash 速度: 18MHz (72MHz / 4)
void SPI_SetFlashSpeed(void)
{
	SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | SPI_BaudRatePrescaler_4;
}

// 切换到 LCD 速度: 36MHz (72MHz / 2)
void SPI_SetLCDSpeed(void)
{
	SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | SPI_BaudRatePrescaler_2;
}

// ===================== CS 使能/禁止（LCD）=====================
void MySPI_Start(void)
{
	MySPI_W_SS(0);
}

void MySPI_Stop(void)
{
	MySPI_W_SS(1);
}

// ===================== 交换一个字节 =====================
void MySPI_SwapByte(uint8_t ByteSend)
{
	SPI1->DR = ByteSend;
	while (!(SPI1->SR & SPI_I2S_FLAG_RXNE));
}
