/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_sync
\brief      Marshalling descriptors for device sync.

*/

#include "device_sync_marshal_desc.h"

/* device_property_sync_t */

static uint32 device_property_sync_data_size(const void *object, const marshal_member_descriptor_t *member, uint32 array_element)
{
    const device_property_sync_t *sync = object;
    UNUSED(member);
    UNUSED(array_element);
    return sync->size;
}

const marshal_member_descriptor_t device_property_sync_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(device_property_sync_t, bdaddr, addr),
    MAKE_MARSHAL_MEMBER(device_property_sync_t, uint8, client_id),
    MAKE_MARSHAL_MEMBER(device_property_sync_t, uint8, id),
    MAKE_MARSHAL_MEMBER(device_property_sync_t, uint8, size),
    MAKE_MARSHAL_MEMBER_ARRAY(device_property_sync_t, uint8, data, 1),
};

const marshal_type_descriptor_dynamic_t marshal_type_descriptor_device_property_sync_t =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_DYNAMIC_ARRAY(device_property_sync_t,
            device_property_sync_member_descriptors, device_property_sync_data_size);


/* device_property_sync_cfm_t */

const marshal_member_descriptor_t device_property_sync_cfm_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER(device_property_sync_cfm_t, bdaddr, addr),
    MAKE_MARSHAL_MEMBER(device_property_sync_cfm_t, uint8, client_id),
    MAKE_MARSHAL_MEMBER(device_property_sync_cfm_t, uint8, id),
};

const marshal_type_descriptor_t marshal_type_descriptor_device_property_sync_cfm_t =
    MAKE_MARSHAL_TYPE_DEFINITION(device_property_sync_cfm_t, device_property_sync_cfm_member_descriptors);

/*! X-Macro generate key sync marshal type descriptor set that can be passed to a (un)marshaller
 *  to initialise it.
 *  */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const device_sync_marshal_type_descriptors[NUMBER_OF_MARSHAL_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

