#ifndef INC_TEMPERATURE_SENSOR_H_
#define INC_TEMPERATURE_SENSOR_H_

#include <stdint.h>


typedef enum
{
    TEMP_SENSOR_OK = 0,
    TEMP_SENSOR_ERROR

} TempSensorStatus_t;


/*
 * Check whether the temperature sensor
 * is responding on the I2C bus.
 */
TempSensorStatus_t TempSensor_Init(void);


/*
 * Read the raw 16-bit temperature register.
 */
TempSensorStatus_t TempSensor_ReadRaw(int16_t *raw);


/*
 * Read temperature in Celsius x10.
 *
 * Example:
 * 25.5 C -> 255
 * 30.0 C -> 300
 * -5.0 C -> -50
 */
TempSensorStatus_t TempSensor_ReadCelsiusX10(int16_t *temperature_x10);


#endif /* INC_TEMPERATURE_SENSOR_H_ */
