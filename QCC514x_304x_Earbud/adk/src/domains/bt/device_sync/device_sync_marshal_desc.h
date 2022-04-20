/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_sync
\brief      Marshalling descriptors for device sync.
 
*/

#ifndef DEVICE_SYNC_MARSHAL_DESC_H_
#define DEVICE_SYNC_MARSHAL_DESC_H_

#include "device_sync.h"
#include <bdaddr.h>

#include <marshal_common.h>
#include <marshal.h>

/*@{*/

typedef struct
{
    /*! The device whose property changed */
    bdaddr addr;

    /* Id of client which should handle the data */
    uint8 client_id;

    /*! The property id */
    uint8 id;
} device_property_sync_cfm_t;

/* Create base list of marshal types the key sync will use. */
#define MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(device_property_sync_t) \
    ENTRY(device_property_sync_cfm_t)

/* X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum MARSHAL_TYPES
{
    /* common types must be placed at the start of the enum */
    DUMMY = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /* now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

/* Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const device_sync_marshal_type_descriptors[];

/*@}*/

#endif /* DEVICE_SYNC_MARSHAL_DESC_H_ */
