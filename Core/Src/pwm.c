#include "pwm.h"
#include "main.h"

extern TIM_HandleTypeDef htim2;

static uint8_t current_duty = 0;


void PWM_Init(void)
{
    HAL_TIM_PWM_Start(
        &htim2,
        TIM_CHANNEL_2
    );

    PWM_SetDuty(0);
}


void PWM_SetDuty(uint8_t duty)
{
    uint32_t compare;

    if (duty > 100)
    {
        duty = 100;
    }

    compare = ((htim2.Init.Period + 1U) * duty) / 100U;

    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_2,
        compare
    );

    current_duty = duty;
}


uint8_t PWM_GetDuty(void)
{
    return current_duty;
}
