/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "connection_manager_data.h"
#include "connection_manager_msg.h"
#include "connection_manager_qos.h"
#include "connection_manager_config.h"
#include "connection_manager_list.h"

#include <logging.h>
#include <panic.h>
#include <message.h>
#include <app/bluestack/dm_prim.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(con_manager_internal_msg_id_t)

/*! @{ Macros to make connection manager messages. */
#define MAKE_CM_MSG(TYPE) MESSAGE_MAKE(message, TYPE##_T)

#define MAKE_PRIM_T(TYPE) MESSAGE_MAKE(prim,TYPE##_T); \
                            prim->type = TYPE;

#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); \
                            prim->common.op_code = TYPE; \
                            prim->common.length = sizeof(TYPE##_T);
/*! @} */

/******************************************************************************/
void conManagerSendWritePageTimeout(uint16 page_timeout)
{
    DEBUG_LOG("conManagerSendWritePageTimeout");
    
    MAKE_PRIM_C(DM_HCI_WRITE_PAGE_TIMEOUT_REQ);
    prim->page_timeout = page_timeout;
    VmSendDmPrim(prim);
}

/******************************************************************************/
void conManagerSendOpenTpAclRequest(const tp_bdaddr* tpaddr)
{
    /* Send DM_ACL_OPEN_REQ to open ACL manually */
    MAKE_PRIM_T(DM_ACL_OPEN_REQ);
    BdaddrConvertTypedVmToBluestack(&prim->addrt, &tpaddr->taddr);
    
    prim->flags = 0;
    
    if(tpaddr->transport == TRANSPORT_BLE_ACL)
    {
        prim->flags |= DM_ACL_FLAG_ULP;
    }
    
    VmSendDmPrim(prim);
}

/******************************************************************************/
void conManagerSendOpenTpAclRequestInternally(cm_connection_t *connection)
{
    MAKE_CM_MSG(CON_MANAGER_INTERNAL_MSG_OPEN_TP_ACL);
    message->tpaddr = connection->tpaddr;

    DEBUG_LOG("conManagerSendOpenTpAclRequestInternally");

    MessageSend(ConManagerGetConManagerTask(), CON_MANAGER_INTERNAL_MSG_OPEN_TP_ACL, message);
    ConManagerSetConnectionState(connection, ACL_CONNECTING_INTERNAL);
}

/******************************************************************************/
void conManagerSendCloseTpAclRequest(const tp_bdaddr* tpaddr, bool force)
{
    /* Send DM_ACL_CLOSE_REQ to relinquish control of ACL */
    MAKE_PRIM_T(DM_ACL_CLOSE_REQ);
    BdaddrConvertTypedVmToBluestack(&prim->addrt, &tpaddr->taddr);

    prim->flags = 0;
    /* Reason ignored unless force flag set. Initialise so prim_log never
       contains a confusing reason (uninitialised value) */
    prim->reason = HCI_ERROR_OETC_USER;

    if(force)
    {
        prim->flags |= DM_ACL_FLAG_FORCE;
    }

    if(tpaddr->transport == TRANSPORT_BLE_ACL)
    {
        prim->flags |= DM_ACL_FLAG_ULP;
    }
    
    VmSendDmPrim(prim);
}

/******************************************************************************/
void ConManagerSetupRoleSwitchPolicy(void)
{
    static uint16 connectionDmRsTable = 0;
    MAKE_PRIM_T(DM_LP_WRITE_ROLESWITCH_POLICY_REQ);
    prim->version = 0;
    prim->length = 1;
    prim->rs_table = (uint16 *)&connectionDmRsTable;
    VmSendDmPrim(prim);
}

/******************************************************************************/
void ConManagerSendCloseAclRequest(const bdaddr *addr, bool force)
{
    tp_bdaddr tpaddr;
    BdaddrTpFromBredrBdaddr(&tpaddr, addr);
    conManagerSendCloseTpAclRequest(&tpaddr, force);
}

/******************************************************************************/
void conManagerSendInternalMsgUpdateQos(cm_connection_t* connection)
{
    Task task = ConManagerGetTask(connection);

    DEBUG_LOG("conManagerSendInternalMsgUpdateQos");

    if(task)
    {
        MessageCancelFirst(task, CON_MANAGER_INTERNAL_MSG_UPDATE_QOS);
        MessageSend(task, CON_MANAGER_INTERNAL_MSG_UPDATE_QOS, NULL);
    }
}

/******************************************************************************/
void conManagerSendInternalMsgUpdateQosDelayed(cm_connection_t* connection)
{
    Task task = ConManagerGetTask(connection);

    if(task)
    {
        DEBUG_LOG("conManagerSendInternalMsgUpdateQosDelayed 0x%6x",
                  connection->tpaddr.taddr.addr.lap);

        /* Start a timer before applying preferred LE connection parameters.
           BLE GATT service discovery initiated by GATT client device(handset)
           should have finished. Applying the LE connection parameters for audio 
           streaming QoS after the timeout ensures that data traffic over 
           LE do not cause glitches while audio streaming starts later */
        MessageCancelFirst(task, CON_MANAGER_INTERNAL_MSG_TIMER_UPDATE_QOS);
        MessageSendLater(task, 
                         CON_MANAGER_INTERNAL_MSG_TIMER_UPDATE_QOS, NULL,
                         D_SEC(appConfigDelayApplyBleParamsOnPairingSecs()));
    }
}

void conManagerSendInternalMsgApplyQos(cm_connection_t* connection)
{
    Task connection_task = ConManagerGetTask(connection);
    if (connection_task)
    {
        MessageCancelAll(connection_task, CON_MANAGER_INTERNAL_MSG_APPLY_QOS);
        MessageSendConditionally(connection_task, 
                                 CON_MANAGER_INTERNAL_MSG_APPLY_QOS, NULL, 
                                 &connection->le_update_in_progress);
    }
}



/******************************************************************************/
static void conManagerHandleUpdateQos(cm_connection_t *connection)
{
    ConManagerApplyQosOnConnect(connection);
}

static void conManagerHandleApplyQos(cm_connection_t *connection)
{
    conManagerSendParameterUpdate(connection);
}

static void conManagerHandleConnParamUpdateCfm(cm_connection_t *connection,
                                               const CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM_T *cfm)
{
    if (cfm->status != success)
    {
        /* Remove block on next update */
        connection->le_update_in_progress = 0;
    }
}

static void conManagerHandleQosTimeout(cm_connection_t *connection)
{
    connection->le_update_in_progress = 0;
}

/******************************************************************************/
void ConManagerConnectionHandleMessage(Task task, MessageId id, Message message)
{
    /* This is the per connection message handler. Get the connection from task id */
    cm_connection_t *connection = (cm_connection_t *)task;

    switch (id)
    {
        case CON_MANAGER_INTERNAL_MSG_UPDATE_QOS:
        case CON_MANAGER_INTERNAL_MSG_TIMER_UPDATE_QOS:
            conManagerHandleUpdateQos(connection);
            break;

        case CON_MANAGER_INTERNAL_MSG_APPLY_QOS:
            conManagerHandleApplyQos(connection);
            break;

        case CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM:
            conManagerHandleConnParamUpdateCfm(connection,
                                              (const CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM_T *)message);
            break;

        case CON_MANAGER_INTERNAL_MSG_QOS_TIMEOUT:
            conManagerHandleQosTimeout(connection);
            break;

        default:
            break;
    }
}
