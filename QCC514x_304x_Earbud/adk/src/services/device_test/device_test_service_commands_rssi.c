/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for reading RSSI levels
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_parse.h"

#include <connection_manager.h>
#include <bdaddr.h>
#include <logging.h>
#include <stdio.h>

/*! The base part of the AT command response */
#define RSSI_RESPONSE "+RSSIREAD:"

#define RSSI_RESPONSE_VARIABLE_FMT "%04x%02x%06lx,%d"

/*! Worst case variable length portion */
#define RSSI_RESPONSE_VARIABLE_EXAMPLE "00025B00EB00,-100"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_RSSI_RESPONSE_LEN (sizeof(RSSI_RESPONSE) + sizeof(RSSI_RESPONSE_VARIABLE_EXAMPLE))


/* Fetch an RSSI and display a response  */
static void deviceTestService_RssiSendResponse(Task task, tp_bdaddr *addr)
{
    char response[FULL_RSSI_RESPONSE_LEN];
    int16 rssi;

    if (VmBdAddrGetRssi(addr, &rssi))
    {
        sprintf(response, RSSI_RESPONSE RSSI_RESPONSE_VARIABLE_FMT,
                addr->taddr.addr.nap, addr->taddr.addr.uap, addr->taddr.addr.lap,
                rssi);

        DeviceTestService_CommandResponse(task, response, FULL_RSSI_RESPONSE_LEN);
    }
}


/*! Command handler for AT + RSSIREAD

    This function retrieves the RSSI level for any active Bluetooth 
    connections, and responds with +RSSIREAD:<bdaddr>,<rssi> messages.

    Otherwise errors are reported

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleRssiRead(Task task)
{
    cm_connection_iterator_t iterator;
    tp_bdaddr addr;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRssiRead");

    if (   !DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
    {
        do
        {
            deviceTestService_RssiSendResponse(task, &addr);
        } while (ConManager_IterateNextActiveConnection(&iterator, &addr));
    }

    DeviceTestService_CommandResponseOk(task);
}

