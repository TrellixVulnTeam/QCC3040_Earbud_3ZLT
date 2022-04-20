/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the feature manager priority list for the Earbud application.
*/

#include "earbud_feature_manager_priority_list.h"

static const feature_id_t feature_ids_in_priority_order[] = { feature_id_sco, feature_id_va };
#define SIZE_OF_ID_LIST ARRAY_DIM(feature_ids_in_priority_order)

static const feature_manager_priority_list_t feature_manager_priority_list = { feature_ids_in_priority_order, SIZE_OF_ID_LIST };

const feature_manager_priority_list_t * Earbud_GetFeatureManagerPriorityList(void)
{
    return &feature_manager_priority_list;
}
