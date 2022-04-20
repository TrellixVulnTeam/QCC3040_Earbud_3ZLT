/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy.h
\brief      Internal header for state proxy component.
*/

#ifndef STATE_PROXY_PRIVATE_H
#define STATE_PROXY_PRIVATE_H

#include "state_proxy.h"

#include <task_list.h>
#include <connection_manager_protected.h>

#include <message.h>
#include <panic.h>

/*! \brief Create state proxy specific messages. */
#define MAKE_STATE_PROXY_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#ifndef STATE_PROXY_CONCISE_LOG
#define SP_LOG_VERBOSE DEBUG_LOG_VERBOSE
#else
#define SP_LOG_VERBOSE DEBUG_LOG_NOT_USED
#endif

#define STATE_PROXY_EVENTS_TASK_LIST_INIT_CAPACITY 1

/*! Flags for boolean state monitored by state proxy for a device. */
typedef struct
{
    bool in_case:1;                 /*!< The device is in the case. */
    bool in_motion:1;               /*!< The device is in motion. */
    bool in_ear:1;                  /*!< The device is in the ear. */
    bool is_pairing:1;              /*!< The device is pairing. */
    bool has_handset_pairing:1;     /*!< The device is paired with a handset. */
    bool anc_state:1;               /*!< The devcie earbud has anc enable or disable state. */
    bool anc_demo_state:1;          /*!< The devcie earbud has anc demo state enable or disable. */
    bool adaptivity_status:1;       /*!< The devcie earbud has adaptivity enable or disable. */
    bool leakthrough_state:1;       /*!< The device earbud has leakthrough enable or disable state. */
    bool aanc_quiet_mode_detected:1;    /*!< The device earbud has AANC quiet mode detection state */
    bool aanc_quiet_mode_enabled :1;    /*!< The device earbud has AANC quiet mode enabled or disabled */
    bool aanc_quiet_mode_enable_requested:1;
    bool aanc_quiet_mode_disable_requested:1;
    unsigned unused:19;
} state_proxy_data_flags_t;

/*! Data supplied to clients when state changes. */
typedef struct
{
    /*! Current microphone quality level.
     *  Valid range 0(worst)..15(best).
     *  Value of MIC_QUALITY_UNAVAILABLE indicates no microphone quality data. */
    uint8 mic_quality;

    /*! Current battery state of the device. */
    battery_region_state_t battery;
    uint16 battery_voltage;

    /*! Flags for boolean state monitored by state proxy. */
    state_proxy_data_flags_t flags;

    /*! ANC mode */
    uint8 anc_mode;

    /*! ANC gain */
    uint8 anc_leakthrough_gain;

    /*! Leakthrough Mode */
    uint8 leakthrough_mode;

    /*! AANC FF gain */
    uint8 aanc_ff_gain;

    /* ANC toggle configuration */
    anc_toggle_way_config_t toggle_configurations;

    /* ANC configuration during standalone, playback, SCO & VA */
    uint16 standalone_config;
    uint16 playback_config;
    uint16 sco_config;
    uint16 va_config;

    /*! Time stamp for quiet mode */
    marshal_rtime_t timestamp;
} state_proxy_data_t;

/*! \brief State Proxy internal state. */
typedef struct
{
    /*! State Proxy task */
    TaskData task;

    /*! \todo remove this in favour of...something else? */
    bool is_primary;

    /*! Has initial state been sent to peer to sync up after peer
     * signalling connected. */
    bool initial_state_sent:1;

    /*! Has initial state been received after peer signalling connected. */
    bool initial_state_received:1;

    /*! Is State Proxy currently paused and prevented from forwarding
     * events to peer. */
    bool paused:1;

    /*! TRUE when link quality measurements are enabled. */
    bool measuring_link_quality:1;

    /*! TRUE when mic quality measurements are enabled. */
    bool measuring_mic_quality:1;

    /*! List of clients registered to receive STATE_PROXY_EVENT_T
     *  messages with type specific event updates */
    task_list_t* event_tasks;

    TASK_LIST_WITH_INITIAL_CAPACITY(STATE_PROXY_EVENTS_TASK_LIST_INIT_CAPACITY)  state_proxy_events;

    /*! Combined local state tracked in a single entity suitable for
     *  being marshalled during handover. */
    state_proxy_data_t* local_state;

    /*! Combined remote state tracked in a single entity suitable for
     *  being marshalled during handover. */
    state_proxy_data_t* remote_state;
} state_proxy_task_data_t;

extern state_proxy_task_data_t state_proxy;
#define stateProxy_GetTaskData()                  (&state_proxy)
#define stateProxy_GetTask()                      (&state_proxy.task)
#define stateProxy_IsPrimary()                    (state_proxy.is_primary)
#define stateProxy_IsSecondary()                  (!(stateProxy_IsPrimary()))
#define stateProxy_InitialStateSent()             (state_proxy.initial_state_sent)
#define stateProxy_InitialStateReceived()         (state_proxy.initial_state_received)
#define stateProxy_Paused()                       (state_proxy.paused)
#define stateProxy_GetLocalData()                 (stateProxy_GetTaskData()->local_state)
#define stateProxy_GetRemoteData()                (stateProxy_GetTaskData()->remote_state)
#define stateProxy_IsMeasuringLinkQuality()       (stateProxy_GetTaskData()->measuring_link_quality)
#define stateProxy_SetMesauringLinkQuality(value) (stateProxy_GetTaskData()->measuring_link_quality = (value))
#define stateProxy_IsMeasuringMicQuality()        (stateProxy_GetTaskData()->measuring_mic_quality)
#define stateProxy_SetMeasuringMicQuality(value)  (stateProxy_GetTaskData()->measuring_mic_quality = (value))
#define stateProxy_GetRemoteFlag(flag_name)       (stateProxy_GetRemoteData()->flags.##flag_name)
#define stateProxy_GetLocalFlag(flag_name)        (stateProxy_GetLocalData()->flags.##flag_name)
#define stateProxy_GetRemoteFlags()               (&state_proxy.remote_state->flags)
#define stateProxy_GetLocalFlags()                (&state_proxy.local_state->flags)
#define stateProxy_GetEvents()                    (task_list_flexible_t *)(&state_proxy.state_proxy_events)

/*! Internal messages sent by state_proxy to iteself. */
enum state_proxy_internal_messages
{
    STATE_PROXY_INTERNAL_TIMER_MIC_QUALITY = INTERNAL_MESSAGE_BASE,
    STATE_PROXY_INTERNAL_TIMER_LINK_QUALITY,

    /*! This must be the final message */
    STATE_PROXY_INTERNAL_MESSAGE_END
};
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(STATE_PROXY_INTERNAL_MESSAGE_END)

void stateProxy_MsgStateProxyEventClients(state_proxy_source source,
                                                 state_proxy_event_type type,
                                                 const void* event);

/*! \brief Get the local or remote data.

    \param source Request for local or remote data.
    \return The requested data. */
state_proxy_data_t* stateProxy_GetData(state_proxy_source source);


#endif /* STATE_PROXY_PRIVATE_H */
