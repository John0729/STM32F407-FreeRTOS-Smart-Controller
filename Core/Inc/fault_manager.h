#ifndef INC_FAULT_MANAGER_H_
#define INC_FAULT_MANAGER_H_

#include <stdint.h>


#define FAULT_NONE                 0x0000U

#define FAULT_TEMP_SENSOR          (1U << 0)

#define FAULT_INVALID_COMMAND      (1U << 1)

#define FAULT_INVALID_PARAMETER    (1U << 2)



/*
 * Faults that force the system
 * into FAULT state.
 */
#define FAULT_CRITICAL_MASK FAULT_TEMP_SENSOR


void FaultManager_Init(void);

void FaultManager_Set(uint16_t fault);

void FaultManager_Clear(uint16_t fault);

void FaultManager_ClearAll(void);

uint16_t FaultManager_Get(void);

uint8_t FaultManager_HasCritical(void);


#endif /* INC_FAULT_MANAGER_H_ */
