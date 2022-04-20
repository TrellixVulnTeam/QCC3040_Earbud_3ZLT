/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the  gaia fit test framework plugin
*/

#ifdef ENABLE_EARBUD_FIT_TEST
#include "fit_test_gaia_plugin_private.h"
#include "fit_test_gaia_plugin.h"
#include "gaia_features.h"
#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <phy_state.h>
#include "ui.h"

typedef enum
{
    gaia_fit_test_result_good = 0x01,
    gaia_fit_test_result_bad,
    gaia_fit_test_result_error
}gaia_fit_test_result_t;

static uint8 fitTestGaiaPlugin_ConvertToGaiaPayload(uint8 fit_status);

static void fitTestGaiaPlugin_SendResponse(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    GaiaFramework_SendResponse(t, GAIA_FIT_TEST_FEATURE_ID, pdu_id, length, payload);
}

static void fitTestGaiaPlugin_SendError(GAIA_TRANSPORT *t, uint8 pdu_id, uint8 status_code)
{
    GaiaFramework_SendError(t, GAIA_FIT_TEST_FEATURE_ID, pdu_id, status_code);
}

static void fitTestGaiaPlugin_SendNotification(uint8 notification_id, uint16 length, const uint8 *payload)
{
    GaiaFramework_SendNotification(GAIA_FIT_TEST_FEATURE_ID, notification_id, length, payload);
}

static bool fitTestGaiaPlugin_CanInjectUiInput(void)
{
#ifdef INCLUDE_STEREO
    return FALSE;
#else
    return appPhyStateIsOutOfCase();
#endif /* INCLUDE_STERE0 */
}

static uint8 fitTestGaiaPlugin_ConvertToGaiaPayload(uint8 fit_status)
{
    uint8 result=gaia_fit_test_result_error;

   switch(fit_status)
   {
        case fit_test_result_bad:
           result = gaia_fit_test_result_bad;
       break;

       case fit_test_result_good:
           result = gaia_fit_test_result_good;
       break;

       case fit_test_result_error:
           result = gaia_fit_test_result_error;
       break;

       default:
           result= gaia_fit_test_result_error;
       break;
   }
   return result;
}

static void fitTestGaiaPlugin_ResultAvailableNotification(FIT_TEST_RESULT_IND_T *fit_test_result)
{
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;
    uint8 left_eb_payload;
    uint8 right_eb_payload;

    notification_id = fit_test_result_available_notification;
    payload_length = FIT_TEST_GAIA_TEST_RESULT_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    left_eb_payload=fitTestGaiaPlugin_ConvertToGaiaPayload(fit_test_result->left_earbud_result);
    right_eb_payload=fitTestGaiaPlugin_ConvertToGaiaPayload(fit_test_result->right_earbud_result);

    payload[FIT_TEST_GAIA_TEST_RESULT_LEFT_OFFSET]  = left_eb_payload;
    payload[FIT_TEST_GAIA_TEST_RESULT_RIGHT_OFFSET] = right_eb_payload;

    fitTestGaiaPlugin_SendNotification(notification_id, payload_length, payload);
    free(payload);
}

static void fitTestGaiaPlugin_RoleChangeCompleted(GAIA_TRANSPORT *t, bool is_primary)
{
    UNUSED(t);
    UNUSED(is_primary);
}

static void fitTestGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    FitTest_ClientUnRegister(&fitTestGaiaPlugin_GetTaskData()->task);
}

static void fitTestGaiaPlugin_TransportConnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    FitTest_ClientRegister(&fitTestGaiaPlugin_GetTaskData()->task);
}

static void fitTestGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
}


static void fitTestGaiaPlugin_HandleMessage(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    switch(id)
    {
        case FIT_TEST_RESULT_IND:
        {
            fitTestGaiaPlugin_ResultAvailableNotification((FIT_TEST_RESULT_IND_T *)msg);
        }
        break;
        default:
        break;
    }
}

static void fitTestGaiaPlugin_HandleStartStopCommand(GAIA_TRANSPORT *t,uint16 payload_length,const uint8 *payload)
{
    if(payload_length==FIT_TEST_GAIA_START_STOP_COMMAND_PAYLOAD_LENGTH)
    {
        if(*payload == FIT_TEST_GAIA_START_TEST)
        {
            if(FitTest_IsReady() && fitTestGaiaPlugin_CanInjectUiInput())
            {
                Ui_InjectUiInput(ui_input_fit_test_start);
                fitTestGaiaPlugin_SendResponse(t, fit_test_gaia_start_stop_command, 0, NULL);
            }
            else
            {
                fitTestGaiaPlugin_SendError(t,fit_test_gaia_start_stop_command,incorrect_state);
            }
        }
        else if(*payload == FIT_TEST_GAIA_STOP_TEST)
        {
            if(FitTest_IsRunning())
            {
                Ui_InjectUiInput(ui_input_fit_test_abort);
                fitTestGaiaPlugin_SendResponse(t, fit_test_gaia_start_stop_command, 0, NULL);
            }
            else
            {
                fitTestGaiaPlugin_SendError(t,fit_test_gaia_start_stop_command,incorrect_state);
            }
        }
    }
    else
    {
        fitTestGaiaPlugin_SendError(t,fit_test_gaia_start_stop_command,invalid_parameter);
    }
}

static gaia_framework_command_status_t fitTestGaiaPlugin_CommandHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    switch(pdu_id)
    {
        case fit_test_gaia_start_stop_command:
            fitTestGaiaPlugin_HandleStartStopCommand(t,payload_length,payload);
        break;

        default:
        break;
    }
    return command_handled;
}


void FitTestGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = fitTestGaiaPlugin_CommandHandler,
        .send_all_notifications = fitTestGaiaPlugin_SendAllNotifications,
        .transport_connect = fitTestGaiaPlugin_TransportConnect,
        .transport_disconnect = fitTestGaiaPlugin_TransportDisconnect,
        .role_change_completed = fitTestGaiaPlugin_RoleChangeCompleted,
    };

    DEBUG_LOG_ALWAYS("FitTestGaiaPlugin_Init");

    fit_test_gaia_plugin_task_data_t *fit_test_gaia_data = fitTestGaiaPlugin_GetTaskData();

    /* Initialise plugin framework task data */
    memset(fit_test_gaia_data, 0, sizeof(*fit_test_gaia_data));
    fit_test_gaia_data->task.handler = fitTestGaiaPlugin_HandleMessage;


    GaiaFramework_RegisterFeature(GAIA_FIT_TEST_FEATURE_ID, FIT_TEST_GAIA_PLUGIN_VERSION, &functions);
}
#endif /*ENABLE_EARBUD_FIT_TEST*/
