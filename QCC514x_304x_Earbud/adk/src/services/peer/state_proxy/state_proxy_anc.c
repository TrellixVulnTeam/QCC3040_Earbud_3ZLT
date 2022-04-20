/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_anc.c
\brief      State proxy anc event handling.
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_anc.h"
#include "system_clock.h"

/* framework includes */
#include <peer_signalling.h>

/* system includes */
#include <panic.h>
#include <logging.h>
#include <stdlib.h>

#define stateProxyGetToggleIndex(toggle_way_id) (toggle_way_id - anc_toggle_way_config_id_1)

#define stateProxy_GetMsgId(anc_msg_id) (anc_msg_id - ANC_MESSAGE_BASE)

static size_t stateProxy_GetMsgIdSpecificSize(state_proxy_anc_msg_id_t id)
{
    size_t size = 0;

    switch (id)
    {
        case state_proxy_anc_msg_id_mode:
            size = sizeof(ANC_UPDATE_MODE_CHANGED_IND_T);
            break;

        case state_proxy_anc_msg_id_gain:
            size = sizeof(ANC_UPDATE_GAIN_IND_T);
            break;

        case state_proxy_anc_msg_id_toggle_config:
            size = sizeof(ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T);
            break;

        case state_proxy_anc_msg_id_scenario_config:
            size = sizeof(ANC_SCENARIO_CONFIG_UPDATE_IND_T);
            break;

        case state_proxy_anc_msg_id_reconnection:
            size = sizeof(STATE_PROXY_RECONNECTION_ANC_DATA_T);
            break;

        default:
            size = 0;
            break;
    }

    return size;
}

static void stateProxy_MarshalAncDataToPeer(state_proxy_anc_msg_id_t id, const void* msg)
{
    if(!stateProxy_Paused() && appPeerSigIsConnected())
    {
        void* copy;
        STATE_PROXY_ANC_DATA_T* anc_data = PanicUnlessMalloc(sizeof(STATE_PROXY_ANC_DATA_T));
        marshal_type_t marshal_type = MARSHAL_TYPE(STATE_PROXY_ANC_DATA_T);

        anc_data->msg_id = id;

        /* msg may be NULL for message without payload */
        if(msg)
        {
            size_t msg_size_specific = stateProxy_GetMsgIdSpecificSize(id);
            memcpy(&anc_data->msg, msg, msg_size_specific);
        }

        copy = PanicUnlessMalloc(sizeof(*anc_data));
        memcpy(copy, anc_data, sizeof(*anc_data));
        appPeerSigMarshalledMsgChannelTx(stateProxy_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_STATE_PROXY,
                                         copy, marshal_type);

        free(anc_data);
    }
}

static void stateProxy_UpdateAncState(state_proxy_data_t* state_proxy_data, bool state)
{
    state_proxy_data->flags.anc_state = state;
}

static void stateProxy_UpdateAncMode(state_proxy_data_t* state_proxy_data, uint8 anc_mode)
{
    state_proxy_data->anc_mode = anc_mode;
}

static void stateProxy_UpdateAncLeakthroughGain(state_proxy_data_t* state_proxy_data, uint8 anc_leakthrough_gain)
{
    state_proxy_data->anc_leakthrough_gain = anc_leakthrough_gain;
}

static void stateProxy_UpdateAncToggleConfig(state_proxy_data_t* state_proxy_data, uint8 config_id, uint8 config)
{
    state_proxy_data->toggle_configurations.anc_toggle_way_config[stateProxyGetToggleIndex(config_id)] = config;
}

static void stateProxy_UpdateAncScenarioConfig(state_proxy_data_t* state_proxy_data, uint8 config_id, uint8 config)
{
    switch(config_id)
    {
        case anc_scenario_config_id_standalone:
            state_proxy_data->standalone_config = config;
            break;

        case anc_scenario_config_id_playback:
            state_proxy_data->playback_config = config;
            break;

        case anc_scenario_config_id_sco:
            state_proxy_data->sco_config = config;
            break;

        case anc_scenario_config_id_va:
            state_proxy_data->va_config = config;
            break;
    }
}

static void stateProxy_UpdateAncDemoState(state_proxy_data_t* state_proxy_data, bool state)
{
    state_proxy_data->flags.anc_demo_state = state;
}

static void stateProxy_UpdateAncAdaptivityStatus(state_proxy_data_t* state_proxy_data, bool state)
{
    state_proxy_data->flags.adaptivity_status = state;
}

static void stateProxy_HandleLocalAncStateUpdate(MessageId id)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);

    stateProxy_UpdateAncState(state_proxy_data, (id == ANC_UPDATE_STATE_ENABLE_IND));
}

static void stateProxy_HandleLocalAncModeUpdate(ANC_UPDATE_MODE_CHANGED_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);
    uint8 anc_mode = anc_data->mode;

    stateProxy_UpdateAncMode(state_proxy_data, anc_mode);
}

static void stateProxy_HandleLocalAncGainUpdate(ANC_UPDATE_GAIN_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);
    uint8 anc_gain = anc_data->anc_gain;

    stateProxy_UpdateAncLeakthroughGain(state_proxy_data, anc_gain);
}

static void stateProxy_HandleLocalAncToggleConfigUpdate(ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);
    uint8 config_id = anc_data->anc_toggle_config_id;
    uint8 config = anc_data->anc_config;

    stateProxy_UpdateAncToggleConfig(state_proxy_data, config_id, config);
}

static void stateProxy_HandleLocalScenarioConfigUpdate(ANC_SCENARIO_CONFIG_UPDATE_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);
    uint8 config_id = anc_data->anc_scenario_config_id;
    uint8 config = anc_data->anc_config;

    stateProxy_UpdateAncScenarioConfig(state_proxy_data, config_id, config);
}

static void stateProxy_HandleLocalAncDemoStateUpdate(MessageId id)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);

    stateProxy_UpdateAncDemoState(state_proxy_data, (id == ANC_UPDATE_DEMO_MODE_ENABLE_IND));
}

static void stateProxy_HandleLocalAncAdaptivityStatusUpdate(MessageId id)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_local);

    stateProxy_UpdateAncAdaptivityStatus(state_proxy_data, (id == ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND));
}

static void stateProxy_HandleRemoteAncStateUpdate(state_proxy_anc_msg_id_t id)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);

    stateProxy_UpdateAncState(state_proxy_data, (id == state_proxy_anc_msg_id_enable));
}

static void stateProxy_HandleRemoteAncModeUpdate(ANC_UPDATE_MODE_CHANGED_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    uint8 anc_mode = anc_data->mode;

    stateProxy_UpdateAncMode(state_proxy_data, anc_mode);
}

static void stateProxy_HandleRemoteAncGainUpdate(ANC_UPDATE_GAIN_IND_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    uint8 anc_gain = anc_data->anc_gain;

    stateProxy_UpdateAncLeakthroughGain(state_proxy_data, anc_gain);
}

static void stateProxy_HandleRemoteAncToggleConfigUpdate(STATE_PROXY_ANC_DATA_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    uint8 config_id = anc_data->msg.toggle_config.anc_toggle_config_id;
    uint8 config = anc_data->msg.toggle_config.anc_config;

    stateProxy_UpdateAncToggleConfig(state_proxy_data, config_id, config);
}

static void stateProxy_HandleRemoteScenarioConfigUpdate(STATE_PROXY_ANC_DATA_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    uint8 config_id = anc_data->msg.scenario_config.anc_scenario_config_id;
    uint8 config = anc_data->msg.scenario_config.anc_config;

    stateProxy_UpdateAncScenarioConfig(state_proxy_data, config_id, config);
}

static void stateProxy_HandleRemoteAncDemoStateUpdate(STATE_PROXY_ANC_DATA_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    state_proxy_anc_msg_id_t id = anc_data->msg_id;

    stateProxy_UpdateAncDemoState(state_proxy_data, (id == state_proxy_anc_msg_id_demo_state_enable));
}

static void stateProxy_HandleRemoteAncAdaptivityStatusUpdate(STATE_PROXY_ANC_DATA_T* anc_data)
{
    state_proxy_data_t* state_proxy_data = stateProxy_GetData(state_proxy_source_remote);
    state_proxy_anc_msg_id_t id = anc_data->msg_id;

    stateProxy_UpdateAncAdaptivityStatus(state_proxy_data, (id == state_proxy_anc_msg_id_adaptivity_enable));
}

/*! \brief Get ANC data for initial state message. */
void stateProxy_GetInitialAncData(void)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_GetInitialAncData");
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();

    proxy->local_state->flags.anc_state = AncStateManager_IsEnabled();
    proxy->local_state->anc_mode = AncStateManager_GetMode();
    proxy->local_state->anc_leakthrough_gain = AncStateManager_GetAncGain();

    proxy->local_state->toggle_configurations.anc_toggle_way_config[0] = AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_1);
    proxy->local_state->toggle_configurations.anc_toggle_way_config[1] = AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_2);
    proxy->local_state->toggle_configurations.anc_toggle_way_config[2] = AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_3);

    proxy->local_state->standalone_config = AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_standalone);
    proxy->local_state->playback_config = AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_playback);
    proxy->local_state->sco_config = AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_sco);
    proxy->local_state->va_config = AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_va);

    proxy->local_state->flags.anc_demo_state = AncStateManager_IsDemoStateActive();

    proxy->local_state->flags.adaptivity_status = AncStateManager_GetAdaptiveAncAdaptivity();
}

/*! \brief Handle remote events for ANC data update during reconnect cases. */
void stateProxy_HandleInitialPeerAncData(state_proxy_data_t * new_state)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleInitialPeerAncData");
    state_proxy_task_data_t *proxy = stateProxy_GetTaskData();

    /* Update remote device data if local device is a slave; else ignored */
    if(!StateProxy_IsPrimary())
    {
       STATE_PROXY_ANC_DATA_T anc_msg_data;
       STATE_PROXY_RECONNECTION_ANC_DATA_T anc_data;

       proxy->remote_state->anc_mode = new_state->anc_mode;
       proxy->remote_state->flags.anc_state = new_state->flags.anc_state;
       proxy->remote_state->anc_leakthrough_gain= new_state->anc_leakthrough_gain;
       proxy->remote_state->toggle_configurations = new_state->toggle_configurations;
       proxy->remote_state->standalone_config = new_state->standalone_config;
       proxy->remote_state->playback_config = new_state->playback_config;
       proxy->remote_state->sco_config = new_state->sco_config;
       proxy->remote_state->va_config = new_state->va_config;
       proxy->remote_state->flags.anc_demo_state = new_state->flags.anc_demo_state;
       proxy->remote_state->flags.adaptivity_status = new_state->flags.adaptivity_status;

       anc_data.mode = new_state->anc_mode;
       anc_data.state = new_state->flags.anc_state;
       anc_data.gain = new_state->anc_leakthrough_gain;
       anc_data.toggle_configurations = new_state->toggle_configurations;
       anc_data.standalone_config = new_state->standalone_config;
       anc_data.playback_config = new_state->playback_config;
       anc_data.sco_config = new_state->sco_config;
       anc_data.va_config = new_state->va_config;
       anc_data.anc_demo_state = new_state->flags.anc_demo_state;
       anc_data.adaptivity = new_state->flags.adaptivity_status;

       anc_msg_data.msg_id = state_proxy_anc_msg_id_reconnection;
       anc_msg_data.msg.reconnection_data = anc_data;

       stateProxy_MsgStateProxyEventClients(state_proxy_source_remote,
                                         state_proxy_event_type_anc,
                                         &anc_msg_data);
    }
}

/*! \brief Handle local events for ANC data update. */
void stateProxy_HandleLocalAncUpdate(MessageId id, Message anc_data)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleLocalAncUpdate");

    switch(id)
    {
        case ANC_UPDATE_STATE_DISABLE_IND:
        case ANC_UPDATE_STATE_ENABLE_IND:
            stateProxy_HandleLocalAncStateUpdate(id);
            break;

        case ANC_UPDATE_MODE_CHANGED_IND:
            stateProxy_HandleLocalAncModeUpdate((ANC_UPDATE_MODE_CHANGED_IND_T*)anc_data);
            break;

        case ANC_UPDATE_GAIN_IND:
            stateProxy_HandleLocalAncGainUpdate((ANC_UPDATE_GAIN_IND_T*)anc_data);
            break;

        case ANC_TOGGLE_WAY_CONFIG_UPDATE_IND:
            stateProxy_HandleLocalAncToggleConfigUpdate((ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T*)anc_data);
            break;

        case ANC_SCENARIO_CONFIG_UPDATE_IND:
            stateProxy_HandleLocalScenarioConfigUpdate((ANC_SCENARIO_CONFIG_UPDATE_IND_T*)anc_data);
            break;

        case ANC_UPDATE_DEMO_MODE_DISABLE_IND:
        case ANC_UPDATE_DEMO_MODE_ENABLE_IND:
            stateProxy_HandleLocalAncDemoStateUpdate(id);
            break;

        /* Will be moved to state_proxy_aanc module */
        case ANC_UPDATE_AANC_ADAPTIVITY_PAUSED_IND:
        case ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND:
            stateProxy_HandleLocalAncAdaptivityStatusUpdate(id);
            break;
    }

    stateProxy_MarshalAncDataToPeer(stateProxy_GetMsgId(id), anc_data);
}

/*! \brief Handle remote events for ANC data update. */
void stateProxy_HandleRemoteAncUpdate(const STATE_PROXY_ANC_DATA_T* new_state)
{
    DEBUG_LOG_FN_ENTRY("stateProxy_HandleRemoteAncUpdate");

    switch(new_state->msg_id)
    {
        case state_proxy_anc_msg_id_disable:
        case state_proxy_anc_msg_id_enable:
            stateProxy_HandleRemoteAncStateUpdate(new_state->msg_id);
            break;

        case state_proxy_anc_msg_id_mode:
            stateProxy_HandleRemoteAncModeUpdate(&new_state->msg.mode);
            break;

        case state_proxy_anc_msg_id_gain:
            stateProxy_HandleRemoteAncGainUpdate(&new_state->msg.gain);
            break;

        case state_proxy_anc_msg_id_toggle_config:
            stateProxy_HandleRemoteAncToggleConfigUpdate((STATE_PROXY_ANC_DATA_T*)new_state);
            break;

        case state_proxy_anc_msg_id_scenario_config:
            stateProxy_HandleRemoteScenarioConfigUpdate((STATE_PROXY_ANC_DATA_T*)new_state);
            break;

        case state_proxy_anc_msg_id_demo_state_disable:
        case state_proxy_anc_msg_id_demo_state_enable:
            stateProxy_HandleRemoteAncDemoStateUpdate((STATE_PROXY_ANC_DATA_T*)new_state);
            break;

        /* Will be moved to state_proxy_aanc module */
        case state_proxy_anc_msg_id_adaptivity_disable:
        case state_proxy_anc_msg_id_adaptivity_enable:
            stateProxy_HandleRemoteAncAdaptivityStatusUpdate((STATE_PROXY_ANC_DATA_T*)new_state);
            break;

        default:
            break;

    }

    /* Update peer data to ANC module */
    stateProxy_MsgStateProxyEventClients(state_proxy_source_remote,
                                      state_proxy_event_type_anc,
                                      new_state);

}
