/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      List of PDDUs
 
Single list to avoid accidental duplicates.
 
*/

#ifndef PDDU_MAP_H_
#define PDDU_MAP_H_


/*@{*/

typedef enum
{
    PDDU_ID_BT_DEVICE = 0,
    PDDU_ID_FAST_PAIR,
    PDDU_ID_DEVICE_PSKEY,
    PDDU_ID_PACS,
    PDDU_ID_LEA_BROADCAST_MANAGER,
    PDDU_ID_LEA_UNICAST_MANAGER,
    PDDU_ID_CSIP_SET_MEMBER,
    PDDU_ID_UI_USER_CONFIG
} pddu_id_map_t;

/*@}*/

#endif /* PDDU_MAP_H_ */
