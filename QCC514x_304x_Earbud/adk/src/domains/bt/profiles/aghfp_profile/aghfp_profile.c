/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP component.
*/

#include "aghfp_profile.h"
#include "aghfp_profile_instance.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_audio.h"
#include "aghfp.h"
#include "aghfp_profile_call_list.h"
#include "connection_manager.h"
#include "panic.h"

#include <logging.h>
#include <bandwidth_manager.h>
#include <device_properties.h>
#include <system_state.h>
#include <telephony_messages.h>
#include <device_list.h>
#include <ui.h>
#include <feature.h>
#include <stdio.h>

/*! \brief Application HFP component main data structure. */
agHfpTaskData aghfp_profile_task_data;

dialed_number last_dialed_number;

const aghfpState aghfp_call_status_table[2] =
{
    /* aghfp_call_none */   AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_active */ AGHFP_STATE_CONNECTED_ACTIVE,
};

const aghfpState aghfp_call_setup_table[4] =
{
    /* aghfp_call_setup_none */         AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_setup_incoming */     AGHFP_STATE_CONNECTED_INCOMING,
    /* aghfp_call_setup_outgoing */     AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_setup_remote_alert */ AGHFP_STATE_CONNECTED_IDLE,
};

static const aghfp_audio_params audio_params =
{
    8000,                    /* Bandwidth for both Tx and Rx */
    0x0007,                  /* Max Latency                  */
    sync_air_coding_cvsd,    /* Voice Settings               */
    sync_retx_power_usage,   /* Retransmission Effort       */
    FALSE                    /* Use WB-Speech if available   */
};

#define MAX_CALL_HISTORY 1

static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message);

static void aghfpProfile_InitTaskData(void)
{
    /* set up common hfp profile task handler. */
    aghfp_profile_task_data.task.handler = aghfpProfile_TaskMessageHandler;

    /* create list for SLC notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetSlcStatusNotifyList(), AGHFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create list for general status notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetStatusNotifyList(), AGHFP_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&aghfp_profile_task_data.connect_request_clients);
    TaskList_WithDataInitialise(&aghfp_profile_task_data.disconnect_request_clients);
}

/*! \brief Entering `Initialising HFP` state
*/
static void aghfpProfile_InitAghfpLibrary(void)
{
    uint16 supported_features = aghfp_incoming_call_reject |
                                aghfp_esco_s4_supported |
                                aghfp_codec_negotiation |
                                aghfp_enhanced_call_status;

    uint16 supported_qce_codec = 0;

#ifdef INCLUDE_SWB
    if (FeatureVerifyLicense(APTX_ADAPTIVE_MONO_DECODE))
    {
        DEBUG_LOG("License found for AptX adaptive mono decoder");
        supported_qce_codec = CODEC_64_2_EV3;
    }
    else
    {
        DEBUG_LOG("No license found for AptX adaptive mono decoder");
    }
#endif
    AghfpInitQCE(&aghfp_profile_task_data.task,
                 aghfp_handsfree_18_profile,
                 supported_features,
                 supported_qce_codec);
}


/*! \brief Send SLC status indication to all clients on the list.
 */
static void aghfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr)
{
    Task next_client = NULL;

    while (TaskList_Iterate(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), &next_client))
    {
        MAKE_AGHFP_MESSAGE(APP_AGHFP_SLC_STATUS_IND);
        message->slc_connected = connected;
        message->bd_addr = *bd_addr;
        MessageSend(next_client, APP_AGHFP_SLC_STATUS_IND, message);
    }
}

/*! \brief Handle SLC connect confirmation
*/
static void aghfpProfile_HandleHfpSlcConnectCfm(const AGHFP_SLC_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForAghfp(cfm->aghfp);

    PanicNull(instance);

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectCfm(%p) enum:aghfpState:%d enum:aghfp_connect_status:%d",
              instance, state, cfm->status);

    switch (state)
    {
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        {
            /* Check if SLC connection was successful */
            if (cfm->status == aghfp_connect_success)
            {
                /* Store SLC sink */
                instance->slc_sink = cfm->rfcomm_sink;


                if (instance->bitfields.call_setup == aghfp_call_setup_none)
                {
                    /* If there are no calls being setup then progress to connected state
                       based on the call status
                    */
                    AghfpProfile_SetState(instance, aghfp_call_status_table[instance->bitfields.call_status]);
                }
                else
                {
                    /* If there are calls being setup then progress to connected state
                       based on the call setup stage.
                    */
                    AghfpProfile_SetState(instance, aghfp_call_setup_table[instance->bitfields.call_setup]);
                }

                aghfpProfile_SendSlcStatus(TRUE, &instance->hf_bd_addr);

                return;
            }
            /* Not a successful connection so set to disconnected state */
            AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);

            /* If a call is active or being setup keep the instance to track
               the state in the event of a successful SLC connection.
            */
            if (instance->bitfields.call_status == aghfp_call_none &&
                instance->bitfields.call_setup == aghfp_call_setup_none)
            {
                AghfpProfileInstance_Destroy(instance);
            }
        }
        return;

        default:
        DEBUG_LOG("SLC connect confirmation received in wrong state.");
        return;
    }
}



/*! \brief Handle HFP Library initialisation confirmation
*/
static void aghfpProfile_HandleAghfpInitCfm(const AGHFP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("aghfpProfile_HandleAghfpInitCfm status enum:aghfp_init_status:%d", cfm->status);

    last_dialed_number.number = NULL;
    last_dialed_number.number_len = 0;

    if (cfm->status == aghfp_init_success)
    {
        AghfpProfileInstance_SetAghfp(cfm->aghfp);
        /* Handle the AT+CIND? in the AGHFP profile library.
           Enabling causes AGHFP lib to send a AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND */
        AghfpCindStatusPollClientEnable(cfm->aghfp, TRUE);
        MessageSend(SystemState_GetTransitionTask(), APP_AGHFP_INIT_CFM, 0);
    }
    else
    {
        Panic();
    }
}

/*! \brief Handle SLC connect indication
*/
static void aghfpProfile_HandleHfpSlcConnectInd(const AGHFP_SLC_CONNECT_IND_T *ind)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    if (instance == NULL)
    {
        instance = AghfpProfileInstance_Create(&ind->bd_addr, TRUE);
    }

    PanicNull(instance);

    aghfpState state = AghfpProfile_GetState(instance);

    bool response = FALSE;

    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectInd(%p) enum:aghfpState:%d", instance, state);

    switch (state)
    {
        case AGHFP_STATE_DISCONNECTED:
        {
            instance->hf_bd_addr = ind->bd_addr;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTING_REMOTE);
            response = TRUE;
        }
        break;

        default:
        break;
    }

    AghfpSlcConnectResponse(ind->aghfp, response);
}

/*! \brief Handle HF answering an incoming call
*/
static void aghfpProfile_HandleCallAnswerInd(AGHFP_ANSWER_IND_T *ind)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    PanicNull(instance);

    DEBUG_LOG("aghfpProfile_HandleCallAnswerInd(%p)", instance);

    AghfpSendOk(instance->aghfp);
    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_INCOMING)
    {
        instance->bitfields.call_status = aghfp_call_active;
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
        AghfpProfileCallList_AnswerIncomingCall(instance->call_list);
    }
    else if (state == AGHFP_STATE_CONNECTED_ACTIVE && instance->bitfields.call_setup == aghfp_call_setup_incoming)
    {
        instance->bitfields.call_hold = aghfp_call_held_active;
        instance->bitfields.call_setup = aghfp_call_setup_none;
        AghfpProfileCallList_HoldActiveCall(instance->call_list);
        AghfpProfileCallList_AnswerIncomingCall(instance->call_list);
    }
}

static void aghfpProfile_UpdateCallListAfterHangUp(aghfpState state, call_list_t *call_list)
{
    switch (state)
    {
    case AGHFP_STATE_CONNECTED_ACTIVE:
        AghfpProfileCallList_TerminateActiveCall(call_list);
        break;
    case AGHFP_STATE_CONNECTED_INCOMING:
        AghfpProfileCallList_RejectIncomingCall(call_list);
        break;
    case AGHFP_STATE_CONNECTED_OUTGOING:
        AghfpProfileCallList_OutgoingCallRejected(call_list);
        break;
    default:
       DEBUG_LOG("aghfpProfile_UpdateCallListAfterHangUp: Invalid state");
    }
}

/*! \brief Handle HF rejecting an incoming call or ending an ongoing call
*/
static void aghfpProfile_HandleCallHangUpInd(AGHFP_CALL_HANG_UP_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallHangUpInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleCallHangUpInd: No aghfpInstanceTaskData instance available");
        return;
    }

    AghfpSendOk(instance->aghfp);
    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_ACTIVE ||
        state == AGHFP_STATE_CONNECTED_INCOMING ||
        state == AGHFP_STATE_CONNECTED_OUTGOING)
    {
        Ui_InformContextChange(ui_provider_telephony, context_voice_connected);
        aghfpProfile_UpdateCallListAfterHangUp(state, instance->call_list);
        if (instance->bitfields.call_hold == aghfp_call_held_none)
        {
            instance->bitfields.call_status = aghfp_call_none;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_IDLE);
        }
        else if (instance->bitfields.call_hold == aghfp_call_held_active)
        {
            instance->bitfields.call_hold = aghfp_call_held_remaining;
            AghfpSendCallHeldIndicator(instance->aghfp, instance->bitfields.call_hold);
        }
    }
}

/*! \brief Handle disconnect of the SLC
*/
static void aghfpProfile_HandleSlcDisconnectInd(AGHFP_SLC_DISCONNECT_IND_T *message)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(message->aghfp);

    DEBUG_LOG("aghfpProfile_HandleSlcDisconnectInd(%p) enum:aghfp_disconnect_status:%d",
              instance, message->status);

    if (instance)
    {
        aghfpProfile_SendSlcStatus(FALSE, &instance->hf_bd_addr);

        instance->slc_sink = NULL;
        AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);
        /* For link loss we wait for the HF to attempt a reconnection */
        if (message->status != aghfp_disconnect_link_loss &&
            instance->bitfields.call_hold == aghfp_call_held_none)
        {
            AghfpProfileInstance_Destroy(instance);
        }
    }
}

/*! \brief Handle audio connect confirmation
*/
static void aghfpProfile_HandleAgHfpAudioConnectCfm(AGHFP_AUDIO_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(cfm->aghfp);

    PanicNull(instance);

    DEBUG_LOG("agHfpProfile_HandleAgHfpAudioConnectCfm(%p) enum:aghfp_audio_connect_status:%d", instance, cfm->status);

    if (cfm->status == aghfp_audio_connect_success)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_CONNECTED_IND);
        instance->sco_sink = cfm->audio_sink;
        AghfpProfile_StoreConnectParams(cfm);
        if (instance->bitfields.in_band_ring && instance->bitfields.call_setup == aghfp_call_setup_incoming)
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RING_REQ);
            message->addr = instance->hf_bd_addr;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ, message);
        }
    }
    else
    {
        DEBUG_LOG("agHfpProfile_HandleAgHfpAudioConnectCfm: Connection failure. Status %d", cfm->status);
    }
}

/*! \brief Handle audio disconnect confirmation
*/
static void aghfpProfile_HandleHfpAudioDisconnectInd(AGHFP_AUDIO_DISCONNECT_IND_T *ind)
{
  aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

  PanicNull(instance);

  DEBUG_LOG("aghfpProfile_HandleHfpAudioDisconnectInd(%p) enum:aghfp_audio_disconnect_status:%d", instance, ind->status);

  switch (ind->status)
  {
  case aghfp_audio_disconnect_success:
      TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_DISCONNECTED_IND);
      instance->sco_sink = NULL;
      break;
  case aghfp_audio_disconnect_in_progress:
      break;
  default:
     break;
  }
}

/*! \brief Handle send call HFP indications confirmation. Note this is
           the HFP indications not a AGHFP library ind
*/
static void aghfpProfile_HandleSendCallIndCfm(AGHFP_SEND_CALL_INDICATOR_CFM_T* cfm)
{
    DEBUG_LOG("AGHFP_SEND_CALL_INDICATOR_CFM enum:aghfp_lib_status:%d", cfm->status);
}

/*! \brief Handle audio connected indications confirmation
*/
static void aghfpProfile_HandleAudioConnectInd(AGHFP_AUDIO_CONNECT_IND_T * message)
{
    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(message->aghfp);

    if (!instance)
    {
        DEBUG_LOG_WARN("aghfpProfile_HandleAudioConnectInd - No instance found");
        return;
    }

    bool accept = FALSE;

    switch (instance->state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        {
            if (!AghfpProfile_IsScoActiveForInstance(instance))
            {
                DEBUG_LOG("aghfpProfile_HandleAudioConnectInd, accepting");
                accept = TRUE;
            }
        }
        break;

        default:
            DEBUG_LOG("aghfpProfile_HandleAudioConnectInd in wrong state, rejecting");
        break;
    }

    AghfpAudioConnectResponse(instance->aghfp, accept, instance->sco_supported_packets ^ sync_all_edr_esco, AghfpProfile_GetAudioParams(instance));
}

/*! \brief Handle unknown AT commands from the HF.
*/
static void aghfpProfile_HandleUnrecognisedAtCommand(AGHFP_UNRECOGNISED_AT_CMD_IND_T* message)
{
    AghfpSendError((message)->aghfp);
}

/*! \brief Handle unknown NREC command from HF.
           Send ERROR unconditionally since we don't support NR/EC at the moment.
*/
static void aghfpProfile_HandleNrecSetupInd(AGHFP_NREC_SETUP_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleNrecSetupInd");
    AghfpSendError((ind)->aghfp);
}

/*! \brief Handle caller id command from HF.
*/
static void aghfpProfile_HandleCallerIdInd(AGHFP_CALLER_ID_SETUP_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallerIdInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleClipInd: No aghfpInstanceTaskData instance for aghfp");
        return;
    }


    instance->bitfields.caller_id_active_remote = ind->enable;
}


/*! \brief Handle dial command from HF.
*/
static void aghfpProfile_HandleDialInd(AGHFP_DIAL_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleDialInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleDialInd: No aghfpInstanceTaskData instance for aghfp");
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    if (state != AGHFP_STATE_CONNECTED_IDLE)
    {
        DEBUG_LOG("aghfpProfile_HandleDialInd: HF attempting to dial while not idle. Current state: enum:aghfpState:%d", state);
        return;
    }

    AghfpProfile_SetLastDialledNumber(ind->size_number, ind->number);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);
}

/*! \brief Handle HF requesting network operator ind
*/
static void aghfpProfile_HandleNetworkOperatorInd(AGHFP_NETWORK_OPERATOR_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleNetworkOperatorInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleNetworkOperatorInd: No aghfpInstanceTaskData instance for aghfp");
        return;
    }

    if (instance->network_operator != NULL)
    {
        AghfpSendNetworkOperator(ind->aghfp, 0, strlen((char*)instance->network_operator), instance->network_operator);
    }
    else
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleNetworkOperatorInd: No network operator available");
    }
}

/*! \brief Handle HF requesting subscriber number.
*/
static void aghfpProfile_HandleSubscriberNumberInd(AGHFP_SUBSCRIBER_NUMBER_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleSubscriberNumberInd");
    AghfpSendSubscriberNumbersComplete(ind->aghfp);
}

/*! \brief Handle AT+CIND message.
*/
static void aghfpProfile_HandleCallIndicationsStatusReqInd(AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallIndicationsStatusReqInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleCallIndicationsStatusReqInd: No aghfpInstanceTaskData instance for aghfp");
        return;
    }

    DEBUG_LOG("Call status: enum:aghfp_call_status:%d", instance->bitfields.call_status);
    DEBUG_LOG("Call status: enum:aghfp_call_setup:%d", instance->bitfields.call_setup);

    AghfpCallIndicatorsStatusResponse(ind->aghfp,
                                      aghfp_service_present,           /* aghfp_service_availability */
                                      instance->bitfields.call_status, /* call status active/not active */
                                      instance->bitfields.call_setup,  /* not in setup, incoming, outgoing... etc */
                                      instance->bitfields.call_hold,   /* aghfp_call_held_status */
                                      5,                               /* Signal level */
                                      aghfp_roam_none,                 /* aghfp_roam_status */
                                      5);                              /* Battery level */
}

/*! \brief Respond to the HF setting up an audio connection
*/
static void aghfpProfile_HandleCallAudioParamsReqInd(void)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallAudioParamsReqInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        AghfpSetAudioParams(instance->aghfp, instance->sco_supported_packets ^ sync_all_edr_esco, AghfpProfile_GetAudioParams(instance));
    }
}

/*! \brief Handle incoming calls in different states
*/
static void aghfpProfile_HandleIncomingInd(aghfpInstanceTaskData *instance)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleIncomingInd");

    aghfpState state = AghfpProfile_GetState(instance);

    if (instance->bitfields.call_setup != aghfp_call_setup_incoming)
    {
        AghfpProfileCallList_AddIncomingCall(instance->call_list);
    }

    if (AGHFP_STATE_CONNECTED_IDLE == state)
    {
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_INCOMING);
    }
    else if (AGHFP_STATE_DISCONNECTED == state)
    {
        /* If we're not connected then update the call_setup
           so it can be transferred on SLC set-up
        */
        instance->bitfields.call_setup = aghfp_call_setup_incoming;
    }
    else if (AGHFP_STATE_CONNECTED_ACTIVE == state)
    {
        instance->bitfields.call_setup = aghfp_call_setup_incoming;
        AghfpSendCallSetupIndicator(instance->aghfp, instance->bitfields.call_setup);
        if (instance->bitfields.caller_id_active_host && instance->bitfields.caller_id_active_remote)
        {
            AghfpSendCallWaitingNotification(instance->aghfp,
                                             instance->clip.clip_type,
                                             instance->clip.size_clip_number,
                                             instance->clip.clip_number,
                                             0,
                                             NULL);
        }
        else
        {
            AghfpSendCallWaitingNotification(instance->aghfp,
                                             0,
                                             0,
                                             0,
                                             0,
                                             NULL);
        }
    }
}

/*! \brief Return a list of all current calls
*/
static void aghfpProfile_HandleGetCurrentCallsInd(AGHFP_CURRENT_CALLS_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleGetCurrentCallsInd");
    call_list_element_t *call;

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG("aghfpProfile_HandleGetCurrentCallsInd: No AGHFP profile instance found");
        AghfpSendError(ind->aghfp);
        return;
    }

    if (ind->last_idx == 0)
    {
        for_each_call(instance->call_list, call)
        {
            DEBUG_LOG_ALWAYS("call->idx %d", call->call.idx);
            AghfpSendCurrentCall(ind->aghfp, &call->call);
        }

        AghfpSendCurrentCallsComplete(ind->aghfp);
    }
}

/*! \brief Handle a request to perform a memory dial from the HF.
*/
static void agfhpProfile_HandleMemoryDialInd(AGHFP_MEMORY_DIAL_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("agfhpProfile_HandleMemoryDialInd size_number %d number %p", ind->size_number, ind->number);

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleDialInd: No aghfpInstanceTaskData instance for aghfp");
        AghfpSendError(ind->aghfp);
        return;
    }

    if (ind->size_number > MAX_CALL_HISTORY || last_dialed_number.number == NULL)
    {
        DEBUG_LOG("agfhpProfile_HandleMemoryDialInd Can not perform memory dial. Call index %d Last dialled number present %d", ind->size_number,
                                                                                           last_dialed_number.number);
        AghfpSendError(ind->aghfp);
        return;
    }

    AghfpSendOk(ind->aghfp);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);
}

/*! \brief Handle a request to perform a memory dial from the HF.
*/
static void agfhpProfile_HandleRedialLastCall(AGHFP_LAST_NUMBER_REDIAL_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("agfhpProfile_HandleRedialLastCall");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForAghfp(ind->aghfp);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleDialInd: No aghfpInstanceTaskData instance for aghfp");
        AghfpSendError(ind->aghfp);
        return;
    }

    if (last_dialed_number.number == NULL)
    {
        DEBUG_LOG("agfhpProfile_HandleRedialLastCall Can not perform memory dial. Last dialled number present %d", (last_dialed_number.number != NULL));
        AghfpSendError(ind->aghfp);
        return;
    }

    AghfpSendOk(ind->aghfp);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);

}

/*! \brief Handles messages into the AGHFP profile
*/
static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("aghfpProfile_TaskMessageHandler MESSAGE:AghfpMessageId:0x%04X", id);

    switch (id)
    {
        case AGHFP_INIT_CFM:
            aghfpProfile_HandleAghfpInitCfm((AGHFP_INIT_CFM_T *)message);
            break;

        case AGHFP_SLC_CONNECT_IND:
            aghfpProfile_HandleHfpSlcConnectInd((AGHFP_SLC_CONNECT_IND_T *)message);
            break;

        case AGHFP_SLC_CONNECT_CFM:
            aghfpProfile_HandleHfpSlcConnectCfm((AGHFP_SLC_CONNECT_CFM_T *)message);
            break;

        case AGHFP_SEND_CALL_INDICATOR_CFM:
            aghfpProfile_HandleSendCallIndCfm((AGHFP_SEND_CALL_INDICATOR_CFM_T*)message);
            break;

        case AGHFP_ANSWER_IND:
            aghfpProfile_HandleCallAnswerInd((AGHFP_ANSWER_IND_T *)message);
            break;

        case AGHFP_CALL_HANG_UP_IND:
            aghfpProfile_HandleCallHangUpInd((AGHFP_CALL_HANG_UP_IND_T *)message);
            break;

        case AGHFP_SLC_DISCONNECT_IND:
            aghfpProfile_HandleSlcDisconnectInd((AGHFP_SLC_DISCONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_IND:
            aghfpProfile_HandleAudioConnectInd((AGHFP_AUDIO_CONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_CFM:
             aghfpProfile_HandleAgHfpAudioConnectCfm((AGHFP_AUDIO_CONNECT_CFM_T *)message);
             break;

        case AGHFP_AUDIO_DISCONNECT_IND:
             aghfpProfile_HandleHfpAudioDisconnectInd((AGHFP_AUDIO_DISCONNECT_IND_T*)message);
             break;

        case AGHFP_UNRECOGNISED_AT_CMD_IND:
            aghfpProfile_HandleUnrecognisedAtCommand((AGHFP_UNRECOGNISED_AT_CMD_IND_T*)message);
            break;

         case AGHFP_NREC_SETUP_IND:
            aghfpProfile_HandleNrecSetupInd((AGHFP_NREC_SETUP_IND_T*)message);
            break;

        case AGHFP_CALLER_ID_SETUP_IND:
            aghfpProfile_HandleCallerIdInd((AGHFP_CALLER_ID_SETUP_IND_T*)message);
            break;

        case AGHFP_DIAL_IND:
            aghfpProfile_HandleDialInd((AGHFP_DIAL_IND_T*)message);
            break;

        case AGHFP_NETWORK_OPERATOR_IND:
            aghfpProfile_HandleNetworkOperatorInd((AGHFP_NETWORK_OPERATOR_IND_T*)message);
            break;

        case AGHFP_SUBSCRIBER_NUMBER_IND:
            aghfpProfile_HandleSubscriberNumberInd((AGHFP_SUBSCRIBER_NUMBER_IND_T*)message);
            break;

        case AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND:
            aghfpProfile_HandleCallIndicationsStatusReqInd((AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T*)message);
            break;

        case AGHFP_APP_AUDIO_PARAMS_REQUIRED_IND:
            aghfpProfile_HandleCallAudioParamsReqInd();
            break;

        case AGHFP_CURRENT_CALLS_IND:
            aghfpProfile_HandleGetCurrentCallsInd((AGHFP_CURRENT_CALLS_IND_T*)message);
            break;

        case AGHFP_MEMORY_DIAL_IND:
            agfhpProfile_HandleMemoryDialInd((AGHFP_MEMORY_DIAL_IND_T*)message);
            break;

        case AGHFP_LAST_NUMBER_REDIAL_IND:
            agfhpProfile_HandleRedialLastCall((AGHFP_LAST_NUMBER_REDIAL_IND_T*)message);
            break;

    default:
        DEBUG_LOG("aghfpProfile_TaskMessageHandler default handler MESSAGE:AghfpMessageId:0x%04X", id);
    }
}

/*! \brief Retrieve the device object using its source
*/
static device_t aghfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

const aghfp_audio_params * AghfpProfile_GetAudioParams(aghfpInstanceTaskData *instance)
{
    static aghfp_audio_params negotiated_audio_params;
    sync_pkt_type packet_type;
    uint8 wbs_codec;
	/* use pre-negotiated audio_params if possible */
    if (AghfpGetNegotiatedAudioParams(instance->aghfp, &packet_type, &negotiated_audio_params) && AghfpCodecHasBeenNegotiated(instance->aghfp, &wbs_codec))
    {
        DEBUG_LOG_INFO("AghfpProfile_GetAudioParams: using negotiated_audio_params");
        return &negotiated_audio_params;
    }
    else
    {
        DEBUG_LOG_INFO("AghfpProfile_GetAudioParams: using default audio_params" );
        return &audio_params;
    }
}

/*! \brief Connect HFP to the HF
*/
void AghfpProfile_Connect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Connect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        instance = AghfpProfileInstance_Create(bd_addr, TRUE);
    }

    /* Check if not already connected */
    if (!AghfpProfile_IsConnectedForInstance(instance))
    {
        /* Store address of AG */
        instance->hf_bd_addr = *bd_addr;

        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CONNECT_REQ);

        /* Send message to HFP task */
        message->addr = *bd_addr;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(bd_addr));
    }
}

/*! \brief Disconnect from the HF
*/
void AghfpProfile_Disconnect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Disconnect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_INFO("AghfpProfile_Disconnect - No instance found");
        return;
    }

    if (!AghfpProfile_IsDisconnected(instance))
    {
        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_DISCONNECT_REQ);

        message->instance = instance;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_DISCONNECT_REQ,
                                 message, AghfpProfileInstance_GetLock(instance));
    }
}

/*! \brief Is HFP connected */
bool AghfpProfile_IsConnectedForInstance(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) >= AGHFP_STATE_CONNECTED_IDLE) && (AghfpProfile_GetState(instance) <= AGHFP_STATE_CONNECTED_ACTIVE));
}

/*! \brief Is HFP connected */
Task AghfpProfile_GetInstanceTask(aghfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &instance->task;
}

/*! \brief Is HFP disconnected */
bool AghfpProfile_IsDisconnected(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) < AGHFP_STATE_CONNECTING_LOCAL) || (AghfpProfile_GetState(instance) > AGHFP_STATE_DISCONNECTING));
}

/*! \brief Is HFP SCO active with the specified HFP instance. */
bool AghfpProfile_IsScoActiveForInstance(aghfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->sco_sink != NULL);
}


bool AghfpProfile_Init(Task init_task)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Init");
    UNUSED(init_task);

    aghfpProfile_InitTaskData();

    aghfpProfile_InitAghfpLibrary();

    ConManagerRegisterConnectionsClient(&aghfp_profile_task_data.task);

    return TRUE;
}

void AghfpProfile_CallIncomingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallIncomingInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        instance = PanicNull(AghfpProfileInstance_Create(bd_addr, TRUE));
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_DISCONNECTED:
            aghfpProfile_HandleIncomingInd(instance);
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

bool AghfpProfile_HoldActiveCall(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_HoldActiveCall");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return FALSE;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_DISCONNECTED:
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_HOLD_CALL_REQ);
            message->instance = instance;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_HOLD_CALL_REQ, message);
        }
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return FALSE;
    }

    return TRUE;
}

bool AghfpProfile_ReleaseHeldCall(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ReleaseHeldCall");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return FALSE;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_DISCONNECTED:
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ);
            message->instance = instance;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ, message);
        }
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return FALSE;
    }

    return TRUE;
}

void AghfpProfile_CallOutgoingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallOutgoingInd Address: nap %#x lap %#x uap %#x", bd_addr->nap,
                                                                                        bd_addr->lap,
                                                                                        bd_addr->uap);

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG("AghfpProfile_CallOutgoingInd - No aghfp instance found");
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("AghfpProfile_CallOutgoingInd: State enum:aghfpState:%d", state);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_OUTGOING);
        break;
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_OutgoingCallAnswered(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_OutgoingCallAnswered");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    AghfpProfileCallList_OutgoingCallAnswered(instance->call_list);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
            return;
        case AGHFP_STATE_CONNECTED_OUTGOING:
            instance->bitfields.call_status = aghfp_call_active;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_EnableInBandRinging(const bdaddr *bd_addr, bool enable)
{
    DEBUG_LOG("AghfpProfile_EnableInBandRinging");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if(!instance)
    {
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
    case AGHFP_STATE_CONNECTED_IDLE:
        instance->bitfields.in_band_ring = enable;
        AghfpInBandRingToneEnable(instance->aghfp, enable);
    default:
        return;
    }
}

void AghfpProfile_SetClipInd(clip_data clip)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_SetClipInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    if (clip.size_clip_number < 3)
    {
        DEBUG_LOG_ERROR("AghfpProfile_SetClipInd: Invalid number size %d", clip.size_clip_number);
        return;
    }

    for_all_aghfp_instances(instance, &iterator)
     {
        if (instance->clip.clip_number == NULL)
        {
            instance->clip.clip_number = malloc(clip.size_clip_number);
        }

        memmove(instance->clip.clip_number, clip.clip_number, clip.size_clip_number);

        instance->clip.clip_type = clip.clip_type;
        instance->clip.size_clip_number = clip.size_clip_number;

        instance->bitfields.caller_id_active_host = TRUE;
    }
}

void AghfpProfile_ClearClipInd(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearClipInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->clip.clip_number != NULL)
        {
            free(instance->clip.clip_number);
        }
        instance->clip.clip_number = NULL;
        instance->clip.clip_type = 0;
        instance->clip.size_clip_number = 0;

        instance->bitfields.caller_id_active_host = FALSE;
    }
}

void AghfpProfile_SetNetworkOperatorInd(char *network_operator)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_SetNetworkOperatorInd");


    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    if (strlen(network_operator) == 0  || strlen(network_operator) > 17)
    {
        DEBUG_LOG_ERROR("AghfpProfile_SetNetworkOperatorInd: Invalid size %d", strlen(network_operator));
        return;
    }

    char network_op_with_quotes[20];
    sprintf(network_op_with_quotes, "\"%s\"\0", network_operator);

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->network_operator == NULL)
        {
            instance->network_operator = malloc(strlen(network_op_with_quotes) + 1);
        }

        memmove(instance->network_operator, network_op_with_quotes, strlen(network_op_with_quotes) + 1);
    }
}

void AghfpProfile_ClearNetworkOperatorInd(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearNetworkOperatorInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->network_operator != NULL)
        {
            free(instance->network_operator);
            instance->network_operator = NULL;
        }
    }
}

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    aghfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = aghfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = AghfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

void AghfpProfile_ClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), task);
}

void AghfpProfile_RegisterStatusClient(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), task);
}

void AghfpProfile_ClearCallHistory(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearCallHistory");

    if (last_dialed_number.number != NULL)
    {
        free(last_dialed_number.number);
        last_dialed_number.number_len = 0;
        last_dialed_number.number = NULL;
    }
}

/*! \brief Update the last number dialled by the HF
*/
void AghfpProfile_SetLastDialledNumber(uint16 length, uint8* number)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_UpdateLastDialledNumber");

    if (last_dialed_number.number != NULL)
    {
        free(last_dialed_number.number);
    }

    last_dialed_number.number = PanicNull(malloc(length));
    memmove(last_dialed_number.number, number, length);
    last_dialed_number.number_len = length;
}
