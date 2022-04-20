/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   C:\work\src\csa\vmbi_critical\earbud\src\earbud_init_bt.h
\brief      Short description.

ADK version

*/

#include "earbud_init_bt.h"

#include "earbud_init.h"
#include "bt_device_class.h"
#include "earbud_config.h"
#include <connection_no_ble.h>
#include <unexpected_message.h>
#include <system_state.h>
#include <connection_message_dispatcher.h>
#include <app_task.h>
#include <app/bluestack/dm_prim.h>
#include <multidevice.h>

#include <panic.h>
#include <logging.h>

/* Needed only for message dispatcher registration */
#include <le_scan_manager.h>
#include <pairing.h>
#include <connection_manager.h>
#include <link_policy.h>
#include <authentication.h>
#include <le_advertising_manager.h>
#include "earbud_test.h"
#include <peer_find_role.h>
#include <local_addr.h>
#include <mirror_profile.h>
#include <fast_pair.h>
#include <bt_device.h>
#include "earbud_sm.h"

/*!< Structure used while initialising */
initData    app_init;

#ifdef INCLUDE_MIRRORING

#define IS_ACL_DISCONNECT_FOR_BLE(flags) (DM_ACL_FLAG_ULP & flags)
/*! \brief This function checks the address received in CL_DM_ACL_CLOSED_IND and
    if the address belongs to the local device it changes it to the address of
    the peer device.

    Rarely, if dynamic handover fails, the stack can send a CL_DM_ACL_CLOSED_IND
    to the application with the local device's BR/EDR BD_ADDR, instead of the
    peer earbud's BR/EDR BD_ADDR. This occurs due to the handling of address
    swapping during handover. To work-around this issue, this function changes
    any CL_DM_ACL_CLOSED_IND containing this device's BR/EDR BD_ADDR to the
    address of the peer earbud's BR/EDR BD_ADDR. This means the disconnection is
    handled correctly in the application.

    \param[in] pointer to CL_DM_ACL_CLOSED_IND_T message
*/
static bool appValidateAddressInDisconnetInd(CL_DM_ACL_CLOSED_IND_T *ind)
{
    bdaddr my_addr;
    bool ret_val=FALSE;

    BdaddrSetZero(&my_addr);

    /* Check if the received address is public */
    if(ind->taddr.type == TYPED_BDADDR_PUBLIC &&
        !IS_ACL_DISCONNECT_FOR_BLE(ind->flags) &&
        appDeviceGetMyBdAddr(&my_addr))
    {
        /* Check if the address received in CL_DM_ACL_CLOSED_IND_T message and
         * local device address is same
         */
        if(BdaddrIsSame(&my_addr, &ind->taddr.addr))
        {
            /* If the address received is same as local device address then update it
             * to peer device address */
            if(appDeviceGetPeerBdAddr(&ind->taddr.addr))
            {
                ret_val=TRUE;
                DEBUG_LOG_VERBOSE("appValidateAddressInDisconnetInd, Address in CL_DM_ACL_CLOSED_IND updated to addr %04x,%02x,%06lx",
                          ind->taddr.addr.nap,
                          ind->taddr.addr.uap,
                          ind->taddr.addr.lap);
            }
        }
    }

    return ret_val;
}

#endif


/*! \brief Forward CL_INIT_CFM message to the init task handler. */
static void appInitFwdClInitCfm(const CL_INIT_CFM_T * cfm)
{
    CL_INIT_CFM_T *copy = PanicUnlessNew(CL_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(SystemState_GetTransitionTask(), CL_INIT_CFM, copy);
}

/*! \brief Handle Connection library confirmation message */
static void appInitHandleClInitCfm(const CL_INIT_CFM_T *cfm)
{
    if (cfm->status != success)
        Panic();

    /* Set the class of device to indicate this is a headset */
    ConnectionWriteClassOfDevice(AUDIO_MAJOR_SERV_CLASS | RENDER_MAJOR_SERV_CLASS |
                                 AV_MAJOR_DEVICE_CLASS | HEADSET_MINOR_DEVICE_CLASS
#ifndef INCLUDE_MIRRORING
                                | CAPTURING_MAJOR_SERV_CLASS /* for A2DP SRC */
#endif
                                 );

    /* Allow SDP without security, requires authorisation */
    ConnectionSmSetSecurityLevel(0, 1, ssp_secl4_l0, TRUE, TRUE, FALSE);

    /* Reset security mode config - always turn off debug keys on power on */
    ConnectionSmSecModeConfig(appGetAppTask(), cl_sm_wae_acl_owner_none, FALSE, TRUE);


    appInitFwdClInitCfm(cfm);
}

/*! \brief Connection library Message Handler

    This function is the main message handler for the main application task, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void appHandleClMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_V_VERBOSE("appHandleClMessage called, message id = 0x%x", id);
    /* Handle Connection Library messages that are not sent directly to
       the requestor */
    if (CL_MESSAGE_BASE <= id && id < CL_MESSAGE_TOP)
    {
        bool handled = FALSE;

        if (id == CL_INIT_CFM)
        {
            appInitHandleClInitCfm((const CL_INIT_CFM_T *)message);
            return;
        }

#ifdef INCLUDE_MIRRORING
        if(id == CL_DM_ACL_CLOSED_IND)
        {
            appValidateAddressInDisconnetInd((CL_DM_ACL_CLOSED_IND_T *)message);
        }
#endif

        /* Pass connection library messages in turn to the modules that
           are interested in them.
         */
        handled |= LeScanManager_HandleConnectionLibraryMessages(id, message, handled);
        handled |= Pairing_HandleConnectionLibraryMessages(id, message, handled);
        handled |= ConManagerHandleConnectionLibraryMessages(id, message, handled);
        handled |= appLinkPolicyHandleConnectionLibraryMessages(id, message, handled);
        handled |= appAuthHandleConnectionLibraryMessages(id, message, handled);
        handled |= LeAdvertisingManager_HandleConnectionLibraryMessages(id, message, handled);
#ifndef DISABLE_TEST_API
        handled |= appTestHandleConnectionLibraryMessages(id, message, handled);
#endif
        handled |= PeerFindRole_HandleConnectionLibraryMessages(id, message, handled);
        handled |= LocalAddr_HandleConnectionLibraryMessages(id, message, handled);
        handled |= MirrorProfile_HandleConnectionLibraryMessages(id, message, handled);
#ifdef INCLUDE_FAST_PAIR
        handled |= FastPair_HandleConnectionLibraryMessages(id, message, handled);
#endif
        handled |= BtDevice_HandleConnectionLibraryMessages(id, message, handled);
        handled |= appSmHandleConnectionLibraryMessages(id, message, handled);

        if (handled)
        {
            return;
        }
    }

    DEBUG_LOG_VERBOSE("appHandleClMessage called but unhandled, message id = %d", id);
    UnexpectedMessage_HandleMessage(id);
}

/*! \brief Connection library initialisation */
bool appConnectionInit(Task init_task)
{
    UNUSED(init_task);

    static const msg_filter filter = {msg_group_acl | msg_group_mode_change};
    ConnectionMessageDispatcher_Init();

    /* Initialise the Connection Manager */
#if defined(APP_SECURE_CONNECTIONS)
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigEarbudMaxDevicesSupported(), CONNLIB_OPTIONS_SC_ENABLE);
#else
    ConnectionInitEx3(ConnectionMessageDispatcher_GetHandler(), &filter, appConfigEarbudMaxDevicesSupported(), CONNLIB_OPTIONS_NONE);
#endif

    ConnectionMessageDispatcher_RegisterInitClient(&app_init.task);

    return TRUE;
}

#ifdef USE_BDADDR_FOR_LEFT_RIGHT
bool appConfigInit(Task init_task)
{
    /* Get local device address */
    ConnectionReadLocalAddr(init_task);

    return TRUE;
}

bool appInitHandleReadLocalBdAddrCfm(Message message)
{
    CL_DM_LOCAL_BD_ADDR_CFM_T *cfm = (CL_DM_LOCAL_BD_ADDR_CFM_T *)message;
    if (cfm->status != success)
        Panic();

    InitGetTaskData()->appInitIsLeft = cfm->bd_addr.lap & 0x01;

    DEBUG_LOG_INFO("appInit, bdaddr %04x:%02x:%06x left %d, right %d",
                    cfm->bd_addr.nap, cfm->bd_addr.uap, cfm->bd_addr.lap, appConfigIsLeft(), appConfigIsRight());

    Multidevice_SetType(multidevice_type_pair);
    Multidevice_SetSide(appConfigIsLeft() ? multidevice_side_left : multidevice_side_right);

    return TRUE;
}
#endif



void Earbud_StartBtInit(void)
{
    app_init.task.handler = appHandleClMessage;
}

bool Earbud_RegisterForBtMessages(Task init_task)
{
    UNUSED(init_task);

    return TRUE;
}
