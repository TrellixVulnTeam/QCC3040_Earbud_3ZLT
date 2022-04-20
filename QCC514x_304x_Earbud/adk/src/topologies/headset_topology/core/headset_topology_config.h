/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Configuration / parameters used by the topology.
*/

#ifndef BREDR_SCAN_MANAGER_H
#define BREDR_SCAN_MANAGER_H

#include "bredr_scan_manager.h"

/*! Inquiry scan parameter set */
extern const bredr_scan_manager_parameters_t hs_inquiry_scan_params;

/*! Page scan parameter set */
extern const bredr_scan_manager_parameters_t hs_page_scan_params;

/*! Timeout for a Headset Topology Stop command to complete (in seconds).

    \note This should be set such that in a normal case all activities will
        have completed.
 */
#define HeadsetTopologyConfig_HeadsetTopologyStopTimeoutS()         (10)

#endif  //BREDR_SCAN_MANAGER_H

