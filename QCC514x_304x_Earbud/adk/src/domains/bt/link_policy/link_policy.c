/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy manager general functionality and initialisation.
*/

#include "adk_log.h"
#include "link_policy_private.h"

#include <message.h>
#include <panic.h>
#include <bdaddr.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <app/bluestack/dm_prim.h>

LOGGING_PRESERVE_MESSAGE_TYPE(link_policy_internal_message_t)


/*!< Link Policy Manager data structure */
lpTaskData  app_lp;

/*! Make and populate a bluestack DM primitive based on the type.

    \note that this is a multiline macro so should not be used after a
    control statement (if, while) without the use of braces
 */
#define MAKE_PRIM_T(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->type = TYPE;

static void appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(const bdaddr *bd_addr, uint8 override_value)
{
    MAKE_PRIM_T(DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->host_support_override = override_value;
    VmSendDmPrim(prim);
    DEBUG_LOG("appLinkPolicyBredrSecureConnectionHostSupportOverrideSet 0x%x:%d", bd_addr->lap, override_value);
}

void appLinkPolicyHandleAddressSwap(void)
{
    typed_bdaddr bd_addr_primary = {.type = TYPED_BDADDR_PUBLIC, .addr = {0}};
    typed_bdaddr bd_addr_secondary = {.type = TYPED_BDADDR_PUBLIC, .addr= {0}};

    PanicFalse(appDeviceGetPrimaryBdAddr(&bd_addr_primary.addr));
    PanicFalse(appDeviceGetSecondaryBdAddr(&bd_addr_secondary.addr));
    PanicFalse(!BdaddrIsSame(&bd_addr_primary.addr, &bd_addr_secondary.addr));

#ifdef INCLUDE_SM_PRIVACY_1P2
    ConnectionDmUlpSetPrivacyModeReq(&bd_addr_primary, privacy_mode_device);
    ConnectionDmUlpSetPrivacyModeReq(&bd_addr_secondary, privacy_mode_device);
#endif

    /* By default, BR/EDR secure connections is disabled.
    TWM requires the link between the two earbuds to have BR/EDR secure connections
    enabled, so selectively enable SC for connections to the other earbud.
    The addresses of both earbuds need to be overridden, as the addresses of the
    two devices swap during handover. Handover will fail if both addresses
    are not overridden. */
    appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(&bd_addr_primary.addr, 0x01);
    appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(&bd_addr_secondary.addr, 0x01);
}

static void LinkPolicy_HandleDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T * ind)
{
    const bdaddr * addr = &ind->tpaddr.taddr.addr;

    if (ind->tpaddr.transport == TRANSPORT_BREDR_ACL)
    {
        if (appDeviceIsHandset(addr))
        {
            appLinkPolicyUpdatePowerTable(addr);
        }
    }
}

static void LinkPolicy_HandleConnectInd(const CON_MANAGER_TP_CONNECT_IND_T * ind)
{
    if (ind->tpaddr.transport == TRANSPORT_BLE_ACL);
    {
        tp_bdaddr bredraddress = {.transport=TRANSPORT_BREDR_ACL,
                                  .taddr.type = TYPED_BDADDR_PUBLIC,
                                  .taddr.addr=ind->tpaddr.taddr.addr};
        lpPerConnectionState bredr_state;
        lpPerConnectionState le_state;

        /* See if we have a stored state for the LE Link */
        if (ConManagerGetLpStateTp(&ind->tpaddr, &le_state))
        {
            if (le_state.pt_index != POWERTABLE_UNASSIGNED)
            {
                DEBUG_LOG_WARN("LinkPolicy_HandleConnectInd  LE Status was set ?");
                return;
            }
        }

        if (ConManagerGetLpStateTp(&bredraddress, &bredr_state))
        {
            if (bredr_state.pt_index == POWERTABLE_A2DP_STREAMING)
            {
                DEBUG_LOG_INFO("LinkPolicy_HandleConnectInd BREDR Streaming");
                le_state = bredr_state;
                ConManagerRequestDeviceQos(&ind->tpaddr, cm_qos_lea_idle);
                ConManagerSetLpStateTp(&ind->tpaddr, le_state);
            }
        }
        
    }
}

static void appLinkPolicyMessageHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);

    switch (id)
    {
        case CON_MANAGER_TP_DISCONNECT_IND:
            LinkPolicy_HandleDisconnectInd((const CON_MANAGER_TP_DISCONNECT_IND_T *)msg);
            break;

        case CON_MANAGER_TP_CONNECT_IND:
            LinkPolicy_HandleConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *)msg);
            break;

        case LINK_POLICY_SCHEDULED_UPDATE:
            appLinkPolicyForceUpdatePowerTable(NULL);
            break;

        case LINK_POLICY_DISCOVER_ROLE:
            appLinkPolicyHandleDiscoverRole((const LINK_POLICY_DISCOVER_ROLE_T *)msg);
            break;

        default:
            appLinkPolicyHandleConnectionLibraryMessages(id, msg, FALSE);
            break;
    }
}

void appLinkPolicyUpdatePowerTableDeferred(const bdaddr *bd_addr)
{
    UNUSED(bd_addr);

    DEBUG_LOG("appLinkPolicyUpdatePowerTableDeferred");

    MessageSend(LinkPolicyGetTask(), LINK_POLICY_SCHEDULED_UPDATE, NULL);
}

/*! \brief Initialise link policy manager. */
bool appLinkPolicyInit(Task init_task)
{
    lpTaskData *theLp = LinkPolicyGetTaskData();
    theLp->task.handler = appLinkPolicyMessageHandler;
    cm_transport_t transports = cm_transport_bredr;

#if defined(INCLUDE_LEA_LINK_POLICY)
    transports |= cm_transport_ble;
#endif

    ConManagerRegisterTpConnectionsObserver(transports, &theLp->task);


    UNUSED(init_task);
    return TRUE;
}


