/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of responses to AT commands
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_data.h"

#include <vmtypes.h>
#include <sink.h>
#include <panic.h>
#include <byte_utils.h>
#include <logging.h>

#ifdef INCLUDE_DEVICE_TEST_SERVICE

#define CONST_STRING(_s) (_s),(sizeof(_s)-1)

static const uint8 device_test_service_fixed_response_OK[] = "\r\nOK\r\n";
static const uint8 device_test_service_fixed_response_ERROR[] = "\r\nERROR\r\n";


static void deviceTestService_Respond(Task task, const uint8 *response, unsigned response_length)
{
    device_test_service_data_t *dts = DeviceTestServiceGetData();

    UNUSED(task);

    if (!dts->active || !dts->rfc_sink)
    {
        return;
    }

    SinkClaim(dts->rfc_sink, response_length);
    uint8 *base = SinkMap(dts->rfc_sink);
    memcpy(base, response, response_length);
    SinkFlush(dts->rfc_sink, response_length);

}

void DeviceTestService_CommandResponsePartial(Task task,
                                              const char *response, unsigned length,
                                              bool first_part, bool last_part)
{
    uint8 extended_buffer[DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN+4] = "\r\n";
    uint16 target_string_length = length ? MIN(length, DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN) : DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN;
    uint16 string_length;
    uint8 *write_at = first_part ? &extended_buffer[2] : extended_buffer;

    string_length = ByteUtilsStrnlen_S((const uint8 *) response, target_string_length);
    PanicZero(string_length);

    DEBUG_LOG_DEBUG("DeviceTestService_CommandResponsePartial(len:%d,first:%d,last:%d): Starts %c%c",
                    length, first_part, last_part,
                    response[0],response[1]);

    memcpy(write_at, response, string_length);
    write_at += string_length;

    if (last_part)
    {
        *write_at++ = '\r';
        *write_at++ = '\n';
    }

    deviceTestService_Respond(task, extended_buffer, write_at-extended_buffer);
}

void DeviceTestService_CommandResponse(Task task, const char *response, unsigned length)
{
    DeviceTestService_CommandResponsePartial(task, response, length, TRUE, TRUE);
}


void DeviceTestService_CommandResponseOk(Task task)
{
    DEBUG_LOG_FN_ENTRY("DeviceTestService_CommandResponseOK");

    deviceTestService_Respond(task, CONST_STRING(device_test_service_fixed_response_OK));
}

void DeviceTestService_CommandResponseError(Task task)
{
    /* Include command errors in test log */
    DEBUG_LOG_ALWAYS("DeviceTestService_CommandResponseERROR");

    deviceTestService_Respond(task, CONST_STRING(device_test_service_fixed_response_ERROR));
}

void DeviceTestService_CommandResponseOkOrError(Task task, bool success)
{
    if (success)
    {
        DeviceTestService_CommandResponseOk(task);
    }
    else
    {
        DeviceTestService_CommandResponseError(task);
    }
}

#endif /* INCLUDE_DEVICE_TEST_SERVICE */
/*! @} End of group documentation */

