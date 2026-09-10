#include "analog_input.h"
#include "main.h"

#define ADC_DMA_BUFFER_SIZE 32U

extern ADC_HandleTypeDef hadc1;

static uint16_t adc_dma_buffer[ADC_DMA_BUFFER_SIZE];

static volatile uint8_t adc_dma_ready = 0;

static uint16_t adc_dma_average = 0;


void AnalogInput_Init(void)
{

    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)adc_dma_buffer,
        ADC_DMA_BUFFER_SIZE
    );
}


void AnalogInput_Process(void)
{
    uint32_t sum = 0;

    if (adc_dma_ready == 0)
    {
        return;
    }

    adc_dma_ready = 0;

    for (uint32_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++)
    {
        sum += adc_dma_buffer[i];
    }

    adc_dma_average = (uint16_t)(sum / ADC_DMA_BUFFER_SIZE);
}


uint16_t AnalogInput_GetRaw(void)
{
    return adc_dma_average;
}


uint16_t AnalogInput_GetMillivolts(void)
{
    return (uint16_t)(((uint32_t)adc_dma_average * 3300U)/ 4095U);
}


void HAL_ADC_ConvCpltCallback(
    ADC_HandleTypeDef *hadc
)
{
    if (hadc->Instance == ADC1)
    {
        adc_dma_ready = 1;
    }
}
