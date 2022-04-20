/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      This component records the time at which events occur and calculates
            the time difference between two events.

            A one milli-second time resolution is used. The timestamps are stored
            internally in 16-bit variables resulting in a maximum time difference
            between two events of 65535ms.

            The module is used for measurning KPIs on-chip during tests and can
            be removed from production software by defining DISABLE_TIMESTAMP_EVENT.
*/
#ifndef TIMESTAMP_EVENT_H_
#define TIMESTAMP_EVENT_H_

#include "csrtypes.h"

/*! \brief The ids for events that may be timestampted by this component. */
typedef enum
{
    /*! The chip and os have booted */
    TIMESTAMP_EVENT_BOOTED,

    /*! The application software has initialised */
    TIMESTAMP_EVENT_INITIALISED,
    
    /*! Peer Find Role has been called */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_STARTED,
    
    /*! Peer Find Role is scanning/advertising */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERING_CONNECTABLE,
    
    /*! Peer Find Role has discovered a device */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERED_DEVICE,
    
    /*! Peer Find Role is connected as server */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_CONNECTED_SERVER,
    
    /*! Peer Find Role is connected as client */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_CONNECTED_CLIENT,
    
    /*! Peer Find Role client has discovered GATT primary services of server */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DISCOVERED_PRIMARY_SERVICE,
    
    /*! Peer Find Role client is deciding roles */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_DECIDING_ROLES,
    
    /*! Peer Find Role client has received figure of merit */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_MERIT_RECEIVED,
    
    /*! Peer Find Role has notified registered tasks of role  */
    TIMESTAMP_EVENT_PEER_FIND_ROLE_NOTIFIED_ROLE,

    /*! Handset has connected */
    TIMESTAMP_EVENT_HANDSET_CONNECTION_START,

    /*! Handset has connected */
    TIMESTAMP_EVENT_HANDSET_CONNECTED_ACL,

    /*! Connection of handset profiles have completed */
    TIMESTAMP_EVENT_HANDSET_CONNECTED_PROFILES,

    /*! Address swap procedure has been started */
    TIMESTAMP_EVENT_ADDRESS_SWAP_STARTED,

    /*! Address swap procedure has completed */
    TIMESTAMP_EVENT_ADDRESS_SWAP_COMPLETED,

    /*! eSCO mirroring is connecting */
    TIMESTAMP_EVENT_ESCO_MIRRORING_CONNECTING,

    /*! eSCO mirroring is connected */
    TIMESTAMP_EVENT_ESCO_MIRRORING_CONNECTED,

    /*! Role swap command received on Secondary Earbud */
    TIMESTAMP_EVENT_ROLE_SWAP_COMMAND_RECEIVED,

    /*! HFP profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_HFP,

    /*! A2DP profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_A2DP,

    /*! AVRCP profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_AVRCP,

    /*! HFP profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_HFP,

    /*! A2DP profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_A2DP,

    /*! AVRCP profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_AVRCP,

    /*! A2DP mirroring is connecting */
    TIMESTAMP_EVENT_A2DP_MIRRORING_CONNECTING,

    /*! A2DP mirroring is connected */
    TIMESTAMP_EVENT_A2DP_MIRRORING_CONNECTED,

    /*! Clean connections starting */
    TIMESTAMP_EVENT_CLEAN_CONNECTIONS_STARTED,

    /*! Clean connections completed */
    TIMESTAMP_EVENT_CLEAN_CONNECTIONS_COMPLETED,

    /*! Primary started handover */
    TIMESTAMP_EVENT_PRI_HANDOVER_STARTED,

    /*! Primary started critical section of handover */
    TIMESTAMP_EVENT_PRI_HANDOVER_CRITICAL_SECTION_STARTED,

    /*! Primary completed handover */
    TIMESTAMP_EVENT_PRI_HANDOVER_COMPLETED,

    /*! A2DP_MEDIA_START_IND received by app A2DP profile */
    TIMESTAMP_EVENT_A2DP_START_IND,

    /*! A2dpMediaStartResponse function is called after primary/secondary
        A2DP sync is completed */
    TIMESTAMP_EVENT_A2DP_START_RSP,

    /*! A2DP_MEDIA_START_CFM received by app A2DP profile */
    TIMESTAMP_EVENT_A2DP_START_CFM,

    /*! Event to record the time when secondary unmutes its audio 
        following audio sync completion */
    TIMESTAMP_EVENT_KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED,

    /*! AMA profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_AMA,

    /*! AMA profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_AMA,

    /*! GAA profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_GAA,

    /*! GAA profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_GAA,

    /*! GAIA profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_GAIA,

    /*! GAIA profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_GAIA,

    /*! PEER profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_PEER,

    /*! PEER profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_PEER,

    /*! ACCESSORY profile connected to handset */
    TIMESTAMP_EVENT_PROFILE_CONNECTED_ACCESSORY,

    /*! ACCESSORY profile disconnected from handset */
    TIMESTAMP_EVENT_PROFILE_DISCONNECTED_ACCESSORY,

    /*! LE ACL connected from handset */
    TIMESTAMP_EVENT_LE_UNICAST_ACL_CONNECT,

    /*! LE audio ASCS codec configure Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_CONFIGURE_CODEC,

    /*! LE audio ASCS qos configure Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_CONFIGURE_QOS,

    /*! LE audio ASCS enable Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_ENABLE,

    /*! LE audio ASCS disable Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_DISABLE,

    /*! LE audio ASCS receiver start ready Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_RECEIVER_START_READY,

    /*! LE audio ASCS receiver stop ready Ind */
    TIMESTAMP_EVENT_LE_UNICAST_ASCS_RECEIVER_STOP_READY,

    /*! LE audio CIS established */
    TIMESTAMP_EVENT_LE_UNICAST_CIS_ESTABLISH,

    /*! Case comms loopback message transmitted. */
    TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_TX,

    /*! Case comms loopback message received. */
    TIMESTAMP_EVENT_CASECOMMS_LOOPBACK_RX,

    /*! VA wake word has been detected. */
    TIMESTAMP_EVENT_WUW_DETECTED,

    /*! Prompt has played */
    TIMESTAMP_EVENT_PROMPT_PLAY,

    /*! LE Audio Broadcast start waiting for PAST to provide Sync */
    TIMESTAMP_EVENT_LE_BROADCAST_START_PAST_TIMER,

    /*! LE Audio Broadcast started an attempt to sync to a train */
    TIMESTAMP_EVENT_LE_BROADCAST_START_PA_SYNC,

    /*! Audio connected to HFP profile */
    TIMESTAMP_EVENT_HFP_AUDIO_CONNECTED,

    /*! (e)SCO microphone audio stream has started */
    TIMESTAMP_EVENT_SCO_MIC_STREAM_STARTED,

    /*! Always the final event id */
    NUMBER_OF_TIMESTAMP_EVENTS,
} timestamp_event_id_t;

#ifndef DISABLE_TIMESTAMP_EVENT

/*! \brief Timestamp an event

    \param id The id of the event to timestamp
*/
void TimestampEvent(timestamp_event_id_t id);

/*! \brief Timestamp an event with a time offset

    \param id The id of the event to timestamp
    \param offset_ms The time offset in milliseconds
*/
void TimestampEvent_Offset(timestamp_event_id_t id, uint16 offset_ms);

/*! \brief Calculate the time difference between two timestamped events

    \param first_id The id of the first timestamped event
    \param second_id The id of the second timestamped event

    \return The elapsed time between the first and second events in milli-seconds.
*/
uint32 TimestampEvent_Delta(timestamp_event_id_t first_id, timestamp_event_id_t second_id);

/*! \brief Calculate the time difference between a timestamped event and now

    \param start_id The id of the timestamped event. The event start.

    \return The elapsed time the start event and now in milli-seconds.
*/
uint32 TimestampEvent_DeltaFrom(timestamp_event_id_t start_id);

/*! \brief Gets the timestamped event time

    \param id The id of the timestamped event

    \return The timestamped event time.
*/
uint16 TimestampEvent_GetTime(timestamp_event_id_t id);


#else /* DISABLE_TIMESTAMP_EVENT */

/*! \brief Not implemented */
#define TimestampEvent(id)

/*! \brief Not implemented */
#define TimestampEvent_Delta(first_id, second_id) (0)

/*! \brief Not implemented */
#define TimestampEvent_DeltaFrom(first_id) (0)

/*! \brief Not implemented */
#define TimestampEvent_GetTime(timestamp_event_id_t id) (0)

#endif /* DISABLE_TIMESTAMP_EVENT */

#endif /* TIMESTAMP_EVENT_H_ */
