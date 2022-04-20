/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy control of handset role.

This file controls the link role with the handset.
The link policy with the peer earbud in TWM products is controlled by the mirror profile.

For TWM, the handset must be master of the link to the primary earbud - mirroring
is only supported when the primary earbud is ACL slave. Therefore this code requests a
role switch to become slave and will prohibit further role switches once slave.

*/

#include "link_policy_private.h"

#include "logging.h"
#include "bdaddr.h"
#include "bt_device.h"
#include "app/bluestack/dm_prim.h"
#include "panic.h"

/*! Make and populate a bluestack DM primitive based on the type.

    \note that this is a multiline macro so should not be used after a
    control statement (if, while) without the use of braces
 */
#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->common.op_code = TYPE; prim->common.length = sizeof(TYPE##_T);

static void appLinkPolicyUpdateLinkSupervisionTimeout(const bdaddr *bd_addr)
{
    uint16 timeout;

    if (appDeviceIsPeer(bd_addr))
        timeout = appConfigEarbudLinkSupervisionTimeout() * 1000UL / 625;
    else
        timeout = appConfigDefaultLinkSupervisionTimeout() * 1000UL / 625;
    MAKE_PRIM_C(DM_HCI_WRITE_LINK_SUPERV_TIMEOUT_REQ);
    prim->handle = 0;
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->timeout = timeout;
    VmSendDmPrim(prim);
}

/*! \brief Send a prim directly to bluestack to role switch to slave.
    \param The address of remote device link.
    \param The role to request.
*/
static void role_switch(const bdaddr *bd_addr, hci_role new_role)
{
    ConnectionSetRoleBdaddr(&LinkPolicyGetTaskData()->task, bd_addr, new_role);
}

/*! \brief Prevent role switching

    Update the policy for the connection (if any) to the specified
    Bluetooth address, so as to prevent any future role switching.

    \param  bd_addr The Bluetooth address of the device
*/
static void prevent_role_switch(const bdaddr *bd_addr)
{
    MAKE_PRIM_C(DM_HCI_WRITE_LINK_POLICY_SETTINGS_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->link_policy_settings = ENABLE_SNIFF;
    VmSendDmPrim(prim);
}

/*! \brief Discover the current role

    \param  bd_addr The Bluetooth address of the remote device
*/
static void discover_role(const bdaddr *bd_addr)
{
    ConnectionGetRoleBdaddr(&LinkPolicyGetTaskData()->task, bd_addr);

}


void appLinkPolicyHandleClDmAclOpenedIndication(const bdaddr *bd_addr, bool is_ble, bool is_local)
{
    if (!is_ble)
    {
        if (appDeviceIsHandset(bd_addr))
        {
            discover_role(bd_addr);
        }

        /* Set default link supervision timeout if locally inititated (i.e. we're master) */
        if (is_local)
        {
            appLinkPolicyUpdateLinkSupervisionTimeout(bd_addr);
        }
    }
#if defined(INCLUDE_LEA_LINK_POLICY)
    else if (!is_local)
    {
        /* Incoming BLE connection */
        appLinkPolicyUpdatePowerTableDeferred(bd_addr);
    }
#endif
}


static void appLinkPolicySendMessageToDiscoverRoleLater(const bdaddr *bd_addr)
{
    DEBUG_LOG("appLinkPolicySendMessageToDiscoverRoleLater: addr:%06x",bd_addr->lap);
    lpTaskData *theLp = LinkPolicyGetTaskData();

    MAKE_LP_MESSAGE(LINK_POLICY_DISCOVER_ROLE);
    message->bd_addr = *bd_addr;
    MessageSendLater(&theLp->task, LINK_POLICY_DISCOVER_ROLE, message, LINK_POLICY_DISCOVER_ROLE_TIMEOUT_MS);
}

/*! Handle a change in role or discovery of role, triggering any further role
 * switches required */
static void appLinkPolicyHandleRole(const bdaddr *bd_addr, hci_status status, hci_role role)
{
    hci_role new_role = hci_role_dont_care;

    if(appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("appLinkPolicyHandleRole, peer enum:hci_status:%d enum:hci_role:%d",
                        status, role);

        if(status == hci_success && role == hci_role_master)
        {
            appLinkPolicyUpdateLinkSupervisionTimeout(bd_addr);
        }
    }

    if (appDeviceIsHandset(bd_addr))
    {
        DEBUG_LOG("appLinkPolicyHandleRole, handset enum:hci_status:%d enum:hci_role:%d",
                        status, role);

        switch (status)
        {
            case hci_success:
                if (role == hci_role_master)
                {
                    new_role = hci_role_slave;
                }
                else
                {
                    /* This is the desired topology, disallow role switches */
                    prevent_role_switch(bd_addr);
                }
            break;

            case hci_error_role_change_not_allowed:
            case hci_error_role_switch_failed:
            case hci_error_lmp_response_timeout:
            case hci_error_unrecognised:
            case hci_error_controller_busy:
            case hci_error_instant_passed:
            case hci_error_oetc_low_resource:
                 appLinkPolicySendMessageToDiscoverRoleLater(bd_addr);
            break;

            default:
            break;
        }
        if (new_role != hci_role_dont_care)
        {
            role_switch(bd_addr, new_role);
        }
    }
}

static void appLinkPolicyHandleScHostSupportOverrideCfm(const DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_CFM_T *cfm)
{
    if (cfm->status == hci_success)
    {
        DEBUG_LOG("appLinkPolicyHandleScHostSupportOverrideCfm, 0x%x:%d", cfm->bd_addr.lap, cfm->host_support_override);
    }
    else
    {
        Panic();
    }
}


bool appLinkPolicyHandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case CL_DM_ROLE_CFM:
        {
            const CL_DM_ROLE_CFM_T *cfm = message;
            appLinkPolicyHandleRole(&cfm->bd_addr, cfm->status, cfm->role);
        }
        return TRUE;

        case CL_DM_ROLE_IND:
        {
            const CL_DM_ROLE_IND_T *ind = message;
            appLinkPolicyHandleRole(&ind->bd_addr, ind->status, ind->role);
        }
        return TRUE;

        case DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_CFM:
            appLinkPolicyHandleScHostSupportOverrideCfm(message);
            return TRUE;

        case CL_DM_ULP_SET_PRIVACY_MODE_CFM:
        {
            CL_DM_ULP_SET_PRIVACY_MODE_CFM_T *cfm = (CL_DM_ULP_SET_PRIVACY_MODE_CFM_T *)message;
            PanicFalse(cfm->status == hci_success);
            return TRUE;
        }
    }
    return already_handled;
}

void appLinkPolicyHandleDiscoverRole(const LINK_POLICY_DISCOVER_ROLE_T * msg)
{
    discover_role(&msg->bd_addr);
}

void appLinkPolicyHandoverForceUpdateHandsetLinkPolicy(const bdaddr *bd_addr)
{
    /* Note that this prevents role switch, but allows sniff */
    prevent_role_switch(bd_addr);
    appLinkPolicyForceUpdatePowerTable(bd_addr);
}
