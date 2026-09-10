#ifndef INC_SYSTEM_STATE_H_
#define INC_SYSTEM_STATE_H_

#include <stdint.h>


typedef enum
{
    SYSTEM_STATE_INIT = 0,

    SYSTEM_STATE_IDLE,

    SYSTEM_STATE_MANUAL,

    SYSTEM_STATE_AUTO,

    SYSTEM_STATE_FAULT

} SystemState_t;


void SystemState_Init(void);

uint8_t SystemState_Set(SystemState_t new_state);

SystemState_t SystemState_Get(void);


#endif /* INC_SYSTEM_STATE_H_ */
