/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    fast_pair_marshal_desc.h

DESCRIPTION
    Creates tables of marshal type descriptors for Fast Pair data types

*/

#ifndef FAST_PAIR_MARSHAL_DESC_H
#define FAST_PAIR_MARSHAL_DESC_H

#include "marshal_common_desc.h"
#include "types.h"
#include "app/marshal/marshal_if.h"
#include "fast_pair_msg_stream_dev_info.h"

/* Data struture to be marshalled/unmarshalled by fast pair */
typedef struct{
    uint8 rfcomm_channel;
    fast_pair_msg_stream_dev_info dev_info;
}fast_pair_marshal_data;


#define FAST_PAIR_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(fast_pair_marshal_data) \
    ENTRY(fast_pair_msg_stream_dev_info)


/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    FAST_PAIR_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    FAST_PAIR_MARSHAL_OBJ_TYPE_COUNT
};
#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const  mtd_fast_pair[];
#endif // FAST_PAIR_MARSHAL_DESC_H
