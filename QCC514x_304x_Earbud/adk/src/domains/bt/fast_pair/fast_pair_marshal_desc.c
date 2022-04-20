/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    fast_pair_marshal_desc.h

DESCRIPTION
    Creates tables of marshal type descriptors for Fast Pair data types

*/

#include "fast_pair_marshal_desc.h"

static const marshal_type_descriptor_t mtd_fast_pair_msg_stream_dev_info =
        MAKE_MARSHAL_TYPE_DEFINITION_BASIC(fast_pair_msg_stream_dev_info);

static const marshal_member_descriptor_t mmd_fast_pair_data[] =
{
    MAKE_MARSHAL_MEMBER(fast_pair_marshal_data, uint8, rfcomm_channel),
    MAKE_MARSHAL_MEMBER(fast_pair_marshal_data, fast_pair_msg_stream_dev_info, dev_info)
};

static const marshal_type_descriptor_t mtd_fast_pair_marshal_data =
        MAKE_MARSHAL_TYPE_DEFINITION(fast_pair_marshal_data, mmd_fast_pair_data);


/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &mtd_##type,
const marshal_type_descriptor_t * const  mtd_fast_pair[] =
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    FAST_PAIR_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION
