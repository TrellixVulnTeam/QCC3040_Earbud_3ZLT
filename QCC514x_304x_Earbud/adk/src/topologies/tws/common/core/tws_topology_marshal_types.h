/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Handover types used by TWS topology.
*/
#ifndef TWS_TOPOLOGY_MARSHAL_TYPES_H
#define TWS_TOPOLOGY_MARSHAL_TYPES_H

#include "marshal_common.h"
#include "service_marshal_types.h"
#include "tws_topology_handover.h"
#include <hydra_macros.h>

/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    LAST_SERVICE_MARSHAL_TYPE = NUMBER_OF_SERVICE_MARSHAL_OBJECT_TYPES-1, /* Subtracting 1 to keep the marshal types contiguous */
    TWS_TOPOLOGY_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_TWS_TOPOLOGY_MARSHAL_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

#endif /* TWS_TOPOLOGY_MARSHAL_TYPES_H */
