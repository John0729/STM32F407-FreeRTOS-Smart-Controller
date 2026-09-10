#ifndef INC_ANALOG_INPUT_H_
#define INC_ANALOG_INPUT_H_

#include <stdint.h>

void AnalogInput_Init(void);

void AnalogInput_Process(void);

uint16_t AnalogInput_GetRaw(void);

uint16_t AnalogInput_GetMillivolts(void);

#endif
