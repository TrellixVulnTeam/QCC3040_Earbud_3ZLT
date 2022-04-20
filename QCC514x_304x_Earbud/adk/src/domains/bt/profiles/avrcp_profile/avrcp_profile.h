/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version
\file
\defgroup   avrcp_profile AVRCP Profile
\ingroup    profiles
\brief      Header file for AVRCP State Machine

@startuml

    [*] -down-> NULL
    NULL -down-> DISCONNECTED

    DISCONNECTED : No AVRCP connection
    DISCONNECTED -down-> CONNECTING_LOCAL : ConnectReq
    DISCONNECTED -down-> CONNECTING_LOCAL_WAIT_RESPONSE : ConnectReq/clients to notify
    DISCONNECTED -down-> CONNECTING_REMOTE_WAIT_RESPONSE : ConnectInd/clients to notify
    DISCONNECTED -down-> CONNECTING_REMOTE : ConnectInd

    CONNECTING_LOCAL_WAIT_RESPONSE -down-> CONNECTING_LOCAL : client notification complete
    CONNECTING_REMOTE_WAIT_RESPONSE -down-> CONNECTING_REMOTE : client notification complete

    CONNECTING_LOCAL -down-> CONNECTED : AVRCP_CONNECT_CFM
    CONNECTING_REMOTE -down-> CONNECTED : AVRCP_CONNECT_CFM

    CONNECTED -left-> DISCONNECTING : Disconnect Req/Ind

    DISCONNECTING -right-> DISCONNECTED

@enduml

*/

#ifndef AVRCP_PROFILE_H_
#define AVRCP_PROFILE_H_
#include "avrcp.h"
#include "av_typedef.h"
#include "audio_sources_media_control_interface.h"

/*! \brief Get operation lock */
#define appAvrcpGetLock(theInst)        ((theInst)->avrcp.lock)

/*!@{ \name Lock reasons
    The AVRCP instance can be 'locked' for a number of reasons listed here,
    potentially simultaneously.

    The lock can be used in MessageSendConditionally() to hold a message back
    until all locks are released.
*/
#define APP_AVRCP_LOCK_STATE            (1 << 0)
#define APP_AVRCP_LOCK_PASSTHROUGH_REQ  (1 << 1)
#define APP_AVRCP_LOCK_PASSTHROUGH_IND  (1 << 2)
/*!@} */

/*! \brief Set operation lock */
#define appAvrcpSetLock(theInst, set_lock)      ((theInst)->avrcp.lock |= (set_lock))

/*! \brief Clear operation lock */
#define appAvrcpClearLock(theInst, clear_lock)    ((theInst)->avrcp.lock &= ~(clear_lock))

/*! \brief Is AVRCP state is 'disconnected' */
#define appAvrcpIsDisconnected(theInst) \
    ((theInst)->avrcp.state == AVRCP_STATE_DISCONNECTED)

/*! \brief Is AVRCP play status is active */
#define appAvrcpIsPlayStatusActive(theInst) \
    (((theInst)->avrcp.play_status == avrcp_play_status_playing) || \
     ((theInst)->avrcp.play_status == avrcp_play_status_fwd_seek) || \
     ((theInst)->avrcp.play_status == avrcp_play_status_rev_seek))

/*! \brief Is AVRCP state is 'connected' */
#define appAvrcpIsConnected(theInst) \
    ((theInst)->avrcp.state == AVRCP_STATE_CONNECTED)

/*! Check if the specified event type is supported */
#define appAvrcpIsEventSupported(theInst, event) \
    ((theInst)->avrcp.bitfields.supported_events & (1 << ((event) - 1)))

#define appAvrcpSetEventSupported(theInst, event) \
    ((theInst)->avrcp.bitfields.supported_events |= (1 << ((event) - 1)))

#define appAvrcpClearEventSupported(theInst, event) \
    ((theInst)->avrcp.bitfields.supported_events &= ~(1 << ((event) - 1)))


/*! Check if the specified event type is supported */
#define appAvrcpIsEventChanged(theInst, event) \
    ((theInst)->avrcp.bitfields.changed_events & (1 << ((event) - 1)))

#define appAvrcpSetEventChanged(theInst, event) \
    ((theInst)->avrcp.bitfields.changed_events |= (1 << ((event) - 1)))

#define appAvrcpClearEventChanged(theInst, event) \
    ((theInst)->avrcp.bitfields.changed_events &= ~(1 << ((event) - 1)))


/*! Check if the specified event type has been registered */
#define appAvrcpIsEventRegistered(theInst, event) \
    ((theInst)->avrcp.bitfields.registered_events & (1 << ((event) - 1)))

#define appAvrcpSetEventRegistered(theInst, event) \
    ((theInst)->avrcp.bitfields.registered_events |= (1 << ((event) - 1)))

#define appAvrcpClearEventRegistered(theInst, event) \
    ((theInst)->avrcp.bitfields.registered_events &= ~(1 << ((event) - 1)))


/*! \brief Gets the interface for the avrcp remote commands

    \return pointer to the media_control_interface for avrcp
*/
const media_control_interface_t * AvrcpProfile_GetMediaControlInterface(void);

void appAvrcpInstanceInit(avInstanceTaskData *theAv);
void appAvrcpHandleAvrcpConnectIndicationNew(avTaskData *theAv, const AVRCP_CONNECT_IND_T *ind);
void appAvrcpRejectAvrcpConnectIndicationNew(avTaskData *theAv, const AVRCP_CONNECT_IND_T *ind);
void appAvrcpRemoteControl(avInstanceTaskData *theInst, avc_operation_id op_id, uint8 rstate, bool beep, uint16 repeat_ms);
void appAvrcpVendorPassthroughRequest(avInstanceTaskData *theInst, avc_operation_id op_id, uint16 size_payload, const uint8 *payload);
void appAvAvrcpPlayStatusNotification(avInstanceTaskData *theInst, avrcp_play_status play_status);
void appAvrcpVendorPassthroughResponse(avInstanceTaskData *theInst, avrcp_response_type);

avAvrcpState appAvrcpGetState(avInstanceTaskData *theAv);
bool appAvrcpIsValidClient(avInstanceTaskData *theInst, Task client_task);

Task appAvrcpVendorPassthroughRegister(avInstanceTaskData *theInst, Task client_task);
void appAvrcpNotificationsRegister(avInstanceTaskData *theInst, uint16 events);
void appAvAvrcpVolumeNotification(avInstanceTaskData *theInst, uint8 volume);


void appAvrcpInstanceHandleMessage(avInstanceTaskData *theInst, MessageId id, Message message);
void appAvrcpClearPlaybackLock(avInstanceTaskData *theInst);


void appAvVolumeUpRemoteStart(audio_source_t source);
void appAvVolumeUpRemoteStop(audio_source_t source);
void appAvVolumeDownRemoteStart(audio_source_t source);
void appAvVolumeDownRemoteStop(audio_source_t source);

#ifdef INCLUDE_MIRRORING
/*! 
    \brief Handle Veto check during handover
    \param[in] the_inst     AV instance refernce \ref avInstanceTaskData
    \return bool
*/
bool AvrcpProfile_Veto(avInstanceTaskData *the_inst);

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] the_inst     AV instance refernce \ref avInstanceTaskData
    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
void AvrcpProfile_Commit(avInstanceTaskData *the_inst, bool is_primary);
#endif /* INCLUDE_MIRRORING */

#endif /* AVRCP_PROFILE_H_ */
