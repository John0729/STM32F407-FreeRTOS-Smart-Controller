#ifndef INC_PWM_H_
#define INC_PWM_H_

#include <stdint.h>

void PWM_Init(void);

void PWM_SetDuty(uint8_t duty);

uint8_t PWM_GetDuty(void);

#endif
