/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       handover_profile.h
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Handover Profile
*/

#ifndef HANDOVER_PROFILE_H_
#define HANDOVER_PROFILE_H_

#include <handover_if.h>
#include <domain_message.h>

/*! Messages that can be sent by handover profile to client tasks. */
typedef enum
{
    /*! Module initialisation complete */
    HANDOVER_PROFILE_INIT_CFM = HANDOVER_PROFILE_MESSAGE_BASE,

    /*! Handover Profile link to peer established. */
    HANDOVER_PROFILE_CONNECTION_IND,

    /*! Confirmation of a connection request. */
    HANDOVER_PROFILE_CONNECT_CFM,

    /*! Confirmation of a disconnect request. */
    HANDOVER_PROFILE_DISCONNECT_CFM,

    /*! Handover Profile link to peer removed. */
    HANDOVER_PROFILE_DISCONNECTION_IND,

    /*! Handover complete indication */
    HANDOVER_PROFILE_HANDOVER_COMPLETE_IND,

    /*! This must be the final message */
    HANDOVER_PROFILE_MESSAGE_END
}handover_profile_messages_t;

/*! Handover Profile status. */
typedef enum
{
    HANDOVER_PROFILE_STATUS_SUCCESS = 0,
    HANDOVER_PROFILE_STATUS_PEER_CONNECT_FAILED,
    HANDOVER_PROFILE_STATUS_PEER_CONNECT_CANCELLED,
    HANDOVER_PROFILE_STATUS_PEER_DISCONNECTED,
    HANDOVER_PROFILE_STATUS_PEER_LINKLOSS,
    HANDOVER_PROFILE_STATUS_HANDOVER_VETOED,
    HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT,
    HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE
}handover_profile_status_t;

/*! brief Confirmation of the result of a connection request. */
typedef struct
{
    /*! Status of the connection request. */
    handover_profile_status_t status;
} HANDOVER_PROFILE_CONNECT_CFM_T;

/*! brief Confirmation of the result of a disconnect request. */
typedef HANDOVER_PROFILE_CONNECT_CFM_T HANDOVER_PROFILE_DISCONNECT_CFM_T;

#ifdef INCLUDE_MIRRORING

/*! \brief Initialise the handover profile.

    Called at start up to initialise the Handover Profile task.

    \param[in]  init_task   Task to send confirmation message to.

    \return TRUE: Post successful initialization of the task.
            FALSE: Otherwise.
*/
bool HandoverProfile_Init(Task init_task);

/*! \brief Register to receive peer signalling notifications.
    \param[in]  client_task Task to send notification.
*/
void HandoverProfile_ClientRegister(Task client_task);

/*! \brief Unregister to stop receiving peer signalling notifications.
    \param[in]  client_task Task to send notification.
*/
void HandoverProfile_ClientUnregister(Task client_task);

/*! \brief Create L2CAP channel to the Peer earbud.

    SDP search for Handover PSM and create L2CAP channel with the Peer earbud.

    A HANDOVER_PROFILE_CONNECT_CFM message shall be sent with status 

    HANDOVER_PROFILE_STATUS_PEER_CONNECT_FAILED: If the connection fails.
    HANDOVER_PROFILE_STATUS_SUCCESS: This status is also sent with 
          HANDOVER_PROFILE_CONNECTION_IND message to all registered client post 
          succesful connection.

    \param[in]  task        Task to send confirmation message to.
    \param[in]  peer_addr   Address of peer earbud.

*/
void HandoverProfile_Connect(Task task,const bdaddr *peer_addr);

/*! \brief Distroy L2CAP channel to the Peer earbud if exists.

    Post disconnection the HANDOVER_PROFILE_DISCONNECT_CFM message is sent with status 
    HANDOVER_PROFILE_STATUS_SUCCESS

    \param[in]  task        Task to send confirmation message to.
*/
void HandoverProfile_Disconnect(Task task);

/*! \brief Performs handover to the peer device.

    This is a blocking call and returns after handover is complete or if any failure.
    \ref handover_profile_status_t for return types.

    Handover profile will determine which handsets are connected and attempt to
    handover all connected handsets.

    \return \ref handover_profile_status_t. Returns,
            1. HANDOVER_PROFILE_STATUS_SUCCESS if handover is successful.
            2. HANDOVER_PROFILE_STATUS_SUCCESS if peer earbud is not conected.
               Caller need to invoke \ref HandoverProfile_PeerConnect and try again.
            3. HANDOVER_PROFILE_STATUS_HANDOVER_VETOED if any of the clients vetoed handover.
            4. HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT if handover terminated due to timeout.
            5. HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE if any other failure occured.
*/
handover_profile_status_t HandoverProfile_Handover(void);

/*! \brief Handle subsystem version information.
    \param[in] info The subsystem version information.
    \note Handover profile will only handover if the firmware and patch version
    info on the primary and secondary earbuds matches. Matching firmware is
    required since handover involves a binary state transfer from primary to
    secondary. A firmware mismatch means the secondary may incorrectly interpret
    the binary state data leading to invalid state.
*/
void HandoverProfile_HandleSubsystemVersionInfo(const MessageSubsystemVersionInfo *info);

/*! \brief The application must provide a NULL termiated array of handover interfaces. */
extern const handover_interface * handover_clients[];

/*! \brief Cancel any queued SDP search sent by this module.

    If handover profile has sent an SDP search request and is waiting for the
    response, this function will immediately cancel the SDP search request,
    or flush it from the queue if it is still queued in the connection
    library.

    The intention is that this function is called directly before the peer
    ACL is force disconnected. Otherwise, a queued SDP search request could
    trigger the ACL to be re-connected in order to do the SDP search.
*/
void HandoverProfile_TerminateSdpPrimitive(void);

#else

#define HandoverProfile_Init(init_task) (FALSE)

#define HandoverProfile_ClientRegister(client_task) /* Nothing to do */

#define HandoverProfile_ClientUnregister(client_task) /* Nothing to do */

#define HandoverProfile_Connect(task, peer_addr) /* Nothing to do */

#define HandoverProfile_Disconnect(task) /* Nothing to do */

#define HandoverProfile_Handover() (HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE)

#define HandoverProfile_HandleSubsystemVersionInfo(info) /* Nothing to do */

#endif /* INCLUDE_MIRRORING */

#endif /*HANDOVER_PROFILE_H_*/

