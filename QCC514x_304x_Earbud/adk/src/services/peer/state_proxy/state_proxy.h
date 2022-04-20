/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\defgroup   state_proxy State Proxy
\ingroup    peer_service
\brief      A component providing local and remote state.
*/

#ifndef STATE_PROXY_H
#define STATE_PROXY_H

#include <phy_state.h>
#include <av.h>
#include <hfp_profile.h>
#include <pairing.h>
#include <connection_manager.h>
#include <anc_state_manager.h>
#include <aec_leakthrough.h>
#include <battery_region.h>

/*! Types of device for which state changes are monitored. */
typedef enum
{
    /*! This device. */
    state_proxy_source_local,

    /*! Remote device for which state is being proxied. */
    state_proxy_source_remote,
} state_proxy_source;

/*! Link Quality Message. */
typedef struct
{
    /*! Received Signal Strength Indication in dB.
     *  Range -128..127. */
    int8 rssi;

    /*! Measure of the quality of the connection.
     *  Range 0(worst)..65535(best). */
    uint16 link_quality;

    /*! BT address of the remote device on the connection. */
    tp_bdaddr device;

} STATE_PROXY_LINK_QUALITY_T;

/*! Enumeration of event types supported by state proxy. */
typedef enum
{
    state_proxy_event_type_phystate             = 1UL << 0,
    state_proxy_event_type_is_pairing           = 1UL << 1,
    state_proxy_event_type_battery_state        = 1UL << 2,
    state_proxy_event_type_battery_voltage      = 1UL << 3,
    state_proxy_event_type_pairing              = 1UL << 4,
    /* State proxy will only measure and report link quality / RSSI for the mirrored handset */
    state_proxy_event_type_link_quality         = 1UL << 5,
    state_proxy_event_type_mic_quality          = 1UL << 6,
    state_proxy_event_type_anc                  = 1UL << 7,
    state_proxy_event_type_leakthrough          = 1UL << 8,
    state_proxy_event_type_aanc                 = 1UL << 9,
    state_proxy_event_type_aanc_logging         = 1UL << 10,
} state_proxy_event_type;

/*\{*/

/*! Messages sent by the state proxy component to clients. */
enum state_proxy_messages
{
    /*! Event notification of change in state of a monitored device. */
    STATE_PROXY_EVENT = STATE_PROXY_MESSAGE_BASE,

    /*! Notification that state_proxy_initial_state_t message transmitted. */
    STATE_PROXY_EVENT_INITIAL_STATE_SENT,

    /*! Notification that state_proxy_initial_state_t message has been received. */
    STATE_PROXY_EVENT_INITIAL_STATE_RECEIVED,
};

/*! Value indicating microphone quality is unavailable i.e. SCO inactive. */
#define MIC_QUALITY_UNAVAILABLE 0xFF

/*! Definition of data for state_proxy_event_type_mic_quality events.
 */
typedef struct
{
    /*! Current microphone quality level.
     *  Valid range 0(worst)..15(best).
     *  Value of MIC_QUALITY_UNAVAILABLE indicates no microphone quality data. */
    uint8 mic_quality;
} STATE_PROXY_MIC_QUALITY_T;

/*! \brief Events sent by state proxy to ANC module. */
typedef enum
{
    /* These should be inline with anc_msg_t ids(except reconnection id)*/
    state_proxy_anc_msg_id_disable = 0,
    state_proxy_anc_msg_id_enable,
    state_proxy_anc_msg_id_mode,
    state_proxy_anc_msg_id_gain,
    state_proxy_anc_msg_id_toggle_config,
    state_proxy_anc_msg_id_scenario_config,
    state_proxy_anc_msg_id_demo_state_disable,
    state_proxy_anc_msg_id_demo_state_enable,
    state_proxy_anc_msg_id_adaptivity_disable,
    state_proxy_anc_msg_id_adaptivity_enable,

    /* This has to be the last id */
    state_proxy_anc_msg_id_reconnection
} state_proxy_anc_msg_id_t;


/*! Definition of data to be sent upon reconnection to ANC module.
 */
typedef struct
{
    bool state;
    uint8 mode;
    uint8 gain;
    anc_toggle_way_config_t toggle_configurations;
    uint16 standalone_config;
    uint16 playback_config;
    uint16 sco_config;
    uint16 va_config;
    bool anc_demo_state;
    bool adaptivity;
} STATE_PROXY_RECONNECTION_ANC_DATA_T;

/*! Definition of data for state_proxy_event_type_anc events.
 */
typedef struct
{
    /*! Type of ANC update. */
    state_proxy_anc_msg_id_t msg_id;

    /*! Payload of the ANC update message.
     *  Note that some msg types may not have a payload. */
    union
    {
        ANC_UPDATE_MODE_CHANGED_IND_T mode;
        ANC_UPDATE_GAIN_IND_T gain;
        ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T toggle_config;
        ANC_SCENARIO_CONFIG_UPDATE_IND_T scenario_config;
        STATE_PROXY_RECONNECTION_ANC_DATA_T reconnection_data;
    } msg;
} STATE_PROXY_ANC_DATA_T;

/*! Definition of data for state_proxy_event_type_leakthrough events. */
typedef leakthrough_sync_data_t STATE_PROXY_LEAKTHROUGH_DATA_T;

/*! Definition of data for state_proxy_event_type_aanc_logging events. */
typedef AANC_LOGGING_T STATE_PROXY_AANC_LOGGING_T;

typedef struct
{
    bool aanc_quiet_mode_detected;
    bool aanc_quiet_mode_enabled;
    bool aanc_quiet_mode_enable_requested;
    bool aanc_quiet_mode_disable_requested;
    marshal_rtime_t timestamp;
} STATE_PROXY_AANC_DATA_T;

/*! Definition of message notifying clients of change in specific state. */
typedef struct
{
    /*! Source of the state change. */
    state_proxy_source source;

    /*! Type of the state change. */
    state_proxy_event_type type;

    /*! System clock time that the event was generated in ms. */
    uint32 timestamp;

    /*! Payload of the state change message.
     *  Note that some event types may not have a payload. */
    union
    {
        PHY_STATE_CHANGED_IND_T phystate;
        PAIRING_ACTIVITY_T handset_activity;
        STATE_PROXY_LINK_QUALITY_T link_quality;
        STATE_PROXY_MIC_QUALITY_T mic_quality;
        STATE_PROXY_ANC_DATA_T anc_data;
        STATE_PROXY_AANC_DATA_T aanc_data;
        STATE_PROXY_LEAKTHROUGH_DATA_T leakthrough_data;
        STATE_PROXY_AANC_LOGGING_T aanc_logging;
    } event;
} STATE_PROXY_EVENT_T;


/*! \brief Initialise the State Proxy component.

    \param[in] init_task Task of the initialisation component.

    \return bool TRUE Initialisation successful
                 FALSE Initialisation failed
*/
bool StateProxy_Init(Task init_task);

/*! \brief Register a task for event(s) updates.
 
    Register a client task to receive updates for changes in specific event types.

    \param[in] client_task Task to register for #STATE_PROXY_EVENT_T messages. 
    \param[in] event_mask Mask of state_proxy_event_type events to register.
*/
void StateProxy_EventRegisterClient(Task client_task, state_proxy_event_type event_mask);

/*! \brief Unregister event(s) updates for the specified task.
 
    \param[in] client_task Task to unregister from further #STATE_PROXY_EVENT_T message for event_mask events. 
    \param[in] event_mask Mask of events types to unregister.
*/
void StateProxy_EventUnregisterClient(Task client_task, state_proxy_event_type event_mask);

/*! \brief Register for events concerning state proxy itself. */
void StateProxy_StateProxyEventRegisterClient(Task client_task);

/*! \brief Send current device state to peer to initialise event baseline.
*/
void StateProxy_SendInitialState(void);

/*! \brief Inform state proxy of current device Primary/Secondary role.
    \param primary TRUE primary role, FALSE secondary role.
*/
void StateProxy_SetRole(bool primary);

/*! \brief Prevent State Proxy from forwarding any events to peer.
    \note A Call to StateProxy_SendInitialState restarts forwarding.
*/
void StateProxy_Stop(void);

/*! \brief Has initial state been received from peer.
    \return TRUE if initial state received, otherwise FALSE.
*/
bool StateProxy_InitialStateReceived(void);

/* Peer state access functions */
bool StateProxy_IsPeerInCase(void);
bool StateProxy_IsPeerOutOfCase(void);
bool StateProxy_IsPeerInEar(void);
bool StateProxy_IsPeerOutOfEar(void);
bool StateProxy_IsPeerPairing(void);
bool StateProxy_HasPeerHandsetPairing(void);

/* Local state access functions */
bool StateProxy_IsInCase(void);
bool StateProxy_IsOutOfCase(void);
bool StateProxy_IsInEar(void);
bool StateProxy_IsOutOfEar(void);
bool StateProxy_IsPairing(void);
bool StateProxy_HasHandsetPairing(void);

void StateProxy_GetLocalAndRemoteBatteryLevels(uint16 *battery_level, uint16 *peer_battery_level);
void StateProxy_GetLocalAndRemoteBatteryStates(battery_region_state_t *battery_state, 
                                               battery_region_state_t *peer_battery_state);
bool StateProxy_IsPrimary(void);
bool StateProxy_GetPeerAncState(void);
uint8 StateProxy_GetPeerAncMode(void);
bool StateProxy_GetPeerLeakthroughState(void);
uint8 StateProxy_GetPeerLeakthroughMode(void);
bool StateProxy_GetPeerQuietModeDetectedState(void);
bool StateProxy_GetPeerQuietModeEnabledState(void);
#endif /* STATE_PROXY_H */
