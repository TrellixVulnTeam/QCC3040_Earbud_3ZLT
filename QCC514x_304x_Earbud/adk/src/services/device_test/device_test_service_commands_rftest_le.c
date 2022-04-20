/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of radio test commands for the Bluetooth Low Energy (LE)
            in the device test service.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_rftest.h"
#include "device_test_service_commands_helper.h"
#include "device_test_service_commands_rftest_common.h"
#include "device_test_parse.h"

#include <connection.h>

#include <stdio.h>
#include <logging.h>

#ifdef INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2

/*! The base part of the AT command response */
#define LE_RXTEST_RESPONSE "+RFTESTPACKETS:"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_LE_RXTEST_RESPONSE_LEN (sizeof(LE_RXTEST_RESPONSE) + 5)

/*! \brief Handler function for BLE TX test confirm

    Sends an AT command response based on the status in the message.
    success causes an OK response, otherwise ERROR

    \param tx_test_cfm Confirmation message
 */
static void deviceTestService_HandleBleTxStartCfm(const CL_DM_BLE_TRANSMITTER_TEST_CFM_T *tx_test_cfm)
{
    DEBUG_LOG_ALWAYS("deviceTestService_HandleBleTxStartCfm enum:hci_status:%d",
                        tx_test_cfm->status);

    DeviceTestService_CommandResponseOkOrError(LE_RESPONSE_TASK(),
                                               tx_test_cfm->status == hci_success);
}

/*! \brief Handler function for BLE RX test confirm

    Sends an AT command response based on the status in the message.
    success causes an OK response, otherwise ERROR

    \param rx_test_cfm Confirmation message
 */
static void deviceTestService_HandleBleRxStartCfm(const CL_DM_BLE_RECEIVER_TEST_CFM_T *rx_test_cfm)
{
    DEBUG_LOG_ALWAYS("deviceTestService_HandleBleRxStartCfm enum:hci_status:%d",
                        rx_test_cfm->status);

    DeviceTestService_CommandResponseOkOrError(LE_RESPONSE_TASK(),
                                               rx_test_cfm->status == hci_success);
}

/*! \brief Handler function for BLE STOP test confirm

    Sends an AT command response based on the status in the message.
    On success causes an OK response as well as the quantity of LE packets
    that have been received (+RFTESTPACKETS), otherwise ERROR

    \param stop_cfm Confirmation message
 */
static void deviceTestService_HandleBleStopCfm(const CL_DM_BLE_TEST_END_CFM_T *stop_cfm)
{
    Task stashed_task = LE_RESPONSE_TASK();

    DEBUG_LOG_ALWAYS("deviceTestService_HandleBleStopCfm enum:hci_status:%d %u packets",
                        stop_cfm->status, stop_cfm->number_of_rx_packets);

    deviceTestService_RfTest_TearDownOnTestCompletion();

    if (stop_cfm->status == hci_success)
    {
        char response[FULL_LE_RXTEST_RESPONSE_LEN];

        // Make use of C combining two adjacent strings
        sprintf(response, LE_RXTEST_RESPONSE "%u", stop_cfm->number_of_rx_packets);

        DeviceTestService_CommandResponse(stashed_task, response, FULL_LE_RXTEST_RESPONSE_LEN);
        DeviceTestService_CommandResponseOk(stashed_task);
    }
    else
    {
        DeviceTestService_CommandResponseError(stashed_task);
    }
}

bool DeviceTestService_HandleConnectionLibraryMessages_RfTest(MessageId id, Message message, bool already_handled)
{
    UNUSED(already_handled);

    if (!LE_RESPONSE_TASK())
    {
        return FALSE;
    }

    switch (id)
    {
        case CL_DM_BLE_TRANSMITTER_TEST_CFM:
            deviceTestService_HandleBleTxStartCfm((const CL_DM_BLE_TRANSMITTER_TEST_CFM_T *)message);
            break;

        case CL_DM_BLE_RECEIVER_TEST_CFM:
            deviceTestService_HandleBleRxStartCfm((const CL_DM_BLE_RECEIVER_TEST_CFM_T *)message);
            break;

        case CL_DM_BLE_TEST_END_CFM:
            deviceTestService_HandleBleStopCfm((const CL_DM_BLE_TEST_END_CFM_T *)message);
            break;

        default:
            return FALSE;
    }
    LE_RESPONSE_TASK() = NULL;

    return TRUE;
}


void deviceTestService_RfTest_LeTxStart(void)
{
    ConnectionBleTransmitterTest(SETTING(le_channel), SETTING(le_length), SETTING(le_pattern));

    LE_RUNNING() = TRUE;

    deviceTestService_RfTest_SetupForTestCompletion();
}

void deviceTestService_RfTest_LeRxStart(void)
{
    ConnectionBleReceiverTest(SETTING(le_channel));

    LE_RUNNING() = TRUE;

    deviceTestService_RfTest_SetupForTestCompletion();
}


/*! \brief Command handler for AT+RFTESTLETXSTART

    The function decides if the command is allowed \b and \b has \b been
    \b configured and if so starts a test transmission for the Bluetooth
    Low Energy carrier.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
    \param[in] letx_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestLeTxStart(Task task, 
                            const struct DeviceTestServiceCommand_HandleRfTestLeTxStart *letx_params)
{
    uint16 channel = letx_params->lechannel;
    uint16 length = letx_params->lelength;
    uint16 pattern = letx_params->pattern;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestLeTxStart Chan:%d Len:%d Pattern:%d",
                        channel, length, pattern);

    if (   !DeviceTestService_CommandsAllowed()
        || !(channel <= 39)
        || !(length <= 255)
        || !(pattern <= 7))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    SETTING(le_channel) = channel;
    SETTING(le_length) = length;
    SETTING(le_pattern) = pattern;

    LE_RESPONSE_TASK() = task;

    /* Delay command action to allow OK to be sent */
    MessageSendLater(RFTEST_TASK(),
                     RFTEST_INTERNAL_LETXSTART, NULL,
                     deviceTestService_CommandDelayMs());
}

/*! \brief Command handler for AT+RFTESTLERXSTART

    The function decides if the command is allowed \b and \b has \b been
    \b configured and if so starts a receiver test for the Bluetooth
    Low Energy carrier.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
    \param[in] lerx_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestLeRxStart(Task task, 
                            const struct DeviceTestServiceCommand_HandleRfTestLeRxStart *lerx_params)
{
    uint16 channel = lerx_params->lechannel;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestLeRxStart Chan:%d",
                        channel);

    if (   !DeviceTestService_CommandsAllowed()
        || !(channel <= 39))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    SETTING(le_channel) = channel;

    LE_RESPONSE_TASK() = task;

    /* Delay command action to allow OK to be sent */
    MessageSendLater(RFTEST_TASK(),
                     RFTEST_INTERNAL_LERXSTART, NULL,
                     deviceTestService_CommandDelayMs());
}




/*! @} End of group documentation */

#else /* !INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */

#ifdef INCLUDE_DEVICE_TEST_SERVICE

/* Include stubs of LE commands, which are optional.
   Only needed if DTS is supported */

static void DeviceTestServiceCommand_HandleRfTestLe(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTest. RF Test LE Commands not supported");

    DeviceTestService_CommandResponseError(task);
}

void DeviceTestServiceCommand_HandleRfTestLeTxStart(Task task, 
                            const struct DeviceTestServiceCommand_HandleRfTestLeTxStart *letx_params)
{
    UNUSED(letx_params);

    DeviceTestServiceCommand_HandleRfTestLe(task);
}

void DeviceTestServiceCommand_HandleRfTestLeRxStart(Task task, 
                            const struct DeviceTestServiceCommand_HandleRfTestLeRxStart *lerx_params)
{
    UNUSED(lerx_params);

    DeviceTestServiceCommand_HandleRfTestLe(task);
}

#endif /* INCLUDE_DEVICE_TEST_SERVICE */

#endif /* INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */
