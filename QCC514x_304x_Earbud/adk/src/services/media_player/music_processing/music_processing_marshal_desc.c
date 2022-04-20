/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Marshalling definitions.

*/

#include "music_processing_marshal_desc.h"

static uint32 music_processing_eq_info_size(const void *object, const marshal_member_descriptor_t *member, uint32 array_element)
{
    const music_processing_eq_info_t *info = object;
    UNUSED(member);
    UNUSED(array_element);
    return info->payload_length;
}

const marshal_member_descriptor_t music_processing_eq_info_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(music_processing_eq_info_t, marshal_rtime_t, timestamp),
    MAKE_MARSHAL_MEMBER(music_processing_eq_info_t, uint8, eq_change_type),
    MAKE_MARSHAL_MEMBER(music_processing_eq_info_t, uint8, payload_length),
    MAKE_MARSHAL_MEMBER_ARRAY(music_processing_eq_info_t, uint8, payload, 1),
};

const marshal_type_descriptor_dynamic_t marshal_type_descriptor_music_processing_eq_info_t =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_DYNAMIC_ARRAY(music_processing_eq_info_t,
            music_processing_eq_info_member_descriptors, music_processing_eq_info_size);

/*! X-Macro generate key sync marshal type descriptor set that can be passed to a (un)marshaller
 *  to initialise it.
 *  */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const music_processessing_marshal_type_descriptors[NUMBER_OF_MARSHAL_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION
