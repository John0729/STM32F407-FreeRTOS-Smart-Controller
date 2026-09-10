/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "analog_input.h"
#include "pwm.h"
#include "temperature_sensor.h"
#include "oled.h"
#include "system_state.h"
#include "fault_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
    char text[UART_TX_BUFFER_SIZE];

} UartResponse_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SENSOR_UPDATE_PERIOD_MS     500U

#define DISPLAY_UPDATE_PERIOD_MS    200U

#define TEMP_SENSOR_FAIL_LIMIT      3U

#define CONTROL_UPDATE_PERIOD_MS    20U

#define UART_RX_EVENT_FLAG    (1U << 0)
#define UART_TX_EVENT_FLAG    (1U << 1)
#define COMMAND_QUEUE_DEPTH    8U
#define RESPONSE_QUEUE_DEPTH   8U

#define SENSOR_RECOVERY_REQUEST_FLAG    (1U << 0)

#define CONTROL_RECOVERY_OK_FLAG        (1U << 0)
#define CONTROL_RECOVERY_FAIL_FLAG      (1U << 1)

#define CONTROL_FAULT_EVENT_FLAG      (1U << 2)

/* AUTO mode: 25 C = 0%, 40 C = 100% PWM */
#define AUTO_TEMP_MIN_X10          250

#define AUTO_TEMP_MAX_X10          400


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* Definitions for backgroundTask */
osThreadId_t backgroundTaskHandle;
const osThreadAttr_t backgroundTask_attributes = {
  .name = "backgroundTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for controlTask */
osThreadId_t controlTaskHandle;
const osThreadAttr_t controlTask_attributes = {
  .name = "controlTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for uartTask */
osThreadId_t uartTaskHandle;
const osThreadAttr_t uartTask_attributes = {
  .name = "uartTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

volatile uint8_t button_event = 0;

volatile uint16_t uart_rx_length = 0;


/* Data received by UART DMA */
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE + 1U];


/* Stores one complete UART command */
char uart_line_buffer[UART_RX_BUFFER_SIZE + 1U];

static uint16_t uart_line_length = 0U;


/* UART response */
char uart_tx_buffer[UART_TX_BUFFER_SIZE];


/* Result after ASCII parsing. */
ProtocolCommand_t received_command;

/*
 * Latest valid temperature.
 *
 * Unit:
 * Celsius x10
 *
 * 25.5 C = 255
 */
static int16_t latest_temperature_x10 = 0;

/*
 * Software timing.
 */
static uint8_t temp_sensor_fail_count = 0;

TempSensorStatus_t debug_temp_init;
TempSensorStatus_t debug_temp_read;

static osMessageQueueId_t commandQueueHandle;
static osMessageQueueId_t responseQueueHandle;

static osEventFlagsId_t faultEventHandle;

static uint8_t recovery_pending = 0U;

volatile uint32_t stack_free_background = 0U;
volatile uint32_t stack_free_sensor     = 0U;
volatile uint32_t stack_free_display    = 0U;
volatile uint32_t stack_free_control    = 0U;
volatile uint32_t stack_free_uart       = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
void StartBackgroundTask(void *argument);
void StartSensorTask(void *argument);
void StartDisplayTask(void *argument);
void StartControlTask(void *argument);
void StartUartTask(void *argument);

/* USER CODE BEGIN PFP */

/* =========================================================
 * Common / Utility
 * ========================================================= */
static const char *App_StateToString(
    SystemState_t state
);


/* =========================================================
 * UART
 * ========================================================= */
static void App_SendText(const char *text);

static void App_StartUartReceive(void);

static void App_ProcessUartRxData(void);

static void App_ProcessUartCommand(void);

static void App_HandleCommand(void);

static void App_SendStatus(void);

static void App_QueueResponse(const UartResponse_t *response);


/* =========================================================
 * Sensor
 * ========================================================= */
static void App_ProcessSensor(void);

static uint8_t App_RecoverTemperatureSensor(void);


/* =========================================================
 * Fault Management
 * ========================================================= */
static void App_ReportFault(uint16_t fault);

static void App_ProcessPendingFaults(void);

static void App_CheckCriticalFault(void);


/* =========================================================
 * Control
 * ========================================================= */
static void App_HandleControlCommand(const ProtocolCommand_t *command);

static void App_ProcessControl(void);


/* =========================================================
 * Display
 * ========================================================= */
static void App_ProcessDisplay(void);


/* =========================================================
 * Background
 * ========================================================= */
static void App_ProcessBackground(void);

static void App_UpdateStackUsage(void);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static const char *App_StateToString(
    SystemState_t state
)
{
    switch (state)
    {
        case SYSTEM_STATE_INIT:
            return "INIT";

        case SYSTEM_STATE_IDLE:
            return "IDLE";

        case SYSTEM_STATE_MANUAL:
            return "MANUAL";

        case SYSTEM_STATE_AUTO:
            return "AUTO";

        case SYSTEM_STATE_FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

static void App_SendText(const char *text)
{
    HAL_UART_Transmit(
        &huart2,

        (uint8_t *)text,

        (uint16_t)strlen(text),

        100
    );
}

static void App_StartUartReceive(void)
{
    uart_rx_length = 0U;


    /*
     * Clear old contents.
     */
    memset(
        uart_rx_buffer,
        0,
        sizeof(uart_rx_buffer)
    );


    /* Start UART DMA receive with IDLE detection */
    HAL_UARTEx_ReceiveToIdle_DMA(
        &huart2,

        uart_rx_buffer,

        UART_RX_BUFFER_SIZE
    );


    /* Half-transfer event is not needed */
    if (huart2.hdmarx != NULL)
    {
        __HAL_DMA_DISABLE_IT(
            huart2.hdmarx,
            DMA_IT_HT
        );
    }
}

static void App_ProcessUartRxData(void)
{
    uint16_t received_length;
    uint16_t i;

    received_length = uart_rx_length;

    if (received_length > UART_RX_BUFFER_SIZE)
    {
        received_length = UART_RX_BUFFER_SIZE;
    }

    for (i = 0U; i < received_length; i++)
    {
        char ch =
            (char)uart_rx_buffer[i];

        /*
         * CR / LF means that one
         * complete command has arrived.
         */
        if ((ch == '\r') || (ch == '\n'))
        {
            if (uart_line_length > 0U)
            {
                App_ProcessUartCommand();
            }
        }
        else
        {
            /*
             * Append received character.
             */
            if (uart_line_length < UART_RX_BUFFER_SIZE)
            {
                uart_line_buffer[uart_line_length] = ch;

                uart_line_length++;
            }
            else
            {
                /*
                 * Command too long.
                 */
                uart_line_length = 0U;

                App_SendText("ERR COMMAND TOO LONG\r\n");
            }
        }
    }

    /*
     * Ready to receive the next
     * UART DMA chunk.
     */
    App_StartUartReceive();
}

static void App_ProcessUartCommand(void)
{
    ProtocolStatus_t status;

    uart_line_buffer[uart_line_length] = '\0';


    status =
        Protocol_ParseAscii(
            uart_line_buffer,
            &received_command
        );


    if (status == PROTOCOL_OK)
    {
        switch (received_command.command)
        {
            case PROTOCOL_CMD_START:
            case PROTOCOL_CMD_STOP:
            case PROTOCOL_CMD_SET_PWM:
            case PROTOCOL_CMD_AUTO_MODE:
            case PROTOCOL_CMD_MANUAL_MODE:
            case PROTOCOL_CMD_CLEAR_FAULT:
            {
                if (osMessageQueuePut(commandQueueHandle, &received_command, 0U, 0U) != osOK)
                {
                    App_SendText("ERR COMMAND QUEUE FULL\r\n");
                }

                break;
            }

            default:
            {
                App_HandleCommand();
                break;
            }
        }
    }
    else if (status == PROTOCOL_INVALID_COMMAND)
    {
        App_ReportFault(FAULT_INVALID_COMMAND);

        App_SendText("ERR UNKNOWN COMMAND\r\n");
    }
    else if (status == PROTOCOL_INVALID_PARAMETER)
    {
    	App_ReportFault(FAULT_INVALID_PARAMETER);

        App_SendText("ERR INVALID PARAMETER\r\n");
    }


    /* Ready for next command */
    uart_line_length = 0U;

    uart_line_buffer[0] = '\0';
}

/*
 * UART command dispatcher.
 */
static void App_HandleCommand(void)
{
    switch (received_command.command)
    {
        case PROTOCOL_CMD_PING:
        {
            App_SendText("PONG\r\n");

            break;
        }

        case PROTOCOL_CMD_GET_STATUS:
        {
            App_SendStatus();

            break;
        }

        case PROTOCOL_CMD_READ_SENSOR:
        {
            int32_t temperature;

            int32_t positive_temperature;


            if ((FaultManager_Get() & FAULT_TEMP_SENSOR) != 0U)
            {
                App_SendText("ERR TEMP SENSOR\r\n");

                break;
            }


            temperature = latest_temperature_x10;


            if (temperature < 0)
            {
                positive_temperature =- temperature;

                snprintf(
                    uart_tx_buffer,
                    sizeof(uart_tx_buffer),

                    "TEMP=-%ld.%ldC\r\n",

                    (long)
                        (positive_temperature / 10),

                    (long)
                        (positive_temperature % 10)
                );
            }
            else
            {
                snprintf(
                    uart_tx_buffer,
                    sizeof(uart_tx_buffer),

                    "TEMP=%ld.%ldC\r\n",

                    (long)
                        (temperature / 10),

                    (long)
                        (temperature % 10)
                );
            }


            App_SendText(uart_tx_buffer);

            break;
        }

        case PROTOCOL_CMD_LED_ON:
        {
            HAL_GPIO_WritePin(
                GPIOC,
                GPIO_PIN_14,
                GPIO_PIN_SET
            );

            App_SendText("OK LED=ON\r\n");

            break;
        }

        case PROTOCOL_CMD_LED_OFF:
        {
            HAL_GPIO_WritePin(
                GPIOC,
                GPIO_PIN_14,
                GPIO_PIN_RESET
            );

            App_SendText(
                "OK LED=OFF\r\n"
            );

            break;
        }

        case PROTOCOL_CMD_LED_TOGGLE:
        {
            HAL_GPIO_TogglePin(
                GPIOC,
                GPIO_PIN_14
            );

            App_SendText(
                "OK LED=TOGGLE\r\n"
            );

            break;
        }

        case PROTOCOL_CMD_READ_ADC:
        {
            snprintf(
                uart_tx_buffer,
                sizeof(uart_tx_buffer),

                "ADC=%u\r\n",

                (unsigned int)
                    AnalogInput_GetRaw()
            );


            App_SendText(uart_tx_buffer);

            break;
        }


        default:
        {
            App_SendText("ERR UNKNOWN COMMAND\r\n");

            break;
        }
    }
}

static void App_SendStatus(void)
{
    uint16_t fault;
    int32_t temperature;
    int32_t positive_temperature;

    fault = FaultManager_Get();

    temperature = latest_temperature_x10;

    /*
     * Temperature sensor fault
     */
    if ((fault & FAULT_TEMP_SENSOR) != 0U)
    {
        snprintf(
            uart_tx_buffer,
            sizeof(uart_tx_buffer),
            "MODE=%s TEMP=ERR ADC=%u PWM=%u FAULT=0x%04X\r\n",
            App_StateToString(SystemState_Get()),
            (unsigned int)AnalogInput_GetRaw(),
            (unsigned int)PWM_GetDuty(),
            (unsigned int)fault
        );
    }

    /*
     * Negative temperature
     */
    else if (temperature < 0)
    {
        positive_temperature = -temperature;

        snprintf(
            uart_tx_buffer,
            sizeof(uart_tx_buffer),
            "MODE=%s TEMP=-%ld.%ldC ADC=%u PWM=%u FAULT=0x%04X\r\n",
            App_StateToString(SystemState_Get()),
            (long)(positive_temperature / 10),
            (long)(positive_temperature % 10),
            (unsigned int)AnalogInput_GetRaw(),
            (unsigned int)PWM_GetDuty(),
            (unsigned int)fault
        );
    }

    /*
     * Positive temperature
     */
    else
    {
        snprintf(
            uart_tx_buffer,
            sizeof(uart_tx_buffer),
            "MODE=%s TEMP=%ld.%ldC ADC=%u PWM=%u FAULT=0x%04X\r\n",
            App_StateToString(SystemState_Get()),
            (long)(temperature / 10),
            (long)(temperature % 10),
            (unsigned int)AnalogInput_GetRaw(),
            (unsigned int)PWM_GetDuty(),
            (unsigned int)fault
        );
    }

    App_SendText(uart_tx_buffer);
}

static void App_QueueResponse(const UartResponse_t *response)
{
    if (response == NULL)
    {
        return;
    }

    if (osMessageQueuePut(responseQueueHandle, response, 0U, 0U) == osOK)
    {
        /*
         * Wake UART task because
         * a response is ready.
         */
        osThreadFlagsSet(uartTaskHandle, UART_TX_EVENT_FLAG);
    }
}

/*
 * Periodically read I2C
 * temperature sensor.
 */
static void App_ProcessSensor(void)
{
    int16_t temperature;

    if (TempSensor_ReadCelsiusX10(&temperature) == TEMP_SENSOR_OK)
    {
        latest_temperature_x10 = temperature;

        /* Sensor is working normally */
        temp_sensor_fail_count = 0U;
    }
    else
    {
        if (temp_sensor_fail_count < TEMP_SENSOR_FAIL_LIMIT)
        {
            temp_sensor_fail_count++;

            if (temp_sensor_fail_count == TEMP_SENSOR_FAIL_LIMIT)
            {
                App_ReportFault(FAULT_TEMP_SENSOR);
            }
        }
    }
}

static uint8_t App_RecoverTemperatureSensor(void)
{
    int16_t temperature;

    /*
     * Reset STM32 I2C peripheral state.
     *
     * This function is executed only
     * by SensorTask, so I2C ownership
     * is preserved.
     */
    if (HAL_I2C_DeInit(&hi2c1) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        return 0U;
    }

    /*
     * Give the bus / sensor a short
     * settling time.
     */
    osDelay(5U);

    /*
     * Check whether LM75 ACKs.
     */
    if (TempSensor_Init() != TEMP_SENSOR_OK)
    {
        return 0U;
    }

    /*
     * Verify with a real temperature read.
     */
    if (TempSensor_ReadCelsiusX10(&temperature) != TEMP_SENSOR_OK)
    {
        return 0U;
    }

    latest_temperature_x10 = temperature;
    temp_sensor_fail_count = 0U;

    return 1U;
}

static void App_ReportFault(uint16_t fault)
{
    uint32_t result;

    if (fault == FAULT_NONE)
    {
        return;
    }

    /*
     * Store the fault bit(s).
     */
    result = osEventFlagsSet(faultEventHandle, (uint32_t)fault);

    if ((result & osFlagsError) != 0U)
    {
        return;
    }

    /*
     * Wake ControlTask so that
     * the fault can be handled
     * immediately.
     */
    osThreadFlagsSet(controlTaskHandle, CONTROL_FAULT_EVENT_FLAG);
}

static void App_ProcessPendingFaults(void)
{
    uint32_t pending_faults;

    pending_faults = osEventFlagsGet(faultEventHandle);

    if ((pending_faults & osFlagsError) != 0U)
    {
        return;
    }

    if (pending_faults == 0U)
    {
        return;
    }

    /*
     * Remove the pending report
     * from the RTOS event object.
     */
    osEventFlagsClear(faultEventHandle, pending_faults);

    /*
     * Only ControlTask calls this
     * function at runtime.
     */
    FaultManager_Set((uint16_t)pending_faults);
}

/*
 * If a critical hardware fault
 * exists, force the system into
 * FAULT state.
 */
static void App_CheckCriticalFault(void)
{
    if (FaultManager_HasCritical())
    {
        if (SystemState_Get() != SYSTEM_STATE_FAULT)
        {
            SystemState_Set(SYSTEM_STATE_FAULT);
        }

        PWM_SetDuty(0);
    }
}

static void App_HandleControlCommand(const ProtocolCommand_t *command)
{

	UartResponse_t response;

	if (command == NULL)
	{
		return;
	}

	memset(&response, 0, sizeof(response));

    switch (command->command)
    {
		case PROTOCOL_CMD_START:
		{
			if (FaultManager_HasCritical())
			{
				snprintf(
					response.text,
					sizeof(response.text),
					"ERR CRITICAL FAULT\r\n"
				);
			}
			else
			{
				SystemState_Set(SYSTEM_STATE_MANUAL);

				snprintf(
					response.text,
					sizeof(response.text),
					"OK MODE=MANUAL\r\n"
				);
			}

			App_QueueResponse(&response);

			break;
		}

		case PROTOCOL_CMD_STOP:
		{
		    PWM_SetDuty(0);

		    if (SystemState_Get() != SYSTEM_STATE_FAULT)
		    {
		        SystemState_Set(SYSTEM_STATE_IDLE);
		    }

		    snprintf(
		        response.text,
		        sizeof(response.text),
		        "OK MODE=%s PWM=0\r\n",
		        App_StateToString(
		            SystemState_Get()
		        )
		    );

		    App_QueueResponse( &response);

		    break;
		}

		case PROTOCOL_CMD_SET_PWM:
		{
		    uint16_t requested_duty;

		    requested_duty = command->data;

		    /*
		     * Critical fault always
		     * blocks PWM commands.
		     */
		    if (FaultManager_HasCritical())
		    {
		        snprintf(
		            response.text,
		            sizeof(response.text),
		            "ERR CRITICAL FAULT\r\n"
		        );

		        App_QueueResponse(&response);

		        break;
		    }

		    /*
		     * PWM command is only allowed
		     * in MANUAL mode.
		     */
		    if (SystemState_Get() != SYSTEM_STATE_MANUAL)
		    {
		        snprintf(
		            response.text,
		            sizeof(response.text),
		            "ERR MODE NOT MANUAL\r\n"
		        );

		        App_QueueResponse(&response);

		        break;
		    }

		    /*
		     * Validate duty range.
		     */
		    if (requested_duty > 100U)
		    {
		        /*
		         * This function executes in
		         * ControlTask context, so direct
		         * FaultManager_Set is allowed.
		         */
		        FaultManager_Set(FAULT_INVALID_PARAMETER);

		        snprintf(
		            response.text,
		            sizeof(response.text),
		            "ERR PWM RANGE 0-100\r\n"
		        );

		        App_QueueResponse(&response);

		        break;
		    }

		    PWM_SetDuty((uint8_t)requested_duty);

		    snprintf(
		        response.text,
		        sizeof(response.text),
		        "OK PWM=%u\r\n",
		        (unsigned int)PWM_GetDuty()
		    );

		    App_QueueResponse(&response);

		    break;
		}

        case PROTOCOL_CMD_AUTO_MODE:
        {
        	if (FaultManager_HasCritical())
			{
				snprintf(
					response.text,
					sizeof(response.text),
					"ERR CRITICAL FAULT\r\n"
				);
			}
			else
			{
				SystemState_Set(SYSTEM_STATE_AUTO);

				snprintf(
					response.text,
					sizeof(response.text),
					"OK MODE=AUTO\r\n"
				);
			}

			App_QueueResponse(&response);

			break;
        }

        case PROTOCOL_CMD_MANUAL_MODE:
        {
            if (FaultManager_HasCritical())
            {
                snprintf(
                    response.text,
                    sizeof(response.text),
                    "ERR CRITICAL FAULT\r\n"
                );
            }
            else
            {
                SystemState_Set(SYSTEM_STATE_MANUAL);

                snprintf(
                    response.text,
                    sizeof(response.text),
                    "OK MODE=MANUAL\r\n"
                );
            }

            App_QueueResponse(&response);

            break;
        }

        case PROTOCOL_CMD_CLEAR_FAULT:
        {
            if (recovery_pending)
            {
                snprintf(
                    response.text,
                    sizeof(response.text),
                    "ERR RECOVERY BUSY\r\n"
                );

                App_QueueResponse(&response);

                break;
            }

            recovery_pending = 1U;

            if (osThreadFlagsSet(sensorTaskHandle, SENSOR_RECOVERY_REQUEST_FLAG) & osFlagsError)
            {
                recovery_pending = 0U;

                snprintf(
                    response.text,
                    sizeof(response.text),
                    "ERR RECOVERY REQUEST\r\n"
                );

                App_QueueResponse(&response);
            }

            /*
             * No response yet.
             *
             * SensorTask must first
             * perform the recovery.
             */

            break;
        }

        default:
            break;
    }
}

/*
 * System control behavior.
 */
static void App_ProcessControl(void)
{
    SystemState_t state;

    uint8_t duty;

    int32_t temperature;


    state = SystemState_Get();


    switch (state)
    {
        case SYSTEM_STATE_INIT:

            if (PWM_GetDuty() != 0U)
            {
                PWM_SetDuty(0);
            }

            break;


        case SYSTEM_STATE_IDLE:

            if (PWM_GetDuty() != 0U)
            {
                PWM_SetDuty(0);
            }

            break;


		/* MANUAL mode: PWM is controlled by UART command */
        case SYSTEM_STATE_MANUAL:

            break;


        /*
         * AUTO mode:
         *
         * Temperature automatically
         * determines PWM duty.
         */
        case SYSTEM_STATE_AUTO:

            temperature = latest_temperature_x10;


            if (temperature <= AUTO_TEMP_MIN_X10)
            {
                duty = 0U;
            }
            else if (temperature >= AUTO_TEMP_MAX_X10)
            {
                duty = 100U;
            }
            else
            {
                /*
                 * Linear mapping:
                 *
                 * 25 C -> 0%
                 * 40 C -> 100%
                 */
                duty = (uint8_t)(((temperature - AUTO_TEMP_MIN_X10) * 100L) / (AUTO_TEMP_MAX_X10 - AUTO_TEMP_MIN_X10));
            }


            PWM_SetDuty(duty);

            break;


        case SYSTEM_STATE_FAULT:

            if (PWM_GetDuty() != 0U)
            {
                PWM_SetDuty(0);
            }

            break;


        default:

            break;
    }
}

static void App_ProcessDisplay(void)
{
    char text[22];
    SystemState_t state;

    OLED_Clear();

    /* =========================
     * MODE
     * ========================= */
    state = SystemState_Get();

    switch (state)
    {
        case SYSTEM_STATE_INIT:
            OLED_DrawString(0, 0, "MODE: INIT");
            break;

        case SYSTEM_STATE_IDLE:
            OLED_DrawString(0, 0, "MODE: IDLE");
            break;

        case SYSTEM_STATE_MANUAL:
            OLED_DrawString(0, 0, "MODE: MANUAL");
            break;

        case SYSTEM_STATE_AUTO:
            OLED_DrawString(0, 0, "MODE: AUTO");
            break;

        case SYSTEM_STATE_FAULT:
            OLED_DrawString(0, 0, "MODE: FAULT");
            break;

        default:
            OLED_DrawString(0, 0, "MODE: UNKNOWN");
            break;
    }

    /* =========================
     * TEMPERATURE
     * ========================= */
    snprintf(
        text,
        sizeof(text),
        "TEMP: %d.%dC",
        latest_temperature_x10 / 10,
        (latest_temperature_x10 < 0) ? -(latest_temperature_x10 % 10) : (latest_temperature_x10 % 10)
    );

    OLED_DrawString(0, 1, text);

    /* =========================
     * ADC
     * ========================= */
    snprintf(
        text,
        sizeof(text),
        "ADC: %u",
        (unsigned int)AnalogInput_GetRaw()
    );

    OLED_DrawString(0, 2, text);

    /* =========================
     * PWM
     * ========================= */
    snprintf(
        text,
        sizeof(text),
        "PWM: %u%%",
        (unsigned int)PWM_GetDuty()
    );

    OLED_DrawString(0, 3, text);

    /* =========================
     * FAULT
     * ========================= */
    if (FaultManager_Get() == 0U)
    {
        OLED_DrawString(0, 4, "FAULT: NONE");
    }
    else
    {
        OLED_DrawString(0, 4, "FAULT: YES");
    }

    OLED_Update();
}

static void App_ProcessBackground(void)
{
	AnalogInput_Process();

	if (button_event)
	{
	  button_event = 0;

	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_14);
	}
}

static void App_UpdateStackUsage(void)
{
    stack_free_background = osThreadGetStackSpace(backgroundTaskHandle);

    stack_free_sensor = osThreadGetStackSpace(sensorTaskHandle);

    stack_free_display = osThreadGetStackSpace(displayTaskHandle);

    stack_free_control = osThreadGetStackSpace(controlTaskHandle);

    stack_free_uart = osThreadGetStackSpace(uartTaskHandle);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  FaultManager_Init();

  SystemState_Init();

  AnalogInput_Init();

  PWM_Init();

  /*
   * SPI OLED
   */

  HAL_Delay(100);

  OLED_Init();

  OLED_Clear();

  OLED_DrawString(0, 0, "SYSTEM START");

  OLED_Update();

  /*
   * I2C temperature sensor
   */
  debug_temp_init = TempSensor_Init();

  if (debug_temp_init == TEMP_SENSOR_OK)
  {
      debug_temp_read = TempSensor_ReadCelsiusX10(&latest_temperature_x10);
  }
  else
  {
      debug_temp_read = TEMP_SENSOR_ERROR;
  }

  if ((debug_temp_init != TEMP_SENSOR_OK) || (debug_temp_read != TEMP_SENSOR_OK))
  {
      FaultManager_Set(FAULT_TEMP_SENSOR);
  }

  /*
   * Decide initial state.
   */
  if (FaultManager_HasCritical())
  {
      SystemState_Set(SYSTEM_STATE_FAULT);
  }
  else
  {
      SystemState_Set(SYSTEM_STATE_IDLE);
  }

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */

  commandQueueHandle = osMessageQueueNew(COMMAND_QUEUE_DEPTH, sizeof(ProtocolCommand_t), NULL);
  if (commandQueueHandle == NULL)
  {
      Error_Handler();
  }

  responseQueueHandle = osMessageQueueNew(RESPONSE_QUEUE_DEPTH, sizeof(UartResponse_t), NULL);
  if (responseQueueHandle == NULL)
  {
      Error_Handler();
  }

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of backgroundTask */
  backgroundTaskHandle = osThreadNew(StartBackgroundTask, NULL, &backgroundTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

  /* creation of controlTask */
  controlTaskHandle = osThreadNew(StartControlTask, NULL, &controlTask_attributes);

  /* creation of uartTask */
  uartTaskHandle = osThreadNew(StartUartTask, NULL, &uartTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  faultEventHandle = osEventFlagsNew(NULL);

  if (faultEventHandle == NULL)
  {
      Error_Handler();
  }
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_11;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC14 */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PE7 PE8 PE9 */
  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_PIN)
{
	if(GPIO_PIN == GPIO_PIN_0)
	{
		button_event = 1;
	}
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART2)
	    {
	        uart_rx_length = Size;

	        osThreadFlagsSet(uartTaskHandle, UART_RX_EVENT_FLAG);
	    }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartBackgroundTask */
/**
  * @brief  Function implementing the backgroundTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartBackgroundTask */
void StartBackgroundTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
	uint32_t last_stack_check;
	uint32_t now;

	last_stack_check = osKernelGetTickCount();

	for (;;)
	{
		App_ProcessBackground();

		now = osKernelGetTickCount();

		if ((now - last_stack_check) >= 1000U)
		{
			App_UpdateStackUsage();

			last_stack_check = now;
		}

		osDelay(1);
	}
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
	uint32_t flags;
  /* Infinite loop */
	for (;;)
	{
		flags = osThreadFlagsWait(
			SENSOR_RECOVERY_REQUEST_FLAG,
			osFlagsWaitAny,
			SENSOR_UPDATE_PERIOD_MS
		);

		/*
		 * Recovery request received.
		 */
		if ((flags & osFlagsError) == 0U)
		{
			if ((flags & SENSOR_RECOVERY_REQUEST_FLAG) != 0U)
			{
				if (App_RecoverTemperatureSensor())
				{
					osThreadFlagsSet(controlTaskHandle, CONTROL_RECOVERY_OK_FLAG);
				}
				else
				{
					osThreadFlagsSet(controlTaskHandle, CONTROL_RECOVERY_FAIL_FLAG);
				}
			}
		}
		/*
		 * Timeout means normal
		 * periodic sensor update.
		 */
		else if (flags == osFlagsErrorTimeout)
		{
			App_ProcessSensor();
		}
	}
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
* @brief Function implementing the displayTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  /* Infinite loop */
  for(;;)
  {
	  App_ProcessDisplay();
	  osDelay(DISPLAY_UPDATE_PERIOD_MS);
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartControlTask */
/**
* @brief Function implementing the controlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */
  /* Infinite loop */
	ProtocolCommand_t command;
	UartResponse_t response;
	uint32_t flags;

	for (;;)
	{
		/*
		 * Apply all reported faults.
		 */
		App_ProcessPendingFaults();

		/*
		 * Safety first.
		 */
		App_CheckCriticalFault();

		/*
		 * Process all pending
		 * control commands.
		 */
		while (osMessageQueueGet(commandQueueHandle, &command, NULL, 0U) == osOK)
		{
			App_HandleControlCommand(&command);

			/*
			 * A new fault may have been
			 * reported while handling
			 * the command.
			 */
			App_ProcessPendingFaults();

			App_CheckCriticalFault();
		}

		/*
		 * Normal periodic control.
		 */
		App_ProcessControl();

		/*
		 * Wait for asynchronous events
		 * or next control period.
		 */
		flags = osThreadFlagsWait(
			CONTROL_RECOVERY_OK_FLAG |
			CONTROL_RECOVERY_FAIL_FLAG |
			CONTROL_FAULT_EVENT_FLAG,
			osFlagsWaitAny,
			CONTROL_UPDATE_PERIOD_MS
		);

		/*
		 * Timeout is normal.
		 */
		if ((flags & osFlagsError) == 0U)
		{
			/*
			 * New fault reported.
			 */
			if ((flags & CONTROL_FAULT_EVENT_FLAG) != 0U)
			{
				App_ProcessPendingFaults();

				App_CheckCriticalFault();
			}

			/*
			 * Sensor recovery succeeded.
			 */
			if ((flags & CONTROL_RECOVERY_OK_FLAG) != 0U)
			{
				FaultManager_ClearAll();

				SystemState_Set(SYSTEM_STATE_IDLE);

				PWM_SetDuty(0);

				recovery_pending = 0U;

				memset(
					&response,
					0,
					sizeof(response)
				);

				snprintf(
					response.text,
					sizeof(response.text),
					"OK FAULT=0x%04X MODE=%s\r\n",
					(unsigned int)FaultManager_Get(),
					App_StateToString(SystemState_Get())
				);

				App_QueueResponse(&response);
			}

			/*
			 * Sensor recovery failed.
			 */
			if ((flags & CONTROL_RECOVERY_FAIL_FLAG) != 0U)
			{
				FaultManager_ClearAll();

				FaultManager_Set(FAULT_TEMP_SENSOR);

				SystemState_Set(SYSTEM_STATE_FAULT);

				PWM_SetDuty(0);

				recovery_pending = 0U;

				memset(
					&response,
					0,
					sizeof(response)
				);

				snprintf(
					response.text,
					sizeof(response.text),
					"ERR FAULT=0x%04X\r\n",
					(unsigned int)FaultManager_Get()
				);

				App_QueueResponse(&response);
			}
		}
	}

  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartUartTask */
/**
* @brief Function implementing the uartTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartTask */
void StartUartTask(void *argument)
{
  /* USER CODE BEGIN StartUartTask */

	uint32_t flags;
	UartResponse_t response;

	/*
	 * UART belongs to this task.
	 * Start the first DMA reception.
	 */
	App_StartUartReceive();

  /* Infinite loop */
	for (;;)
	{
		flags =
			osThreadFlagsWait(
				UART_RX_EVENT_FLAG |
				UART_TX_EVENT_FLAG,
				osFlagsWaitAny,
				osWaitForever
			);

		/*
		 * Check RTOS flag error.
		 */
		if ((flags & osFlagsError) != 0U)
		{
			continue;
		}

		/*
		 * Process RX first.
		 */
		if ((flags & UART_RX_EVENT_FLAG) != 0U)
		{
			App_ProcessUartRxData();
		}

		/*
		 * Send all pending responses.
		 */
		if ((flags & UART_TX_EVENT_FLAG) != 0U)
		{
			while (osMessageQueueGet(
					   responseQueueHandle,
					   &response,
					   NULL,
					   0U
				   ) == osOK)
			{
				App_SendText(response.text);
			}
		}
	}
  /* USER CODE END StartUartTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
