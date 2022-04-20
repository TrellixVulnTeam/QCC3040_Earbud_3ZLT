/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_time_profiler.c
\brief      Source file for Fast Pair Time Profiler
*/

#ifdef FAST_PAIR_TIME_PROFILER
#include "fast_pair_time_profiler.h"

/* Array of snapped fast pair event times */
uint32 fast_pair_event_time[fast_pair_event_last];

/*! @brief Snap Fast Pair events which are handled in different fast pair state machines.
    Calculate Fast Pair completion time and log the snapped Fast Pair event times.
 */
void fastpair_TimeProfiler(fast_pair_state_event_id event_id)
{
    fastPairTaskData *theFastPair;
    uint16 id = 0xFFFF;

    theFastPair = fastPair_GetTaskData();

    switch(fastPair_GetState(theFastPair))
    {
        case FAST_PAIR_STATE_NULL:
        break;

        case FAST_PAIR_STATE_IDLE:
        {
            switch (event_id)
            {
                case fast_pair_state_event_connect:
                    id = fast_pair_event_ble_connect;
                    if (fast_pair_event_time[0] == 0)
                    {
                        memset(&fast_pair_event_time[0], 0, sizeof(fast_pair_event_time));
                    }
                break;

                case fast_pair_state_event_kbp_write:
                    id = fast_pair_event_kbp_write;
                break;

                case fast_pair_state_event_disconnect:
                    id = fast_pair_event_ble_disconnect;
                break;

                case fast_pair_state_event_crypto_hash:
                    id =fast_pair_event_bloom_filter_gen;
                break;

                default:
                break;
            }
        }
        break;

        case FAST_PAIR_STATE_WAIT_AES_KEY:
        {
            switch (event_id)
            {
                case fast_pair_state_event_crypto_shared_secret:
                    id = fast_pair_event_ecdh_key_gen;
                break;

                case fast_pair_state_event_crypto_hash:
                    id = fast_pair_event_aes_key_gen;
                break;

                case fast_pair_state_event_crypto_decrypt:
                    id = fast_pair_event_decrypt_kbp;
                break;

                default:
                break;
            }
        }
        break;

        case FAST_PAIR_STATE_WAIT_PAIRING_REQUEST:
        {
            switch (event_id)
            {
                case fast_pair_state_event_crypto_encrypt:
                    id = fast_pair_event_kbp_response;
                break;

                case fast_pair_state_event_pairing_request:
                    id = fast_pair_event_pairing_request;
                break;

                default:
                break;

            }
        }
        break;

        case FAST_PAIR_STATE_WAIT_PASSKEY:
        {
            switch (event_id)
            {
                case fast_pair_state_event_passkey_write:
                    id = fast_pair_event_passkey_write;
                break;

                case fast_pair_state_event_crypto_decrypt:
                    id = fast_pair_event_decrypt_passkey;
                break;

                case fast_pair_state_event_provider_passkey:
                    id = fast_pair_event_provider_passkey;
                break;

                case fast_pair_state_event_crypto_encrypt:
                    id = fast_pair_event_encrypt_provider_passkey;
                break;

                default:
                break;
            }
        }
        break;

        case FAST_PAIR_STATE_WAIT_ACCOUNT_KEY:
        {
            switch (event_id)
            {
                case fast_pair_state_event_auth:
                    id = fast_pair_event_auth_received;
                break;

                case fast_pair_state_event_account_key_write:
                    id = fast_pair_event_account_key_write;
                break;

                case fast_pair_state_event_crypto_decrypt:
                     id = fast_pair_event_account_key_decrypt;
                break;

                default:
                break;
            }
        }
        break;

        case FAST_PAIR_STATE_WAIT_ADDITIONAL_DATA:
        {
            switch (event_id)
            {
                case fast_pair_state_event_additional_data_write:
                    id = fast_pair_event_additional_data_write;
                break;

                default:
                break;
            }
        }
        break;

        case FAST_PAIR_STATE_PNAME:
        {
            switch (event_id)
            {
                case fast_pair_state_event_pname_write:
                   id = fast_pair_event_pname_write;
                break;

                default:
                break;
            }
        }
        break;

        default:
        break;
    }

    if(event_id == fast_pair_state_event_timer_expire)
    {
        id = fast_pair_event_timer_expired;
    }

    if((id == 0xFFFF) || (id > fast_pair_event_last))
    {
        return;
    }

    /* Snap the fast pair event time */
    fast_pair_event_time[id] = VmGetClock();

    /* After the device is paired with the handset using fast pair, log the fast pair event times */
    if( ((id == fast_pair_event_ble_disconnect) ||
        (id == fast_pair_event_timer_expired)) && BtDevice_IsPairedWithHandset())
    {
        DEBUG_LOG("FP TIME : %d", fast_pair_event_time[fast_pair_event_ble_disconnect] - fast_pair_event_time[fast_pair_event_ble_connect]);

        DEBUG_LOG("FAST PAIR EVENT TIMES");
        DEBUG_LOG("fast_pair_event_ble_connect: %d",fast_pair_event_time[0]);
        DEBUG_LOG("fast_pair_event_kbp_write: %d",fast_pair_event_time[1]);
        DEBUG_LOG("fast_pair_event_ecdh_key_gen: %d",fast_pair_event_time[2]);
        DEBUG_LOG("fast_pair_event_aes_key_gen: %d",fast_pair_event_time[3]);
        DEBUG_LOG("fast_pair_event_decrypt_kbp: %d",fast_pair_event_time[4]);
        DEBUG_LOG("fast_pair_event_kbp_response: %d",fast_pair_event_time[5]);
        DEBUG_LOG("fast_pair_event_pairing_request: %d",fast_pair_event_time[6]);
        DEBUG_LOG("fast_pair_event_passkey_write: %d",fast_pair_event_time[7]);
        DEBUG_LOG("fast_pair_event_decrypt_passkey: %d",fast_pair_event_time[8]);
        DEBUG_LOG("fast_pair_event_provider_passkey: %d",fast_pair_event_time[9]);
        DEBUG_LOG("fast_pair_event_encrypt_provider_passkey: %d",fast_pair_event_time[10]);
        DEBUG_LOG("fast_pair_event_auth_received: %d",fast_pair_event_time[11]);
        DEBUG_LOG("fast_pair_event_hfp_conn_ind: %d",fast_pair_event_time[12]);
        DEBUG_LOG("fast_pair_event_a2dp_conn_ind: %d",fast_pair_event_time[13]);
        DEBUG_LOG("fast_pair_event_account_key_write: %d",fast_pair_event_time[14]);
        DEBUG_LOG("fast_pair_event_account_key_decrypt: %d",fast_pair_event_time[15]);
        DEBUG_LOG("fast_pair_event_additional_data_write: %d",fast_pair_event_time[16]);
        DEBUG_LOG("fast_pair_event_pname_write: %d",fast_pair_event_time[17]);
        DEBUG_LOG("fast_pair_event_ble_disconnect: %d",fast_pair_event_time[18]);
        DEBUG_LOG("fast_pair_event_bloom_filter_gen: %d",fast_pair_event_time[19]);
        DEBUG_LOG("fast_pair_event_timer_expired: %d",fast_pair_event_time[20]);

        /* Break down of Fast Pair event times into 8 levels. */

        DEBUG_LOG("FP LEVEL L1 (from ble connect to kbp write) : %d ms", fast_pair_event_time[fast_pair_event_kbp_write] - fast_pair_event_time[fast_pair_event_ble_connect]);
        DEBUG_LOG("FP LEVEL L2 (from kbp write to kbp response) : %d ms", fast_pair_event_time[fast_pair_event_kbp_response] - fast_pair_event_time[fast_pair_event_kbp_write]);
        DEBUG_LOG("FP LEVEL L3 (from kbp response to pairing request): %d ms", fast_pair_event_time[fast_pair_event_pairing_request] - fast_pair_event_time[fast_pair_event_kbp_response]);
        DEBUG_LOG("FP LEVEL L4 (from pairing request to auth cfm received): %d ms", fast_pair_event_time[fast_pair_event_auth_received] - fast_pair_event_time[fast_pair_event_pairing_request]);
        DEBUG_LOG("FP LEVEL L5 (from auth cfm received to hfp connect ind): %d ms", fast_pair_event_time[fast_pair_event_hfp_conn_ind] - fast_pair_event_time[fast_pair_event_auth_received]);
        DEBUG_LOG("FP LEVEL L6 (from hfp conn ind to a2dp connect ind): %d ms", fast_pair_event_time[fast_pair_event_a2dp_conn_ind] - fast_pair_event_time[fast_pair_event_hfp_conn_ind]);
        DEBUG_LOG("FP LEVEL L7 (from a2dp conn ind to account key write): %d ms", fast_pair_event_time[fast_pair_event_account_key_decrypt] - fast_pair_event_time[fast_pair_event_a2dp_conn_ind]);
        DEBUG_LOG("FP LEVEL L8 (from account key write to ble disconnect): %d ms", fast_pair_event_time[fast_pair_event_ble_disconnect] - fast_pair_event_time[fast_pair_event_account_key_decrypt]);
    }
}
#endif /* FAST_PAIR_TIME_PROFILER */