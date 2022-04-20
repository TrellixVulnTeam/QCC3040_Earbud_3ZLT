/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The tws_topology marshal type declarations and handover header file.

@{
*/
#ifndef TWS_TOPOLOGY_HANDOVER_H
#define TWS_TOPOLOGY_HANDOVER_H

#ifdef INCLUDE_MIRRORING
#include <csrtypes.h>
#include <marshal_common.h>
#include <app/marshal/marshal_if.h>

extern const marshal_type_descriptor_t marshal_type_descriptor_tws_topology_marshal_data_t;

#define TWS_TOPOLOGY_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(tws_topology_marshal_data_t)

#endif /* INCLUDE_MIRRORING */
#endif // TWS_TOPOLOGY_HANDOVER_H
