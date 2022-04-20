/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "sdp.h"
#include "bt_device.h"
#include "system_state.h"
#include "pairing.h"
#include "le_advertising_manager.h"
#include "le_scan_manager.h"

#include "connection_manager.h"
#include "connection_manager_data.h"
#include "connection_manager_list.h"
#include "connection_manager_config.h"
#include "connection_manager_notify.h"
#include "connection_manager_qos.h"
#include "connection_manager_msg.h"

#include <logging.h>

#include <message.h>
#include <panic.h>
#include <app/bluestack/dm_prim.h>
#include "connection_abstraction.h"

#define MAX_UINT16 (0xFFFF)

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(av_headset_conn_manager_messages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(CON_MANAGER, CON_MANAGER_MESSAGE_END)

#endif

/*!< Connection manager task data */
conManagerTaskData  con_manager;

/******************************************************************************/
bool ConManagerAnyLinkConnected(void)
{
    return conManagerAnyLinkInState(cm_transport_all, ACL_CONNECTED);
}

void ConManagerDisconnectOtherHandset(tp_bdaddr *new_connection)
{
    cm_connection_iterator_t iterator;
    tp_bdaddr existing_connection;

    if (!new_connection ||
        new_connection->transport != TRANSPORT_BREDR_ACL ||
        appDeviceIsPeer(&new_connection->taddr.addr))
    {
        return;
    }

    if (ConManager_IterateFirstActiveConnection(&iterator, &existing_connection))
    {
        do
        {
            if (!BdaddrTpIsSame(&existing_connection, new_connection) &&
                (existing_connection.transport == TRANSPORT_BREDR_ACL) &&
                appDeviceIsHandset(&existing_connection.taddr.addr))
            {
                DEBUG_LOG_FN_ENTRY("conManagerDisconnectOtherHandset 0x%x", existing_connection.taddr.addr.lap);
                ConManagerSendCloseAclRequest(&existing_connection.taddr.addr, TRUE);
            }
        } while (ConManager_IterateNextActiveConnection(&iterator, &existing_connection));
    }
}

/******************************************************************************/
static uint16 conManagerGetPageTimeout(const tp_bdaddr* tpaddr)
{
    uint32 page_timeout = appConfigPageTimeout();

    bool is_peer = appDeviceIsPeer(&tpaddr->taddr.addr);
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);

    /* setup page timeout depending on the type of device the connection is for */
    if (is_peer)
    {
        page_timeout = appConfigEarbudPageTimeout();
    }
    else
    {
        if (conManagerGetConnectionState(connection) == ACL_DISCONNECTED_LINK_LOSS)
        {
            /* Increase page timeout as connection was previously disconnected due to link-loss */
            page_timeout *= appConfigHandsetLinkLossPageTimeoutMultiplier();
        }
    }

    if (page_timeout > MAX_UINT16)
        page_timeout = MAX_UINT16;

    DEBUG_LOG("conManagerGetPageTimeout, page timeout %u ms", (page_timeout * US_PER_SLOT)/1000UL);
    return (uint16)page_timeout;
}

/******************************************************************************/
static bool conManagerIsConnectingBle(void)
{
    if(conManagerAnyLinkInState(cm_transport_ble, ACL_CONNECTING_PENDING_PAUSE))
    {
        return TRUE;
    }

    if(conManagerAnyLinkInState(cm_transport_ble, ACL_CONNECTING_INTERNAL))
    {
        return TRUE;
    }

    if(conManagerAnyLinkInState(cm_transport_ble, ACL_CONNECTING))
    {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************/
static bool conManagerPauseLeScan(cm_connection_t* connection)
{
     if(!con_manager.is_le_scan_paused)
     {
         ConManagerDebugConnectionVerbose(connection);
         DEBUG_LOG("conManagerPauseLeScan");

         LeScanManager_Pause(&con_manager.task);
         ConManagerSetConnectionState(connection, ACL_CONNECTING_PENDING_PAUSE);
         return TRUE;
     }
     return FALSE;
}

/******************************************************************************/
static void conManagerResumeLeScanIfPaused(void)
{
   LeScanManager_Resume(&con_manager.task);
   con_manager.is_le_scan_paused = FALSE;
}

/******************************************************************************/
static bool conManagerPrepareForConnection(cm_connection_t* connection)
{
    const tp_bdaddr* tpaddr = ConManagerGetConnectionTpAddr(connection);

    PanicNull((void *)tpaddr);

    if(tpaddr->transport == TRANSPORT_BLE_ACL)
    {
        if(conManagerPauseLeScan(connection))
        {
            return FALSE;
        }
    }
    else
    {
        conManagerSendWritePageTimeout(conManagerGetPageTimeout(tpaddr));
    }

    return TRUE;
}

/******************************************************************************/
static void conManagerPrepareForConnectionComplete(void)
{
    cm_list_iterator_t iterator;
    bool connecting = FALSE;
    cm_connection_t* connection = ConManagerListHeadConnection(&iterator);
    
    DEBUG_LOG("conManagerPrepareForConnectionComplete");
    
    while(connection)
    {
        cm_connection_state_t state = conManagerGetConnectionState(connection);

        switch(state)
        {
            case ACL_CONNECTING_PENDING_PAUSE:
                DEBUG_LOG("conManagerPrepareForConnectionComplete Continue Connection");
                conManagerSendOpenTpAclRequestInternally(connection);
                connecting = TRUE;
                break;

            default:
                break;
        }
        
        connection = ConManagerListNextConnection(&iterator);
    }
    
    if(!connecting)
    {
        conManagerResumeLeScanIfPaused();
    }
}

/******************************************************************************/
static uint16 *ConManagerCreateAclImpl(const tp_bdaddr* tpaddr)
{
    /* Attempt to find existing connection */
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);

    DEBUG_LOG_FN_ENTRY("ConManagerCreateAclImpl");
    ConManagerDebugAddress(tpaddr);

    /* Reset connection for re-use if in link loss state */
    if(conManagerGetConnectionState(connection) == ACL_DISCONNECTED_LINK_LOSS)
    {
        ConManagerSetConnectionState(connection, ACL_DISCONNECTED);
        connection = NULL;
    }

    if(!connection)
    {
        /* Create new connection */
        connection = ConManagerAddConnection(tpaddr, ACL_CONNECTING, TRUE);
        
        if(conManagerPrepareForConnection(connection))
        {
            conManagerSendOpenTpAclRequestInternally(connection);
        }
    }

    conManagerAddConnectionUser(connection);

    DEBUG_LOG("ConManagerCreateAclImpl end");
    ConManagerDebugConnectionVerbose(connection);

    /* Return pointer to lock, will always be set */
    return conManagerGetConnectionLock(connection);
}

/******************************************************************************/
uint16 *ConManagerCreateAcl(const bdaddr *addr)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    return ConManagerCreateAclImpl(&tpaddr);
}

/******************************************************************************/
static void conManagerReleaseAclImpl(const tp_bdaddr* tpaddr)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if (connection)
    {
        conManagerRemoveConnectionUser(connection);

        DEBUG_LOG("conManagerReleaseAclImpl ConnState:%d InUse:%d",
                conManagerGetConnectionState(connection),conManagerConnectionIsInUse(connection));
        ConManagerDebugConnection(connection);

        if (!conManagerConnectionIsInUse(connection))
        {
            /* If we are waiting for something to occur before we actually
               send an open message, simply remove the connection */
            if(   ACL_CONNECTING_PENDING_PAUSE == conManagerGetConnectionState(connection)
               || ACL_CONNECTING_INTERNAL == conManagerGetConnectionState(connection))
            {
                conManagerRemoveConnection(connection);
            }
            else
            {
                /* Depending on address type conn_tpaddr may not be same as tpaddr */
                const tp_bdaddr* conn_tpaddr = ConManagerGetConnectionTpAddr(connection);

                /* Closure of an LE ACL can take some time due to active ATT/GATT 
                   connections and signalling delays on LE. 
                   To avoid this, force the ACL closure.
                   Maintain behaviour for BREDR as preference is for clean connection
                   closures */
                if (conn_tpaddr->transport == TRANSPORT_BLE_ACL)
                {
                        conManagerSendCloseTpAclRequest(conn_tpaddr, TRUE);
                }
                else
                {
                        conManagerSendCloseTpAclRequest(conn_tpaddr, FALSE);
                }
            }
            conManagerNotifyObservers(tpaddr, cm_notify_message_disconnect_requested, hci_success);
        }
    }
}

/******************************************************************************/
void ConManagerReleaseAcl(const bdaddr *addr)
{
    /* Attempt to find existing connection */
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    conManagerReleaseAclImpl(&tpaddr);
}

static void conManagerCheckForForcedDisconnect(tp_bdaddr *tpaddr)
{
    if (con_manager.forced_disconnect_task)
    {
        if(NULL != tpaddr)
        {
            DEBUG_LOG("conManagerCheckForForcedDisconnect 0x%06x now dropped", tpaddr->taddr.addr.lap);
        }

        if (!ConManagerFindFirstActiveLink(cm_transport_all))
        {
            MessageSend(con_manager.forced_disconnect_task, CON_MANAGER_CLOSE_ALL_CFM, NULL);
            con_manager.forced_disconnect_task = NULL;
        }
    }
}

/*! \brief If there are no remaining LE links send the confirmation message. */
static void conManagerCheckForAllLeDisconnected(void)
{
    if (   con_manager.all_le_disconnect_requester
        && !ConManagerFindFirstActiveLink(cm_transport_ble))
    {
        DEBUG_LOG("conManagerCheckForAllLeDisconnected all LE links disconnected");
        MessageSend(con_manager.all_le_disconnect_requester, CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM, NULL);
        con_manager.all_le_disconnect_requester = NULL;
    }
}

/******************************************************************************/
static void ConManagerSetConnInterval(cm_connection_t *connection, uint16 conn_interval)
{
    if(connection)
    {
        connection->conn_interval = conn_interval;
    }
}

/******************************************************************************/
static void ConManagerSetConnLatency(cm_connection_t *connection, uint16 conn_latency)
{
    if(connection)
    {
        connection->slave_latency = conn_latency;
    }
}

/*! \brief Handle completion of connection parameter update.
 */
static void conManagerHandleBleConnectionUpdateCompleteInd(typed_bdaddr *taddr, uint16 status, uint16 interval, uint16 latency)
{
    DEBUG_LOG("conManagerHandleClDmBleConnectionUpdateCompleteInd, status enum:hci_status:%u, conn interval %d, slave latency %d",
               status, interval, latency);

    tp_bdaddr tpaddr = {.taddr = *taddr, .transport = TRANSPORT_BLE_ACL};
    cm_connection_t *connection;

    connection = ConManagerFindConnectionFromBdAddr(&tpaddr);
    if (connection)
    {
        Task task = ConManagerGetTask(connection);

        MessageCancelAll(task, CON_MANAGER_INTERNAL_MSG_QOS_TIMEOUT);
        connection->le_update_in_progress = 0;

        if(status == hci_success)
        {
            /* Preserve the connection parameter changes */
            ConManagerSetConnInterval(connection, interval);
            ConManagerSetConnLatency(connection, latency);
            conManagerNotifyConnParamsObservers(ConManagerGetConnectionTpAddr(connection), 
                                                interval, latency);

            conManagerQosCheckNewConnParams(connection);
        }
    }
}

/*  ACL opened indication handler

    If a new ACL is opened successfully and it is to a handset (where the TWS+
    version needs to be checked everytime) a service attribute search is started.
*/
static void ConManagerHandleAclOpenedIndication(tp_bdaddr * tpaddr, bool success, uint16 flags, bool incoming, uint16 interval, uint16 latency)
{
    cm_connection_t *connection;

    DEBUG_LOG("ConManagerHandleAclOpenedIndication succes:%d interval:%d latency:%d",
              success, interval, latency);

    connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    
    ConManagerDebugAddress(tpaddr);
    
    if(!connection && !incoming)
    {
        DEBUG_LOG("ConManagerHandleAclOpenedIndication, local connection not initiated from connection_manager");
    }

    if (success)
    {
        const bool is_local = !incoming;
        cm_notify_message_t notify = is_local ? cm_notify_message_connected_outgoing : cm_notify_message_connected_incoming;

#ifdef MULTIPOINT_BARGE_IN_ENABLED
        /* Disconnect the first handset before pairing with the second one. */
        ConManagerDisconnectOtherHandset(tpaddr);
#endif

        /* Update local ACL flag */
        ConManagerSetConnectionLocal(connection, is_local);
        appLinkPolicyHandleClDmAclOpenedIndication(&tpaddr->taddr.addr,
                                                   (flags & DM_ACL_FLAG_ULP),
                                                   (~flags & DM_ACL_FLAG_INCOMING));
        /* Add this ACL to list of connections */
        connection = ConManagerAddConnection(tpaddr, ACL_CONNECTED, is_local);

        /* Store the initial connection parameters */
        ConManagerSetConnInterval(connection, interval);
        ConManagerSetConnLatency(connection, latency);

        if (!appDeviceIsPeer(&tpaddr->taddr.addr) && !con_manager.handset_authorise_lock)
        {
            DEBUG_LOG("ConManagerHandleAclOpenedIndication store the handset address to autorise later");
            /* Store address of handset to pair with. */
            con_manager.handset_to_pair_with_bdaddr = tpaddr->taddr.addr;

            /* lock the handset authorisation */
            con_manager.handset_authorise_lock = TRUE;
        }

        DEBUG_LOG("ConManagerHandleAclOpenedIndication, req_handset %04x,%02x,%06lx handset_to_pair_with_bdaddr %04x,%02x,%06lx ", 
                  tpaddr->taddr.addr.nap,
                  tpaddr->taddr.addr.uap,
                  tpaddr->taddr.addr.lap,
                  con_manager.handset_to_pair_with_bdaddr.nap, 
                  con_manager.handset_to_pair_with_bdaddr.uap, 
                  con_manager.handset_to_pair_with_bdaddr.lap);

        conManagerNotifyObservers(tpaddr, notify, hci_success);


        if(tpaddr->transport == TRANSPORT_BLE_ACL)
        {
            /* Apply global connection parameteres immediately. Affects subsequent connections,
               and any qos updates requested */
            ConManagerRequestDefaultQos(cm_transport_ble, cm_qos_low_latency);

            /* If in pairing mode allow aggressive connection parameters and apply QoS Parameter after a timeout*/
            if(!ConManagerIsHandsetPairingMode())
            {
                /* In non pairing mode apply preferred LE connection parameters 
                   immediately to ensure Quality of Service streaming audio/
                   HFP calls/Voice assistant usage etc 
                   The use of a message gives other subscribers to the ACL open 
                   message the opportunity to set a connection or default QoS */
                conManagerSendInternalMsgUpdateQos(connection);
            }
            else
            {
                conManagerSendInternalMsgUpdateQosDelayed(connection);
            }
        }
    }
    else
    {
        /* Remove this ACL from list of connections */
        conManagerRemoveConnection(connection);
    }
    
    if(!conManagerIsConnectingBle())
    {
        conManagerResumeLeScanIfPaused();
    }
}

/*! \brief ACL closed indication handler
*/
static void ConManagerHandleAclClosedIndication(tp_bdaddr *tpaddr, uint16 status)
{
    DEBUG_LOG_FN_ENTRY("ConManagerHandleAclClosedIndication, status enum:hci_status:%u", status);
    ConManagerDebugAddress(tpaddr);

    /* Check if this BDADDR is for handset */
    if((tpaddr->taddr.type == TYPED_BDADDR_PUBLIC) && appDeviceIsHandset(&tpaddr->taddr.addr))
    {
        DEBUG_LOG("ConManagerHandleAclClosedIndication, handset");
    }

    /* If connection timeout/link-loss move to special disconnected state, so that re-opening ACL
     * will use longer page timeout */
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);

    if (connection)
    {
        if ((status == hci_error_conn_timeout) && appDeviceIsHandset(&tpaddr->taddr.addr) && (tpaddr->transport == TRANSPORT_BREDR_ACL))
        {
            ConManagerSetConnectionState(connection, ACL_DISCONNECTED_LINK_LOSS);
            conManagerResetConnectionUsers(connection);
        }
        else
        {
            /* Remove this ACL from list of connections */
            conManagerRemoveConnection(connection);
        }

        /* check if we were trying to disconnect all LE links and they're all now
         * gone, so the confirmation message should be sent */
        conManagerCheckForAllLeDisconnected();
    }

    if (BdaddrIsSame(&tpaddr->taddr.addr,&con_manager.handset_to_pair_with_bdaddr))
    {
        DEBUG_LOG("ConManagerHandleAclClosedIndication set Handset to pair with BD_ADDR to zero and unlock the auth lock");

        /* Set handset to pair with to Zero. */
        BdaddrSetZero(&con_manager.handset_to_pair_with_bdaddr);

        /* unlock the handset authorisation. */
        con_manager.handset_authorise_lock = FALSE;
    }

    DEBUG_LOG("ConManagerHandleAclClosedIndication, req_handset %04x,%02x,%06lx",
                        tpaddr->taddr.addr.nap,
                        tpaddr->taddr.addr.uap,
                        tpaddr->taddr.addr.lap);

    /* Reset qhs connected status for this device if this device is connected over qhs */
    ConManagerSetQhsConnectStatus(&tpaddr->taddr.addr,FALSE);

    /* Indicate to client the connection to this connection has gone */
    conManagerNotifyObservers(tpaddr, cm_notify_message_disconnected, status);
}

/*! \brief Handle confirmation that a DM_ACL_CLOSE_REQ has completed.
 
    Currently only used to complete the ConManagerTerminateAllAcls() API
    by sending a CON_MANAGER_CLOSE_ALL_CFM if the requester is still waiting.
*/
static void ConManagerHandleAclCloseCfm(tp_bdaddr *tpaddr, uint8 status, uint16 flags)
{
    DEBUG_LOG_FN_ENTRY("ConManagerHandleAclCloseCfm, status %d, flags 0x%x", status, flags);
    if(tpaddr)
        ConManagerDebugAddress(tpaddr);

    switch (status)
    {
        case DM_ACL_CLOSE_NO_CONNECTION:
            DEBUG_LOG("ConManagerHandleClDmAclCloseCfm NO ACLs to close");
            /* fall-through */
        case DM_ACL_CLOSE_LINK_TRANSFERRED:
            /* link no longer on this device, treat as success
               fall-through */
        case DM_ACL_CLOSE_SUCCESS:
            /* if this CLOSE_CFM was for a forced disconnect of all ACLs,
               remove all connection instances and check if requester
               still needs a confirmation message sent */
            if ((flags & (DM_ACL_FLAG_FORCE|DM_ACL_FLAG_ALL)) == (DM_ACL_FLAG_FORCE|DM_ACL_FLAG_ALL))
            {
                conManagerRemoveAllConnection();
                conManagerCheckForForcedDisconnect(tpaddr);
            }
            break;
        case DM_ACL_CLOSE_BUSY:
            /* indicates Bluestack already has a close req in progress,
               ignore and wait for another close cfm to arrive */
            break;
        default:
            break;
    }
}


/*! \brief Decide whether we allow a BR/EDR device to connect
    based on the device type and how many devices allowed to connect
    at the same time.
 */
static bool conManagerIsBredrAddressAuthorised(const bdaddr* bd_addr, dm_protocol_id protocol_id, uint32 channel, bool incoming)
{
    /* Always allow connection from peer */
    if (appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW peer");
        return TRUE;
    }
    else if (appDeviceTypeIsSink(bd_addr))
    {
        DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW Sink Device");
        return TRUE;
    }
    else if (appDeviceIsHandset(bd_addr))
    {
        DEBUG_LOG("conManagerIsBredrAddressAuthorised, auth_from_handset %04x,%02x,%06lx handset_to_pair_with_bdaddr %04x,%02x,%06lx",
                bd_addr->nap,
                bd_addr->uap,
                bd_addr->lap,
                con_manager.handset_to_pair_with_bdaddr.nap,
                con_manager.handset_to_pair_with_bdaddr.uap,
                con_manager.handset_to_pair_with_bdaddr.lap);

        if(con_manager.handset_connect_allowed)
        {
            if(con_manager.handset_authorise_callback.AuthoriseConnection)
            {
                DEBUG_LOG("conManagerIsBredrAddressAuthorised - handset callback");
                return con_manager.handset_authorise_callback.AuthoriseConnection(bd_addr, protocol_id, channel, incoming);
            }
            else if (BtDevice_GetNumberOfHandsetsConnectedOverBredr() > appConfigMaxNumOfHandsetsCanConnect())
            {
                /* If we have number of connected ACLs more than handsets we can allow to connect 
               then authorise the hndset who opened the ACL first */
                if(BdaddrIsSame(bd_addr, &con_manager.handset_to_pair_with_bdaddr))
                {
                    DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW handset");
                    return TRUE;
                }
                else
                {
                    DEBUG_LOG("conManagerIsBredrAddressAuthorised, REJECT");
                    return FALSE;
                }
            }
            DEBUG_LOG("conManagerIsBredrAddressAuthorised, ALLOW handset");
            return TRUE;
        }
    }

    DEBUG_LOG("conManagerIsBredrAddressAuthorised, REJECT");
    return FALSE;
}

/*! \brief Decide whether we allow connection to a given transport 
    (BR/EDR or BLE)
 */
static bool conManagerIsTransportAuthorised(cm_transport_t transport)
{
    return (con_manager.connectable_transports & transport) == transport;
}

/*! \brief Decide whether we allow a device to connect a given protocol
 */

static bool conManagerIsConnectionAuthorised(const bdaddr* bd_addr, dm_protocol_id protocol_id, uint32 channel, bool incoming)
{
    cm_transport_t transport_mask = cm_transport_bredr;
    
    if(protocol_id == protocol_le_l2cap)
        transport_mask = cm_transport_ble;

    if(conManagerIsTransportAuthorised(transport_mask))
    {
        if(transport_mask == cm_transport_bredr)
        {
            return conManagerIsBredrAddressAuthorised(bd_addr, protocol_id, channel, incoming);
        }
        else
        {
            DEBUG_LOG("conManagerIsConnectionAuthorised, ALLOW BLE");
            return TRUE;
        }
    }
    
    return FALSE;
}

/*! \brief Handle authentication.
 */
static void ConManagerHandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind)
{
    bool authorise;

    DEBUG_LOG("ConManagerHandleClSmAuthoriseIndication, protocol %d, channel %d, incoming %d",
                 ind->protocol_id, ind->channel, ind->incoming);

    authorise = conManagerIsConnectionAuthorised(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming);

    ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, authorise);
}

/*! 
    \brief Handle mode change event for a remote device
*/
static void conManagerHandleDmModeChangeEvent(bdaddr *addr, uint8 mode, uint16 interval)
{
    tp_bdaddr vm_addr;
    BdaddrTpFromBredrBdaddr(&vm_addr, addr);
    DEBUG_LOG("conManagerHandleDmModeChangeEvent addr=%x,%x,%x interval=%u mode=%d",
              addr->nap, addr->uap, addr->lap, interval, mode);

    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&vm_addr);

    if(connection)
    {
      /* Preserve the mode change parameters */
      connection->mode = mode;
      connection->sniff_interval = interval;
    }
}

/******************************************************************************/
bool ConManagerHandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled)
{
    switch (id)
    {
        case CL_SM_AUTHORISE_IND:
            if (!already_handled)
            {
                ConManagerHandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message);
            }
            return TRUE;

        case CL_DM_ACL_OPENED_IND:
        {
            CL_DM_ACL_OPENED_IND_T * ind = (CL_DM_ACL_OPENED_IND_T*)message;
            tp_bdaddr tpaddr;
            BdaddrTpFromTypedAndFlags(&tpaddr, &ind->bd_addr, ind->flags);
            DEBUG_LOG("ConManagerHandleAclOpenedIndication, enum:hci_status:%d, flags:0x%x, cod 0x%x", ind->status, ind->flags, ind->dev_class);
            ConManagerHandleAclOpenedIndication(&tpaddr, ind->status == hci_success, ind->flags, ind->flags & DM_ACL_FLAG_INCOMING, ind->conn_interval, ind->conn_latency);
            return TRUE;
        }

        case CL_DM_ACL_CLOSED_IND:
        {
            CL_DM_ACL_CLOSED_IND_T *ind = (CL_DM_ACL_CLOSED_IND_T*)message;
            tp_bdaddr tpaddr;
            BdaddrTpFromTypedAndFlags(&tpaddr, &ind->taddr, ind->flags);
            ConManagerHandleAclClosedIndication(&tpaddr, ind->status);
            return TRUE;
        }

        case CL_DM_ACL_CLOSE_CFM:
        {
            tp_bdaddr tpaddr;
            CL_DM_ACL_CLOSE_CFM_T * cfm = (CL_DM_ACL_CLOSE_CFM_T *)message;
            BdaddrTpFromTypedAndFlags(&tpaddr, &cfm->taddr, cfm->flags);
            ConManagerHandleAclCloseCfm(&tpaddr, cfm->status, cfm->flags);
            return TRUE;
        }

        case CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND:
        {
            CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *ind = (CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *)message;

            ConnectionDmBleAcceptConnectionParUpdateResponse(TRUE, &ind->taddr,
                                                             ind->id,
                                                             ind->conn_interval_min, ind->conn_interval_max,
                                                             ind->conn_latency,
                                                             ind->supervision_timeout);
            return TRUE;
        }

        case CL_DM_BLE_CONNECTION_UPDATE_COMPLETE_IND:
        {
            CL_DM_BLE_CONNECTION_UPDATE_COMPLETE_IND_T *ind = (CL_DM_BLE_CONNECTION_UPDATE_COMPLETE_IND_T*)message;
            conManagerHandleBleConnectionUpdateCompleteInd(&ind->taddr, ind->status, ind->conn_interval, ind->conn_latency);
            return TRUE;
        }

        case CL_DM_MODE_CHANGE_EVENT:
        {
            CL_DM_MODE_CHANGE_EVENT_T * cfm = (CL_DM_MODE_CHANGE_EVENT_T *)message;
            conManagerHandleDmModeChangeEvent(&cfm->bd_addr, cfm->mode, cfm->interval);
            return TRUE;
        }

        default:
            return already_handled;
    }
}
/******************************************************************************/
static void conManagerHandleScanManagerPauseCfm(void)
{
    con_manager.is_le_scan_paused = TRUE;
    conManagerPrepareForConnectionComplete();
}

/******************************************************************************/
static void conManagerHandleInternalAclOpenReq(const CON_MANAGER_INTERNAL_MSG_OPEN_TP_ACL_T *internal)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(&internal->tpaddr);

    if (ACL_CONNECTING_INTERNAL == conManagerGetConnectionState(connection))
    {
        DEBUG_LOG("conManagerHandleInternalAclOpenReq");
        ConManagerDebugAddressVerbose(&internal->tpaddr);

        if (TRANSPORT_BLE_ACL == internal->tpaddr.transport)
        {
            ConManagerApplyQosPreConnect(connection);
        }

        ConManagerSetConnectionState(connection, ACL_CONNECTING);
        conManagerSendOpenTpAclRequest(&internal->tpaddr);
    }
    else
    {
        DEBUG_LOG("conManagerHandleInternalAclOpenReq. Connection gone inactive. State:%d",
                        conManagerGetConnectionState(connection));
        ConManagerDebugAddressVerbose(&internal->tpaddr);

        /* Now we have no links, resume LE if neccesary */
        if(!conManagerIsConnectingBle())
        {
            conManagerResumeLeScanIfPaused();
        }
    }
}


/******************************************************************************/
/*! \brief Connection manager message handler.
 */
static void ConManagerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case LE_SCAN_MANAGER_PAUSE_CFM:
            conManagerHandleScanManagerPauseCfm();
            break;

        case CON_MANAGER_INTERNAL_MSG_OPEN_TP_ACL:
            conManagerHandleInternalAclOpenReq((const CON_MANAGER_INTERNAL_MSG_OPEN_TP_ACL_T *)message);
            break;

        case PAIRING_PAIR_CFM:
        default:
            break;
    }
}

/******************************************************************************/
bool ConManagerInit(Task init_task)
{
    DEBUG_LOG("ConManagerInit");
    memset(&con_manager, 0, sizeof(conManagerTaskData));
    ConManagerConnectionInit();
    conManagerNotifyInit();
    ConnectionManagerQosInit();

    /* Set up task handler */
    con_manager.task.handler = ConManagerHandleMessage;

    /*Set Pause Status as FALSE in init*/
    con_manager.is_le_scan_paused = FALSE;

    /* Default to allow BR/EDR connection until told otherwise */
    ConManagerAllowConnection(cm_transport_bredr, TRUE);

    /* setup role switch policy */
    ConManagerSetupRoleSwitchPolicy();
    UNUSED(init_task);
    return TRUE;
}

/******************************************************************************/
Task ConManagerGetConManagerTask(void)
{
    return &con_manager.task;
}

/******************************************************************************/
bool ConManagerIsConnected(const bdaddr *addr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if(connection)
    {
        return (conManagerGetConnectionState(connection) == ACL_CONNECTED);
    }
    return FALSE;
}

/******************************************************************************/
bool ConManagerIsTpConnected(const tp_bdaddr *tpaddr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    return (conManagerGetConnectionState(connection) == ACL_CONNECTED);
}

/******************************************************************************/
bool ConManagerIsAclLocal(const bdaddr *addr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if(connection)
    {
        return conManagerConnectionIsLocallyInitiated(connection);
    }
    return FALSE;
}

bool ConManagerIsTpAclLocal(const tp_bdaddr *tpaddr)
{
    const cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    return conManagerConnectionIsLocallyInitiated(connection);
}

/******************************************************************************/
void ConManagerSetLpState(const bdaddr *addr, lpPerConnectionState lp_state)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    conManagerSetLpState(connection, lp_state);
}

/******************************************************************************/
void ConManagerSetLpStateTp(const tp_bdaddr *addr, lpPerConnectionState lp_state)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(addr);
    conManagerSetLpState(connection, lp_state);
}


/******************************************************************************/
bool ConManagerGetLpState(const bdaddr *addr, lpPerConnectionState *lp_state)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);

    return conManagerGetLpState(connection, lp_state);
}

/******************************************************************************/
bool ConManagerGetLpStateTp(const tp_bdaddr *addr, lpPerConnectionState *lp_state)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(addr);

    return conManagerGetLpState(connection, lp_state);
}

/******************************************************************************/
bool ConManagerGetPowerMode(const tp_bdaddr *tpaddr,lp_power_mode* mode)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if(connection && mode)
    {
        *mode = connection->mode;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************/
bool ConManagerGetSniffInterval(const tp_bdaddr *tpaddr, uint16* sniff_interval)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if(connection && sniff_interval)
    {
        *sniff_interval = connection->sniff_interval;
        return TRUE;
    }
    else
    {
       return FALSE;
    }
}

/******************************************************************************/
bool ConManagerGetConnInterval(const tp_bdaddr *tpaddr, uint16* conn_interval)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if(connection && conn_interval)
    {
        *conn_interval = connection->conn_interval;
        return TRUE;
    }
    else
    {
       return FALSE;
    }
}

/******************************************************************************/
bool ConManagerGetSlaveLatency(const tp_bdaddr *tpaddr, uint16* slave_latency)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBdAddr(tpaddr);
    if(connection && slave_latency)
    {
        *slave_latency = connection->slave_latency;
        return TRUE;
    }
    else
    {
       return FALSE;
    }
}

/******************************************************************************/
void ConManagerAllowHandsetConnect(bool allowed)
{
    con_manager.handset_connect_allowed = allowed;

    if(con_manager.handset_connect_allowed)
    {
        /* Indicate to observer client that handset connection is allowed */
        conManagerNotifyAllowedConnectionsObservers(cm_handset_allowed);
    }
    else
    {
        /* Indicate to observer client that handset connection is not allowed */
        conManagerNotifyAllowedConnectionsObservers(cm_handset_disallowed);
        
    }
}

/******************************************************************************/
bool ConManagerIsHandsetConnectAllowed(void)
{
    return con_manager.handset_connect_allowed;
}

/******************************************************************************/
void ConManagerAllowConnection(cm_transport_t transport_mask, bool enable)
{
    if(enable)
    {
        con_manager.connectable_transports |= transport_mask;
    }
    else
    {
        con_manager.connectable_transports &= ~transport_mask;
    }

    if((transport_mask & cm_transport_ble) == cm_transport_ble)
    {
        LeAdvertisingManager_EnableConnectableAdvertising(&con_manager.task, enable);
    }
}

/******************************************************************************/
bool ConManagerIsConnectionAllowed(cm_transport_t transport_mask)
{
    return conManagerIsTransportAuthorised(transport_mask);
}

/******************************************************************************/
void ConManagerHandsetPairingMode(bool allowed)
{
    con_manager.handset_pairing_mode = allowed;
}

/******************************************************************************/
bool ConManagerIsHandsetPairingMode(void)
{
    return con_manager.handset_pairing_mode;
}

/******************************************************************************/
uint16 *ConManagerCreateTpAcl(const tp_bdaddr *tpaddr)
{
    return ConManagerCreateAclImpl(tpaddr);
}

/******************************************************************************/
void ConManagerReleaseTpAcl(const tp_bdaddr *tpaddr)
{
    conManagerReleaseAclImpl(tpaddr);
}

/******************************************************************************/
bool ConManagerAnyTpLinkConnected(cm_transport_t transport_mask)
{
    return conManagerAnyLinkInState(transport_mask, ACL_CONNECTED);
}

/******************************************************************************/
void ConManagerTerminateAllAcls(Task requester)
{
    DEBUG_LOG("ConManagerTerminateAllAcls");

    PanicFalse(!con_manager.forced_disconnect_task || (con_manager.forced_disconnect_task  == requester));
    con_manager.forced_disconnect_task = requester;
    /* Address is ignored, but can't pass a NULL pointer */
    bdaddr addr = {0};
    ConnectionDmAclDetach(&addr, hci_error_oetc_user, TRUE);
}

/******************************************************************************/
void ConManagerDisconnectAllLeConnectionsRequest(Task requester)
{
    bool have_le_connection = FALSE;
    cm_list_iterator_t iterator;
    cm_connection_t* connection = ConManagerListHeadConnection(&iterator);

    DEBUG_LOG("ConManagerDisconnectAllLeConnections");

    PanicFalse(!con_manager.all_le_disconnect_requester);

    while (connection)
    {
        cm_connection_state_t state = conManagerGetConnectionState(connection);

        if (   connection->tpaddr.transport == TRANSPORT_BLE_ACL
            && state != ACL_DISCONNECTED
            && state != ACL_DISCONNECTED_LINK_LOSS)
        {
            have_le_connection = TRUE;
            con_manager.all_le_disconnect_requester = requester;
            conManagerReleaseAclImpl(&connection->tpaddr);
        }

        connection = ConManagerListNextConnection(&iterator);
    }

    if (!have_le_connection)
    {
        MessageSend(requester, CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM, NULL);
    }
}

/******************************************************************************/
void ConManagerSetQlmpConnectStatus(const bdaddr *addr, bool qlmp_connected)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if(connection)
    {
        connection->bitfields.qlmp_connected = qlmp_connected;
    }
}

/******************************************************************************/
void ConManagerSetQhsSupportStatus(const bdaddr *addr, bool qhs_supported)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if(connection)
    {
        connection->bitfields.qhs_supported = qhs_supported;
    }
}

/******************************************************************************/
void ConManagerSetQhsConnectStatus(const bdaddr *addr, bool qhs_connected)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if(connection)
    {
        connection->bitfields.qhs_connected = qhs_connected;
    }

}

/******************************************************************************/
bool ConManagerGetQhsConnectStatus(const bdaddr *addr)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    return connection ? connection->bitfields.qhs_connected : FALSE;
}

/******************************************************************************/
void ConManagerSetFastExitSniffSubrateSupportStatus(const bdaddr *addr, bool supported)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    if (connection)
    {
        connection->bitfields.fast_exit_sniff_subrate_supported = supported;
    }
}

/******************************************************************************/
bool ConManagerGetFastExitSniffSubrateSupportStatus(const bdaddr *addr)
{
    cm_connection_t *connection = ConManagerFindConnectionFromBredrBdaddr(addr);
    return connection ? connection->bitfields.fast_exit_sniff_subrate_supported : FALSE;
}

/******************************************************************************/
void ConManager_SetAuthoriseCallback(deviceType dev_type, con_manager_authorise_callback_t callback)
{
    /* Currently only supported for handset devices */
    PanicFalse(dev_type == DEVICE_TYPE_HANDSET);
    
    con_manager.handset_authorise_callback = callback;
}

/******************************************************************************/
void ConManager_SetPageTimeout(uint16 page_timeout)
{
    con_manager.page_timeout = page_timeout;
}

/******************************************************************************/
bool ConManager_IterateFirstActiveConnection(cm_connection_iterator_t* iterator, tp_bdaddr *addr)
{
    cm_connection_t* conn = ConManagerFindFirstActiveLink(cm_transport_all);

    if (iterator)
    {
        if (conn)
        {
            iterator->_state = (void *)conn;
            if (addr)
            {
                *addr = *ConManagerGetConnectionTpAddr(conn);
            }
            return TRUE;
        }
        else
        {
            iterator->_state = NULL;
        }
    }
    return FALSE;
}

/******************************************************************************/
bool ConManager_IterateNextActiveConnection(cm_connection_iterator_t* iterator, tp_bdaddr *addr)
{
    cm_connection_t *connection;

    if (iterator)
    {
        connection = (cm_connection_t *)iterator->_state;

        if (connection)
        {
            connection = ConManagerFindNextActiveLink(connection, cm_transport_all);

            if (connection)
            {
                iterator->_state = (void *)connection;
                if (addr)
                {
                    *addr = *ConManagerGetConnectionTpAddr(connection);
                }
                return TRUE;
            }
        }
        iterator->_state = NULL;
    }
    return FALSE;
}
