/*!
    \copyright Copyright (c) 2021 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file
    \brief The aghfp_profile c type definitions.
*/

#ifndef AGHFP_PROFILE_TYPEDEF_H
#define AGHFP_PROFILE_TYPEDEF_H

#include "aghfp.h"
#include "source_param_types.h"
#include "aghfp_profile_call_list.h"

#define AGHFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY 1
#define AGHFP_STATUS_NOTIFY_LIST_INIT_CAPACITY 6

/* WARNING: Correct operation of the state machine  is dependent
   on the ordering of the states. Do not change. */
typedef enum
{
    /*! Initial state */
    AGHFP_STATE_NULL,
    /*! No AGHFP connection */
    AGHFP_STATE_DISCONNECTED,
    /*! Locally initiated connection in progress */
    AGHFP_STATE_CONNECTING_LOCAL,
    /*! Remotely initiated connection is progress */
    AGHFP_STATE_CONNECTING_REMOTE,
    /*! AGHFP connected, no call in progress */
    AGHFP_STATE_CONNECTED_IDLE,
    /*! AGHFP connected, outgoing call in progress */
    AGHFP_STATE_CONNECTED_OUTGOING,
    /*! AGHFP connected, incoming call in progress */
    AGHFP_STATE_CONNECTED_INCOMING,
    /*! AGHFP connected, active call in progress */
    AGHFP_STATE_CONNECTED_ACTIVE,
    /*! AGHFP disconnecting in progress */
    AGHFP_STATE_DISCONNECTING
} aghfpState;

typedef struct
{
  uint8 clip_type;
  uint8 size_clip_number;
  uint8 *clip_number;
} clip_data;

typedef struct {
    AGHFP *aghfp_lib_instance;
    uint8 num_of_instances;
} aghfpTaskData;

typedef struct {
    uint8* number;
    uint16 number_len;
} dialed_number;

typedef struct
{
    /*! AG supports in-band ringing tone */
    unsigned in_band_ring:1;
    /*! Caller ID is active on the remote*/
    unsigned caller_id_active_remote:1;
    /*! Caller ID is active on the host */
    unsigned caller_id_active_host:1;
    /*! Current call setup state*/
    aghfp_call_setup_status call_setup:2;
    /*! Current call status*/
    aghfp_call_status call_status:1;
    /*! Current call hold status */
    aghfp_call_held_status call_hold:2;
    /*! Flag indicating if we have accepted the call */
    unsigned call_accepted:1;
} agHfpTaskDataBitfields;

typedef struct
{
    /* AGHFP task */
    TaskData task;
    /* aghfp lib instance */
    AGHFP *aghfp;
    /* AGHFP state */
    aghfpState state;
    /* HF address */
    bdaddr hf_bd_addr;
    /* Type of packets supported between the AG and HF*/
    uint16 sco_supported_packets;
    /* AGHFP bitfields */
    agHfpTaskDataBitfields bitfields;
    /* Operation lock */
    uint16 aghfp_lock;
    /* Sink for the (e)SCO audio connection */
    Sink sco_sink;
    /* Sink for the service level connection */
    Sink slc_sink;
    /* State of HFP as voice source */
    source_state_t source_state;
    /* Audio connection is wideband (16 kHz) */
    bool using_wbs;
    /* Audio connection wideband (16 kHz) codec */
    aghfp_wbs_codec codec;
    /*  Number of slots in the retransmission window */
    uint8 wesco;
    /* eSCO interval in slots */
    uint8 tesco;
    /* Qualcomm Codec Mode ID Selected - If in Qualcomm Codec Extensions mode. */
    uint16 qce_codec_mode_id;
    /* Phone number to report to the HF */
    clip_data clip;
    /* Network operator name. NULL terminated */
    uint8 *network_operator;
    /* List of active/held calls */
    call_list_t *call_list;
} aghfpInstanceTaskData;

#endif // AGHFP_PROFILE_TYPEDEF_H
