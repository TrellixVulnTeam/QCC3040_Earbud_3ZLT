/*****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_log.c

DESCRIPTION
    Logs AMA commands and responses
*/

#ifdef INCLUDE_AMA
#define DEBUG_LOG_MODULE_NAME ama_log
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR
#include <stdio.h>
#include "ama_log.h"

#define AMALOG_PREFIX  "AMA%cx "

/*
 * The %s printf format specifier doesn't work with DEBUG_LOG_VERBOSE when the string is in RAM,
 * so instead use vprintf instead.
 */
void AmaLog_LogVaArg(const char* fmt, ...)
{
    
    va_list va_parameters;
    va_start(va_parameters, fmt);
    vprintf(fmt, va_parameters);
    va_end(va_parameters);
}

static void amaLog_Array(bool sending, Command command, uint8 *start, size_t length)
{
    if (debug_log_level_ama_log >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        if (start && length)
        {
            size_t index;
            AmaLog_LogVaArg(AMALOG_PREFIX "%d [%lu]:", sending ? 'T' : 'R', command, length);
            for (index = 0; index < length; index++)
            {
                AmaLog_LogVaArg(" %02X", start[index]);
            }
            AmaLog_LogVaArg("\n");
        }
    }
}


void AmaLog_ControlEnvelope(bool sending,
                                     ControlEnvelope* control_envelope,
                                     uint8 *packed_envelope,
                                     size_t envelope_size)
{
    DEBUG_LOG_VERBOSE(AMALOG_PREFIX "Command enum:_Command:%d, payload_case enum:ControlEnvelope__PayloadCase:%d",
        sending ? 'T' : 'R',
        control_envelope->command, control_envelope->payload_case);

    if (control_envelope->payload_case == CONTROL_ENVELOPE__PAYLOAD_RESPONSE)
    {
        Response* response = control_envelope->u.response;
        DEBUG_LOG_VERBOSE(AMALOG_PREFIX "Response error_code enum:ErrorCode:%d, payload_case enum:Response__PayloadCase:%d",
            sending ? 'T' : 'R', response->error_code, response->payload_case);
    }

    if (packed_envelope && envelope_size)
    {
        amaLog_Array(sending, control_envelope->command, packed_envelope, envelope_size);
    }
}

#endif /* INCLUDE_AMA */

