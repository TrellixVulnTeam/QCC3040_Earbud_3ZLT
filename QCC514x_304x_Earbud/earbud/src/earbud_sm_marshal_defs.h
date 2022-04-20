/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_sm_marshal_defs.h
\brief      Definition of messages that can be sent between Earbud application entities.
*/

#ifndef EARBUD_SM_MARSHAL_DEFS_H
#define EARBUD_SM_MARSHAL_DEFS_H

#include "earbud_sm_private.h"

/* framework includes */
#include <marshal_common.h>

/* system includes */
#include <marshal.h>

typedef struct earbud_sm_msg_empty_payload
{
    uint8 dummy;
} earbud_sm_msg_empty_payload_t;

typedef struct earbud_sm_msg_bdaddr_payload
{
    bdaddr bd_addr;
} earbud_sm_msg_bdaddr_payload_t;

typedef struct 
{
    bool enable;
    marshal_rtime_t mute_timestamp;
}earbud_sm_msg_gaming_mode_t;

typedef struct 
{
    uint32 id_in_progress;
}earbud_sm_req_dfu_started_t;

typedef earbud_sm_msg_empty_payload_t earbud_sm_req_dfu_active_when_in_case_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_req_factory_reset_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_ind_dfu_start_timeout_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_req_stereo_audio_mix_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_req_mono_audio_mix_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_req_delete_handsets_t;
typedef earbud_sm_msg_bdaddr_payload_t earbud_sm_req_delete_handset_if_full_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_ind_gone_in_case_while_pairing_t;
typedef earbud_sm_msg_empty_payload_t earbud_sm_ind_doffed_t;

/* Create base list of marshal types the Earbud SM will use. */
#define MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(earbud_sm_req_dfu_active_when_in_case_t) \
    ENTRY(earbud_sm_req_factory_reset_t) \
    ENTRY(earbud_sm_ind_dfu_start_timeout_t) \
    ENTRY(earbud_sm_req_stereo_audio_mix_t) \
    ENTRY(earbud_sm_req_mono_audio_mix_t) \
    ENTRY(earbud_sm_req_delete_handsets_t) \
    ENTRY(earbud_sm_req_dfu_started_t) \
    ENTRY(earbud_sm_req_delete_handset_if_full_t) \
    ENTRY(earbud_sm_msg_gaming_mode_t) \
    ENTRY(earbud_sm_ind_gone_in_case_while_pairing_t) \
    ENTRY(earbud_sm_ind_doffed_t)

/* X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
typedef enum
{
    /* now expand the marshal types specific to this component. */
    DUMMY = (NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1),
    MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_SM_MARSHAL_OBJECT_TYPES
} earbud_sm_marshal_types_t;
#undef EXPAND_AS_ENUMERATION

/* Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const earbud_sm_marshal_type_descriptors[];

#endif /* EARBUD_SM_MARSHAL_DEFS_H */
