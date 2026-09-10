#include "protocol.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>


/*
 * Remove:
 *
 * leading spaces
 * trailing spaces
 * \r
 * \n
 *
 * Then convert command to uppercase.
 *
 * Therefore:
 *
 * ping
 * Ping
 * PING
 *
 * are treated the same.
 */
static void Protocol_Normalize(
    char *text
)
{
    char *start;

    size_t length;

    size_t i;


    if (text == NULL)
    {
        return;
    }


    /*
     * Skip leading spaces.
     */
    start = text;

    while ((*start != '\0') && isspace((unsigned char)*start))
    {
        start++;
    }


    /*
     * Move string to beginning.
     */
    if (start != text)
    {
        memmove(
            text,
            start,
            strlen(start) + 1U
        );
    }


    /*
     * Remove trailing spaces,
     * CR and LF.
     */
    length = strlen(text);

    while ((length > 0U) && isspace((unsigned char)text[length - 1U]
           ))
    {
        text[length - 1U] = '\0';

        length--;
    }


    /*
     * Convert to uppercase.
     */
    for (i = 0U; i < length; i++)
    {
        text[i] = (char)toupper((unsigned char)text[i]);
    }
}


/*
 * Parse:
 *
 * PWM 50
 *
 * Returns:
 *
 * value = 50
 */
static ProtocolStatus_t Protocol_ParsePwm(
    char *input,
    ProtocolCommand_t *output
)
{
    char *parameter;

    char *end_ptr;

    unsigned long value;


    parameter = input + 3;


    /* Parse commands such as "PWM 50" */
    if ((*parameter != '\0') && !isspace((unsigned char)*parameter))
    {
        return PROTOCOL_INVALID_COMMAND;
    }


    /*
     * Skip spaces.
     */
    while ((*parameter != '\0') && isspace((unsigned char)*parameter))
    {
        parameter++;
    }


    /*
     * No value.
     */
    if (*parameter == '\0')
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    /*
     * Negative PWM is invalid.
     */
    if (*parameter == '-')
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    value =
        strtoul(
            parameter,
            &end_ptr,
            10
        );


    /*
     * Nothing was converted.
     */
    if (end_ptr == parameter)
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    /*
     * Ignore trailing spaces.
     */
    while ((*end_ptr != '\0') && isspace((unsigned char)*end_ptr))
    {
        end_ptr++;
    }


    /*
     * Unexpected characters.
     *
     * Example:
     *
     * PWM 50ABC
     */
    if (*end_ptr != '\0')
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    /*
     * Protocol data field is uint16_t.
     */
    if (value > 65535UL)
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    output->command =
        PROTOCOL_CMD_SET_PWM;

    output->data =
        (uint16_t)value;


    return PROTOCOL_OK;
}


ProtocolStatus_t Protocol_ParseAscii(char *input, ProtocolCommand_t *output)
{
    if ((input == NULL) || (output == NULL))
    {
        return PROTOCOL_INVALID_PARAMETER;
    }


    output->command = PROTOCOL_CMD_NONE;

    output->data = 0U;


    /*
     * Clean and normalize command.
     */
    Protocol_Normalize(input);


    /*
     * Empty line.
     */
    if (input[0] == '\0')
    {
        return PROTOCOL_EMPTY;
    }


    /*
     * PING
     */
    if (strcmp(input, "PING") == 0)
    {
        output->command = PROTOCOL_CMD_PING;

        return PROTOCOL_OK;
    }


    /*
     * START
     */
    if (strcmp(input, "START") == 0)
    {
        output->command = PROTOCOL_CMD_START;

        return PROTOCOL_OK;
    }


    /*
     * STOP
     */
    if (strcmp(input, "STOP") == 0)
    {
        output->command = PROTOCOL_CMD_STOP;

        return PROTOCOL_OK;
    }


    /*
     * MANUAL
     */
    if (strcmp(input, "MANUAL") == 0)
    {
        output->command = PROTOCOL_CMD_MANUAL_MODE;

        return PROTOCOL_OK;
    }


    /*
     * AUTO
     */
    if (strcmp(input, "AUTO") == 0)
    {
        output->command = PROTOCOL_CMD_AUTO_MODE;

        return PROTOCOL_OK;
    }


    /*
     * STATUS
     */
    if (strcmp(input, "STATUS") == 0)
    {
        output->command = PROTOCOL_CMD_GET_STATUS;

        return PROTOCOL_OK;
    }


    /*
     * TEMP
     */
    if (strcmp(input, "TEMP") == 0)
    {
        output->command = PROTOCOL_CMD_READ_SENSOR;

        return PROTOCOL_OK;
    }


    /*
     * ADC
     */
    if (strcmp(input, "ADC") == 0)
    {
        output->command = PROTOCOL_CMD_READ_ADC;

        return PROTOCOL_OK;
    }


    /*
     * CLEAR
     */
    if (strcmp(input, "CLEAR") == 0)
    {
        output->command = PROTOCOL_CMD_CLEAR_FAULT;

        return PROTOCOL_OK;
    }


    /*
     * LED ON
     */
    if (strcmp(input, "LED ON") == 0)
    {
        output->command = PROTOCOL_CMD_LED_ON;

        return PROTOCOL_OK;
    }


    /*
     * LED OFF
     */
    if (strcmp(input, "LED OFF") == 0)
    {
        output->command = PROTOCOL_CMD_LED_OFF;

        return PROTOCOL_OK;
    }


    /*
     * LED TOGGLE
     */
    if (strcmp(input, "LED TOGGLE") == 0)
    {
        output->command = PROTOCOL_CMD_LED_TOGGLE;

        return PROTOCOL_OK;
    }


    /*
     * PWM xxx
     */
    if (strncmp(input, "PWM", 3U) == 0)
    {
        return Protocol_ParsePwm(input, output);
    }


    /*
     * Unknown command.
     */
    return PROTOCOL_INVALID_COMMAND;
}
