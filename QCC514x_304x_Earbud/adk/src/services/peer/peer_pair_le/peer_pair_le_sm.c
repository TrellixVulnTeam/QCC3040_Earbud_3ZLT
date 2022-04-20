/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Implementation of state machine transitions for the PEER PAIRING OVER LE service
*/

#include <gatt.h>
#include <panic.h>
#include <util.h>

#include <logging.h>

#include <bt_device.h>
#include <connection_manager.h>
#include <gatt_root_key_server.h>
#include <gatt_root_key_server_uuids.h>
#include <gatt_handler.h>
 
#include "peer_pair_le.h"
#include "peer_pair_le_sm.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_private.h"
#include "peer_pair_le_key.h"
#include "pairing_protected.h"
#include <uuid.h>
#include <ui.h>


static bool peer_pair_le_is_discovery_state(PEER_PAIR_LE_STATE state)
{
    return    PEER_PAIR_LE_STATE_DISCOVERY == state
           || PEER_PAIR_LE_STATE_SELECTING == state;
}


bool peer_pair_le_in_pairing_state(void)
{
    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            return TRUE;

        default:
            return FALSE;
    }
}


bool peer_pair_le_is_advertising_state(PEER_PAIR_LE_STATE state)
{
    switch (state)
    {
        case PEER_PAIR_LE_STATE_DISCOVERY:
        case PEER_PAIR_LE_STATE_SELECTING:
        case PEER_PAIR_LE_STATE_CONNECTING:
            return TRUE;

        default:
            return FALSE;
    }
}


bool PeerPairLeIsRunning(void)
{
    return peer_pair_le_get_state() > PEER_PAIR_LE_STATE_IDLE;
}


static void peer_pair_le_cancel_advertising(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_cancel_advertising");

    if (NULL == ppl->advert_handle)
    {
        DEBUG_LOG_WARN("peer_pair_le_cancel_advertising. Advert handle wasn't acquired");

        return;
    }
    LeAdvertisingManager_ReleaseAdvertisingDataSet(ppl->advert_handle);
    ppl->advert_handle = NULL;
}


static void peer_pair_le_stop_advertising(void)
{
    DEBUG_LOG("peer_pair_le_stop_advertising");

    peer_pair_le_cancel_advertising();
}

bool peer_pair_le_enable_advertising(void)
{
    const le_adv_select_params_t adv_select_params = {.set = le_adv_data_set_peer};
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    ppl->advert_handle = LeAdvertisingManager_SelectAdvertisingDataSet(PeerPairLeGetTask(), &adv_select_params);
    if (!ppl->advert_handle)
    {
        return FALSE;
    }
    ppl->advertising_active = TRUE;
    return TRUE;
}

static void peer_pair_le_start_advertising(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    /* try to enable advertising first, but then check if we already know it was busy */
    if (!peer_pair_le_enable_advertising())
    {
        if (ppl->advertising_active)
        {
            MessageSendConditionally(PeerPairLeGetTask(),
                                     PEER_PAIR_LE_INTERNAL_ENABLE_ADVERTISING, NULL,
                                     &ppl->advertising_active);
        }
        else
        {
            DEBUG_LOG_ERROR("peer_pair_le_start_advertising Unable to select advertising set, and not active");
            Panic();
        }
    }
}


static void peer_pair_le_start_scanning(void)
{
    uint8 uuid_common[] = {UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE)}; 
    uint8 uuid_left[] = {UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_LEFT)};
    uint8 uuid_right[] = {UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_RIGHT)};
    tp_bdaddr filter_address;

    le_advertising_report_filter_t filter;

    DEBUG_LOG("peer_pair_le_start_scanning");

    PeerPairLe_DeviceSetAllEmpty();

    filter.ad_type = ble_ad_type_complete_uuid128;
    filter.ad_type_additional = ble_ad_type_more_uuid128;
    filter.interval = filter.size_pattern = sizeof(uuid_common);
    filter.find_tpaddr = NULL;

    if (PeerPairLeExpectedDeviceIsSet())
    {
        filter_address.transport = TRANSPORT_BLE_ACL;
        filter_address.taddr.type = TYPED_BDADDR_PUBLIC;
        filter_address.taddr.addr = PeerPairLeExpectedDevice();
        filter.find_tpaddr = &filter_address;
    }

    if (PeerPairLeIsLeft())
    {
        filter.pattern = uuid_right;
    }
    else if (PeerPairLeIsRight())
    {
        filter.pattern = uuid_left;
    }
    else
    {
        filter.pattern = uuid_common;
    }

    LeScanManager_Start(PeerPairLeGetTask(),le_scan_interval_fast, &filter);
}


static void peer_pair_le_stop_scanning(void)
{
    DEBUG_LOG("peer_pair_le_stop_scanning");

    if (LeScanManager_IsTaskScanning(PeerPairLeGetTask()))
    {
        MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN);
        LeScanManager_Stop(PeerPairLeGetTask());
    }
}


static void peer_pair_le_enter_discovery(PEER_PAIR_LE_STATE old_state)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_enter_discovery");

    ppl->gatt_cid = INVALID_CID;

    PeerPairLe_DeviceSetAllEmpty();

    /* in rediscovery we are already advertising, no need to restart it */
    if (old_state != PEER_PAIR_LE_STATE_REDISCOVERY)
    {
        peer_pair_le_start_advertising();
    }

    if (!peer_pair_le_is_discovery_state(old_state))
    {
        peer_pair_le_start_scanning();
    }
}

static void peer_pair_le_enter_rediscovery(void)
{
    DEBUG_LOG("peer_pair_le_enter_rediscovery, wait for LE scan disabled before reentering in discovery");
}

static void peer_pair_le_exit_discovery(PEER_PAIR_LE_STATE new_state)
{
    DEBUG_LOG("peer_pair_le_exit_discovery");

    MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_TIMEOUT_REDISCOVERY);

    switch (new_state)
    {
        case PEER_PAIR_LE_STATE_SELECTING:
            /* We have seen at least one advert we like, so nothing changes */
            DEBUG_LOG("peer_pair_le_exit_discovery: Expected, nothing to see here");
            break;

        case PEER_PAIR_LE_STATE_REDISCOVERY:
            /* Going from DISCOVERY to REDISCOVERY state requires stopping scanning,
             so that DISCOVERY state can start it again. */
            peer_pair_le_stop_scanning();
        break;

        case PEER_PAIR_LE_STATE_DISCOVERY:
            /* Going from SELECTING to DISCOVERY state requires stopping advertising,
               so that DISCOVERY state can start it again. */
            peer_pair_le_stop_advertising();
            break;

        case PEER_PAIR_LE_STATE_CONNECTING:
            /* We have identified a device. Need to stop scanning */
            peer_pair_le_stop_scanning();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_stop_scanning();
            break;
            
        default:
            DEBUG_LOG("peer_pair_le_exit_discovery: Need to do something, state:%d", new_state);
            Panic();
            break;
    }
}

static void peer_pair_le_exit_advertising_states(void)
{
    MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_ENABLE_ADVERTISING);
}


/* Compare bdaddr. Treat LAP as most important distinguisher.
    Rationale: Expect devices to be from same MFR (if public) and if a RRA address
               checks the most bits first.

    Do not just use memcmp as the bdaddr structure is not packed.
 */
static bool peer_pair_le_bdaddr_greater(const typed_bdaddr *first, const typed_bdaddr *second)
{
    if (first->addr.lap != second->addr.lap)
    {
        return first->addr.lap > second->addr.lap;
    }

    if (first->addr.nap != second->addr.nap)
    {
        return first->addr.nap > second->addr.nap;
    }

    return first->addr.uap > second->addr.uap;
}


static bool peer_pair_le_is_own_address_odd(const typed_bdaddr *first)
{
    if (first->addr.lap & 0x01)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
} 

static void peer_pair_le_enter_selecting(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    uint32 delay = appConfigPeerPairLeTimeoutPeerSelect();
    typed_bdaddr found_addr, local_addr;

    if (PeerPairLeExpectedDeviceIsSet())
    {
        /* If connecting to a fixed device, update the delay so that only one
           side will delay ( assuming both devices saw each other ) 
           If only one device saw the other, then a) connection is unlikely to
           succeed and b) the connection attempt will be immediate OR after
           two seconds. */
        delay = 0;
    }

    found_addr = ppl->scanned_devices[0].taddr;
    local_addr = ppl->local_addr;
    /* If we can pair, then devices will see each other well within the long timeout
       Use double the delay on one of them */

     /* If the found address is random, since we do not know our own random address we need to take the decision 
        based on the local public address. Hence if we find our address is odd add 2 secs of extra time.
        Else if the found address is Public, the one with greater address gets the extra time. */
     if( ( (found_addr.type == TYPED_BDADDR_RANDOM)&& (peer_pair_le_is_own_address_odd(&local_addr)) )||
         ( (found_addr.type == TYPED_BDADDR_PUBLIC)&& peer_pair_le_bdaddr_greater(&found_addr, &local_addr)) )
    {
        /*! \todo Need an approach if RRA is the same as ours. 
            Best not detected here */
        delay += appConfigPeerPairLeTimeoutPeerSelect();
    }

    DEBUG_LOG("peer_pair_le_enter_selecting. Delay %d ms", delay);

    /*! \todo Want to randomise settings somewhat. Otherwise crossovers will be VERY likely.
        That will include the timeout from first scan detected, as well as intervals used. */
    MessageSendLater(PeerPairLeGetTask(), PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN, NULL, 
                     delay);
}


static void peer_pair_le_enter_pending_local_addr(void)
{
    DEBUG_LOG("peer_pair_le_enter_pending_local_addr");
    ConnectionReadLocalAddr(PeerPairLeGetTask());
}


static void peer_pair_le_enter_idle(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_enter_idle");

    /* Probably only ever enter this state with a find pending */
    if (ppl->find_pending)
    {
        MessageSend(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_FIND_PEER, NULL);
    }
}


static void peer_pair_le_enter_connecting(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    tp_bdaddr tp_addr;
    tp_addr.transport = TRANSPORT_BLE_ACL;
    tp_addr.taddr = ppl->scanned_devices[0].taddr;

    DEBUG_LOG("peer_pair_le_enter_connecting lap 0x%04x", tp_addr.taddr.addr.lap);
    
    ConManagerCreateTpAcl(&tp_addr);

    /* After a call to create, a connection entry exists and
       the Quality of service can be set for the connection */
    ConManagerRequestDeviceQos(&tp_addr, cm_qos_short_data_exchange);
}


static void peer_pair_le_enter_negotiate_p_role(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    gatt_uuid_t uuid[] = {UUID_128_FORMAT_gatt_uuid_t(UUID128_ROOT_KEY_SERVICE)};

    DEBUG_LOG("peer_pair_le_enter_negotiate_p_role cid:%d", ppl->gatt_cid);

    /*! \todo Ask the client to initialise itself, so we don't have to know about UUID */
    GattDiscoverPrimaryServiceRequest(PeerPairLeGetTask(), ppl->gatt_cid, 
                                      gatt_uuid128, uuid);
}


static void peer_pair_le_enter_negotiate_c_role(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    const GRKS_KEY_T *secret = &peer_pair_le_key;
    typed_bdaddr local_taddr;
    typed_bdaddr peer_taddr;

    if (BtDevice_GetPublicAddress(&ppl->local_addr, &local_taddr) &&
        BtDevice_GetPublicAddress(&ppl->peer, &peer_taddr))
    {
        DEBUG_LOG("peer_pair_le_enter_negotiate_c_role");
        GattRootKeyServerReadyForChallenge(&PeerPairLeGetRootKeyServer(),
                                           secret,
                                           &local_taddr.addr,
                                           &peer_taddr.addr);
    }
    else
    {
        /* Typically caused by peer bud disconnecting in the middle of pairing
           procedure. Wait for disconnection. */
        DEBUG_LOG("peer_pair_le_enter_negotiate_c_role failed to resolve address, wait for disconnection");
    }
}


static void peer_pair_le_enter_completed(void)
{
    DEBUG_LOG("peer_pair_le_enter_completed");

    peer_pair_le_release_local_addr_override();

    MessageSend(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED, NULL);

    Pairing_SendPairingCompleteMessageToClients();
}


static void peer_pair_le_exit_completed(void)
{
    DEBUG_LOG("peer_pair_le_exit_completed");

    peer_pair_le_disconnect();
}

static void peer_pair_le_enter_completed_wait_for_disconnect(void)
{
    DEBUG_LOG("peer_pair_le_enter_completed_wait_for_disconnect");

    peer_pair_le_release_local_addr_override();

    MessageSendLater(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED, NULL,
                     appConfigPeerPairLeTimeoutServerCompleteDisconnect());

    Pairing_SendPairingCompleteMessageToClients();
}

static void peer_pair_le_exit_completed_wait_for_disconnect(void)
{
    DEBUG_LOG("peer_pair_le_exit_completed_wait_for_disconnect");

    MessageCancelAll(PeerPairLeGetTask(), PEER_PAIR_LE_INTERNAL_COMPLETED);
}


static void peer_pair_le_enter_initialsed(void)
{
    DEBUG_LOG("peer_pair_le_enter_initialsed");

    peer_pair_le_stop_service();

    Ui_InformContextChange(ui_provider_peer_pairing, context_peer_pairing_idle);
}

static void peer_pair_le_exit_initialsed(void)
{
    DEBUG_LOG("peer_pair_le_exit_initialsed");

    Ui_InformContextChange(ui_provider_peer_pairing, context_peer_pairing_active);
}


static void peer_pair_le_enter_pairing_as_server(void)
{
    DEBUG_LOG("peer_pair_le_enter_pairing_as_server");
    
    peer_pair_le_cancel_advertising();
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    typed_bdaddr tb;
    
    tb = ppl->peer;

    Pairing_PairLePeer(PeerPairLeGetTask(), &tb, TRUE);
}


static void peer_pair_le_enter_pairing_as_client(void)
{
    DEBUG_LOG("peer_pair_le_enter_pairing_as_client");

    peer_pair_le_cancel_advertising();
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    typed_bdaddr tb;

    tb = ppl->peer;

    Pairing_PairLePeer(PeerPairLeGetTask(), &tb, FALSE);
}

void peer_pair_le_set_state(PEER_PAIR_LE_STATE new_state)
{
    PEER_PAIR_LE_STATE old_state = peer_pair_le_get_state();

    if (old_state == new_state)
    {
        DEBUG_LOG_ERROR("peer_pair_le_set_state. Attempt to transition to same state enum:PEER_PAIR_LE_STATE:%d",
                   old_state);
        Panic();    /*! \todo Remove panic once implementation stable */
        return;
    }

    DEBUG_LOG_STATE("peer_pair_le_set_state. Transition enum:PEER_PAIR_LE_STATE:%d->enum:PEER_PAIR_LE_STATE:%d",
                old_state, new_state);

    /* Pattern is to run functions for exiting state first */
    switch (old_state)
    {
        case PEER_PAIR_LE_STATE_INITIALISED:
            peer_pair_le_exit_initialsed();
            break;

        case PEER_PAIR_LE_STATE_DISCOVERY:
        case PEER_PAIR_LE_STATE_SELECTING:
            peer_pair_le_exit_discovery(new_state);
            break;

        case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
            peer_pair_le_exit_completed_wait_for_disconnect();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED:
            peer_pair_le_exit_completed();
            break;

        default:
            break;
    }

    if (    peer_pair_le_is_advertising_state(old_state)
        && !peer_pair_le_is_advertising_state(new_state))
    {
        peer_pair_le_exit_advertising_states();
    }

    peer_pair_le.state = new_state;

    switch (new_state)
    {
        case PEER_PAIR_LE_STATE_INITIALISED:
            peer_pair_le_enter_initialsed();
            break;

        case PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR:
            peer_pair_le_enter_pending_local_addr();
            break;

        case PEER_PAIR_LE_STATE_IDLE:
            peer_pair_le_enter_idle();
            break;

        case PEER_PAIR_LE_STATE_DISCOVERY:
            peer_pair_le_enter_discovery(old_state);
            break;

        case PEER_PAIR_LE_STATE_SELECTING:
            peer_pair_le_enter_selecting();
            break;

        case PEER_PAIR_LE_STATE_REDISCOVERY:
            peer_pair_le_enter_rediscovery();
            break;

        case PEER_PAIR_LE_STATE_CONNECTING:
            peer_pair_le_enter_connecting();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_enter_pairing_as_server();
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
            peer_pair_le_enter_pairing_as_client();
            break;

        case PEER_PAIR_LE_STATE_NEGOTIATE_P_ROLE:
            peer_pair_le_enter_negotiate_p_role();
            break;

        case PEER_PAIR_LE_STATE_NEGOTIATE_C_ROLE:
            peer_pair_le_enter_negotiate_c_role();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
            peer_pair_le_enter_completed_wait_for_disconnect();
            break;

        case PEER_PAIR_LE_STATE_COMPLETED:
            peer_pair_le_enter_completed();
            break;

        default:
            break;
    }
}
