#ifndef __ADC_SENSOR_H
#define __ADC_SENSOR_H

#include "stm32f10x.h"

void     ADC_Sensor_Init(void);
uint16_t ADC_Sensor_Read(void);

#endif
