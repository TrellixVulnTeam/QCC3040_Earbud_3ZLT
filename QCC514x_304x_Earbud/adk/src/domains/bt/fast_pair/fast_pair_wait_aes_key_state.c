/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_aes_key_state.c
\brief      Fast Pair Wait for AES Key State Event handling
*/
#include "fast_pair_wait_aes_key_state.h"
#include "fast_pair_pairing_if.h"
#include "bt_device.h"
#include "device_properties.h"
#include "fast_pair_events.h"
#include "fast_pair_advertising.h"
#include "fast_pair_gfps.h"

#include "connection_manager.h"
#include "focus_device.h"
#include "ui.h"

#include "handset_service_config.h"

#define FAST_PAIR_MSG_TYPE_KEY_BASED_PAIRING_REQ 0x00
#define FAST_PAIR_MSG_TYPE_DEVICE_ACTION_REQ     0x10

/* Flags under KBP request */
#define FAST_PAIR_REQ_START_PAIRING     (0x40) /* Byte-1, bit-6 (MSB bit-1) */
#define FAST_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY_FLAG_BIT (0x10) /* Byte-1, bit-4 (MSB bit-3) */
#define FAST_PAIR_REQ_ADDITIONAL_DATA_PNAME_NOTIFY     (0x20)

/* Flags under KBP Device action message type */
#define FAST_PAIR_DEVICE_ACTION_REQ_DEVICE_ACTION_FLAG             (0x80) /* Byte-1, bit-7 (MSB bit-0) */
#define FAST_PAIR_DEVICE_ACTION_REQ_ADDITIONAL_DATA_CHARS_FLAG     (0x40) /* Byte-1, bit-6 (MSB bit-1) */
#define FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_OCTET_NUMBER           (10)

static bool fastPair_EcdhSharedSecretEventHandler(fast_pair_state_event_crypto_shared_secret_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastPair_EcdhSharedSecretEventHandler");

    theFastPair = fastPair_GetTaskData();

    if(args->crypto_shared_secret_cfm->status == success)
    {
        ConnectionEncryptBlockSha256(&theFastPair->task, args->crypto_shared_secret_cfm->shared_secret_key, (CL_CRYPTO_SHA_DATA_LEN*2));

        status = TRUE;
    }
    else
    {
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
    return status;
}

static bool fastPair_CheckAESKey(fast_pair_state_event_crypto_hash_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    
    DEBUG_LOG("fastPair_CheckAESKey");

    theFastPair = fastPair_GetTaskData();

    if(args->crypto_hash_cfm->status == success)
    {
        memcpy(theFastPair->session_data.aes_key, args->crypto_hash_cfm->hash, FAST_PAIR_AES_KEY_LEN);
        ConnectionDecryptBlockAes(&theFastPair->task, (uint16 *)theFastPair->session_data.encrypted_data, theFastPair->session_data.aes_key);

        status = TRUE;
    }
    else
    {
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
    return status;
}


static void fastPair_ConvertBigEndianBDAddress(bdaddr *device_addr, uint16 *decrypted_data, uint16 data_offset)
{
    uint8 *decrypted_packet_be = (uint8 *)decrypted_data;

    DEBUG_LOG("fastPair_ConvertBigEndianBDAddress");

    if(device_addr != NULL)
    {
         device_addr->nap = (uint16)(decrypted_packet_be[data_offset]<<8)| decrypted_packet_be[data_offset+1];
         device_addr->uap = (uint8)(decrypted_packet_be[data_offset+2]);
         device_addr->lap = (uint32)(decrypted_packet_be[data_offset+3] & 0xFF) << 16 | (uint32)(decrypted_packet_be[data_offset+4]) << 8 | (uint32)decrypted_packet_be[data_offset+5];
    }
}

static bool fastPair_MatchProviderAddress(uint16 *decrypted_data)
{
    bool status = FALSE;
    uint8 index;
    bdaddr provider_addr;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    fastPair_ConvertBigEndianBDAddress(&provider_addr, decrypted_data, FAST_PAIR_PROVIDER_ADDRESS_OFFSET);

    for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
    {
        /*! Check if the provider adress is matching with any random address entry in the table */
        if(BdaddrIsSame(&theFastPair->own_random_address[index], &provider_addr))
        {
            /*! Copy the random address */
            memcpy(&theFastPair->rpa_bd_addr, &theFastPair->own_random_address[index], sizeof(bdaddr));
            status = TRUE;
        }
    }

    DEBUG_LOG("Provider addr provided by FP Seeker %04x%02x%06lx\n", provider_addr.nap, provider_addr.uap, provider_addr.lap);
    DEBUG_LOG("Local BLE Address %04x%02x%06lx\n", theFastPair->rpa_bd_addr.nap, theFastPair->rpa_bd_addr.uap, theFastPair->rpa_bd_addr.lap);

    if(status == FALSE)
    {
        DEBUG_LOG("Fast Pair provider addr mismatch!");
    }
    return status;
}

static bool fastPair_MatchSeekerAddress(uint16 *decrypted_data)
{
    bool status = FALSE;
    bdaddr seeker_addr;
    device_t device;
    bdaddr handset_addr;
    uint16 pdl_index = 0; // Most recently paired device index
    
    fastPair_ConvertBigEndianBDAddress(&seeker_addr, decrypted_data, FAST_PAIR_SEEKER_ADDRESS_OFFSET);

    DEBUG_LOG("fastPair_MatchSeekerAddress. Seeker addr in KbP request %04x%02x%06lx\n", seeker_addr.nap, seeker_addr.uap, seeker_addr.lap);

    if(BtDevice_GetIndexedDevice(pdl_index, &device))
    {
        handset_addr = DeviceProperties_GetBdAddr(device);
        DEBUG_LOG("MRU handset_addr %04x%02x%06lx",handset_addr.nap, handset_addr.uap, handset_addr.lap);

        if(BdaddrIsSame(&handset_addr, &seeker_addr))
        {
            DEBUG_LOG("fastPair_MatchSeekerAddress. Seeker Address matching");
            status = TRUE;
        }
        else
        {
            DEBUG_LOG("Fast Pair Seeker Address mismatch!");
        }
    }

    return status;
}

static void fastPair_ClearProcessedAccountKeys(void)
{
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    /* Free fetched account keys during subsequent pairing */
    free(theFastPair->session_data.account_key.keys);
    theFastPair->session_data.account_key.keys = NULL;
    theFastPair->session_data.account_key.num_keys = 0;
    theFastPair->session_data.account_key.num_keys_processed = 0;
}

static uint8* fastPair_GenerateKbPResponse(void)
{
    uint8 *response = PanicUnlessMalloc(FAST_PAIR_ENCRYPTED_REQUEST_LEN);
    uint16 i;
    bdaddr local_addr;
    
    DEBUG_LOG("fastPair_GenerateKbPResponse");

    /* Check local addrss */
    appDeviceGetMyBdAddr(&local_addr);

    DEBUG_LOG("Local BD Address %04x%02x%06lx\n", local_addr.nap, local_addr.uap, local_addr.lap);

    response[0] = 0x01;
    response[1] = (local_addr.nap >> 8) & 0xFF;
    response[2] = local_addr.nap & 0xFF;
    response[3] = local_addr.uap;
    response[4] = (local_addr.lap >> 16) & 0xFF;
    response[5] = (local_addr.lap >> 8) & 0xFF;
    response[6] = local_addr.lap & 0xFF;
    for (i = 7; i < 16; i++)
    {
        response[i] = UtilRandom() & 0xFF;
    }

    return response;

}

static bool fastpair_StateWaitAESKeyHandleAuthCfm(fast_pair_state_event_auth_args_t* args)
{
    le_adv_data_set_t data_set;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    if(args->auth_cfm->status == auth_status_success)
    {
        DEBUG_LOG("fastpair_StateWaitAESKeyHandleAuthCfm. CL_SM_AUTHENTICATE_CFM status %d", args->auth_cfm->status);
        data_set = le_adv_data_set_handset_unidentifiable;
        fastPair_SetIdentifiable(data_set);

        /* After setting the identifiable parameter to unidentifiable, Set the FP state to idle */
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);

        return TRUE;
    }
    return FALSE;
}


/* Handle Device action request packet. Parse the packet and act accordingly */
static void fastPair_HandleDeviceActionRequestPacket(uint8* decrypted_data_be)
{
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastPair_HandleDeviceActionRequestPacket called");

    theFastPair = fastPair_GetTaskData();

    if(theFastPair->session_data.account_key.num_keys)
    {
        fastPair_ClearProcessedAccountKeys();
    }

    /* If Device action is set, dont do anything as it is not supported */
    if(decrypted_data_be[1] & FAST_PAIR_DEVICE_ACTION_REQ_DEVICE_ACTION_FLAG )
    {
        DEBUG_LOG_ERROR("fastPair_HandleDeviceActionRequestPacket: FAST_PAIR_DEVICE_ACTION_REQ_DEVICE_ACTION_FLAG set. Ignoring as not supported. ");
    }

    theFastPair->session_data.kbp_action_request_data_id = 0xFF;
    /* If additional data characteristics is set */
    if(decrypted_data_be[1] & FAST_PAIR_DEVICE_ACTION_REQ_ADDITIONAL_DATA_CHARS_FLAG)
    {
       DEBUG_LOG("fastPair_HandleDeviceActionRequestPacket: FAST_PAIR_DEVICE_ACTION_REQ_ADDITIONAL_DATA_CHARS_FLAG set. ");

       if(decrypted_data_be[FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_OCTET_NUMBER] == FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_PNAME)
       {
           DEBUG_LOG("fastPair_HandleDeviceActionRequestPacket: FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_PNAME set. ");
           theFastPair->session_data.kbp_action_request_data_id = decrypted_data_be[FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_OCTET_NUMBER];
       }
       else
       {
           DEBUG_LOG_ERROR("fastPair_HandleDeviceActionRequestPacket: Unsupported Data ID %X",decrypted_data_be[FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_OCTET_NUMBER]);
       }

       fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_ADDITIONAL_DATA);
    }
    else
    {
        /* Move to idle state incase of invalid flag values */
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
}

/* Handle KBP request packet. Parse KBP packet and act accordingly. */
static void fastPair_HandleKBPRequestPacket(uint8* decrypted_data_be)
{
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastPair_HandleKBPRequestPacket called. Flags Byte %X",decrypted_data_be[1] );

    theFastPair = fastPair_GetTaskData();

    fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_PAIRING_REQUEST);

    if(theFastPair->session_data.account_key.num_keys)
    {
        fastPair_ClearProcessedAccountKeys();
    }

    /* If seeker request provider to be initiate pairing, send pairing request to seeker
       using BD Addr provided by seeker in encrypted KbP packet over BR/EDR Transport
     */
    if(decrypted_data_be[1] & FAST_PAIR_REQ_START_PAIRING)
    {
        DEBUG_LOG("Provider_Initiated_pairing");

        fastPair_StopTimer();

        fastPair_ConvertBigEndianBDAddress(&theFastPair->seeker_addr, (uint16 *)decrypted_data_be, FAST_PAIR_SEEKER_ADDRESS_OFFSET);

        DEBUG_LOG("seeker_bd addr %04x:%02x:%06x", theFastPair->seeker_addr.nap, theFastPair->seeker_addr.uap, theFastPair->seeker_addr.lap);

        theFastPair->provider_pairing_requested = TRUE;

        /* Stop pairing initiated by earbud applicaiton */
        Pairing_PairStop(&theFastPair->task);
    }

    /* If seeker wants to write account key retroactively, check the decrypted KbP request flag bit 3(MSB) set and verify 
       the BD address of the bonded device.
    */
    if(decrypted_data_be[1] & FAST_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY_FLAG_BIT)
    {
        if(fastPair_MatchSeekerAddress((uint16 *)decrypted_data_be))
        { 
            DEBUG_LOG("Accept the request of writing account key retroactively.");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_ACCOUNT_KEY);
            /* After accepting one retroactively writing account key request, reset the flag so that
               another request can't be entertained 
            */
            fastPair_SetRetroactivelyWritingAccountKeyFlag(FALSE);
        }
        else
        {
            DEBUG_LOG("Reject the request.");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
    }
    else
    {
        unsigned connected_handsets;
        /* Get the total number of connected BR/EDR handsets */
        connected_handsets = HandsetService_GetNumberOfConnectedBredrHandsets();
        /* If the number of connected BR/EDR handsets is two,
            Disconnect the non-active handset before pairing with 
            the third handset which is going to attempt subsequent pairing.
            Inject ui_input_disconnect_lru_handset to the disconnect the Least
            Recently Used (LRU) device in this case.
        */
        if(connected_handsets == handsetService_BredrAclMaxConnections())
        {        
            DEBUG_LOG("Disconnect the LRU handset to allow subsequent pairing with new handset.");
            Ui_InjectUiInput(ui_input_disconnect_lru_handset);
        }
    }

    theFastPair->session_data.kbp_pname_request_flag = FALSE;
    /* Seeker requests that the provider shall notify the existing name */
    if(decrypted_data_be[1] & FAST_PAIR_REQ_ADDITIONAL_DATA_PNAME_NOTIFY)
    {
        DEBUG_LOG("fastPair_HandleKBPRequestPacket: FAST_PAIR_REQ_ADDITIONAL_DATA_PNAME_NOTIFY bit set");
        theFastPair->session_data.kbp_pname_request_flag = TRUE;
    }
}

static bool fastPair_ValidateAESKey(fast_pair_state_event_crypto_decrypt_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    
    DEBUG_LOG("fastPair_ValidateAESKey");

    theFastPair = fastPair_GetTaskData();

    if(args->crypto_decrypt_cfm->status == success)
    {
        if(fastPair_MatchProviderAddress(args->crypto_decrypt_cfm->decrypted_data))
        {
            uint8 *kbp_data =  (uint8 *)args->crypto_decrypt_cfm->decrypted_data;
            uint8* raw_response = fastPair_GenerateKbPResponse();

            if(FAST_PAIR_MSG_TYPE_KEY_BASED_PAIRING_REQ == kbp_data[0])
            {
                fastPair_HandleKBPRequestPacket(kbp_data);
            }
            else if(FAST_PAIR_MSG_TYPE_DEVICE_ACTION_REQ == kbp_data[0])
            {
                fastPair_HandleDeviceActionRequestPacket(kbp_data);
            }
            else
            {
                DEBUG_LOG_ERROR("fastPair_ValidateAESKey: UNKNOWN MSG TYPE %X in KBP data",kbp_data[0] );
                /*! Set the fast pair state to idle state. */
                fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                return status;
            }

            /* Encrypt Raw KbP Response with AES Key */
            ConnectionEncryptBlockAes(&theFastPair->task, (uint16 *)raw_response, theFastPair->session_data.aes_key);

            free(raw_response);
        }
        else
        {
            if(theFastPair->session_data.account_key.num_keys)
            {
                if(theFastPair->session_data.account_key.num_keys_processed < theFastPair->session_data.account_key.num_keys)
                {
                    /* Use next account key to decrypt KbP packet */
                     memcpy(theFastPair->session_data.aes_key, &theFastPair->session_data.account_key.keys[theFastPair->session_data.account_key.num_keys_processed * FAST_PAIR_AES_KEY_LEN], FAST_PAIR_AES_KEY_LEN);
                    ConnectionDecryptBlockAes(&theFastPair->task, (uint16 *)theFastPair->session_data.encrypted_data, theFastPair->session_data.aes_key);
                    theFastPair->session_data.account_key.num_keys_processed++;
                }
                else
                {
                    fastPair_ClearProcessedAccountKeys();

                    /* The counter is incremented here to adhere to failure
                    handling mechanism as per FastPair specification*/
                    theFastPair->failure_count++;

                    /* No Valid AES Key!. Free it and set fast Pair state to state to Idle */
                    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                }
            }
            else
            {
                /* The counter is incremented here to adhere to failure
                handling mechanism as per FastPair specification*/
                theFastPair->failure_count++;

                /* No Valid AES Key!. Free it and set fast Pair state to state to Idle */
                fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
            }
        }

        status = TRUE;
    }
    else
    {
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
    return status;
}

static bool fastpair_StateWaitAESKeyProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StateWaitAESKeyProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateWaitAESKeyProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));
            }
        }
        status = TRUE;
    }
    return status;
}

bool fastPair_IsProviderPairingRequested(void)
{
    fastPairTaskData *theFastPair;
    theFastPair = fastPair_GetTaskData();
    return theFastPair->provider_pairing_requested;
}

bool fastPair_StateWaitAESKeyHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    
    theFastPair = fastPair_GetTaskData();

    DEBUG_LOG("fastPair_StateWaitAESKeyHandleEvent: EventID=%d", event.id);
    /* Return if event is related to handset connection allowed/disallowed and is handled */
    if(fastPair_HandsetConnectStatusChange(event.id))
    {
        return TRUE;
    }

    switch (event.id)
    {
        case fast_pair_state_event_crypto_shared_secret:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_EcdhSharedSecretEventHandler((fast_pair_state_event_crypto_shared_secret_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_CheckAESKey((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_decrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_ValidateAESKey((fast_pair_state_event_crypto_decrypt_args_t *)event.args);
        }
        break;
        
        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        case fast_pair_state_event_auth:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateWaitAESKeyHandleAuthCfm((fast_pair_state_event_auth_args_t *) event.args);
        }
        break;

        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateWaitAESKeyProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;

        default:
        {
            DEBUG_LOG("Unhandled event [%d]", event.id);
        }
        break;
    }
    return status;
}
