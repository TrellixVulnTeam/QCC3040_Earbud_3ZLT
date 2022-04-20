/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_ui.c
\brief      Intercept UI input messages, add the delay and re-inject locally for 
            synchronisation of UI input messages such as prompts.
*/

#include "peer_ui.h"
#include "peer_ui_typedef.h"
#include "peer_ui_marshal_typedef.h"

#include "logging.h"
#include <av.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <hfp_profile.h>
#include <mirror_profile.h>
#include "peer_signalling.h"
#include "system_clock.h"
#include "ui.h"
#include <ui_indicator_prompts.h>
#include "anc_state_manager.h"
#include "aec_leakthrough.h"
#include "fit_test.h"
#include <phy_state.h>

/* system includes */
#include <stdlib.h>
#include <panic.h>

/*! \brief Peer UI task data structure. */
typedef struct
{
    /*! Peer UI task */
    TaskData task;

} peer_ui_task_data_t;

/*! Instance of the peer Ui. */
peer_ui_task_data_t peer_ui;

#define peerUi_GetTaskData()        (&peer_ui)
#define peerUi_GetTask()            (&peer_ui.task)

/*! The default UI interceptor function */
static inject_ui_input ui_func_ptr_to_return = NULL;

static bool peerUi_IsSynchronisedAudioIndication(ui_indication_type_t ind_type)
{
    return ind_type == ui_indication_type_audio_prompt || ind_type == ui_indication_type_audio_tone;
}

/*! \brief Sends the message to secondary earbud. */
static uint32 peerUi_ForwardUiEventToSecondary(ui_indication_type_t ind_type, uint16 ind_index, uint32 time_to_play)
{
    marshal_rtime_t updated_ttp = time_to_play;
    peer_ui_event_t* msg = PanicUnlessMalloc(sizeof(peer_ui_event_t));

    if (peerUi_IsSynchronisedAudioIndication(ind_type))
    {
        uint32 peerRelayDelay_usecs = appPeerGetPeerRelayDelayBasedOnSystemContext();
        updated_ttp = rtime_add(time_to_play, peerRelayDelay_usecs);
    }

    msg->indication_type = ind_type;
    msg->indication_index = ind_index;
    msg->timestamp = updated_ttp;

    DEBUG_LOG("peerUi_ForwardUiEventToSecondary ind type=%d index=%d timestamp=%d us", ind_type, ind_index, updated_ttp);
    appPeerSigMarshalledMsgChannelTx(peerUi_GetTask(),
                                     PEER_SIG_MSG_CHANNEL_PEER_UI,
                                     msg, MARSHAL_TYPE_peer_ui_event_t);

    return updated_ttp;
}

/***********************************
 * Marshalled Message TX CFM and RX
 **********************************/

/*! \brief Handle confirmation of transmission of a marshalled message. */
static void peerUi_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    peerSigStatus status = cfm->status;
    
    if (peerSigStatusSuccess != status)
    {
        DEBUG_LOG("peerUi_HandleMarshalledMsgChannelTxCfm reports failure status 0x%x(%d)",status,status);
    }
}

static void peerUi_InjectUiEvent(peer_ui_event_t* rcvd)
{
    PanicNull(rcvd);

    ui_indication_type_t ind_type = rcvd->indication_type;
    uint16 ind_index = rcvd->indication_index;
    marshal_rtime_t timestamp = rcvd->timestamp;
    int32 time_left_usecs = 0;

    if (peerUi_IsSynchronisedAudioIndication(ind_type))
    {
        rtime_t now = SystemClockGetTimerTime();

        time_left_usecs = rtime_sub(timestamp, now);

        DEBUG_LOG("peerUi_InjectUiEvent now=%u, ttp=%u, time_left=%d", now, timestamp, time_left_usecs);
    }

    /* Notify UI component of the forwarded UI Event, we only notify when the Secondary has
    time left to handle the event, otherwise there is no hope of synchronisation and we shall
    not play a badly synchronised indication to the user. */
    if(rtime_gt(time_left_usecs, UI_SYNC_IND_AUDIO_SS_FIXED_DELAY) ||
       !peerUi_IsSynchronisedAudioIndication(ind_type))
    {
        Ui_NotifyUiEvent(ind_type, ind_index, timestamp);
    }
}

static void peerUi_UpdateUiInputData(ui_input_t ui_input, uint8 data)
{

    if(ui_input == ui_input_anc_set_leakthrough_gain)
    {
        AncStateManager_StoreAncLeakthroughGain(data);
    }

    if(ui_input == ui_input_fit_test_remote_result_ready)
    {
        FitTest_StoreRemotePeerResults(data);
    }
}

static void peerUi_HandleUiInputRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{

    DEBUG_LOG("peerUi_HandleUiInputRxInd");

    /* received message at secondary EB */
    peer_ui_input_t* rcvd = (peer_ui_input_t*)ind->msg;

    PanicNull(rcvd);
    ui_input_t ui_input = rcvd->ui_input;
    marshal_rtime_t timestamp = rcvd->timestamp;
    int8 data = rcvd->data;

    /* difference between timestamp (sent by primary by when to handle the UI input) and
    actual system time when UI input is received by secondary */
    int32 delta;

    /* system time when message received by secondary earbud */
    rtime_t now = SystemClockGetTimerTime();

    /* time left for secondary to handle the ANC UI input */
    delta = rtime_sub(timestamp, now);

    /* Inject UI input to peer with the time left, so it can handle UI input,
    we only UI input if peer earbud is in OutOfCase*/

    if (!appPhyStateIsOutOfCase())
        return;

    peerUi_UpdateUiInputData(ui_input, data);

    if(rtime_gt(delta, 0))
    {
        DEBUG_LOG("peerUi_HandleUiInputRxInd send ui_input(0x%x) in %d ms", ui_input, US_TO_MS(delta));
        Ui_InjectUiInputWithDelay(ui_input, US_TO_MS(delta));
    }
    else
    {
        DEBUG_LOG("peerUi_HandleUiInputRxInd send ui_input(0x%x) in %d ms", ui_input, US_TO_MS(delta));
        Ui_InjectUiInputWithDelay(ui_input, 1);
    }
}

static void peerUi_InjectUiInput(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    peer_ui_input_t* rcvd = (peer_ui_input_t*)ind->msg;

    PanicNull(rcvd);

    ui_input_t ui_input = rcvd->ui_input;

    if (ID_IN_MSG_GRP(UI_INPUTS_AUDIO_CURATION, ui_input))
    {
        DEBUG_LOG("UI_INPUTS_AUDIO_CURATION_MESSAGE_GROUP");
        peerUi_HandleUiInputRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)ind);
    }
}

/*! \brief Handle incoming marshalled messages.*/
static void peerUi_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    PanicNull(ind);

    DEBUG_LOG("peerUi_HandleMarshalledMsgChannelRxInd channel %u type %u", ind->channel, ind->type);
    switch (ind->type)
    {
    case MARSHAL_TYPE_peer_ui_input_t:
        peerUi_InjectUiInput(ind);
        break;

    case MARSHAL_TYPE_peer_ui_event_t:
        peerUi_InjectUiEvent((peer_ui_event_t*)ind->msg);
        break;

    default:
        /* Do not expect any other messages*/
        Panic();
        break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

static uint32 peerUi_ForwardToPeer(ui_indication_type_t type, uint16 index, uint32 time_to_play)
{
    if (appPeerSigIsConnected())
    {
        /* Peer connection exists so incorporate a transmission delay in the time_to_play and forward it to the Peer device. */
        time_to_play = peerUi_ForwardUiEventToSecondary(type, index, time_to_play);
    }
    return time_to_play;
}

#if (defined(ENABLE_ANC) || defined(ENABLE_AEC_LEAKTHROUGH)) || defined(ENABLE_EARBUD_FIT_TEST)
static void peerUi_ForwardUiInputWithDataToPeer(ui_input_t ui_input, uint32 delay, uint8 data)
{
    peer_ui_input_t* msg = PanicUnlessMalloc(sizeof(peer_ui_input_t));

    /* get the current system time */
    rtime_t now = SystemClockGetTimerTime();

    /* add the delay in msec which is sent to peer device, this is the time when we want
     * primary and secondary to handle the UI input at the same time */
    marshal_rtime_t timestamp = rtime_add(now, delay);
    msg->ui_input = ui_input;
    msg->timestamp = timestamp;
    msg->data = data;

    DEBUG_LOG("peerUi_ForwardUiInputWithDataToPeer send ui_input (0x%x) with timestamp %d us", ui_input, timestamp);
    appPeerSigMarshalledMsgChannelTx(peerUi_GetTask(), PEER_SIG_MSG_CHANNEL_PEER_UI, msg, MARSHAL_TYPE_peer_ui_input_t);
}

static void peerUi_ForwardUiInputToPeer(ui_input_t ui_input, uint32 delay)
{
    peerUi_ForwardUiInputWithDataToPeer(ui_input, delay, 0U);
}
#endif

#ifdef ENABLE_EARBUD_FIT_TEST
static uint32 peerUi_ForwardFitTestUiInputToPeer(ui_input_t ui_input)
{
    uint32 delay = PEER_UI_PROMPT_DELAY_MS;

    switch(ui_input)
    {
        case ui_input_fit_test_prepare_test:
        case ui_input_fit_test_start:
        case ui_input_fit_test_abort:
        case ui_input_fit_test_disable:
            peerUi_ForwardUiInputToPeer(ui_input,PEER_UI_PROMPT_DELAY_US);
        break;

        case ui_input_fit_test_remote_result_ready:
            peerUi_ForwardUiInputWithDataToPeer(ui_input, PEER_UI_PROMPT_DELAY_US, FitTest_GetLocalDeviceTestResult());
        break;

        default:
        break;
    }

    return delay;
}
#endif

#ifdef ENABLE_ANC
static uint32 peerUi_ForwardAncUiInputToPeer(ui_input_t ui_input)
{
    uint32 delay = PEER_ANC_UI_INPUT_DELAY_MS;

    switch(ui_input)
    {
        case ui_input_anc_on:
        case ui_input_anc_off:
        case ui_input_anc_set_mode_1:
        case ui_input_anc_set_mode_2:
        case ui_input_anc_set_mode_3:
        case ui_input_anc_set_mode_4:
        case ui_input_anc_set_mode_5:
        case ui_input_anc_set_mode_6:
        case ui_input_anc_set_mode_7:
        case ui_input_anc_set_mode_8:
        case ui_input_anc_set_mode_9:
        case ui_input_anc_set_mode_10:
        case ui_input_anc_set_next_mode:
            peerUi_ForwardUiInputToPeer(ui_input, PEER_ANC_UI_INPUT_DELAY_US);
            break;

        case ui_input_anc_toggle_on_off:
            peerUi_ForwardUiInputToPeer(AncStateManager_IsEnabled() ? ui_input_anc_off : ui_input_anc_on, PEER_ANC_UI_INPUT_DELAY_US);
            break;

        case ui_input_anc_set_leakthrough_gain:
            peerUi_ForwardUiInputWithDataToPeer(ui_input, PEER_ANC_UI_INPUT_DELAY_US, AncStateManager_GetAncGain());
            break;

        case ui_input_anc_adaptivity_toggle_on_off:
            peerUi_ForwardUiInputToPeer(ui_input, PEER_ANC_UI_INPUT_DELAY_US);
            break;

        case ui_input_anc_toggle_way:
            peerUi_ForwardUiInputToPeer(ui_input, PEER_ANC_UI_INPUT_DELAY_US);
            break;

        default:
            break;
    }

    return delay;
}
#endif

#ifdef ENABLE_AEC_LEAKTHROUGH
static uint32 peerUi_ForwardLeakthroughUiInputToPeer(ui_input_t ui_input)
{
    uint32 delay = PEER_LEAKTHROUGH_UI_INPUT_DELAY_MS;

    switch(ui_input)
    {
        case ui_input_leakthrough_on:
        case ui_input_leakthrough_off:
        case ui_input_leakthrough_toggle_on_off:
        case ui_input_leakthrough_set_mode_1:
        case ui_input_leakthrough_set_mode_2:
        case ui_input_leakthrough_set_mode_3:
        case ui_input_leakthrough_set_next_mode:
            peerUi_ForwardUiInputToPeer(ui_input, PEER_LEAKTHROUGH_UI_INPUT_DELAY_US);
            break;

        default:
            break;
    }

    return delay;
}
#endif


/*! brief Interceptor call back function called by UI module on reception of UI input messages */
static void peerUi_Interceptor_FuncPtr(ui_input_t ui_input, uint32 delay)
{
    if(appPeerSigIsConnected())
    {

#ifdef ENABLE_ANC
        if((ui_input >= ui_input_anc_on) && (ui_input <= ui_input_anc_toggle_diagnostic))
            delay = peerUi_ForwardAncUiInputToPeer(ui_input);
#else
        DEBUG_LOG("peerUi_Interceptor_FuncPtr, ANC is not included in the build");
#endif

#ifdef ENABLE_AEC_LEAKTHROUGH
        if((ui_input >= ui_input_leakthrough_on) && (ui_input <= ui_input_leakthrough_set_next_mode))
            delay = peerUi_ForwardLeakthroughUiInputToPeer(ui_input);
#else
        DEBUG_LOG("peerUi_Interceptor_FuncPtr, AEC leakthrough is not included in the build");
#endif

#ifdef ENABLE_EARBUD_FIT_TEST
        if((ui_input >= ui_input_fit_test_prepare_test) && (ui_input <= ui_input_fit_test_disable))
             delay = peerUi_ForwardFitTestUiInputToPeer(ui_input);
#else
        DEBUG_LOG("peerUi_Interceptor_FuncPtr, Fit test is not included in the build");
#endif

        DEBUG_LOG("peerUi_Interceptor_FuncPtr, LEA Unicast is not included in the build");
    }

    /* pass ui_input back to UI module */
    ui_func_ptr_to_return(ui_input, delay);
}

/*! brief Register the peer_ui interceptor function pointer with UI module
    to receive all the ui_input messages */
static void peerUi_Register_Interceptor_Func(void)
{
    /* original UI function pointer received */
    ui_func_ptr_to_return = Ui_RegisterUiInputsInterceptor(peerUi_Interceptor_FuncPtr);
}

/*! Peer UI Message Handler. */
static void peerUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            peerUi_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            peerUi_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        default:
            DEBUG_LOG("peerUi_HandleMessage unhandled message id MESSAGE:0x%x", id);
            break;
    }
}

/*! brief Initialise Peer Ui  module */
bool PeerUi_Init(Task init_task)
{
    DEBUG_LOG("PeerUi_Init");
    peer_ui_task_data_t *theTaskData = peerUi_GetTaskData();
    
    theTaskData->task.handler = peerUi_HandleMessage;

    /* Register with peer signalling to use the peer UI msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(peerUi_GetTask(), 
                                               PEER_SIG_MSG_CHANNEL_PEER_UI,
                                               peer_ui_marshal_type_descriptors,
                                               NUMBER_OF_PEER_UI_MARSHAL_TYPES);

    /* get notification of peer signalling availability to send ui_input messages to peer */
    appPeerSigClientRegister(peerUi_GetTask()); 

    /* register the UI event sniffer function pointer with UI module */
    Ui_RegisterUiEventSniffer(peerUi_ForwardToPeer);

    /* register the peer_ui interceptor function pointer with UI module
    to receive all the ui_inputs messages */
    peerUi_Register_Interceptor_Func();

    UNUSED(init_task);  
    return TRUE;   
}
