#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * UART DMA receive buffer size.
 *
 * Actual commands are much shorter than 64 bytes,
 * but this gives enough space for future expansion.
 */
#define UART_RX_BUFFER_SIZE    64U

/*
 * UART text response buffer size.
 */
#define UART_TX_BUFFER_SIZE    128U


/*
 * Result of ASCII command parsing.
 */
typedef enum
{
    PROTOCOL_OK = 0,

    PROTOCOL_EMPTY,

    PROTOCOL_INVALID_COMMAND,

    PROTOCOL_INVALID_PARAMETER

} ProtocolStatus_t;


/*
 * Supported ASCII commands.
 */
typedef enum
{
    PROTOCOL_CMD_NONE = 0,

    PROTOCOL_CMD_PING,

    PROTOCOL_CMD_START,

    PROTOCOL_CMD_STOP,

    PROTOCOL_CMD_SET_PWM,

    PROTOCOL_CMD_GET_STATUS,

    PROTOCOL_CMD_AUTO_MODE,

    PROTOCOL_CMD_MANUAL_MODE,

    PROTOCOL_CMD_READ_SENSOR,

    PROTOCOL_CMD_CLEAR_FAULT,

    PROTOCOL_CMD_LED_ON,

    PROTOCOL_CMD_LED_OFF,

    PROTOCOL_CMD_LED_TOGGLE,

    PROTOCOL_CMD_READ_ADC

} ProtocolCommandId_t;


/*
 * Parsed command.
 *
 * Example:
 *
 * "PWM 50"
 *
 * command = PROTOCOL_CMD_SET_PWM
 * data    = 50
 */
typedef struct
{
    ProtocolCommandId_t command;

    uint16_t data;

} ProtocolCommand_t;


/*
 * Parse one ASCII command.
 *
 * Examples:
 *
 * PING
 * START
 * STOP
 * MANUAL
 * AUTO
 * PWM 50
 * STATUS
 * TEMP
 * ADC
 * CLEAR
 * LED ON
 * LED OFF
 * LED TOGGLE
 */
ProtocolStatus_t Protocol_ParseAscii(
    char *input,
    ProtocolCommand_t *output
);

#endif
