/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Initialisation functions for the Peer Pairing over LE service
*/

#include <bdaddr.h>
#include "connection_abstraction.h"
#include <limits.h>
#include <logging.h>
#include <panic.h>

#include <gatt.h>
#include <gatt_connect.h>
#include <gatt_handler.h>
#include <gatt_handler_db_if.h>

#include <le_advertising_manager.h>
#include <connection_manager.h>
#include <gatt_root_key_client.h>
#include <gatt_root_key_server.h>
#include <bt_device.h>

#include "device_properties.h"
#include <device_db_serialiser.h>

#include <device_list.h>

#include "peer_pair_le.h"
#include "peer_pair_le_private.h"
#include "peer_pair_le_sm.h"
#include "peer_pair_le_init.h"
#include "peer_pair_le_key.h"
#include "pairing.h"
#include "system_state.h"
#include <gatt_root_key_server_uuids.h>
#include <uuid.h>

#define NUMBER_OF_ADVERT_DATA_ITEMS     1
#define SIZE_PEER_PAIR_LE_ADVERT        18

static unsigned int peer_pair_le_NumberOfAdvItems(const le_adv_data_params_t * params);
static le_adv_data_item_t peer_pair_le_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id);
static void peer_pair_le_ReleaseAdvDataItems(const le_adv_data_params_t * params);
static void peer_pair_le_GattConnect(gatt_cid_t cid);
static void peer_pair_le_GattDisconnect(gatt_cid_t cid);
static void peer_pair_le_clone_peer_keys(void);
static void peer_pair_le_add_paired_device_entries(void);



static const gatt_connect_observer_callback_t peer_pair_le_connect_callback =
{
    .OnConnection = peer_pair_le_GattConnect,
    .OnDisconnection = peer_pair_le_GattDisconnect
};

static const le_adv_data_callback_t peer_pair_le_advert_callback =
{
    .GetNumberOfItems = peer_pair_le_NumberOfAdvItems,
    .GetItem = peer_pair_le_GetAdvDataItems,
    .ReleaseItems = peer_pair_le_ReleaseAdvDataItems
};

/*! In legacy use of peer pair LE, we do not know (or care) if we are a left
    or right device. That uses a different UUID */
static const uint8 peer_pair_le_advert_data_common[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE)
}; 

static const uint8 peer_pair_le_advert_data_left[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_LEFT)
}; 

static const uint8 peer_pair_le_advert_data_right[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_RIGHT)
}; 


static le_adv_data_item_t peer_pair_le_advert;


static void peer_pair_le_send_pair_confirm(peer_pair_le_status_t status)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    MAKE_PEER_PAIR_LE_MESSAGE(PEER_PAIR_LE_PAIR_CFM);

    message->status = status;

    /*! \todo Not a task, but a pointer */
    /* Delay the message until we uninitialise the module
       Cast the data pointer to a Task... as there isn't another conditional
       API based on a pointer. */
    MessageSendConditionallyOnTask(ppl->client, PEER_PAIR_LE_PAIR_CFM, message,
                                   (Task*)&PeerPairLeGetData());
}

static void peer_pair_le_handle_local_bdaddr(const CL_DM_LOCAL_BD_ADDR_CFM_T *cfm)
{
    if (cfm->status != hci_success)
    {
        DEBUG_LOG("Failed to read local BDADDR");
        Panic();
    }
    
    DEBUG_LOG("peer_pair_le_handle_local_bdaddr %04x %02x %06x",
        cfm->bd_addr.nap,
        cfm->bd_addr.uap,
        cfm->bd_addr.lap);
        
    PeerPairLeGetData()->local_addr.type = TYPED_BDADDR_PUBLIC;
    PeerPairLeGetData()->local_addr.addr = cfm->bd_addr;

    peer_pair_le_set_state(PEER_PAIR_LE_STATE_IDLE);
}


static void peer_pair_le_handle_find_peer(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR:
            ppl->find_pending = TRUE;
            return;

        case PEER_PAIR_LE_STATE_IDLE:
            break;

        default:
            DEBUG_LOG("peer_pair_le_handle_find_peer called in bad state: enum:PEER_PAIR_LE_STATE:%d",
                        peer_pair_le_get_state());
            /*! \todo Remove panic and send message to client once code better */
            Panic();
            break;
    }

    ppl->find_pending = FALSE;
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
}


static void peer_pair_le_handle_completed(void)
{
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCONNECTING);
}

static void peer_pair_le_handle_enable_advertising(void)
{
    if (peer_pair_le_is_advertising_state(peer_pair_le_get_state()))
    {
        /* Advertising was not able to be enabled originally as handle
           had not been released. Now it has been released, so 
           we expect the enable to have worked. */
        PanicFalse(peer_pair_le_enable_advertising());
    }
}

static void peer_pair_le_handle_scan_timeout(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    if (PEER_PAIR_LE_STATE_SELECTING != peer_pair_le_get_state())
    {
        return;
    }
    if (BdaddrTypedIsEmpty(&ppl->scanned_devices[0].taddr))
    {
        return;
    }

    if (!BdaddrTypedIsEmpty(&ppl->peer))
    {
        DEBUG_LOG("peer_pair_le_handle_scan_timeout: peer not empty: %02x %04x %02x %06x",
                  ppl->peer.type, ppl->peer.addr.nap, ppl->peer.addr.uap, ppl->peer.addr.lap);
        return;
    }

    /* Eliminate the scan result if RSSI too close */
    if (!BdaddrTypedIsEmpty(&ppl->scanned_devices[1].taddr))
    {
        int difference = ppl->scanned_devices[0].rssi - ppl->scanned_devices[1].rssi;
        if (difference < appConfigPeerPairLeMinRssiDelta())
        {
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
            return;
        }
    }
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_CONNECTING);
}

static void peer_pair_le_handle_rediscovery_timeout(void)
{
    if (PEER_PAIR_LE_STATE_DISCOVERY != peer_pair_le_get_state())
    {
        return;
    }

    peer_pair_le_set_state(PEER_PAIR_LE_STATE_REDISCOVERY);
}

static void peer_pair_le_handle_adv_mgr_select_dataset_cfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *message)
{
    UNUSED(message);

    DEBUG_LOG("peer_pair_le_handle_adv_mgr_select_dataset_cfm sts:le_adv_mgr_status_t:%d",
              message->status);
}

static void peer_pair_le_handle_con_manager_connection(const CON_MANAGER_TP_CONNECT_IND_T *conn)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    
    DEBUG_LOG("peer_pair_le_handle_con_manager_connection %02x %04x %02x %06x",
              conn->tpaddr.taddr.type,
              conn->tpaddr.taddr.addr.nap, conn->tpaddr.taddr.addr.uap, conn->tpaddr.taddr.addr.lap);
    
    ppl->peer = conn->tpaddr.taddr;
    
    switch (peer_pair_le_get_state())
    {
        /* Occurs when peer connects unexpectedly whilst local device is selecting */
        case PEER_PAIR_LE_STATE_SELECTING:
        case PEER_PAIR_LE_STATE_CONNECTING:
        {
            /* When both earbuds trying to open ACL at same time with each other during
            peer pair process then the eb which will get its ACL create request successful
            first will become client and other eb will become in that case server*/
            if (!conn->incoming)
            {
                peer_pair_le_set_state(PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT);
            }
            else
            {
                peer_pair_le_set_state(PEER_PAIR_LE_STATE_PAIRING_AS_SERVER);
            }
        }
        break;
        default:
            DEBUG_LOG("peer_pair_le_handle_con_manager_connection unexpected state"
                      " enum:PEER_PAIR_LE_STATE:%d", peer_pair_le_get_state());
        break;
    }
}

static void peer_pair_le_handle_failure(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG_INFO("peer_pair_le_handle_failure");

    BtDevice_DeleteAllDevicesOfType(DEVICE_TYPE_EARBUD);
    GattRootKeyClientDestroy(&ppl->root_key_client);
    if (PEER_PAIR_LE_STATE_DISCOVERY != peer_pair_le_get_state())
    {
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
    }
}

static void peer_pair_le_handle_con_manager_disconnection(const CON_MANAGER_TP_DISCONNECT_IND_T *conn)
{
    PEER_PAIR_LE_STATE state;
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    
    DEBUG_LOG("peer_pair_le_handle_con_manager_disconnection. lap_ind:0x%x lap_peer:0x%x",
        conn->tpaddr.taddr.addr.lap, ppl->peer.addr.lap);

    if (BdaddrTypedIsSame(&conn->tpaddr.taddr, &ppl->peer))
    {
        state = peer_pair_le_get_state();

        switch (state)
        {
            case PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT:
            case PEER_PAIR_LE_STATE_DISCONNECTING:
                DEBUG_LOG("peer_pair_le_handle_con_manager_disconnection. Peer BLE Link disconnected");
                peer_pair_le_set_state(PEER_PAIR_LE_STATE_INITIALISED);
                break;

            case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
                Pairing_PairStop(PeerPairLeGetTask());
                break;

            case PEER_PAIR_LE_STATE_NEGOTIATE_C_ROLE:
            case PEER_PAIR_LE_STATE_NEGOTIATE_P_ROLE:
                peer_pair_le_handle_failure();
                break;

            default:
                DEBUG_LOG("peer_pair_le_handle_con_manager_disconnection. IGNORED. enum:PEER_PAIR_LE_STATE:%d",
                           state);
                break;
        }
        
        BdaddrTypedSetEmpty(&ppl->peer);
    }
}


static void peer_pair_le_handle_primary_service_discovery(const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* discovery)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    /* Note that peer address will be empty if peer disconnects during service discovery */
    if (gatt_status_success == discovery->status && !BdaddrTypedIsEmpty(&ppl->peer))
    {
        DEBUG_LOG("peer_pair_le_handle_primary_service_discovery. cid:%d start:%d end:%d client:%p, more:%d",
                   discovery->cid, discovery->handle, discovery->end, &ppl->root_key_client, discovery->more_to_come);

        PanicFalse(GattRootKeyClientInit(&ppl->root_key_client,
                              PeerPairLeGetTask(), 
                              discovery->cid, discovery->handle, discovery->end));
    }
    else
    {
        DEBUG_LOG("peer_pair_le_handle_primary_service_discovery failure sts enum:gatt_status_t:%d",
                  discovery->status);
        /* Typically indicates link has disconnected during service discovery */
        peer_pair_le_handle_failure();
    }
}

static void peer_pair_le_handle_client_init(const GATT_ROOT_KEY_CLIENT_INIT_CFM_T *init)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    const GRKS_KEY_T *secret = &peer_pair_le_key;
    typed_bdaddr local_tdaddr;
    typed_bdaddr peer_taddr;

    DEBUG_LOG("peer_pair_le_handle_client_init. sts enum:gatt_root_key_client_status_t:%d",
              init->status);

    if (gatt_root_key_client_status_success == init->status &&
        BtDevice_GetPublicAddress(&ppl->local_addr, &local_tdaddr) &&
        BtDevice_GetPublicAddress(&ppl->peer, &peer_taddr))
    {
        GattRootKeyClientChallengePeer(&ppl->root_key_client, 
                        secret,
                        &local_tdaddr.addr,
                        &peer_taddr.addr);
    }
    else
    {
        /* Typically caused by peer bud disconnecting during procedure. */
        peer_pair_le_handle_failure();
    }

}

static void peer_pair_le_convert_grks_key_to_key(uint16 *dest, const GRKS_KEY_T *src_key)
{
    const uint8 *src = src_key->key;
    int words = GRKS_KEY_SIZE_128BIT_WORDS;
    unsigned word;

    while (words--)
    {
        word = *src++;
        word += ((unsigned)(*src++)) << 8;
        *dest++ = (uint16)word;
    }
}

static void peer_pair_le_convert_key_to_grks_key(GRKS_KEY_T *dest_key, const uint16 *src)
{
    uint8 *dest = dest_key->key;
    int octets = GRKS_KEY_SIZE_128BIT_OCTETS;

    while (octets > 1)
    {
        *dest++ = (uint8)(*src & 0xFF);
        *dest++ = (uint8)(((*src++)>>8) & 0xFF);
        octets -= 2;
    }
}

static void peer_pair_le_get_root_keys(GRKS_KEY_T *ir, GRKS_KEY_T *er)
{
    cl_root_keys    root_keys;
    PanicFalse(ConnectionReadRootKeys(&root_keys));

    peer_pair_le_convert_key_to_grks_key(ir, root_keys.ir);
    peer_pair_le_convert_key_to_grks_key(er, root_keys.er);
}

static void peer_pair_le_handle_client_challenge_ind(const GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND_T *challenge)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_client_challenge_ind. sts enum:gatt_root_key_challenge_status_t:%d",
              challenge->status);

    if (gatt_root_key_challenge_status_success == challenge->status)
    {
        GRKS_IR_KEY_T ir;
        GRKS_IR_KEY_T er;

        /* Service API defined as IR & ER so convert internally */
        peer_pair_le_get_root_keys(&ir, &er);

        GattRootKeyClientWriteKeyPeer(&ppl->root_key_client, &ir, &er);
    }

    /*! \todo Handle challenge failure */
}


static void peer_pair_le_add_device_entry(const typed_bdaddr *address, deviceType type, unsigned flags)
{
    typed_bdaddr public_address;

    PanicFalse(BtDevice_GetPublicAddress(address, &public_address));

    device_t device = BtDevice_GetDeviceCreateIfNew(&public_address.addr, type);
    BtDevice_SetDefaultProperties(device);

    Device_SetPropertyU16(device, device_property_flags, (uint16)flags);
    Device_SetPropertyU16(device, device_property_sco_fwd_features, 0x0);

    /* Now that we have successfully paired, we can set the link behavior
     * within bluestack to disable connection retires */
    BtDevice_SetLinkBehavior(&address->addr);
}


static void peer_pair_le_add_paired_device_entries(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    unsigned local_flags;
    unsigned remote_flags;
    typed_bdaddr local_taddr;
    typed_bdaddr peer_taddr;
    const unsigned primary_flags = DEVICE_FLAGS_PRIMARY_ADDR | DEVICE_FLAGS_MIRRORING_C_ROLE;
    const unsigned secondary_flags = DEVICE_FLAGS_SECONDARY_ADDR;

    PanicFalse(BtDevice_GetPublicAddress(&ppl->local_addr, &local_taddr));
    PanicFalse(BtDevice_GetPublicAddress(&ppl->peer, &peer_taddr));

    if (PeerPairLeIsLeft())
    {
        local_flags = primary_flags;
        remote_flags = secondary_flags;
    }
    else
    {
        local_flags = secondary_flags;
        remote_flags = primary_flags;
    }

    DEBUG_LOG_INFO("peer_pair_le_add_paired_device_entries. Me: %04x Primary:%d C_Role:%d",
                    local_taddr.addr.lap,
                    !!(local_flags & DEVICE_FLAGS_MIRRORING_C_ROLE),
                    !!(local_flags & DEVICE_FLAGS_PRIMARY_ADDR));

    peer_pair_le_add_device_entry(&local_taddr, DEVICE_TYPE_SELF, 
                                  DEVICE_FLAGS_MIRRORING_ME | local_flags);
    
    /* Set the protection for local earbud so just in case when trusted device list(appConfigMaxTrustedDevices)
       is exhasuted, connection library will not overwrite entry of this earbud in the PDL */

    ConnectionAuthSetPriorityDevice(&local_taddr.addr, TRUE);

    DEBUG_LOG_INFO("peer_pair_le_add_paired_device_entries. Peer: %04x Primary:%d C_Role:%d",
                    peer_taddr.addr.lap,
                    !!(remote_flags & DEVICE_FLAGS_MIRRORING_C_ROLE),
                    !!(remote_flags & DEVICE_FLAGS_PRIMARY_ADDR));

    peer_pair_le_add_device_entry(&peer_taddr, DEVICE_TYPE_EARBUD, 
                                  remote_flags);

    /* Set the protection for peer earbud so just in case when trusted device list(appConfigMaxTrustedDevices)
       is exhasuted, connection library will not overwrite entry of this earbud in the PDL */
    ConnectionAuthSetPriorityDevice(&peer_taddr.addr, TRUE);


    /* Now the peer has paired, update the PDL with its persistent device data. This is in order to ensure
       we don't lose peer attributes in the case of unexpected power loss. N.b. Normally serialisation occurs
       during a controlled shutdown of the App. */
    DeviceDbSerialiser_Serialise();
    BtDevice_PrintAllDevices();

}

static void peer_pair_le_clone_peer_keys(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    typed_bdaddr local_taddr;
    typed_bdaddr peer_taddr;
    
    PanicFalse(BtDevice_GetPublicAddress(&ppl->local_addr, &local_taddr));
    PanicFalse(BtDevice_GetPublicAddress(&ppl->peer, &peer_taddr));
    
    DEBUG_LOG("peer_pair_le_clone_peer_keys: peer %d %04x %02x %06x local %d %04x %02x %06x",
              peer_taddr.type, peer_taddr.addr.nap, peer_taddr.addr.uap, peer_taddr.addr.lap,
              local_taddr.type, local_taddr.addr.nap, local_taddr.addr.uap, local_taddr.addr.lap);
        
    /*! \todo Is Panic too severe. Bear in mind this is early days peer pairing */
    PanicFalse(ConnectionTrustedDeviceListDuplicate(&peer_taddr.addr, &local_taddr.addr));
}


static void peer_pair_le_handle_client_keys_written(const GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND_T *written)
{
    peer_pair_le_status_t status = peer_pair_le_status_success;

    DEBUG_LOG("peer_pair_le_handle_client_keys_written. sts enum:peer_pair_le_status_t:%d",
              written->status);

    if (gatt_root_key_client_write_keys_success != written->status)
    {
        status = peer_pair_le_status_failed;
        peer_pair_le_send_pair_confirm(status);
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED);
    }
    else
    {
        peerPairLeRunTimeData *ppl = PeerPairLeGetData();
        typed_bdaddr peer_taddr;
        cl_irk irk;

        PanicFalse(BtDevice_GetPublicAddress(&ppl->peer, &peer_taddr));
        
       /* We have updated the keys on our peer, so the IRK we have for them is wrong */
        ConnectionSmGetLocalIrk(&irk);
        ConnectionReplaceIrk(PeerPairLeGetTask(), &peer_taddr.addr, &irk);
        DEBUG_LOG("peer_pair_le_handle_client_keys_written - ConnectionReplaceIrk");
    }
}

static void peer_pair_le_handle_irk_replaced(const CL_SM_AUTH_REPLACE_IRK_CFM_T * cfm)
{
    DEBUG_LOG("peer_pair_le_handle_irk_replaced enum:connection_lib_status:%d 0x%04x",
              cfm->status, cfm->bd_addr.lap);
    
    peer_pair_le_clone_peer_keys();

    peer_pair_le_add_paired_device_entries();
    peer_pair_le_send_pair_confirm(peer_pair_le_status_success);
    peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED);
}

static void peer_pair_le_handle_server_challenge_ind(const GATT_ROOT_KEY_SERVER_CHALLENGE_IND_T *challenge)
{
    DEBUG_LOG("peer_pair_le_handle_server_challenge_ind. sts enum:gatt_root_key_challenge_status_t:%d",
              challenge->status);

    if (gatt_root_key_challenge_status_success != challenge->status)
    {
        /*! \todo Handle challenge failure */
    }
}

#include "local_addr.h"
static void peer_pair_le_handle_server_keys_written(const GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND_T *written)
{
    cl_root_keys root_keys;
    cl_irk irk;

    DEBUG_LOG("peer_pair_le_handle_server_keys_written");

    DEBUG_LOG("IR: %02x %02x %02x %02x %02x",written->ir.key[0],written->ir.key[1],written->ir.key[2],
                                   written->ir.key[3],written->ir.key[4]);
    DEBUG_LOG("ER: %02x %02x %02x %02x %02x",written->er.key[0],written->er.key[1],written->er.key[2],
                                   written->er.key[3],written->er.key[4]);

    peer_pair_le_convert_grks_key_to_key(root_keys.ir, &written->ir);
    peer_pair_le_convert_grks_key_to_key(root_keys.er, &written->er);
    PanicFalse(ConnectionSetRootKeys(&root_keys));
    LocalAddr_ReconfigureBleGeneration();

    ConnectionSmGetLocalIrk(&irk);
    DEBUG_LOG("Local IRK :%04x %04x %04x %04x",irk.irk[0],irk.irk[1],irk.irk[2],irk.irk[3]);

    peer_pair_le_clone_peer_keys();

    peer_pair_le_send_pair_confirm(peer_pair_le_status_success);
    peer_pair_le_add_paired_device_entries();

    peer_pair_le_set_state(PEER_PAIR_LE_STATE_COMPLETED_WAIT_FOR_DISCONNECT);

    GattRootKeyServerKeyUpdateResponse(written->instance);
}

static void peer_pair_le_handle_start_scan (const LE_SCAN_MANAGER_START_CFM_T* message)
{
    if(LE_SCAN_MANAGER_RESULT_SUCCESS != message->status)
    {
        DEBUG_LOG(" peer_pair_le_handle_start_scan. Unable to acquire scan. status enum:le_scan_result_t:%d",
                  message->status);
    }
}

static void peer_pair_le_handle_stop_scan(const LE_SCAN_MANAGER_STOP_CFM_T* message)
{
    if (LE_SCAN_MANAGER_RESULT_SUCCESS != message->status)
    {
        DEBUG_LOG_WARN(" peer_pair_le_handle_stop_scan. Unable to stop scan");
    }

    if (peer_pair_le_get_state() == PEER_PAIR_LE_STATE_REDISCOVERY)
    {
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
    }
}

static void peer_pair_le_handle_adv_mgr_release_dataset_cfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG("peer_pair_le_handle_adv_mgr_release_dataset_cfm. sts enum:le_adv_mgr_status_t:%d",
              cfm->status);

    if (ppl)
    {
        ppl->advertising_active = FALSE;
    }
}

static void peerPairLe_handle_pairing_pair_confirm_success(void)
{
    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_NEGOTIATE_C_ROLE);
            break;

        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_NEGOTIATE_P_ROLE);
            break;

        default:
            DEBUG_LOG("peerPairLe_handle_pairing_pair_confirm. success in unexpected state enum:PEER_PAIR_LE_STATE:%d",
                        peer_pair_le_get_state());
            Panic();
            break;
    }
}

static void peerPairLe_handle_pairing_pair_confirm_not_success(void)
{
    switch (peer_pair_le_get_state())
    {
        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
            peer_pair_le_set_state(PEER_PAIR_LE_STATE_DISCOVERY);
            break;

        default:
            Panic();
            break;
    }
}

static void peerPairLe_handle_pairing_pair_confirm(PAIRING_PAIR_CFM_T *cfm)
{
    DEBUG_LOG("peerPairLe_handle_pairing_pair_confirm enum:pairingStatus:%d addr=%04x %02x %06x",
              cfm->status,
              cfm->device_bd_addr.nap,
              cfm->device_bd_addr.uap,
              cfm->device_bd_addr.lap);

    switch(cfm->status)
    {
        case pairingSuccess:
            peerPairLe_handle_pairing_pair_confirm_success();
            break;

        case pairingTimeout:
        case pairingFailed:
            peerPairLe_handle_pairing_pair_confirm_not_success();
            break;

        default:
            Panic();
            break;
    }
}

static void peerPairLe_handle_pairing_stop(const PAIRING_STOP_CFM_T *cfm)
{
    DEBUG_LOG("peerPairLe_handle_pairing_stop enum:pairingStatus:%d",
              cfm->status);

    /* If we receive a pairingInProgress status then we are/should be
       guaranteed a subsequent PAIRING_PAIR_CFM */
    if (cfm->status != pairingInProgress)
    {
        peer_pair_le_handle_failure();
    }
}

static void peer_pair_le_handler(Task task, MessageId id, Message message)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    UNUSED(task);

    if (!ppl)
    {
        /* We are not initialised. Discard message */
        return;
    }

    switch (id)
    {
        /* ---- Internal messages ---- */
        case PEER_PAIR_LE_INTERNAL_FIND_PEER:
            peer_pair_le_handle_find_peer();
            break;

        case PEER_PAIR_LE_INTERNAL_COMPLETED:
            peer_pair_le_handle_completed();
            break;

        case PEER_PAIR_LE_INTERNAL_ENABLE_ADVERTISING:
            peer_pair_le_handle_enable_advertising();
            break;

        /* ---- Timeouts ---- */
        case PEER_PAIR_LE_TIMEOUT_FROM_FIRST_SCAN :
            peer_pair_le_handle_scan_timeout();
            break;

        case PEER_PAIR_LE_TIMEOUT_REDISCOVERY:
            peer_pair_le_handle_rediscovery_timeout();
            break;

        /* ---- connection library responses (directed to this task) ---- */
        case CL_DM_LOCAL_BD_ADDR_CFM:
            peer_pair_le_handle_local_bdaddr((const CL_DM_LOCAL_BD_ADDR_CFM_T *)message);
            break;

        case CL_SM_AUTH_REPLACE_IRK_CFM:
            peer_pair_le_handle_irk_replaced((const CL_SM_AUTH_REPLACE_IRK_CFM_T *)message);
            break;

        /* ---- Advertising Manager messages ---- */
        case LE_ADV_MGR_SELECT_DATASET_CFM:
            peer_pair_le_handle_adv_mgr_select_dataset_cfm((const LE_ADV_MGR_SELECT_DATASET_CFM_T *)message);
            break;

        /* ---- Connection Manager messages ---- */
        case CON_MANAGER_TP_CONNECT_IND:
            peer_pair_le_handle_con_manager_connection((const CON_MANAGER_TP_CONNECT_IND_T *)message);
            break;
            
        case CON_MANAGER_TP_DISCONNECT_IND:
            peer_pair_le_handle_con_manager_disconnection((const CON_MANAGER_TP_DISCONNECT_IND_T *)message);
            break;
            
        /* ---- GATT messages ---- */
        case GATT_DISCOVER_PRIMARY_SERVICE_CFM:
            peer_pair_le_handle_primary_service_discovery((const GATT_DISCOVER_PRIMARY_SERVICE_CFM_T* )message);
            break;

        case GATT_ROOT_KEY_CLIENT_INIT_CFM:
            peer_pair_le_handle_client_init((const GATT_ROOT_KEY_CLIENT_INIT_CFM_T *)message);
            break;

        case GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND:
            peer_pair_le_handle_client_challenge_ind((const GATT_ROOT_KEY_CLIENT_CHALLENGE_PEER_IND_T *)message);
            break;

        case GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND:
            peer_pair_le_handle_client_keys_written((const GATT_ROOT_KEY_CLIENT_WRITE_KEY_IND_T *)message);
            break;

        case GATT_ROOT_KEY_SERVER_CHALLENGE_IND:
            peer_pair_le_handle_server_challenge_ind((const GATT_ROOT_KEY_SERVER_CHALLENGE_IND_T *)message);
            break;

        case GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND:
            peer_pair_le_handle_server_keys_written((GATT_ROOT_KEY_SERVER_KEY_UPDATE_IND_T *)message);
            break;

         /* ---- LE Advertising Manager messages ---- */
         case LE_ADV_MGR_RELEASE_DATASET_CFM:
             peer_pair_le_handle_adv_mgr_release_dataset_cfm((const LE_ADV_MGR_RELEASE_DATASET_CFM_T *)message);
             break;

         /* ---- LE SCAN Manager messages ---- */
        case LE_SCAN_MANAGER_START_CFM:
            DEBUG_LOG("peer_pair_le_handler LE Scan Manager is Started! enum:le_scan_result_t:%d",
                      ((LE_SCAN_MANAGER_START_CFM_T*)message)->status);
            peer_pair_le_handle_start_scan((LE_SCAN_MANAGER_START_CFM_T*)message);
            break;
        
        case LE_SCAN_MANAGER_STOP_CFM:
            DEBUG_LOG("peer_pair_le_handler LE Scan Manager is Stopped!  enum:le_scan_result_t:%d",
                      ((LE_SCAN_MANAGER_STOP_CFM_T*)message)->status);
            peer_pair_le_handle_stop_scan((LE_SCAN_MANAGER_STOP_CFM_T*)message);
            break;

        case LE_SCAN_MANAGER_ADV_REPORT_IND:
            PeerPairLe_HandleFoundDeviceScan((LE_SCAN_MANAGER_ADV_REPORT_IND_T*) message);
            break;

        /* ---- Pairing module messages ---- */
        case PAIRING_PAIR_CFM:
            peerPairLe_handle_pairing_pair_confirm((PAIRING_PAIR_CFM_T *)message);
            break;

        case PAIRING_STOP_CFM:
            peerPairLe_handle_pairing_stop((PAIRING_STOP_CFM_T *)message);
            break;

        default:
            DEBUG_LOG("peer_pair_le_handler. Unhandled message MESSAGE:peer_pair_le_internal_message_t:0x%x",
                      id);
            break;
    }
} 

/* Return the number of items in the advert.
   For simplicity/safety don't make the same check when getting data items.
 */
static unsigned int peer_pair_le_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    unsigned int items = 0;

    if (peer_pair_le_is_advertising_state(peer_pair_le_get_state()))
    {
        if((le_adv_data_set_peer == params->data_set) && \
           (le_adv_data_completeness_full == params->completeness) && \
           (le_adv_data_placement_advert == params->placement))
        {
            items = NUMBER_OF_ADVERT_DATA_ITEMS;
        }
    }

    return items;
}

static le_adv_data_item_t peer_pair_le_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(id);

    if((le_adv_data_set_peer == params->data_set) && \
        (le_adv_data_completeness_full == params->completeness) && \
        (le_adv_data_placement_advert == params->placement))
    {
        return peer_pair_le_advert;
    }
    else
    {
        Panic();
        return peer_pair_le_advert;
    };
}

static void peer_pair_le_ReleaseAdvDataItems(const le_adv_data_params_t * params)
{
    UNUSED(params);

    return;
}

static void peer_pair_le_GattConnect(gatt_cid_t cid)
{
    
    PEER_PAIR_LE_STATE state = peer_pair_le_get_state();

    DEBUG_LOG_FN_ENTRY("peer_pair_le_GattConnect. state enum:PEER_PAIR_LE_STATE:%d cid:0x%x", state, cid);

    switch(state)
    {
        case PEER_PAIR_LE_STATE_SELECTING:
        case PEER_PAIR_LE_STATE_DISCOVERY:
        case PEER_PAIR_LE_STATE_REDISCOVERY:
        case PEER_PAIR_LE_STATE_PAIRING_AS_SERVER:
        {
            peerPairLeRunTimeData *ppl = PeerPairLeGetData();
            tp_bdaddr tpaddr;
            if (VmGetBdAddrtFromCid(cid, &tpaddr))
            {
                ppl->gatt_cid = cid;

                if (!BtDevice_BdaddrTypedIsSame(&tpaddr.taddr, &ppl->peer))
                {
                    DEBUG_LOG_INFO("peer_pair_le_GattConnect: %04x %02x %06x != %04x %02x %06x",
                        ppl->peer.addr.nap, ppl->peer.addr.uap, ppl->peer.addr.lap,
                        tpaddr.taddr.addr.nap, tpaddr.taddr.addr.uap, tpaddr.taddr.addr.lap);
                        
                    Panic();
                }

                if (PEER_PAIR_LE_STATE_PAIRING_AS_SERVER != state)
                {
                    peer_pair_le_set_state(PEER_PAIR_LE_STATE_PAIRING_AS_SERVER);
                }
            }
            else
            {
                DEBUG_LOG_WARN("peer_pair_le_GattConnect. Gatt 'gone' by time received GattConnect.");
            }
            break;
        }

        case PEER_PAIR_LE_STATE_UNINITIALISED:
        case PEER_PAIR_LE_STATE_INITIALISED:
        case PEER_PAIR_LE_STATE_PAIRING_AS_CLIENT:
            /* Can't unregister observer. Block debug */
            break;

        default:
            DEBUG_LOG_WARN("peer_pair_le_GattConnect. cid:0x%x. Not in correct state:%d", cid, state);
            break;
    }
}

static void peer_pair_le_GattDisconnect(gatt_cid_t cid)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    DEBUG_LOG_WARN("peer_pair_le_GattDisconnect. cid:0x%x", cid);

    if (ppl)
    {
        if (ppl->gatt_cid == cid)
        {
            ppl->gatt_cid = INVALID_CID;
        }
    }
}

static void peer_pair_le_register_advertising(void)
{
    peer_pair_le_advert.size = SIZE_PEER_PAIR_LE_ADVERT;

    if (PeerPairLeIsLeft())
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_left;
    }
    else if (PeerPairLeIsRight())
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_right;
    }
    else
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_common;
    }

    LeAdvertisingManager_Register(NULL, &peer_pair_le_advert_callback);
}

static void peer_pair_le_register_connections(void)
{
    ConManagerRegisterTpConnectionsObserver(cm_transport_ble, PeerPairLeGetTask());
    GattConnect_RegisterObserver(&peer_pair_le_connect_callback);
}


void peer_pair_le_init(void)
{
    if (PEER_PAIR_LE_STATE_UNINITIALISED == peer_pair_le_get_state())
    {
        gatt_root_key_server_init_params_t prms = {TRUE,GATT_ROOT_KEY_SERVICE_FEATURES_DEFAULT};
        
        PeerPairLeGetTask()->handler = peer_pair_le_handler;

        if (!GattRootKeyServerInit(&PeerPairLeGetRootKeyServer(),
                        PeerPairLeGetTask(),&prms,
                        HANDLE_ROOT_TRANSFER_SERVICE, HANDLE_ROOT_TRANSFER_SERVICE_END))
        {
            DEBUG_LOG("peer_pair_le_init. Server init failed");
            Panic();
        }

        peer_pair_le_register_connections();

        peer_pair_le_register_advertising();

        peer_pair_le_set_state(PEER_PAIR_LE_STATE_INITIALISED);
    }
}

void peer_pair_le_start_service(void)
{
    DEBUG_LOG("peer_pair_le_start_service state enum:PEER_PAIR_LE_STATE:%d",
              peer_pair_le_get_state());

    if (PEER_PAIR_LE_STATE_INITIALISED == peer_pair_le_get_state())
    {
        peer_pair_le.data = (peerPairLeRunTimeData*)PanicNull(calloc(1, sizeof(peerPairLeRunTimeData)));

        /* If local device reset during peer pairing, then a partially paired
           peer device could be present in device database. Delete any partially
           paired peer devices */
        BtDevice_DeleteAllDevicesOfType(DEVICE_TYPE_EARBUD);
        PeerPairLe_DeviceSetAllEmpty();
        BdaddrTypedSetEmpty(&peer_pair_le.data->peer);
        BdaddrTypedSetEmpty(&peer_pair_le.data->local_addr);
        peer_pair_le.data->gatt_cid = INVALID_CID;
        peer_pair_le_set_state(PEER_PAIR_LE_STATE_PENDING_LOCAL_ADDR);
    }
}


void peer_pair_le_disconnect(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();
    tp_bdaddr tp_addr;

    if (ppl)
    {
        tp_addr.transport = TRANSPORT_BLE_ACL;
        tp_addr.taddr = ppl->peer;
    
        if (!BdaddrTypedIsEmpty(&ppl->peer))
        {
            ConManagerReleaseTpAcl(&tp_addr);
        }
    }
}


void peer_pair_le_stop_service(void)
{
    peerPairLeRunTimeData *data = PeerPairLeGetData();

    if (data)
    {
        peer_pair_le_release_local_addr_override();

        peer_pair_le.data = NULL;
        free(data);
    }
}

void peer_pair_le_release_local_addr_override(void)
{
    peerPairLeRunTimeData *data = PeerPairLeGetData();

    if (data && data->local_addr_context)
    {
        LocalAddr_ReleaseOverride(&data->local_addr_context);
    }
}


void PeerPairLe_DeviceSetEmpty(peerPairLeFoundDevice *device)
{
    BdaddrTypedSetEmpty(&device->taddr);
    device->rssi = SCHAR_MIN;
}

void PeerPairLe_DeviceSetAllEmpty(void)
{
    peerPairLeRunTimeData *ppl = PeerPairLeGetData();

    PeerPairLe_DeviceSetEmpty(&ppl->scanned_devices[0]);
    PeerPairLe_DeviceSetEmpty(&ppl->scanned_devices[1]);
}
