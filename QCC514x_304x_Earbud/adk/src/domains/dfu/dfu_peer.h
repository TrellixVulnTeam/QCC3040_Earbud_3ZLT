/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_peer.h
\brief      Header file for the DFU Peer L2CAP channel.
*/

#ifndef DFU_PEER_H_
#define DFU_PEER_H_

#ifdef INCLUDE_DFU_PEER

#include <message.h>
#include "domain_message.h"
#include "handover_if.h"

#include <rtime.h>
#include <upgrade.h>
#include <task_list.h>
#include <upgrade_peer.h>

/*! Defines the the peer upgrade client task list initial capacity */
#define PEER_UPGRADE_CLIENT_LIST_INIT_CAPACITY 1

/*! \brief Message IDs from Upgrade Peer to main application task */
enum dfu_peer_messages_t
{
    /*! Module initialisation complete */
    DFU_PEER_INIT_CFM = DFU_PEER_MESSAGE_BASE,
    DFU_PEER_STARTED,
    DFU_PEER_DISCONNECTED,

    /*! This must be the final message */
    DFU_PEER_MESSAGE_END
};

/*! Internal messages used by peer . */
typedef enum
{
    /*! Message to bring up link to peer */
    DFU_PEER_INTERNAL_STARTUP_REQ = INTERNAL_MESSAGE_BASE,

    /*!
     * In some scenarios, MESSAGE_MORE_DATA from streams may be missed
     * if stream configuration at connection setup is delayed. To compensate
     * for the missed MESSAGE_MORE_DATA, rescan the source buffer after
     * connection setup and if data is available request to process it.
     */
    DFU_PEER_INTERNAL_MESSAGE_MORE_DATA,


    /*! This must be the final message */
    DFU_PEER_INTERNAL_MESSAGE_END
} dfu_peer_internal_messages_t;

/*!
    @brief Exposes mirror profile interface for handover.
*/
extern const handover_interface dfu_peer_handover_if;


/*! Internal message sent to start signalling to a peer */
typedef struct
{
    bdaddr peer_addr;           /*!< Address of peer */
} DFU_PEER_INTERNAL_STARTUP_REQ_T;

typedef enum
{
    DFU_PEER_STATE_INITIALISE = 0,
    DFU_PEER_STATE_IDLE = 1,                 /*!< Awaiting L2CAP registration */
    DFU_PEER_STATE_DISCONNECTED = 2,         /*!< No connection */
    DFU_PEER_STATE_CONNECTING_ACL = 3,       /*!< Connecting ACL */
    DFU_PEER_STATE_CONNECTING_SDP_SEARCH = 4,/*!< Searching for Upgrade Peer service */
    DFU_PEER_STATE_CONNECTING_LOCAL = 5,     /*!< Locally initiated connection in progress */
    DFU_PEER_STATE_CONNECTING_REMOTE = 6,    /*!< Remotely initiated connection in progress */
    DFU_PEER_STATE_CONNECTED = 7,            /*!< Connnected */
    DFU_PEER_STATE_DISCONNECTING = 8,        /*!< Disconnection in progress */
} dfu_peer_state_t;

typedef struct
{
    TaskData        task;

    /*! List of tasks to notify of DEVICE UPGRADE activity. */
    TASK_LIST_WITH_INITIAL_CAPACITY(PEER_UPGRADE_CLIENT_LIST_INIT_CAPACITY) client_list;

    dfu_peer_state_t     state; /*!< Current state of the state machine */

    /* State related to L2CAP device upgrade peer channel */
    uint16          local_psm;        /*!< L2CAP PSM registered */
    uint16          remote_psm;       /*!< L2CAP PSM registered by peer device */

    /* State related to maintaining upgrade signalling with peer */
    bdaddr peer_addr;   /*!< Bluetooth address of the peer we are signalling */
    bool is_primary;   /*!< Act as a flag to ensure specific activities are done only for Primary Earbud */
    Sink            link_sink;       /*!< The sink of the L2CAP link */
    Source          link_source;     /*!< The source of the L2CAP link */
    bool processing;    /*!< Flag to indicate upgrade lib is procesing data*/

} dfu_peer_task_data_t;

bool DfuPeer_EarlyInit(Task init_task);

bool DfuPeer_Init(Task init_task);

/*! \brief Check if Abort is trigerred and device upgrade peer still in use.

    \return TRUE if abort is initiated and device upgrade peer still in use,
            else FALSE.
*/
bool DfuPeer_StillInUseAfterAbort(void);

/*! \brief Add a client to the DEVICE UPGRADE peer module

    Messages will be sent to any task registered through this API

    \param task Task to register as a client
 */
void DfuPeer_ClientRegister(Task task);

/*! \brief Set the current main role (Primary/Secondary).

    Update the current main role in DFU domain and in upgrade peer lib.

    \param role TRUE if Primary and FALSE if Secondary.
    \return none
*/
void DfuPeer_SetRole(bool role);

/*! \brief Initiate Peer connect request

    Called when UPGRADE_PEER_CONNECT_REQ msg is received from upgrade library

    \param none
    \return none
*/
void DfuPeer_InitiateLinkToPeer(void);

/*! \brief Initiate Peer disconnection request

    Called to disconnect the L2CAP connection with Peer device and change the
    state of the device

    \param none
    \return none
*/
void DfuPeer_InitiatePeerLinkDisconnection(void);

/****************************************************************************
NAME
    DfuPeer_SetLinkPolicy()

BRIEF
    Set the peer link policy.

DESCRIPTION
    Update the link policy with peer.

    \param lp_power_mode active or sniff.
    \return none
*/
void DfuPeer_SetLinkPolicy(lp_power_mode mode);

/****************************************************************************
NAME
    DfuPeer_ProcessHostMsg

BRIEF
    Wrapper function to invoke UpgradePeerProcessHostMsg()

    \return none
*/
void DfuPeer_ProcessHostMsg(upgrade_peer_msg_t msg_id, uint8 action);

/****************************************************************************
NAME
    DfuPeer_StoreMd5

BRIEF
    Wrapper function to invoke UpgradePeerStoreMd5()

    \return none
*/
void DfuPeer_StoreMd5(uint32 md5_checksum);

/****************************************************************************
NAME
    DfuPeer_CtxInit

BRIEF
    Wrapper function to invoke UpgradePeerCtxInit()

    \return none
*/
void DfuPeer_CtxInit(void);

/****************************************************************************
NAME
    DfuPeer_CancelDFU

BRIEF
    Wrapper function to invoke UpgradePeerCancelDFU()

    \return none
*/
void DfuPeer_CancelDFU(void);

#else
#define DfuPeer_ClientRegister(tsk)                 ((void)0)
#define DfuPeer_SetRole(role)                       ((void)0)
#define DfuPeer_SetLinkPolicy(mode)                 ((void)0)
#define DfuPeer_InitiateLinkToPeer(void)            ((void)0)
#define DfuPeer_InitiatePeerLinkDisconnection(void) ((void)0)
#define DfuPeer_ProcessHostMsg(msg_id, action)      ((void)0)
#define DfuPeer_StoreMd5(md5_checksum)              ((void)0)
#define DfuPeer_CtxInit(void)                       ((void)0)
#define DfuPeer_CancelDFU(void)                     ((void)0)
#endif

#endif

