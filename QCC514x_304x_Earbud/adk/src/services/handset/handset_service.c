/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service
*/

#include <bdaddr.h>
#include <vm.h>
#include <logging.h>
#include <task_list.h>

#include <bt_device.h>
#include <pairing.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <local_addr.h>
#include <profile_manager.h>
#include <pairing.h>
#include <timestamp_event.h>
#include <key_sync.h>
#include <device_list.h>
#ifdef INCLUDE_TWS
#include <device_sync.h>
#endif
#include <stdlib.h>
#include <ui.h>
#include <focus_device.h>

#include "device_db_serialiser.h"
#include "handset_service.h"
#include "handset_service_sm.h"
#include "handset_service_connectable.h"
#include "handset_service_config.h"
#include "handset_service_extended_advertising.h"
#include "handset_service_protected.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(handset_service_msg_t)
LOGGING_PRESERVE_MESSAGE_TYPE(handset_service_internal_msg_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(HANDSET_SERVICE, HANDSET_SERVICE_MESSAGE_END)

/*! Handset Service module data. */
handset_service_data_t handset_service;

/*
    Helper functions
*/

/*! Get if the handset service is in pairing mode. */
#define HandsetService_IsPairing() (HandsetService_Get()->pairing)

/*! Get handset service LE advertising data set select/release state */
#define HandsetService_GetLeAdvDataSetState() (HandsetService_Get()->ble_adv_state)

/*! Get handset service selected LE advertising data set */
#define HandsetService_GetLeAdvSelectedDataSet() (HandsetService_Get()->le_advert_data_set)

static void handsetService_SendLeConnectableIndication(bool connectable);
static void handsetService_DisconnectAllMessageHandler(Task task, MessageId id, Message message);

static const TaskData disconnect_all_task = {handsetService_DisconnectAllMessageHandler};

/*! Stores if the Handset can be paired.

    \param pairing The headset pairing state.
*/
static void HandsetService_SetPairing(bool pairing)
{ 
    HandsetService_Get()->pairing = pairing;
}

/*! \brief Disable advertising by releasing the LE advertising data set */
static void handsetService_DisableAdvertising(void)
{
    handset_service_data_t *hs = HandsetService_Get();

    HS_LOG("handsetService_DisableAdvertising, release set with handle=%p", hs->le_advert_handle);

    PanicFalse(LeAdvertisingManager_ReleaseAdvertisingDataSet(hs->le_advert_handle));
    
    hs->le_advert_handle = NULL;
    hs->ble_adv_state = handset_service_le_adv_data_set_state_releasing;
    
    HS_LOG("handsetService_DisableAdvertising,  handle=%p", hs->le_advert_handle);
}

/*! \brief Get advertising data set which needs to be selected */
static le_adv_data_set_t handsetService_GetLeAdvDataSetToBeSelected(void)
{    
    bool is_pairing = HandsetService_IsPairing();
    bool is_local_addr_public = LocalAddr_IsPublic();

    HS_LOG("handsetService_GetLeAdvDataSetToBeSelected, Is in pairing:%d, Is local address public:%d", is_pairing, is_local_addr_public);

    le_adv_data_set_t set;

    if(is_pairing || is_local_addr_public)
    {
        set = le_adv_data_set_handset_identifiable;
    }
    else
    {
        set = le_adv_data_set_handset_unidentifiable;
    }

    return set;
}

/*! \brief Enable advertising by selecting the LE advertising data set */
static void handsetService_EnableAdvertising(void)
{
    handset_service_data_t *hs = HandsetService_Get();

    le_adv_select_params_t adv_select_params;
    le_adv_data_set_handle adv_handle = NULL;

    HS_LOG("handsetService_EnableAdvertising, Le Adv State is enum:handset_service_le_adv_data_set_state_t:%x, Le Adv Selected Data Set is enum:le_adv_data_set_t:%x",
           hs->ble_adv_state, hs->le_advert_data_set);

    adv_select_params.set = handsetService_GetLeAdvDataSetToBeSelected();

    adv_handle = LeAdvertisingManager_SelectAdvertisingDataSet(HandsetService_GetTask(), &adv_select_params);
    
    hs->ble_adv_state = handset_service_le_adv_data_set_state_selecting;
    
    hs->le_advert_data_set = adv_select_params.set;

    if (adv_handle != NULL)
    {
        hs->le_advert_handle = adv_handle;

        HS_LOG("handsetService_EnableAdvertising. Selected set with handle=%p", hs->le_advert_handle);
    }
}


void HandsetService_ResolveTpaddr(const tp_bdaddr *tpaddr, tp_bdaddr *resolved_tpaddr)
{
    if((tpaddr->transport == TRANSPORT_BLE_ACL) 
        && (tpaddr->taddr.type == TYPED_BDADDR_RANDOM))
    {
        if(VmGetPublicAddress(tpaddr, resolved_tpaddr))
        {
            return;
        }
        else
        {
            HS_LOG("HandsetService_ResolveTpaddr. failed for addr [%04x,%02x,%06lx]",
                    tpaddr->taddr.addr.nap, tpaddr->taddr.addr.uap, tpaddr->taddr.addr.lap);
        }
    }
    *resolved_tpaddr = *tpaddr;
}

/*! \brief Updates the BLE advertising data 
    \return TRUE if Advertising Data shall be updated, FALSE otherwise.  
*/
bool handsetService_UpdateAdvertisingData(void)
{
    handset_service_le_adv_data_set_state_t le_adv_state = HandsetService_GetLeAdvDataSetState();
    
    if(   (handset_service_le_adv_data_set_state_releasing == le_adv_state) 
       || (handset_service_le_adv_data_set_state_selecting == le_adv_state) )
    {
        HS_LOG("handsetService_UpdateAdvertisingData. Le advertising data set select/release state is enum:handset_service_le_adv_data_set_state_t:%x",
               le_adv_state);
        return TRUE;
    }

    handset_service_data_t *hs = HandsetService_Get();
    bool is_le_connectable = HandsetService_IsBleConnectable();
    unsigned le_connections = HandsetServiceSm_GetLeAclConnectionCount();
    bool have_spare_le_connections = le_connections < handsetService_LeAclMaxConnections();
    le_adv_data_set_t data_set = handsetService_GetLeAdvDataSetToBeSelected();
    bool is_le_adv_data_set_update_needed = HandsetService_GetLeAdvSelectedDataSet() != data_set;
    bool pairing_possible = HandsetServiceSm_CouldDevicesPair();

    HS_LOG("handsetService_UpdateAdvertisingData. state enum:handset_service_le_adv_data_set_state_t:%x Connectable:%d Spare LE:%d PairingPossible:%d",
              le_adv_state, is_le_connectable, have_spare_le_connections, pairing_possible);

    if(hs->le_advert_handle)
    {
        bool disable_advertising = !is_le_connectable || is_le_adv_data_set_update_needed || !have_spare_le_connections || pairing_possible;

        HS_LOG("handsetService_UpdateAdvertisingData. Active data set is 0x%x. Requested data set is 0x%x. Disable:%d",
               hs->le_advert_data_set, data_set, disable_advertising);

        if (disable_advertising)
        {
            handsetService_DisableAdvertising();
            return TRUE;
        }
    }
    else
    {
        bool enable_advertising = is_le_connectable && have_spare_le_connections && !pairing_possible;

        HS_LOG("handsetService_UpdateAdvertisingData. There is no active data set. Enable:%d",
                enable_advertising);

        if (enable_advertising)
        {
            handsetService_EnableAdvertising();
            return TRUE;
        }
    }

    return FALSE;
}

/*! Try to find an active handset state machine for a device_t.

    \param device Device to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetSmForDevice(device_t device)
{
    handset_service_state_machine_t *sm_match = NULL;

    FOR_EACH_HANDSET_SM(sm)
    {
        if (   (sm->state != HANDSET_SERVICE_STATE_NULL)
            && (sm->handset_device == device))
        {
            sm_match = sm;
            break;
        }
    }

    return sm_match;
}

/*! Try to find an active BR/EDR handset state machine for an address.

    \param[in] addr BR/EDR address to search for.
    
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetSmForBredrAddr(const bdaddr *addr)
{
    handset_service_state_machine_t *sm_match = NULL;
    const bdaddr *bredr_bdaddr;

    DEBUG_LOG_VERBOSE("handsetService_GetSmForBredrAddr Searching for addr [%04x,%02x,%06lx]",
                        addr->nap, addr->uap, addr->lap);

    if (!BdaddrIsZero(addr))
    {
        FOR_EACH_HANDSET_SM(sm)
        {
            bredr_bdaddr = &sm->handset_addr;

            DEBUG_LOG_VERBOSE("handsetService_GetSmForBredrAddr Check SM [%p] state [%d] addr [%04x,%02x,%06lx]",
                              sm, sm->state, bredr_bdaddr->nap, bredr_bdaddr->uap, bredr_bdaddr->lap);

            if (   (sm->state != HANDSET_SERVICE_STATE_NULL)
                && BdaddrIsSame(bredr_bdaddr, addr))
            {
                sm_match = sm;
                break;
            }
        }
    }

    return sm_match;
}

/*! Try to find an active LE handset state machine for a typed address.

    This function will check both the type (PUBLIC or RANDOM) and the bdaddr
    match the LE address for a handset state machine.

    \param taddr Typed bdaddr to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetLeSmForTypedBdAddr(const typed_bdaddr *taddr)
{
    handset_service_state_machine_t *sm_match = NULL;

    DEBUG_LOG_VERBOSE("handsetService_GetLeSmForTypedBdAddr searching for type [%d] addr [%04x,%02x,%06lx]",
                       taddr->type, taddr->addr.nap, taddr->addr.uap, taddr->addr.lap);

    if (!BdaddrTypedIsEmpty(taddr))
    {
        FOR_EACH_HANDSET_SM(sm)
        {
            typed_bdaddr le_taddr = sm->le_addr.taddr;

            DEBUG_LOG_VERBOSE("handsetService_GetLeSmForTypedBdAddr Check SM [%p] state [%d] type [%d] addr [%04x,%02x,%06lx]",
                          sm, sm->state, le_taddr.type, le_taddr.addr.nap, le_taddr.addr.uap, le_taddr.addr.lap);

            if (   (sm->state != HANDSET_SERVICE_STATE_NULL)
                && BtDevice_BdaddrTypedIsSame(taddr, &le_taddr))
            {
                sm_match = sm;
                break;
            }
        }
    }

    return sm_match;
}

/*! Try to find an active handset state machine for a tp_bdaddr.

    Note: handset_service currently supports only one handset sm.

    \param tp_addr Address to search for.
    \return Pointer to the matching state machine, or NULL if no match.
*/
static handset_service_state_machine_t *handsetService_GetSmForTpBdAddr(const tp_bdaddr *tp_addr)
{
    handset_service_state_machine_t *sm = NULL;

    DEBUG_LOG("handsetService_GetSmForTpBdAddr transport [%d] type [%d] addr [%04x,%02x,%06lx]",
              tp_addr->transport, tp_addr->taddr.type,
              tp_addr->taddr.addr.nap, tp_addr->taddr.addr.uap, tp_addr->taddr.addr.lap);

    switch (tp_addr->transport)
    {
    case TRANSPORT_BREDR_ACL:
        {
            /* First try to match the BR/EDR address */
            sm = handsetService_GetSmForBredrAddr(&tp_addr->taddr.addr);
            if (!sm)
            {
                /* Second try to match the device_t handle */
                device_t dev = BtDevice_GetDeviceForBdAddr(&tp_addr->taddr.addr);
                if (dev)
                {
                    sm = handsetService_GetSmForDevice(dev);
                }
            }

            /* Third try to match to the LE address. */
            if (!sm)
            {
                sm = handsetService_GetLeSmForTypedBdAddr(&tp_addr->taddr);
            }
        }
        break;

    case TRANSPORT_BLE_ACL:
        {
            /* First try to match the LE address to a sm LE addr */
            sm = handsetService_GetLeSmForTypedBdAddr(&tp_addr->taddr);

            /* Second try to match the bdaddr component to a sm BR/EDR address */
            if (!sm)
            {
                tp_bdaddr resolved_tpaddr;
                HandsetService_ResolveTpaddr(tp_addr, &resolved_tpaddr);
                sm = handsetService_GetSmForBredrAddr(&resolved_tpaddr.taddr.addr);
            }
        }
        break;

    default:
        HS_LOG("handsetService_GetSmForTpBdAddr Unsupported transport type %d", tp_addr->transport);
    }
    
    return sm;
}

handset_service_state_machine_t *HandsetService_GetSmForBdAddr(const bdaddr *addr)
{
    tp_bdaddr tp_addr = {0};

    BdaddrTpFromBredrBdaddr(&tp_addr, addr);
    return handsetService_GetSmForTpBdAddr(&tp_addr);
}

/*! \brief Create a new instance of a handset state machine.

    This will return NULL if a new state machine cannot be created,
    for example if the maximum number of handsets already exists.

    \param device Device to create state machine for.

    \return Pointer to new state machine, or NULL if it couldn't be created.
*/
static handset_service_state_machine_t *handsetService_CreateSm(device_t device)
{
    handset_service_state_machine_t *new_sm = NULL;

    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state == HANDSET_SERVICE_STATE_NULL)
        {
            new_sm = sm;
            HandsetServiceSm_Init(new_sm);
            HandsetServiceSm_SetDevice(new_sm, device);
            BdaddrTpSetEmpty(&new_sm->le_addr);
            HandsetServiceSm_SetState(new_sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            /* A device already exists. Pairing may be possible if cross transport
               key derivation is used. */
            break;
        }
    }

    return new_sm;
}

/*! \brief Create a new instance of a handset state machine for a LE connection.

    This will return NULL if a new state machine cannot be created,
    for example if the maximum number of handsets already exists.

    \param addr Address to create state machine for.

    \return Pointer to new state machine, or NULL if it couldn't be created.
*/
static handset_service_state_machine_t *handsetService_CreateLeSm(const tp_bdaddr *addr)
{
    handset_service_state_machine_t *new_sm = NULL;

    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state == HANDSET_SERVICE_STATE_NULL)
        {
            new_sm = sm;
            HandsetServiceSm_Init(new_sm);
            new_sm->le_addr = *addr;
            HandsetServiceSm_SetState(new_sm, HANDSET_SERVICE_STATE_DISCONNECTED);

                /* If the address is resolvable then adding pairing can be an issue */
            bool random = (addr->taddr.type != TYPED_BDADDR_PUBLIC);
            bool resolvable = ((addr->taddr.addr.nap & 0xC000) == 0x4000);
            new_sm->pairing_possible = random && resolvable;

            if (new_sm->pairing_possible)
            {
                DEBUG_LOG("handsetService_CreateLeSm for 0x%06x - kicking off timeout", addr->taddr.addr.lap);
                MESSAGE_MAKE(message, HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT_T);

                message->address = *addr;
                message->sm = new_sm;
                MessageSendLater(HandsetService_GetTask(),
                                 HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT, message,
                                 handsetService_AdvertisingSuspensionForPairingMs());
            }
            break;
        }
    }

    return new_sm;
}

handset_service_state_machine_t *handsetService_FindOrCreateSm(const tp_bdaddr *tp_addr)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForTpBdAddr(tp_addr);
    
    if(!sm)
    {
        if (tp_addr->transport == TRANSPORT_BLE_ACL)
        {
            sm = handsetService_CreateLeSm(tp_addr);
        }
        else if(tp_addr->transport == TRANSPORT_BREDR_ACL)
        {
            if(!HandsetServiceSm_MaxBredrAclConnectionsReached())
            {
                device_t device = BtDevice_GetDeviceForBdAddr(&tp_addr->taddr.addr);
                sm = handsetService_CreateSm(device);
            }
        }
        else
        {
            Panic();
        }
    }
    
    return sm;
}

/* \brief Check if a new handset is allowed to connect

   This function will check if a new handset should be allowed to connect.

   Currently we do not support more than one handset connected at a time, so
   we must be able to reject or disconnect any other handset that tries to
   connect.

   A handset is considered connected if the BR/EDR ACL is connected.
   For example, if the ACL is connected but the BR/EDR profiles are connecting
   then it is considered to be connected.
*/
bool handsetService_CheckHandsetCanConnect(const bdaddr *addr)
{
    if(HandsetServiceSm_MaxBredrAclConnectionsReached())
    {
        handset_service_state_machine_t *sm = HandsetService_GetSmForBdAddr(addr);
        
        if(!sm || !HandsetServiceSm_IsBredrAclConnected(sm))
        {
            return FALSE;
        }
    }
    
    return TRUE;
}

bool HandsetService_IsBleConnected(void)
{
    unsigned num_connections = HandsetServiceSm_GetLeAclConnectionCount();

    return num_connections != 0;
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_CONNECT_REQ to a state machine. */
static void handsetService_InternalConnectReq(handset_service_state_machine_t *sm, uint32 profiles)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T);
    req->device = sm->handset_device;
    req->profiles = profiles;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_REQ, req);
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ to a state machine. */
static void handsetService_InternalDisconnectReq(handset_service_state_machine_t *sm,
    const bdaddr *addr, uint32 exclude)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T);
    req->addr = *addr;
    req->exclude = exclude;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ, req);
}

/*! \brief Send a HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ to a state machine. */
static void handsetService_InternalConnectStopReq(handset_service_state_machine_t *sm)
{
    MESSAGE_MAKE(req, HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T);
    req->device = sm->handset_device;
    MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ, req);
}

/*! \brief Helper function for starting a connect req. */
static void handsetService_ConnectReq(Task task, const bdaddr *addr, uint32 profiles)
{
    handset_service_state_machine_t *sm = HandsetService_GetSmForBdAddr(addr);
    device_t dev = BtDevice_GetDeviceForBdAddr(addr);

    TimestampEvent(TIMESTAMP_EVENT_HANDSET_CONNECTION_START);

    /* If the state machine doesn't exist yet, and we are allowed to connect
       to a new handset, create a new state machine. */
    if (!sm && handsetService_CheckHandsetCanConnect(addr))
    {
        HS_LOG("handsetService_ConnectReq creating new handset sm");
        sm = handsetService_CreateSm(dev);
    }

    if (sm)
    {
        if (!sm->handset_device)
        {
            HandsetServiceSm_SetDevice(sm, dev);
        }
        HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_cancelled);
        handsetService_InternalConnectReq(sm, profiles);
        TaskList_AddTask(&sm->connect_list, task);
    }
    else
    {
        HS_LOG("handsetService_ConnectReq Couldn't create a new handset sm");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_CFM_T);
        cfm->addr = *addr;
        cfm->status = handset_service_status_failed;
        MessageSend(task, HANDSET_SERVICE_CONNECT_CFM, cfm);
    }
}

/*
    Message handler functions
*/

static void handsetService_HandlePairingActivity(const PAIRING_ACTIVITY_T* pair_activity)
{
    DEBUG_LOG("handsetService_HandlePairingActivity");

    switch (pair_activity->status)
    {
        case pairingActivityInProgress:
            if (!HandsetService_IsPairing())
            {
                HS_LOG("handsetService_HandlePairingActivity. Pairing Active");
                HandsetService_SetPairing(TRUE);
                handsetService_ObserveConnections();
                ConManagerHandsetPairingMode(TRUE);
            }
            break;

        case pairingActivityNotInProgress:
            if (HandsetService_IsPairing())
            {
                HS_LOG("handsetService_HandlePairingActivity. Pairing Inactive");
                HandsetService_SetPairing(FALSE);
                ConManagerHandsetPairingMode(FALSE);
            }
            break;

        case pairingActivitySuccess:
            {
                DEBUG_LOG("handsetService_HandlePairingActivity pairingSuccess");

                handset_service_state_machine_t *sm = HandsetService_GetSmForBdAddr(&pair_activity->device_addr);
                if(sm)
                {
                    sm->pairing_possible = FALSE;

                    device_t dev = BtDevice_GetDeviceForBdAddr(&pair_activity->device_addr);

                    DEBUG_LOG("handsetService_HandlePairingActivity SM Found device %p", dev);

                    if(!dev && pair_activity->permanent)
                    {
                        DEBUG_LOG("handsetService_HandlePairingActivity Create New Handset Device");
                        dev = PanicNull(BtDevice_GetDeviceCreateIfNew(&pair_activity->device_addr, DEVICE_TYPE_HANDSET));
                        PanicFalse(BtDevice_SetDefaultProperties(dev));
                    }

                    if(dev && (BtDevice_GetDeviceType(dev) == DEVICE_TYPE_HANDSET))
                    {
                        uint8 handset_service_profile_list[] =
                        {
                            profile_manager_hfp_profile,
                            profile_manager_a2dp_profile,
                            profile_manager_avrcp_profile,
                            profile_manager_max_number_of_profiles
                        };

                        /* If this is an LE only handset this will not yet have been populated */
                        if(!sm->handset_device)
                        {
                            HandsetServiceSm_SetDevice(sm, dev);
                        }

                        DEBUG_LOG("handsetService_HandlePairingActivity Synchronise Link Keys");
                        PanicFalse(BtDevice_SetFlags(dev, DEVICE_FLAGS_NOT_PAIRED, 0));
                        PanicFalse(BtDevice_SetFlags(dev, DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD, DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD));
                        BtDevice_AddSupportedProfilesToDevice(dev, DEVICE_PROFILE_AVRCP|DEVICE_PROFILE_A2DP|DEVICE_PROFILE_HFP);
                        Device_SetProperty(dev, device_property_profiles_connect_order, handset_service_profile_list, sizeof(handset_service_profile_list));

                        /* Now that we have successfully paired, we can set the link behavior within bluestack to disable connection retires */
                        BtDevice_SetLinkBehavior(&pair_activity->device_addr);
                        KeySync_Sync();
                    }
                    /* Update the PDL with the device in the persistent device data. This is in order to ensure
                       we don't lose device information in case of device disconnecting right after pairing 
                       without a profile connection */
                    if (BtDevice_isKnownBdAddr(&pair_activity->device_addr))
                    {
                        DEBUG_LOG("handsetService_HandlePairingActivity Known Device, update DB");
                        DeviceDbSerialiser_Serialise();
                    }
                }
            }
            break;

        default:
            break;
    }

    handsetService_UpdateAdvertisingData();
    UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
}

/*! \brief Update the state of LE advertising data set select/release operation */
static void handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_t state)
{
    handset_service_data_t *hs = HandsetService_Get();
    
    hs->ble_adv_state = state;
    handsetService_UpdateAdvertisingData();
    UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
}

/*! \brief Make message for LE connectable indication and send it to task list */
static void handsetService_SendLeConnectableIndication(bool connectable)
{    
    MESSAGE_MAKE(le_connectable_ind, HANDSET_SERVICE_LE_CONNECTABLE_IND_T);
    le_connectable_ind->status = handset_service_status_success;
    le_connectable_ind->le_connectable = connectable;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), HANDSET_SERVICE_LE_CONNECTABLE_IND, le_connectable_ind);
}

static void handsetService_HandleLeAdvMgrSelectDatasetCfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *cfm)
{
    HS_LOG("handsetService_HandleLeAdvMgrSelectDatasetCfm, cfm status is %x", cfm->status );
    
    if (cfm->status == le_adv_mgr_status_success)
    {
        handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_selected);
        handsetService_SendLeConnectableIndication(TRUE);
    }
    else
    {
        Panic();
    }

}

static void handsetService_HandleLeAdvMgrReleaseDatasetCfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm)
{
    HS_LOG("handsetService_HandleLeAdvMgrReleaseDatasetCfm, cfm status is %x", cfm->status );
    
    if (cfm->status == le_adv_mgr_status_success)
    {
        handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_not_selected);
        handsetService_SendLeConnectableIndication(FALSE);
    }
    else
    {
        Panic();
    }
}

/* \brief Handle a CON_MANAGER_TP_CONNECT_IND for BR/EDR and BLE connections */
static void handsetService_HandleConManagerTpConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    TRANSPORT_T transport = ind->tpaddr.transport;
    const typed_bdaddr* taddr = &ind->tpaddr.taddr;
    
    HS_LOG("handsetService_HandleConManagerTpConnectInd type[%d] addr [%04x,%02x,%06lx] incoming [%d]",
           taddr->type, taddr->addr.nap, taddr->addr.uap, taddr->addr.lap, ind->incoming);

    if (ind->incoming)
    {
        if(!BtDevice_LeDeviceIsPeer(&ind->tpaddr))
        {
            handset_service_state_machine_t *sm;

            if(transport == TRANSPORT_BLE_ACL)
            {
                tp_bdaddr tpbdaddr;
                HandsetService_ResolveTpaddr(&ind->tpaddr, &tpbdaddr);
                sm = PanicNull(handsetService_FindOrCreateSm(&tpbdaddr));
                HS_LOG("handsetService_HandleConManagerTpConnectInd received for LE handset %p", sm);

#ifdef INCLUDE_BLE_PAIR_HANDSET_ON_CONNECT
                Pairing_PairLeAddress(HandsetService_GetTask(), taddr);
#endif
                HandsetServiceSm_HandleConManagerBleTpConnectInd(sm, ind);
            }
            else if(transport == TRANSPORT_BREDR_ACL)
            {
                device_t device = BtDevice_GetDeviceForBdAddr(&taddr->addr);
                
                if(!device)
                {
                    device = PanicNull(BtDevice_GetDeviceCreateIfNew(&taddr->addr, DEVICE_TYPE_HANDSET));
                    HS_LOG("handsetService_HandleConManagerTpConnectInd Create new handset device %p", device);
                    PanicFalse(BtDevice_SetDefaultProperties(device));

                    BtDevice_SetFlags(device, DEVICE_FLAGS_NOT_PAIRED, DEVICE_FLAGS_NOT_PAIRED);
                }
                
                sm = handsetService_FindOrCreateSm(&ind->tpaddr);
                HS_LOG("handsetService_HandleConManagerTpConnectInd received for BR/EDR handset %p", sm);
                
                if(sm)
                {
                    if(!HandsetServiceSm_GetHandsetDeviceIfValid(sm))
                    {
                        HandsetServiceSm_SetDevice(sm, device);
                    }

                    /* As handset just connected it cannot have profile connections, so clear flags */
                    BtDevice_SetConnectedProfiles(sm->handset_device, 0);

                    /* Forward the connection to the state machine. */
                    HandsetServiceSm_HandleConManagerBredrTpConnectInd(sm, ind);
                }
            }
            handsetService_UpdateAdvertisingData();
            UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
        }
    }
}

/* \brief Handle a CON_MANAGER_TP_DISCONNECT_IND for BR/EDR and BLE disconnections */
static void handsetService_HandleConManagerTpDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T *ind)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForTpBdAddr(&ind->tpaddr);

    HS_LOG("handsetService_HandleConManagerTpDisconnectInd sm [%p] type[%d] addr [%04x,%02x,%06lx]",
           sm, 
           ind->tpaddr.taddr.type, 
           ind->tpaddr.taddr.addr.nap,
           ind->tpaddr.taddr.addr.uap,
           ind->tpaddr.taddr.addr.lap);

    if(sm)
    {
        if (ind->tpaddr.transport == TRANSPORT_BLE_ACL)
        {
            HandsetServiceSm_HandleConManagerBleTpDisconnectInd(sm, ind);

            handsetService_UpdateAdvertisingData();
            UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
        }
        else if(ind->tpaddr.transport == TRANSPORT_BREDR_ACL)
        {
            HandsetServiceSm_HandleConManagerBredrTpDisconnectInd(sm, ind);
        }
    }
}

static void handsetService_HandleProfileManagerConnectedInd(const CONNECTED_PROFILE_IND_T *ind)
{
    bdaddr addr = DeviceProperties_GetBdAddr(ind->device);
    bool is_handset = appDeviceIsHandset(&addr);

    HS_LOG("handsetService_HandleProfileManagerConnectedInd device 0x%x profile 0x%x handset %d [%04x,%02x,%06lx]",
           ind->device,
           ind->profile,
           is_handset,
           addr.nap,
           addr.uap,
           addr.lap);

    if (is_handset)
    {
        handset_service_state_machine_t *sm = HandsetService_GetSmForBdAddr(&addr);

        /* If state machine doesn't exist yet, need to create a new one */
        if (!sm)
        {
            HS_LOG("handsetService_HandleProfileManagerConnectedInd creating new handset sm");
            sm = handsetService_CreateSm(ind->device);
        }

        /* TBD: handset_service currently only supports one active handset, 
                so what should happen if a new device can't be created? */
        assert(sm);
        
        if (!sm->handset_device)
        {
            HandsetServiceSm_SetDevice(sm, ind->device);
        }

        /* Forward the connect ind to the state machine */
        HandsetServiceSm_HandleProfileManagerConnectedInd(sm, ind);
    }
}

static void handsetService_HandleProfileManagerDisconnectedInd(const DISCONNECTED_PROFILE_IND_T *ind)
{
    if (ind != NULL)
    {
        /* ACL disconnection may cause the Device handle in the indication to be invalid, if the device no longer exists, ignore the indication. */
        if(DeviceList_IsDeviceOnList(ind->device))
        {
            bdaddr addr = DeviceProperties_GetBdAddr(ind->device);
            bool is_handset = appDeviceIsHandset(&addr);

            HS_LOG("handsetService_HandleProfileManagerDisconnectedInd device 0x%x profile 0x%x handset %d [%04x,%02x,%06lx]",
                   ind->device,
                   ind->profile,
                   is_handset,
                   addr.nap,
                   addr.uap,
                   addr.lap);

            if (is_handset)
            {
                handset_service_state_machine_t *sm = handsetService_GetSmForDevice(ind->device);

                if (sm)
                {
                    /* Forward the disconnect ind to the state machine */
                    HandsetServiceSm_HandleProfileManagerDisconnectedInd(sm, ind);
                }
            }
        }
        else
        {
            HS_LOG("handsetService_HandleProfileManagerDisconnectedInd, device doesn't exists anymore in the database");    
        }
    }
    else
    {
        HS_LOG("handsetService_HandleProfileManagerDisconnectedInd, shouldn't expect NULL indication");
    }
}

static void handsetService_HandlePairingPairCfm(void)
{
    DEBUG_LOG("handsetService_HandlePairingPairCfm");
}

static void handsetService_HandlePairingTimeout(const HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT_T *message)
{
    handset_service_state_machine_t *sm;

    HS_LOG("handsetService_HandlePairingTimeout 0x%06lx", message->address.taddr.addr.lap);

    sm = handsetService_GetSmForTpBdAddr(&message->address);
    /* Check that the SM still matches */
    if (sm && sm == message->sm && sm->state != HANDSET_SERVICE_STATE_NULL)
    {
        if (sm->pairing_possible)
        {
            sm->pairing_possible = FALSE;
            handsetService_UpdateAdvertisingData();
            UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
        }
    }
}

static void handsetService_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Connection Manager messages */
    case CON_MANAGER_TP_CONNECT_IND:
        handsetService_HandleConManagerTpConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *)message);
        break;

    case CON_MANAGER_TP_DISCONNECT_IND:
        handsetService_HandleConManagerTpDisconnectInd((const CON_MANAGER_TP_DISCONNECT_IND_T *)message);
        break;

    /* Profile Manager messages */
    case CONNECTED_PROFILE_IND:
        handsetService_HandleProfileManagerConnectedInd((const CONNECTED_PROFILE_IND_T *)message);
        break;

    case DISCONNECTED_PROFILE_IND:
        handsetService_HandleProfileManagerDisconnectedInd((const DISCONNECTED_PROFILE_IND_T *)message);
        break;

    /* BREDR Scan Manager messages */
    case BREDR_SCAN_MANAGER_PAGE_SCAN_PAUSED_IND:
    case BREDR_SCAN_MANAGER_PAGE_SCAN_RESUMED_IND:
        /* These are informational so no need to act on them. */
        break;

    /* Pairing messages */
    case PAIRING_ACTIVITY:
        HS_LOG("handsetService_MessageHandler MESSAGE:0x%x", id);
        handsetService_HandlePairingActivity((PAIRING_ACTIVITY_T*)message);
        break;

    case PAIRING_PAIR_CFM:
        HS_LOG("handsetService_MessageHandler MESSAGE:0x%x", id);
        handsetService_HandlePairingPairCfm();
        break;

    /* LE Advertising Manager messages */
    case LE_ADV_MGR_SELECT_DATASET_CFM:
        HS_LOG("handsetService_MessageHandler MESSAGE:0x%x", id);
        handsetService_HandleLeAdvMgrSelectDatasetCfm((LE_ADV_MGR_SELECT_DATASET_CFM_T*)message);
        break;

    case LE_ADV_MGR_RELEASE_DATASET_CFM:
        HS_LOG("handsetService_MessageHandler MESSAGE:0x%x", id);
        handsetService_HandleLeAdvMgrReleaseDatasetCfm((LE_ADV_MGR_RELEASE_DATASET_CFM_T*)message);
        break;

    case HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT:
        handsetService_HandlePairingTimeout((const HANDSET_SERVICE_INTERNAL_POSSIBLE_PAIRING_TIMEOUT_T *)message);
        break;

    /* Device property related messages */
    case BT_DEVICE_SELF_CREATED_IND:
        HS_LOG("handsetService_MessageHandler BT_DEVICE_SELF_CREATED_IND");
        handsetService_HandleSelfCreated();
        break;

#ifdef INCLUDE_TWS
    case DEVICE_SYNC_PROPERTY_UPDATE_IND:
        HS_LOG("handsetService_MessageHandler DEVICE_SYNC_PROPERTY_UPDATE_IND");
        handsetService_HandleConfigUpdate();
        break;
#endif

    default:
        HS_LOG("handsetService_MessageHandler unhandled id MESSAGE:0x%x", id);
        break;
    }
}

static bool HandsetService_AuthoriseConnection(const bdaddr* bd_addr, dm_protocol_id protocol_id, uint32 channel, bool incoming)
{
    UNUSED(protocol_id);
    UNUSED(channel);
    UNUSED(incoming);
    
    HS_LOG("HandsetService_AuthoriseConnection [%04x,%02x,%06lx] protocol %u, channel %lu, incoming %u", 
           bd_addr->nap,
           bd_addr->uap,
           bd_addr->lap,
           protocol_id,
           channel,
           incoming);
    
    if(HandsetService_GetSmForBdAddr(bd_addr))
    {
        HS_LOG("HandsetService_AuthoriseConnection Accept");
        return TRUE;
    }
    
    HS_LOG("HandsetService_AuthoriseConnection Reject");
    return FALSE;
}

static void handsetService_DisconnectAllComplete(handset_service_status_t status)
{
    task_list_t *disconnect_all_tasklist = TaskList_GetFlexibleBaseTaskList(HandsetService_GetDisconnectAllClientList());
    handset_service_data_t *hs = HandsetService_Get();

    DEBUG_LOG_FN_ENTRY("handsetService_DisconnectAllComplete");

    hs->disconnect_all_in_progress = FALSE;

    if (TaskList_Size(disconnect_all_tasklist))
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T);
        cfm->status = status;

        TaskList_MessageSend(disconnect_all_tasklist, HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM, cfm);
        TaskList_RemoveAllTasks(disconnect_all_tasklist);
    }
}


static bool handsetService_GetConnectedBredrHandsetAddress(bdaddr *addr)
{
    bool bredr_handset = FALSE;
    PanicNull(addr);

    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state != HANDSET_SERVICE_STATE_NULL)
        {
            if(HandsetServiceSm_IsBredrAclConnected(sm))
            {
                *addr = sm->handset_addr;
                bredr_handset = TRUE;
                break;
            }
        }
    }
    return bredr_handset;
}

static bool handsetService_DisconnectOneConnectedHandset(void)
{
    bool disconnecting = FALSE;
    bdaddr bredr_address;

    DEBUG_LOG_FN_ENTRY("handsetService_DisconnectOneConnectedHandset");

    if(handsetService_GetConnectedBredrHandsetAddress(&bredr_address))
    {
        DEBUG_LOG_VERBOSE("handsetService_DisconnectOneConnectedHandset disconnecting 0x%04x", bredr_address.lap);
        HandsetService_DisconnectRequest((Task)&disconnect_all_task, &bredr_address, 0);
        disconnecting = TRUE;
    }

    if(!disconnecting)
    {
        tp_bdaddr le_handset_tpaddr;
        if(HandsetService_GetConnectedLeHandsetTpAddress(&le_handset_tpaddr))
        {
            DEBUG_LOG_VERBOSE("handsetService_DisconnectOneConnectedHandset: LE Handset connected. Disconnecting 0x%04x", le_handset_tpaddr.taddr.addr.lap);
            HandsetService_DisconnectTpAddrRequest((Task)&disconnect_all_task, &le_handset_tpaddr, 0);
            disconnecting = TRUE;
        }
    }
    return disconnecting;
}

static void handsetService_DisconnectAllMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_FN_ENTRY("handsetService_DisconnectAllMessageHandler message = 0x%04x", id);

    if(id == HANDSET_SERVICE_DISCONNECT_CFM)
    {
        HANDSET_SERVICE_DISCONNECT_CFM_T* cfm = (HANDSET_SERVICE_DISCONNECT_CFM_T*)message;
        DEBUG_LOG_VERBOSE("handsetService_DisconnectAllMessageHandler disconnected 0x%04x", cfm->addr.lap);

        if(!((cfm->status == handset_service_status_success) && handsetService_DisconnectOneConnectedHandset()))
        {
            /* nothing left to disconnect, message the clients */
            handsetService_DisconnectAllComplete(cfm->status);
        }
    }
}

/*
    Public functions
*/

static const con_manager_authorise_callback_t authorise_callback = {HandsetService_AuthoriseConnection};

bool HandsetService_Init(Task task)
{
    handset_service_data_t *hs = HandsetService_Get();
    memset(hs, 0, sizeof(*hs));
    hs->task_data.handler = handsetService_MessageHandler;

    ConManager_SetAuthoriseCallback(DEVICE_TYPE_HANDSET, authorise_callback);

    ProfileManager_ClientRegister(&hs->task_data);

    Pairing_ActivityClientRegister(&hs->task_data);

    BtDevice_RegisterListener(&hs->task_data);

#ifdef INCLUDE_TWS
    DeviceSync_RegisterForNotification(&hs->task_data);
#endif

    HandsetServiceSm_Init(hs->state_machine);
    HandsetServiceMultipointSm_Init();
    HandsetServiceConfig_Init();
    handsetService_ConnectableInit();
    HandsetServiceExtAdv_Init();

    TaskList_InitialiseWithCapacity(HandsetService_GetClientList(), HANDSET_SERVICE_CLIENT_LIST_INIT_CAPACITY);
    TaskList_InitialiseWithCapacity(HandsetService_GetDisconnectAllClientList(), HANDSET_SERVICE_DISCONNECT_ALL_CLIENT_LIST_INIT_CAPACITY);

    UNUSED(task);
    return TRUE;
}

void HandsetService_ClientRegister(Task client_task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), client_task);
}

void HandsetService_ClientUnregister(Task client_task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), client_task);
}

static void HandsetService_RegisterMessageGroup(Task client_task, message_group_t group)
{
    PanicFalse(group == HANDSET_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), client_task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(HANDSET_SERVICE, HandsetService_RegisterMessageGroup, NULL);

void HandsetService_ConnectAddressRequest(Task task, const bdaddr *addr, uint32 profiles)
{
    HS_LOG("HandsetService_ConnectAddressRequest addr [%04x,%02x,%06lx] profiles 0x%x",
            addr->nap, addr->uap, addr->lap, profiles);

    handsetService_ConnectReq(task, addr, profiles);
}

void HandsetService_DisconnectAll(Task task)
{
    DEBUG_LOG_FN_ENTRY("HandsetService_DisconnectAll");

    handset_service_data_t *hs = HandsetService_Get();

    if(task)
    {
        TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HandsetService_GetDisconnectAllClientList()), task);
    }

    if(!hs->disconnect_all_in_progress)
    {
        hs->disconnect_all_in_progress = TRUE;

        if(!handsetService_DisconnectOneConnectedHandset())
        {
            handsetService_DisconnectAllComplete(handset_service_status_success);
        }
    }
}

void HandsetService_DisconnectTpAddrRequest(Task task, const tp_bdaddr *tp_addr, uint32 exclude)
{
    HS_LOG("HandsetService_DisconnectTpAddrRequest transport [%d] type [%d] addr [%04x,%02x,%06lx], exclude 0x%08x",
              tp_addr->transport, tp_addr->taddr.type,
              tp_addr->taddr.addr.nap, tp_addr->taddr.addr.uap, tp_addr->taddr.addr.lap,
              exclude);

    handset_service_state_machine_t *sm = handsetService_GetSmForTpBdAddr(tp_addr);

    if (sm)
    {
        HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_cancelled);
        handsetService_InternalDisconnectReq(sm, &tp_addr->taddr.addr, exclude);
        TaskList_AddTask(&sm->disconnect_list, task);
    }
    else
    {
        HS_LOG("HandsetService_DisconnectTpAddrRequest sm not found");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_DISCONNECT_CFM_T);
        cfm->addr = tp_addr->taddr.addr;
        cfm->status = handset_service_status_success;
        MessageSend(task, HANDSET_SERVICE_DISCONNECT_CFM, cfm);
    }
}

void HandsetService_DisconnectRequest(Task task, const bdaddr *addr, uint32 exclude)
{
    HS_LOG("HandsetService_DisconnectRequest addr [%04x,%02x,%06lx], exclude 0x%08x",
            addr->nap, addr->uap, addr->lap, exclude);

    tp_bdaddr tp_addr = {0};

    BdaddrTpFromBredrBdaddr(&tp_addr, addr);
    HandsetService_DisconnectTpAddrRequest(task, &tp_addr, exclude);
}

void HandsetService_StopConnect(Task task, const bdaddr *addr)
{
    handset_service_state_machine_t *sm = HandsetService_GetSmForBdAddr(addr);

    HS_LOG("HandsetService_StopConnect task[0x%p] addr [%04x,%02x,%06lx]",
           task, 
           addr->nap, 
           addr->uap, 
           addr->lap);

    if (sm)
    {
        if(sm->connect_stop_task)
        {
            if(sm->connect_stop_task != task)
            {
                DEBUG_LOG_ERROR("HandsetService_StopConnect Called by two Tasks: task %p connect_stop_task %p", task, sm->connect_stop_task);
                Panic();
            }
            else
            {
                DEBUG_LOG_WARN("HandsetService_StopConnect called twice; task %p", task);
            }
        }
        handsetService_InternalConnectStopReq(sm);
        sm->connect_stop_task = task;

        /* Flush any queued internal connect requests */
        HandsetServiceSm_CancelInternalConnectRequests(sm);
    }
    else
    {
        HS_LOG("HandsetService_StopConnect no handset connection to stop");

        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_STOP_CFM_T);
        cfm->addr = *addr;
        cfm->status = handset_service_status_disconnected;
        MessageSend(task, HANDSET_SERVICE_CONNECT_STOP_CFM, cfm);
    }
}

void HandsetService_StopReconnect(Task task)
{
    HS_LOG("HandsetService_StopReconnect task[%p]", task);

    HandsetServiceMultipointSm_StopReconnect(task);
}

void HandsetService_ConnectableRequest(Task task)
{
    HS_LOG("HandsetService_ConnectableRequest");
    UNUSED(task);

    HandsetService_SetBleConnectable(TRUE);
    handsetService_ConnectableAllowBredr(TRUE);
    HandsetServiceSm_EnableConnectableIfMaxConnectionsNotActive();
}

void HandsetService_CancelConnectableRequest(Task task)
{
    HS_LOG("HandsetService_CancelConnectableRequest");
    UNUSED(task);

    /* HandsetService_SetBleConnectable(FALSE) is not called here because
       the handset service continues BLE advertising even if BREDR is set to be
       not connectable. */
    handsetService_ConnectableEnableBredr(FALSE);
    handsetService_ConnectableAllowBredr(FALSE);
}

void HandsetService_SendConnectedIndNotification(device_t device,
    uint32 profiles_connected)
{
    PanicNull(device);
    MESSAGE_MAKE(ind, HANDSET_SERVICE_CONNECTED_IND_T);
    ind->addr = DeviceProperties_GetBdAddr(device);
    ind->profiles_connected = profiles_connected;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), HANDSET_SERVICE_CONNECTED_IND, ind);
}

void HandsetService_SendDisconnectedIndNotification(const bdaddr *addr,
    handset_service_status_t status)
{
    MESSAGE_MAKE(ind, HANDSET_SERVICE_DISCONNECTED_IND_T);
    ind->addr = *addr;
    ind->status = status;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), HANDSET_SERVICE_DISCONNECTED_IND, ind);
}

void HandsetService_SendFirstProfileConnectedIndNotification(device_t device)
{
    PanicNull(device);
    MESSAGE_MAKE(ind, HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND_T);
    ind->addr = DeviceProperties_GetBdAddr(device);
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HandsetService_GetClientList()), HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND, ind);
}

bool HandsetService_Connected(device_t device)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForDevice(device);

    if (sm)
    {
        return (HANDSET_SERVICE_STATE_CONNECTED_BREDR == sm->state)
                || (HANDSET_SERVICE_STATE_CONNECTED_LE == sm->state);
    }

    return FALSE;
}

bool HandsetService_IsBredrConnected(const bdaddr *addr)
{
    handset_service_state_machine_t *sm = handsetService_GetSmForBredrAddr(addr);
    bool bredr_connected = FALSE;

    if (sm)
    {
        bredr_connected = HandsetServiceSm_IsBredrAclConnected(sm);
    }

    HS_LOG("HandsetService_IsBredrConnected bredr_connected %u", bredr_connected);

    return bredr_connected;
}

bool HandsetService_IsAnyBredrConnected(void)
{
    return (HandsetServiceSm_GetBredrAclConnectionCount() != 0);
}

bool HandsetService_GetConnectedBredrHandsetTpAddress(tp_bdaddr * tp_addr)
{
    bool bredr_handset = FALSE;
    PanicNull(tp_addr);
    bdaddr addr;
    bredr_handset = handsetService_GetConnectedBredrHandsetAddress(&addr);
    if(bredr_handset)
    {
        BdaddrTpFromBredrBdaddr(tp_addr, &addr);
    }
    return bredr_handset;
}

bool HandsetService_GetConnectedLeHandsetTpAddress(tp_bdaddr *tp_addr)
{
    unsigned le_handset_count = 0;

    PanicNull(tp_addr);
    FOR_EACH_HANDSET_SM(sm)
    {
        if (sm->state != HANDSET_SERVICE_STATE_NULL)
        {
            if (HandsetServiceSm_IsLeConnected(sm))
            {
                if (le_handset_count == 0)
                {
                    *tp_addr = HandsetServiceSm_GetLeTpBdaddr(sm);
                }
                le_handset_count++;
            }
        }
    }

    if (le_handset_count > 1)
    {
        /* Additional API needed to allow for multiple LE handsets */
        DEBUG_LOG_WARN("HandsetService_GetConnectedLeHandsetTpAddress More than one LE exists (%d total)",
                        le_handset_count);
    }

    return le_handset_count != 0;
}

void HandsetService_SetBleConnectable(bool connectable)
{
    handset_service_data_t *hs = HandsetService_Get();

    DEBUG_LOG("HandsetService_SetBleConnectable connectable %d le_connectable %d adv_hdl %p",
              connectable, hs->ble_connectable, hs->le_advert_handle);

    if (connectable == hs->ble_connectable)
    {
        /* We still need to send a HANDSET_SERVICE_LE_CONNECTABLE_IND when the
           requested value is the same as the current value */
        handsetService_SendLeConnectableIndication(connectable);
    }
    else
    {
        /* Connectable flag has changed so may need to update the advertising
           data to match the new value. */
        hs->ble_connectable = connectable;
        if(   !handsetService_UpdateAdvertisingData()
           || !HandsetServiceExtAdv_UpdateAdvertisingData())
        {
            /* Advertisement data was not updated. Possibly device is already connected
               over LE and there is no active advertisement set.
               Send the indication to inform the client. */
            handsetService_SendLeConnectableIndication(connectable);
        }
    }
}

unsigned HandsetService_GetNumberOfConnectedBredrHandsets(void)
{
    unsigned num_handsets = HandsetServiceSm_GetBredrAclConnectionCount();
    DEBUG_LOG_FN_ENTRY("HandsetService_GetNumberOfConnectedBredrHandsets handsets=%d", num_handsets);
    return num_handsets;
}

void HandsetService_ReconnectRequest(Task task, uint32 profiles)
{
    DEBUG_LOG_FN_ENTRY("HandsetService_ReconnectRequest");
    HandsetServiceMultipointSm_ReconnectRequest(task, profiles);
}

void HandsetService_DisconnectLruHandsetRequest(Task task)
{
    device_t handset_device;
    DEBUG_LOG("HandsetService_DisconnectLruHandsetRequest");

    if(Focus_GetDeviceForUiInput(ui_input_disconnect_lru_handset, &handset_device))
    {
        bdaddr handset_addr = DeviceProperties_GetBdAddr(handset_device);
        HandsetService_DisconnectRequest(task, &handset_addr, 0);
    }
}
