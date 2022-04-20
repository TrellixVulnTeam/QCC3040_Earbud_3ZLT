 /*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to enable LE connections to the handset 
            
*/
#ifndef HEADSET_TOPOLOGY_PROC_ALLOW_LE_CONNECTIONS_H
#define HEADSET_TOPOLOGY_PROC_ALLOW_LE_CONNECTIONS_H

#include "headset_topology_procedures.h"

extern const procedure_fns_t hs_proc_allow_le_connection_fns;

/*! Structure definining the parameters for the procedure to enable/disable
    LE connectability to handset.
*/
typedef struct
{
    bool enable;
} ALLOW_LE_CONNECTION_PARAMS_T;

/*! Parameter definition for connectable enable */
extern const ALLOW_LE_CONNECTION_PARAMS_T hs_topology_procedure_allow_le_connection_enable;
#define PROC_ALLOW_LE_CONNECTION_DATA_ENABLE  ((Message)&hs_topology_procedure_allow_le_connection_enable)

/*! Parameter definition for connectable disable */
extern const ALLOW_LE_CONNECTION_PARAMS_T hs_topology_procedure_allow_le_connection_disable;
#define PROC_ALLOW_LE_CONNECTION_DATA_DISABLE  ((Message)&hs_topology_procedure_allow_le_connection_disable)



#endif /* HEADSET_TOPOLOGY_PROC_ALLOW_LE_CONNECTIONS_H */
