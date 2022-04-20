/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_aanc.c
\brief      State proxy aanc event handling.
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_aanc.h"
#include "system_clock.h"
#include "aanc_quiet_mode.h"
#include "anc_state_manager.h"

/* framework includes */
#include <peer_signalling.h>

/* system includes */
#include <logging.h>

/*! \brief Get ANC data for initial state message. */
void stateProxy_GetInitialAancData(void)
{
     DEBUG_LOG_FN_ENTRY("stateProxy_GetInitialAancData");
     state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
     state_proxy_data_flags_t *local_device_proxy_flags =stateProxy_GetLocalFlags();
     local_device_proxy_flags->aanc_quiet_mode_detected=AancQuietMode_IsQuietModeDetected();
     local_device_proxy_flags->aanc_quiet_mode_enabled=AancQuietMode_IsQuietModeEnabled();
     local_device_proxy_flags->aanc_quiet_mode_enable_requested= AancQuietMode_IsQuietModeEnableRequested();
     local_device_proxy_flags->aanc_quiet_mode_disable_requested=AancQuietMode_IsQuietModeDisableRequested();
     proxy->local_state->timestamp=AancQuietMode_GetTimestamp();
}

/*! \brief Handle remote events for AANC data update during reconnect cases. */
void stateProxy_HandleInitialPeerAancData(state_proxy_data_t * new_state)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleInitialPeerAancData");
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();

    /* Update remote device data if local device is a slave; else ignored */
    if(!StateProxy_IsPrimary())
    {
        STATE_PROXY_AANC_DATA_T aanc_data;
        state_proxy_data_flags_t *remote_device_proxy_flags =stateProxy_GetRemoteFlags();
        remote_device_proxy_flags->aanc_quiet_mode_detected = new_state->flags.aanc_quiet_mode_detected;
        remote_device_proxy_flags->aanc_quiet_mode_enabled = new_state->flags.aanc_quiet_mode_enabled;
        remote_device_proxy_flags->aanc_quiet_mode_enable_requested=new_state->flags.aanc_quiet_mode_enable_requested;
        remote_device_proxy_flags->aanc_quiet_mode_disable_requested=new_state->flags.aanc_quiet_mode_disable_requested;
        proxy->remote_state->timestamp=new_state->timestamp;

        aanc_data.aanc_quiet_mode_detected=new_state->flags.aanc_quiet_mode_detected;
        aanc_data.aanc_quiet_mode_enabled=new_state->flags.aanc_quiet_mode_enabled;
        aanc_data.aanc_quiet_mode_enable_requested=new_state->flags.aanc_quiet_mode_enable_requested;
        aanc_data.aanc_quiet_mode_disable_requested=new_state->flags.aanc_quiet_mode_disable_requested;
        aanc_data.timestamp=new_state->timestamp;

        stateProxy_MsgStateProxyEventClients(state_proxy_source_remote,
                                          state_proxy_event_type_aanc,
                                          &aanc_data);
    }
}

static void stateProxy_HandleAncQuietModeUpdateInd(const AANC_UPDATE_QUIETMODE_IND_T* aanc_data)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleAncQuietModeUpdateInd");

    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    state_proxy_data_flags_t *local_device_proxy_flags =stateProxy_GetLocalFlags();
    local_device_proxy_flags->aanc_quiet_mode_detected = aanc_data->aanc_quiet_mode_detected;
    local_device_proxy_flags->aanc_quiet_mode_enabled = aanc_data->aanc_quiet_mode_enabled;
    local_device_proxy_flags->aanc_quiet_mode_enable_requested=aanc_data->aanc_quiet_mode_enable_requested;
    local_device_proxy_flags->aanc_quiet_mode_disable_requested=aanc_data->aanc_quiet_mode_disable_requested;
    proxy->local_state->timestamp=aanc_data->timestamp;

}

static void stateProxy_UpdatePeerDevice(void)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_UpdatePeerDevice");

    if(!stateProxy_Paused() && appPeerSigIsConnected())
    {
        void* copy;
        state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
        state_proxy_data_flags_t *local_device_proxy_flags =stateProxy_GetLocalFlags();
        STATE_PROXY_AANC_DATA_T* aanc_data = PanicUnlessMalloc(sizeof(STATE_PROXY_AANC_DATA_T));
        marshal_type_t marshal_type = MARSHAL_TYPE(STATE_PROXY_AANC_DATA_T);

        aanc_data->aanc_quiet_mode_detected = local_device_proxy_flags->aanc_quiet_mode_detected;
        aanc_data->aanc_quiet_mode_enabled = local_device_proxy_flags->aanc_quiet_mode_enabled;
        aanc_data->aanc_quiet_mode_disable_requested = local_device_proxy_flags->aanc_quiet_mode_disable_requested;
        aanc_data->aanc_quiet_mode_enable_requested = local_device_proxy_flags->aanc_quiet_mode_enable_requested;
        aanc_data->timestamp=proxy->local_state->timestamp;

        /* Cancel any pending messages of this type - its more important to send
        the latest state, so cancel any pending messages. */
        appPeerSigMarshalledMsgChannelTxCancelAll(stateProxy_GetTask(),
                                                  PEER_SIG_MSG_CHANNEL_STATE_PROXY,
                                                  marshal_type);
        copy = PanicUnlessMalloc(sizeof(*aanc_data));
        memcpy(copy, aanc_data, sizeof(*aanc_data));
        appPeerSigMarshalledMsgChannelTx(stateProxy_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_STATE_PROXY,
                                         copy, marshal_type);
        free(aanc_data);
    }
}

/*! \brief Handle local events for AANC data update. */

void stateProxy_HandleAancUpdateInd(MessageId id, Message aanc_data)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleAancUpdateInd");

    switch(id)
    {
        case AANC_UPDATE_QUIETMODE_IND:
             stateProxy_HandleAncQuietModeUpdateInd((const AANC_UPDATE_QUIETMODE_IND_T*)aanc_data);
        break;

        default:
        break;

    }
    stateProxy_UpdatePeerDevice();
}

/*! \brief Handle remote events for AANC data update. */
void stateProxy_HandleRemoteAancUpdate(const STATE_PROXY_AANC_DATA_T* new_state)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleRemoteAancUpdate");

    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    state_proxy_data_flags_t *remote_device_proxy_flags =stateProxy_GetRemoteFlags();
    STATE_PROXY_AANC_DATA_T aanc_data;

    remote_device_proxy_flags->aanc_quiet_mode_detected=new_state->aanc_quiet_mode_detected;
    remote_device_proxy_flags->aanc_quiet_mode_enabled=new_state->aanc_quiet_mode_enabled;
    remote_device_proxy_flags->aanc_quiet_mode_enable_requested=new_state->aanc_quiet_mode_enable_requested;
    remote_device_proxy_flags->aanc_quiet_mode_disable_requested=new_state->aanc_quiet_mode_disable_requested;
    proxy->remote_state->timestamp=new_state->timestamp;

    aanc_data.aanc_quiet_mode_detected=new_state->aanc_quiet_mode_detected;
    aanc_data.aanc_quiet_mode_enabled=new_state->aanc_quiet_mode_enabled;
    aanc_data.aanc_quiet_mode_enable_requested=new_state->aanc_quiet_mode_enable_requested;
    aanc_data.aanc_quiet_mode_disable_requested=new_state->aanc_quiet_mode_disable_requested;
    aanc_data.timestamp=new_state->timestamp;

    stateProxy_MsgStateProxyEventClients(state_proxy_source_remote,
                                      state_proxy_event_type_aanc,
                                      &aanc_data);

}

/*! \brief Handle local events for AANC FF gain update. */
static void stateProxy_HandleAancFFGainUpdateInd(const STATE_PROXY_AANC_LOGGING_T* aanc_data)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    proxy->local_state->aanc_ff_gain = aanc_data->aanc_ff_gain;

    if(!stateProxy_Paused() && appPeerSigIsConnected())
    {
        void* copy;
        marshal_type_t marshal_type = MARSHAL_TYPE(STATE_PROXY_AANC_LOGGING_T);

        /* Cancel any pending messages of this type - its more important to send
        the latest state, so cancel any pending messages. */
        appPeerSigMarshalledMsgChannelTxCancelAll(stateProxy_GetTask(),
                                                  PEER_SIG_MSG_CHANNEL_STATE_PROXY,
                                                  marshal_type);

        copy = PanicUnlessMalloc(sizeof(*aanc_data));
        memcpy(copy, aanc_data, sizeof(*aanc_data));

        appPeerSigMarshalledMsgChannelTx(stateProxy_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_STATE_PROXY,
                                         copy, marshal_type);
    }
}

/*! \brief Handle local events for AANC logging update. */

void stateProxy_HandleAancLoggingUpdate(MessageId id, Message aanc_data)
{
    switch(id)
    {
        case AANC_FF_GAIN_UPDATE_IND:
             stateProxy_HandleAancFFGainUpdateInd((const STATE_PROXY_AANC_LOGGING_T*)aanc_data);
        break;

        default:
        break;

    }
}

/*! \brief Handle remote events for AANC logging update.
        Updates ANC SM with remote device logging data if local device is primary*/
void stateProxy_HandleRemoteAancLoggingUpdate(const STATE_PROXY_AANC_LOGGING_T* remote_aanc_data)
{
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();
    proxy->remote_state->aanc_ff_gain = remote_aanc_data->aanc_ff_gain;

    /* Update remote device data if local device is a primary; else ignored */
    if(StateProxy_IsPrimary())
    {
       STATE_PROXY_AANC_LOGGING_T aanc_logging;

       aanc_logging.aanc_ff_gain = remote_aanc_data->aanc_ff_gain;

       /* Updating ANC SM with secondary FF gain*/
        stateProxy_MsgStateProxyEventClients(state_proxy_source_remote,
                                             state_proxy_event_type_aanc_logging,
                                             &aanc_logging);
    }
}
