/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of common code for radio test support in the device 
            test service.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_commands_helper.h"
#include "device_test_service_commands_rftest_common.h"
#include "device_test_parse.h"

#include <bdaddr.h>
#include <power_manager.h>
#include <pio_monitor.h>
#include <connection.h>
#include <system_reboot.h>
#include <stdio.h>
#include <logging.h>
#include <touch_config.h>
#include <touch.h>

#include <test2.h>


#ifdef INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2

/*! State of the RFTest portion of device test service */
struct deviceTestServiceRfTestState deviceTestService_rf_test_state = {0};

LOGGING_PRESERVE_MESSAGE_TYPE(rftest_internal_message_t)

static void deviceTestService_Rftest_task_handler(Task task, MessageId id, Message message);

TaskData device_test_service_rftest_task = {.handler = deviceTestService_Rftest_task_handler };

void deviceTestService_RfTest_SetupForTestCompletion(void)
{
    if (CONFIGURED(stop_time))
    {
        MessageSendLater(RFTEST_TASK(),
                         RFTEST_INTERNAL_TEST_TIMEOUT, NULL,
                         SETTING(test_timeout));
    }
    if (CONFIGURED(stop_pio))
    {
        PioMonitorRegisterTask(RFTEST_TASK(), SETTING(test_stop_pio));
    }

    if (CONFIGURED(stop_touch))
    {
        /* add clients to receive touch event notifications */
        TouchSensorActionClientRegister(RFTEST_TASK());
    }
}

void deviceTestService_RfTest_TearDownOnTestCompletion(void)
{
    BREDR_RUNNING() = FALSE;
    LE_RUNNING() = FALSE;
    LE_RESPONSE_TASK() = NULL;

    MessageCancelAll(RFTEST_TASK(), RFTEST_INTERNAL_TEST_TIMEOUT);

    if (CONFIGURED(stop_pio))
    {
        PioMonitorUnregisterTask(RFTEST_TASK(), SETTING(test_stop_pio));
    }

    if (CONFIGURED(stop_touch))
    {
        /* add clients to receive touch event notifications */
        TouchSensorClientUnRegister(RFTEST_TASK());
    }
}

#ifdef INCLUDE_CAPSENSE
/*! \brief Internal function to process touchpad press

    Test will be terminated if the message is from the touchpad

    \param change Message from touchpad.
*/
static void deviceTestService_CheckTestStopOnTouchpad(const TOUCH_SENSOR_ACTION_T *action_msg)
{
    UNUSED(action_msg);
    /* Check for unexpected message(s) and stop for the future */
    if (!RUNNING())
    {
        if (CONFIGURED(stop_touch))
        {
            TouchSensorClientUnRegister(RFTEST_TASK());
        }
        return;
    }

    /* If this is a touch of some sort, reboot. Any event from the Touch module counts. */
    DEBUG_LOG_ALWAYS("deviceTestService_CheckTestStopOnTouchpad - rebooting");    
    SystemReboot_Reboot();
}
#endif

/*! \brief Internal function to process requested PIO changes

    Test will be terminated if the message is for the configured PIO

    \param change Message from PIO monitoring. May not be for expected PIO.
 */
static void deviceTestService_CheckTestStopOnPio(const MessagePioChanged *change)
{
    bool pio_is_set;

    /* Check for unexpected message(s) and stop for the future */
    if (!RUNNING())
    {
        if (CONFIGURED(stop_pio))
        {
            PioMonitorUnregisterTask(RFTEST_TASK(), SETTING(test_stop_pio));
        }
        return;
    }

    if (   PioMonitorIsPioInMessage(change, SETTING(test_stop_pio), &pio_is_set)
        && pio_is_set)
    {
        DEBUG_LOG_ALWAYS("deviceTestService_CheckTestStopOnPio - rebooting");

        SystemReboot_Reboot();
    }
}

/*! \brief Internal function to handle a timeout during a test */
static void deviceTestService_TestStoppedOnTimeout(void)
{
    DEBUG_LOG_ALWAYS("deviceTestService_TestStoppedOnTimeout. Reboot:%d",
                        SETTING(test_reboot));

    if (SETTING(test_reboot))
    {
        SystemReboot_Reboot();
    }
    else
    {
        bool response = FALSE;

        if (RUNNING())
        {
            if (BREDR_RUNNING())
            {
                response = Test2RfStop();
            }
            if (LE_RUNNING())
            {
                ConnectionBleTestEnd();
                response = TRUE;
            }
            DEBUG_LOG_ALWAYS("deviceTestService_TestStoppedOnTimeout - command response:%d",
                             response);
        }

        deviceTestService_RfTest_TearDownOnTestCompletion();
    }
}



/* \brief Task handler for RF testing

    This is required for two purposes
    1) delaying the start of a test so that an OK response has a chance to arrive
    2) timeout for the end of a test step

    \param[in]  task    The task the message was sent to
    \param      id      The message identifier
    \param[in]  message The message content
 */
static void deviceTestService_Rftest_task_handler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_FN_ENTRY("deviceTestService_Rftest_task_handler MESSAGE:rftest_internal_message_t:0x%x", id);

    switch(id)
    {
        case RFTEST_INTERNAL_CARRIER_WAVE:
            deviceTestService_RfTestBredr_CarrierTest();
            break;

        case RFTEST_INTERNAL_TXSTART:
            deviceTestService_RfTestBredr_TxStart();
            break;

        case RFTEST_INTERNAL_DUTMODE:
            deviceTestService_RfTestBredr_DutMode();
            break;

        case RFTEST_INTERNAL_LETXSTART:
            deviceTestService_RfTest_LeTxStart();
            break;

        case RFTEST_INTERNAL_LERXSTART:
            deviceTestService_RfTest_LeRxStart();
            break;

        case RFTEST_INTERNAL_TEST_TIMEOUT:
            deviceTestService_TestStoppedOnTimeout();
            break;

        case MESSAGE_PIO_CHANGED:
            deviceTestService_CheckTestStopOnPio((const MessagePioChanged *)message);
            break;

#ifdef INCLUDE_CAPSENSE
        case TOUCH_SENSOR_ACTION:
    	    deviceTestService_CheckTestStopOnTouchpad((const TOUCH_SENSOR_ACTION_T *) message);
            break;
#endif
	    
	    
        default:
            break;
    }
}


/*! \brief Command handler for AT + RFTESTSTOP

    The function decides if the commands is allowed and if so stops any RF test 
    that is in progress. It will send an OK response even if there is no 
    RF testing in progress.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleRfTestStop(Task task)
{
    bool response = FALSE;

    if (!DeviceTestService_CommandsAllowed())
    {
        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestStop. Disallowed");

        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (LE_RUNNING())
    {
        ConnectionBleTestEnd();
        LE_RESPONSE_TASK() = task;
        return;
    }

    /* The Stop command also destroys the current connection (if any)
       so can't call the stop command if not running */
    if (BREDR_RUNNING())
    {
        response = Test2RfStop();
    }
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestStop (bredr) - command response:%d",
                     response);

    deviceTestService_RfTest_TearDownOnTestCompletion();

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGSTOPTIME=%d:reboot, %d:timeMs 

    Decide if the command is allowed and save the stop time ready for use
    by subsequent test commands. 

    An ERROR response is sent if the command is not allowed, or the time
    parameter is illegal, otherwise an OK response is sent.

    \param[in] task The task to be used in command responses
    \param[in] stoptime_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgStopTime(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopTime *stoptime_params)
{
    uint16 reboot = stoptime_params->reboot;
    uint16 test_timeout = stoptime_params->timeMs;
    
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgStopTime reboot:%d,time:%dms",
                        reboot, test_timeout);

    CONFIGURED(stop_time) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(reboot<=1))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(stop_time) = TRUE;
    SETTING(test_reboot) = reboot;
    SETTING(test_timeout) = test_timeout;

    DeviceTestService_CommandResponseOk(task);
}

/*! \brief Command handler for AT+RFTESTCFGSTOPTOUCH 

    Decide if the command is allowed and register with the touchpad
    for subsequent test commands.

    An ERROR response is sent if the command is not allowed, otherwise an OK response is sent.

    \param[in] task The task to be used in command responses
    \param[in] stoptouch_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgStopTouch(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgStopTouch");

    CONFIGURED(stop_touch) = FALSE;

#ifndef INCLUDE_CAPSENSE
    DeviceTestService_CommandResponseError(task);
    return;
#else   
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(stop_touch) = TRUE;

    DeviceTestService_CommandResponseOk(task);
#endif
}

/*! \brief Command handler for AT+RFTESTCFGSTOPPIO=%d:pio 

    Decide if the command is allowed and save the supplied pio number ready
    for use by subsequent test commands.

    An ERROR response is sent if the command is not allowed, or the time
    parameter is notsupported, otherwise an OK response is sent.

    \param[in] task The task to be used in command responses
    \param[in] stoppio_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgStopPio(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopPio *stoppio_params)
{
    uint16 pio = stoppio_params->pio;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgStopPio. pio:%d", pio);

    CONFIGURED(stop_pio) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(pio <= 95))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(stop_pio) = TRUE;
    SETTING(test_stop_pio) = pio;

    DeviceTestService_CommandResponseOk(task);
}

/*! @} End of group documentation */

#else /* !INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */


#endif /* INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */
