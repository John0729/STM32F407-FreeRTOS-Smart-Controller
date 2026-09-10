#include "temperature_sensor.h"
#include "main.h"


#define LM75_I2C_ADDR_7BIT    0x48U
#define LM75_I2C_ADDR         (LM75_I2C_ADDR_7BIT << 1U)

#define LM75_REG_TEMP         0x00U

#define LM75_I2C_TIMEOUT      100U


extern I2C_HandleTypeDef hi2c1;


/*
 * Check whether LM75 responds to its I2C address.
 */
TempSensorStatus_t TempSensor_Init(void)
{
    if (HAL_I2C_IsDeviceReady(
            &hi2c1,
            LM75_I2C_ADDR,
            3,
            LM75_I2C_TIMEOUT
        ) != HAL_OK)
    {
        return TEMP_SENSOR_ERROR;
    }

    return TEMP_SENSOR_OK;
}


/*
 * Read LM75 temperature register.
 *
 * LM75 returns two bytes:
 *
 * data[0] = MSB
 * data[1] = LSB
 *
 * Example:
 *
 * data[0] = 0x19
 * data[1] = 0x80
 *
 * raw = 0x1980
 */
TempSensorStatus_t TempSensor_ReadRaw(
    int16_t *raw
)
{
    uint8_t data[2];

    if (raw == NULL)
    {
        return TEMP_SENSOR_ERROR;
    }

    if (HAL_I2C_Mem_Read(
            &hi2c1,
            LM75_I2C_ADDR,
            LM75_REG_TEMP,
            I2C_MEMADD_SIZE_8BIT,
            data,
            2,
            LM75_I2C_TIMEOUT
        ) != HAL_OK)
    {
        return TEMP_SENSOR_ERROR;
    }

    *raw = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);

    return TEMP_SENSOR_OK;
}


/*
 * Convert LM75 raw register value
 * to Celsius x10.
 *
 * Classic LM75:
 *
 * Temperature resolution = 0.5 C
 *
 * Example:
 *
 * raw register = 0x1980
 *
 * 0x1980 >> 7
 * = 51
 *
 * 51 * 0.5 C
 * = 25.5 C
 *
 * Celsius x10:
 *
 * 25.5 C -> 255
 */
TempSensorStatus_t TempSensor_ReadCelsiusX10(
    int16_t *temperature_x10
)
{
    int16_t raw_register;
    uint16_t raw_9bit;
    int16_t temperature_half_degree;

    if (temperature_x10 == NULL)
    {
        return TEMP_SENSOR_ERROR;
    }

    if (TempSensor_ReadRaw(&raw_register) != TEMP_SENSOR_OK)
    {
        return TEMP_SENSOR_ERROR;
    }

    /*
     * LM75 temperature data occupies
     * bits [15:7].
     */
    raw_9bit = ((uint16_t)raw_register >> 7);

    /*
     * Sign extend the 9-bit
     * two's complement value.
     *
     * bit 8 is the sign bit.
     */
    if ((raw_9bit & 0x0100U) != 0U)
    {
        raw_9bit |= 0xFE00U;
    }

    temperature_half_degree = (int16_t)raw_9bit;

    /*
     * Each count = 0.5 C.
     *
     * We want Celsius x10:
     *
     * 0.5 * 10 = 5
     */
    *temperature_x10 = temperature_half_degree * 5;

    return TEMP_SENSOR_OK;
}
