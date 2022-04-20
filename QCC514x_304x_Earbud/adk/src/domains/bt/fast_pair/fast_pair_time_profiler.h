/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_time_profiler.h
\brief      Header file for the Fast Pair Time Profiler
*/

#ifndef FAST_PAIR_TIME_PROFILER_H_
#define FAST_PAIR_TIME_PROFILER_H_

#ifdef FAST_PAIR_TIME_PROFILER

#include <task_list.h>
#include <logging.h>
#include <panic.h>
#include <bt_device.h>

#include "fast_pair.h"


typedef enum
{
  /* IDLE */
  fast_pair_event_ble_connect,                   /* Phone is BLE Connected with the Device */
  fast_pair_event_kbp_write,                     /* BLE SDP is completed, first key base pairing (kbp) is written */
  /* WAIT_AES_KEY */
  fast_pair_event_ecdh_key_gen,                  /* ECDH shared secret key generation */
  fast_pair_event_aes_key_gen,                   /* AES key generation */
  fast_pair_event_decrypt_kbp,                   /* Decrypting KbP payload */
  /* WAIT_PAIRING_REQUEST */
  fast_pair_event_kbp_response =5,               /* Encrypt kbp response */
  fast_pair_event_pairing_request,               /* BR\EDR pairing is requested */
  /* WAIT_PASSKEY */
  fast_pair_event_passkey_write,                 /* Passkey written over BLE */
  fast_pair_event_decrypt_passkey,               /* Decrypting Passkey */
  fast_pair_event_provider_passkey,              /* Provider passkey received from CL */
  fast_pair_event_encrypt_provider_passkey =10,  /* Encrypting provider passkey */
  /* WAIT_ACCOUNT_KEY */
  fast_pair_event_auth_received,                 /* Authenticate cfm received */
  fast_pair_event_hfp_conn_ind,                  /* Hfp connect indication received (APP_HFP_CONNECTED_IND) */
  fast_pair_event_a2dp_conn_ind,                 /* A2dp connect indication received (AV_A2DP_CONNECTED_IND) */
  fast_pair_event_account_key_write,             /* Fast Account Key is Written, FP completed successfully */
  fast_pair_event_account_key_decrypt = 15,      /* Decrypting account key */
  /* WAIT_ADDITIONAL_DATA */
  fast_pair_event_additional_data_write,         /* Additional data write occured */
  /* WAIT_PNAME_WRITE */
  fast_pair_event_pname_write,                   /* Personalized name is written */
  /* IDLE_END */
  fast_pair_event_ble_disconnect,                /* BLE got disconnected */
  fast_pair_event_bloom_filter_gen,              /* Generating bloom filter */
  fast_pair_event_timer_expired,                 /* Fast Pair timer expired */

  fast_pair_event_last
} fast_pair_profile_id;

extern uint32 fast_pair_event_time[fast_pair_event_last];


void fastpair_TimeProfiler(fast_pair_state_event_id event_id);
#endif /* FAST_PAIR_TIME_PROFILER */

#endif /* FAST_PAIR_TIME_PROFILER_H_ */
