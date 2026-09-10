#include "system_state.h"


static SystemState_t current_state = SYSTEM_STATE_INIT;


void SystemState_Init(void)
{
    current_state = SYSTEM_STATE_INIT;
}


SystemState_t SystemState_Get(void)
{
    return current_state;
}


uint8_t SystemState_Set(SystemState_t new_state)
{
    /*
     * Staying in the same state
     * is always allowed.
     */
    if (new_state == current_state)
    {
        return 1U;
    }


    switch (current_state)
    {
        /*
         * INIT can only finish in
         * IDLE or FAULT.
         */
        case SYSTEM_STATE_INIT:

            if ((new_state == SYSTEM_STATE_IDLE) ||
                (new_state == SYSTEM_STATE_FAULT))
            {
                current_state = new_state;

                return 1U;
            }

            break;


        /*
         * IDLE can enter either
         * operating mode or FAULT.
         */
        case SYSTEM_STATE_IDLE:

            if ((new_state == SYSTEM_STATE_MANUAL) ||
                (new_state == SYSTEM_STATE_AUTO) ||
                (new_state == SYSTEM_STATE_FAULT))
            {
                current_state = new_state;

                return 1U;
            }

            break;


        /*
         * MANUAL can stop, switch
         * to AUTO, or enter FAULT.
         */
        case SYSTEM_STATE_MANUAL:

            if ((new_state == SYSTEM_STATE_IDLE) ||
                (new_state == SYSTEM_STATE_AUTO) ||
                (new_state == SYSTEM_STATE_FAULT))
            {
                current_state = new_state;

                return 1U;
            }

            break;


        /*
         * AUTO can stop, switch
         * to MANUAL, or enter FAULT.
         */
        case SYSTEM_STATE_AUTO:

            if ((new_state == SYSTEM_STATE_IDLE) ||
                (new_state == SYSTEM_STATE_MANUAL) ||
                (new_state == SYSTEM_STATE_FAULT))
            {
                current_state = new_state;

                return 1U;
            }

            break;


        /*
         * FAULT requires explicit
         * recovery before returning
         * to IDLE.
         */
        case SYSTEM_STATE_FAULT:

            if (new_state == SYSTEM_STATE_IDLE)
            {
                current_state = new_state;

                return 1U;
            }

            break;


        default:

            break;
    }


    return 0U;
}
