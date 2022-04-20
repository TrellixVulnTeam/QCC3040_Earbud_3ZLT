/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
\brief      Interface to procedure to enable/disable BREDR connectable (pagescan) for peer connectivity.
*/

#ifndef TWS_TOPOLOGY_PROC_ENABLE_CONNECTABLE_PEER_H
#define TWS_TOPOLOGY_PROC_ENABLE_CONNECTABLE_PEER_H

#include <bredr_scan_manager.h>
#include "tws_topology_procedures.h"

extern const procedure_fns_t proc_enable_connectable_peer_fns;

typedef struct
{
    bool enable;
    bool auto_disable;
    bredr_scan_manager_scan_type_t page_scan_type;
} ENABLE_CONNECTABLE_PEER_PARAMS_T; 

/*! Parameter definition for connectable enable */
extern const ENABLE_CONNECTABLE_PEER_PARAMS_T proc_enable_connectable_peer_enable_fast;
#define PROC_ENABLE_CONNECTABLE_PEER_DATA_ENABLE_FAST  ((Message)&proc_enable_connectable_peer_enable_fast)

/*! Parameter definition for connectable enable */
extern const ENABLE_CONNECTABLE_PEER_PARAMS_T proc_enable_connectable_peer_enable_no_auto_disable_slow;
#define PROC_ENABLE_CONNECTABLE_PEER_DATA_ENABLE_NO_AUTO_DISABLE_SLOW  ((Message)&proc_enable_connectable_peer_enable_no_auto_disable_slow)

/*! Parameter definition for connectable disable */
extern const ENABLE_CONNECTABLE_PEER_PARAMS_T proc_enable_connectable_peer_disable;
#define PROC_ENABLE_CONNECTABLE_PEER_DATA_DISABLE   ((Message)&proc_enable_connectable_peer_disable)

#endif /* TWS_TOPOLOGY_PROC_ENABLE_CONNECTABLE_PEER_H */
