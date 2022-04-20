/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_additional_data_state.c
\brief      Fast Pair Wait for Additional Data State Event handling
*/

#include <stdlib.h>
#include "fast_pair_wait_additional_data_state.h"
#include "fast_pair_session_data.h"
#include "fast_pair_events.h"
#include "fast_pair_gfps.h"
#include "fast_pair_wait_account_key_state.h"
#include "fast_pair_wait_aes_key_state.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_pname_state.h"
#include "cryptoalgo.h"

static bool fastpair_StateAdditionalDataProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));

                /* If the disconnecting device is not a peer earbud i.e. FP seeker, move to idle state. */
				if(FALSE == BtDevice_LeDeviceIsPeer(&(args->disconnect_ind->tpaddr)))
				{
				    DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect: Remote device closed the connection. Moving to FAST_PAIR_STATE_IDLE ");
				    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                }
            }
        }
        status = TRUE;
    }
    return status;
}

static bool fastPair_HandleAdvBloomFilterCalc(fast_pair_state_event_crypto_hash_args_t* args)
{
    DEBUG_LOG("fastPair_HandleAdvBloomFilterCalc");

    if(args->crypto_hash_cfm->status == success)
    {
        fastPair_AdvHandleHashCfm(args->crypto_hash_cfm);
        return TRUE;
    }
    return FALSE;
}

/* Take the decision how to process teh additiona ldata. Right now, it is only Personalized name. */
static bool fastpair_AdditionalDataWriteEventHandler(fast_pair_state_event_additional_data_write_args_t* args)
{
    fastPairTaskData *theFastPair;
    fastPairState prev_state;
    uint8 *decr_data;
    uint8 hmac_sha256_out[SHA256_DIGEST_SIZE];
    uint8 *data_index;
    uint8 *nonce_index;
    uint8 *key_index;
    uint8 data_sz;
    fast_pair_additional_data_type data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_NONE;
    bool status = FALSE;

    DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler called ");

    theFastPair = fastPair_GetTaskData();

    if((NULL == args->enc_data)||(args->size<=ADDITIONAL_DATA_PACKET_DATA_INDEX))
    {
        DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: Error- No pname data or wrong input data ");
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        return FALSE;
    }

    prev_state = theFastPair->prev_state;

    key_index = (uint8 *)theFastPair->session_data.aes_key;
    nonce_index = (uint8 *)(args->enc_data+ ADDITIONAL_DATA_PACKET_NONCE_INDEX);
    data_index = (uint8 *)(args->enc_data+ADDITIONAL_DATA_PACKET_DATA_INDEX);
    data_sz = args->size-ADDITIONAL_DATA_PACKET_DATA_INDEX;
    decr_data = (uint8 *)PanicUnlessMalloc(data_sz*sizeof(uint8));

    /* If Additional data is sent via naming flows 1 or 2 */
    if((FAST_PAIR_STATE_WAIT_ACCOUNT_KEY == prev_state)|| /* Naming flow-1 via inital pairing */
            (FAST_PAIR_STATE_WAIT_AES_KEY == prev_state)) /* Naming flow-2 via KBP Action Request*/
    {
        DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Prev state is %d. So it is in naming flow-1/2",prev_state);

        aesCtr_decrypt(data_index,data_sz,decr_data,key_index,nonce_index);

        hmac_sha256(data_index,data_sz,hmac_sha256_out,key_index,nonce_index);

        //Verify first 8 bytes of data with hmac_sha256_out. They should match.
        if(memcmp(args->enc_data, hmac_sha256_out, ADDITIONAL_DATA_PACKET_NONCE_INDEX)!=0)
        {
            DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: HMAC sha256 of decoded data does not match with the one in input data.");
            status = FALSE;
        }
        else
        {
            /* For naming flow2, only if the DataID is 0x01 in KBP packet declare the data as for personalized name */
            if(FAST_PAIR_STATE_WAIT_AES_KEY == prev_state)
            {
                if(FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_PNAME == theFastPair->session_data.kbp_action_request_data_id)
                {
                    data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME;
                }
                else
                {
                    DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: Unsupported Data ID %d",theFastPair->session_data.kbp_action_request_data_id);
                    status=FALSE;
                }
            }
            else /* Naming flow1 i.e. (FAST_PAIR_STATE_WAIT_ACCOUNT_KEY == prev_state) */
            {
                data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME;
            }
        }
    }
    else
    {
        DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler-ERROR: Came here from unexpected previous state %d",prev_state);
        status = FALSE;
    }

    /* If the data type of additional data is personalized name, move to PNAME state */
    if(data_type == FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME)
    {
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_PNAME);

        DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Calling fastPair_PNameWrite ");
        //Decoding done. Pass the data to pname.
        fastPair_PNameWrite(decr_data,data_sz);
        status = TRUE;
    }
    else
    {
         DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: UNKNOWN data type. Should not have come here ");
         status = FALSE;
    }

    free(decr_data);
    /* Everything done. Move to idle state */
    DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler . Moving to FAST_PAIR_STATE_IDLE ");
    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    return status;
}


static void generateNonce(uint8 nonce[AES_CTR_NONCE_SIZE])
{
    int valid_nonce_idx,i;
    uint8 rand1;

    nonce[0] = UtilRandom()&0xFF;
    valid_nonce_idx = 1;
    while(valid_nonce_idx < AES_CTR_NONCE_SIZE)
    {
        rand1 = UtilRandom()&0xFF;
        /* Check for duplicates */
        for(i=0;i<valid_nonce_idx;i++)
        {
            if(rand1 == nonce[i])
                 break;
        }
        /* No duplicates found*/
        if(i == valid_nonce_idx)
        {
            nonce[valid_nonce_idx] = rand1;
            valid_nonce_idx++;
        }
    }
}

/* Get Encrypted Additional Data with PName */
bool fastPair_GetEncryptedAdditionalDataHavingPName(uint8 encr_add_data_pname[], uint8 *len)
{
    uint8 pname[FAST_PAIR_PNAME_DATA_LEN];
    uint8 pname_len;
    uint8 *nonce;
    uint8 hmac_sha256_out[SHA256_DIGEST_SIZE];
    uint8 *encr_pname;
    fastPairTaskData *theFastPair;

    *len = 0;
    if(FALSE == fastPair_GetPName(pname,&pname_len))
    {
        DEBUG_LOG("fastPair_GetEncryptedAdditionalDataHavingPName: Error in getting stored PName ");
        return FALSE;
    }

    /* If the length is 0 bytes*/
    if(0==pname_len)
    {
        DEBUG_LOG("fastPair_GetEncryptedAdditionalDataHavingPName: PName is empty");
        return FALSE;
    }
    /* If the length is more than allowed. Should not happen*/
    if(pname_len > FAST_PAIR_PNAME_DATA_LEN)
    {
        DEBUG_LOG_ERROR("fastPair_GetEncryptedAdditionalDataHavingPName: SHOULD NOT HAPPEN. PName length %d is more than allowed %d",pname_len,FAST_PAIR_PNAME_DATA_LEN);
        return FALSE;
    }

    nonce= encr_add_data_pname+ADDITIONAL_DATA_PACKET_NONCE_INDEX;
    encr_pname = encr_add_data_pname+ADDITIONAL_DATA_PACKET_DATA_INDEX;
    generateNonce(nonce);

    theFastPair = fastPair_GetTaskData();
    /* Write the encrypted pname directly to input array at its location*/
    aesCtr_encrypt(pname,pname_len,encr_pname,(uint8 *)theFastPair->session_data.aes_key,nonce);

    /* Perform hmac-sha256*/
    hmac_sha256(encr_pname,pname_len,hmac_sha256_out,(uint8 *)theFastPair->session_data.aes_key,nonce);
    /* Copy first 8 bytes of hmac_sha256_out */
    memcpy(encr_add_data_pname, hmac_sha256_out, ADDITIONAL_DATA_PACKET_NONCE_INDEX);

    *len = pname_len+ADDITIONAL_DATA_PACKET_DATA_INDEX;
    return TRUE;
}

bool fastPair_StateWaitAdditionalDataHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateAdditionalDataHandleEvent event [%d]", event.id);

    switch (event.id)
    {
        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateAdditionalDataProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;

        case fast_pair_state_event_timer_expire:
        {
            DEBUG_LOG("fastPair_StateWaitAdditionalDataHandleEvent: Moving to FAST_PAIR_STATE_IDLE ");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
            status = TRUE;
        }
        break;

        case fast_pair_state_event_additional_data_write:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_AdditionalDataWriteEventHandler((fast_pair_state_event_additional_data_write_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            fastPair_HandleAdvBloomFilterCalc((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;
        case fast_pair_state_event_crypto_encrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            fast_pair_state_event_crypto_encrypt_args_t* args= (fast_pair_state_event_crypto_encrypt_args_t *)event.args;
            DEBUG_LOG("fastPair_SendFPNotification for FAST_PAIR_KEY_BASED_PAIRING");
            if(args->crypto_encrypt_cfm->status == success)
            {
                fastPair_SendFPNotification(FAST_PAIR_KEY_BASED_PAIRING, (uint8 *)args->crypto_encrypt_cfm->encrypted_data);
            }
        }
        break;
        case fast_pair_state_event_power_off:
        {
            DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Moving to FAST_PAIR_STATE_IDLE ");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        default:
        {
            DEBUG_LOG("fastPair_StateWaitAdditionalDataHandleEvent: Unhandled event [%d]\n", event.id);
        }
        break;
    }
    return status;
}
