/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of the bdaddr command, provided as an example, for the 
            device test service.

            This illustrates that commands are allowed to take some time to process
            and may use tasks. When the response is finally generated, the current
            state of the device test service should be checked, using 
            DeviceTestService_CommandsAllowed().

            \note This file contains more comments than normal. The functions are
            also extracted in the documentation as an example.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"     // Headers for device test functions 
#include "device_test_service_auth.h"
#include "device_test_parse.h"       // Header for the handler prototype
#include "connection_abstraction.h"
#include <bt_device.h>
#include <stdio.h>       // sprintf
#include <logging.h>     // Log functionality

/*! The base part of the AT command response */
#define LOCAL_BDADDR_RESPONSE "+LOCALBDADDR:"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_BDADDR_RESPONSE_LEN (sizeof(LOCAL_BDADDR_RESPONSE) + 12)

/* pre-declare function to handle messages. Not all command handlers
   need this. */
static void deviceTestService_BdaddrMessageHandler(Task task, MessageId id, Message message);

/* Structure to hold information for this command handler, including 
   the Task */
static struct
{
    TaskData    bdaddr_task;
    bool        single_response_due;
} device_test_service_bdaddr_taskdata = { .bdaddr_task = deviceTestService_BdaddrMessageHandler,
                                        };

static inline void deviceTestService_BdaddrMessageResponse(Task task, bdaddr *local_addr)
{
    // Allocate a buffer guaranteed to be large enough for the response
    // for this command, the response is a fixed size
    char response[FULL_BDADDR_RESPONSE_LEN];

    // Make use of C combining two adjacent strings
    // This makes a single string "+LOCALBDADDR: %04x%02x%06lx"
    sprintf(response, LOCAL_BDADDR_RESPONSE "%04x%02x%06lx",
            local_addr->nap, local_addr->uap, local_addr->lap);

    // Send the response with the Bluetooth address
    DeviceTestService_CommandResponse(task, response, FULL_BDADDR_RESPONSE_LEN);
    // and finally send the OK response expected
    DeviceTestService_CommandResponseOk(task);
}

/*! Message handler.

    Although we only expect one message, the function includes
    safety checks on the message ID and whether we are expecting
    to be called.

    If we are expecting to respond, then we ensure that a response 
    is sent.

    \param task[in] The task this message was sent to
    \param id       Identifier of the message
    \param message  The message content
 */
static void deviceTestService_BdaddrMessageHandler(Task task, MessageId id, Message message)
{
    bdaddr  local_addr = { 0 };
    bool    address_received = FALSE;

    // The message handler is less important, so use _DEBUG rather
    // than _ALWAYS
    DEBUG_LOG_DEBUG("deviceTestService_BdaddrMessageHandler");

    // Check our safety flag. If we are not expecting to send a
    // command response, just exit.
    if (!device_test_service_bdaddr_taskdata.single_response_due)
    {
        return;
    }

    device_test_service_bdaddr_taskdata.single_response_due = FALSE;

    // Check if the device test service is still enabled
    // and if the message is the one we expect.
    if (DeviceTestService_CommandsAllowed())
    {
        if (CL_DM_LOCAL_BD_ADDR_CFM == id)
        {
            const CL_DM_LOCAL_BD_ADDR_CFM_T *local_cfm = (CL_DM_LOCAL_BD_ADDR_CFM_T *)message;

            if (hci_success == local_cfm->status)
            {
                local_addr = local_cfm->bd_addr;
                address_received = TRUE;
            }
        }
    }

    if (address_received)
    {
        deviceTestService_BdaddrMessageResponse(task, &local_addr);
    }
    else
    {
        // Send an error as the fallback response. Should never come
        // here in normal operation.
        DeviceTestService_CommandResponseError(task);
    }
}


/*! Command handler for AT+LOCALBDADDR?

    The function decides if the command is allowed, and if so requests
    the local address, using a task for the response.

    Otherwise errors are reported

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleLocalBdaddr(Task task)
{
    // Always include debug for test commands so they appear in logs.
    // Test commands may affect normal program flow so should 
    // be visible
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleLocalBdaddr");

    // Include a check on whether we should be receiving this command.
    // The parser accepts commands, even if the device test service
    // has not been authenticated.
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    // Note that this function does not send an OK response
    // instead set an internal flag to record that a response is due
    // ( the flag may not be needed, but is a good safety check )
    device_test_service_bdaddr_taskdata.single_response_due = TRUE;

    // Finally ask the connection library to send us the address
    ConnectionReadLocalAddr(&device_test_service_bdaddr_taskdata.bdaddr_task);
}
/*! @} End of group documentation */
