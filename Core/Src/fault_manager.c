#include "fault_manager.h"


static volatile uint16_t fault_flags = FAULT_NONE;


void FaultManager_Init(void)
{
    fault_flags = FAULT_NONE;
}


void FaultManager_Set(uint16_t fault)
{
    fault_flags |= fault;
}


void FaultManager_Clear(uint16_t fault)
{
    fault_flags &= (uint16_t)(~fault);
}


void FaultManager_ClearAll(void)
{
    fault_flags = FAULT_NONE;
}


uint16_t FaultManager_Get(void)
{
    return fault_flags;
}


uint8_t FaultManager_HasCritical(void)
{
    if ((fault_flags & FAULT_CRITICAL_MASK) != 0U)
    {
        return 1U;
    }

    return 0U;
}
