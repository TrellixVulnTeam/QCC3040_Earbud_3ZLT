/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_sm_marshal_defs.c
\brief      Marshal type definitions for Earbud application.
*/

/* component includes */
#include "earbud_sm_marshal_defs.h"

/* framework includes */
#include <marshal_common.h>

/* system includes */
#include <marshal.h>

/*! earbud_sm_empty payload message marshalling type descriptor. */
const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_dfu_active_when_in_case_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_dfu_active_when_in_case_t);

static const marshal_member_descriptor_t earbud_sm_req_dfu_started_t_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(earbud_sm_req_dfu_started_t, uint32, id_in_progress),
} ;

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_dfu_started_t =
    MAKE_MARSHAL_TYPE_DEFINITION(earbud_sm_req_dfu_started_t, earbud_sm_req_dfu_started_t_member_descriptors);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_factory_reset_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_factory_reset_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_ind_dfu_start_timeout_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_ind_dfu_start_timeout_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_stereo_audio_mix_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_stereo_audio_mix_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_mono_audio_mix_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_mono_audio_mix_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_delete_handsets_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_delete_handsets_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_req_delete_handset_if_full_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_req_delete_handset_if_full_t);

static const marshal_member_descriptor_t earbud_sm_msg_gaming_mode_t_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(earbud_sm_msg_gaming_mode_t, uint8, enable),
    MAKE_MARSHAL_MEMBER(earbud_sm_msg_gaming_mode_t, marshal_rtime_t, mute_timestamp),
} ;

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_msg_gaming_mode_t =
    MAKE_MARSHAL_TYPE_DEFINITION(earbud_sm_msg_gaming_mode_t, earbud_sm_msg_gaming_mode_t_member_descriptors);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_ind_gone_in_case_while_pairing_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_ind_gone_in_case_while_pairing_t);

const marshal_type_descriptor_t marshal_type_descriptor_earbud_sm_ind_doffed_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(earbud_sm_ind_doffed_t);

/*! X-Macro generate earbud SM marshal type descriptor set that can be passed to a (un)marshaller
 *  to initialise it.
 *  */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const earbud_sm_marshal_type_descriptors[] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
