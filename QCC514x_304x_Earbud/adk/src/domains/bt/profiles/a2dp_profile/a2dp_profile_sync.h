/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       a2dp_profile_sync.h
\brief      Implementation of an audio_sync_t interface for an a2dp AV instance.
*/

#ifndef A2DP_PROFILE_SYNC_H_
#define A2DP_PROFILE_SYNC_H_


#include "audio_sync.h"
#include "av_typedef.h"


/*! \brief Initialise the audio_sync_t interface instance for an AV instance.

    \param theInst  The AV instance to initialise the audio_sync_t interface for.
*/
void appA2dpSyncInitialise(avInstanceTaskData *theInst);

/*! \brief Get the audio_sync_state_t for a given avA2dpState. 

    \param theInst  The AV instance to get the sync status for.
    
    \return The sync state of theInst
*/
audio_sync_state_t appA2dpSyncGetAudioSyncState(avInstanceTaskData *theInst);

/*! \brief Handler function for audio_sync_msg_t messages sent to an AV instance.

    \param theInst      The AV instance to handle the audio_sync_msg_t message.
    \param id           The message Id.
    \param message      The message payload, or NULL if there is no payload.
*/
void appA2dpSyncHandleMessage(avInstanceTaskData *theInst, MessageId id, Message message);

/*! \brief Register a sync interface to synchronise.

    After the instance is registered the current state, based on
    #audio_sync_state_t, is sent to the registrant.

    \param theInst      The AV instance to synchronise to.
    \param sync_if      The client's interface.

    \note The instance copies the sync_if state internally. This means the client
    does not need to store a audio_sync_t instance.
*/
void appA2dpSyncRegister(avInstanceTaskData *theInst, const audio_sync_t *sync_if);

/*! \brief Un-register instance from synchronisation.

    Will also cancel any #audio_sync_msg_t messages sent to the registered
    #audio_sync_t instance but not delivered yet.

    \param theInst      The AV instance to unregister from.
*/
void appA2dpSyncUnregister(avInstanceTaskData *theInst);

/*! \brief Send connect indication to the sync task ONLY if instance audio is routed

    \param av_instance      The AV instance to sync
*/
void a2dpProfileSync_SendConnectIndication(avInstanceTaskData *av_instance);

/*! \brief Send prepare indication to the sync task ONLY if instance audio is routed

    \param av_instance      The AV instance to sync
*/
void a2dpProfileSync_SendPrepareIndication(avInstanceTaskData *av_instance);

/*! \brief Send active indication to the sync task ONLY if instance audio is routed

    \param av_instance      The AV instance to sync
*/
void a2dpProfileSync_SendActiveIndication(avInstanceTaskData *av_instance);

/*! \brief Send state indication to the sync task ONLY if instance audio is routed

    \param av_instance      The AV instance to sync
    \param state            The state to send
*/
void a2dpProfileSync_SendStateIndication(avInstanceTaskData* av_instance, audio_sync_state_t state);


#endif /* A2DP_PROFILE_SYNC_H_ */
