/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_pname_sync.h
\brief      Header file of Fast Pair personalized name sync component
*/

#ifndef FAST_PAIR_PNAME_SYNC_H
#define FAST_PAIR_PNAME_SYNC_H

#include <marshal_common.h>
#include <marshal.h>
#include <message.h>
#include <task_list.h>
#include "fast_pair.h"


/*! Personalized Name sync task data. */
typedef struct
{
    TaskData task;
} fp_pname_sync_task_data_t;

/*! Component level visibility of pname Sync Task Data */
extern fp_pname_sync_task_data_t pname_sync;

#define fpPNameSync_GetTaskData() (&pname_sync)
#define fpPNameSync_GetTask() (&pname_sync.task)

typedef struct fast_pair_pname_sync_req
{
    uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN];
} fast_pair_pname_sync_req_t;

typedef struct fast_pair_pname_sync_cfm
{
    bool synced;
} fast_pair_pname_sync_cfm_t;

/*! Create base list of marshal types the Personalized Name sync will use. */
#define MARSHAL_TYPES_TABLE_PNAME_SYNC(ENTRY) \
    ENTRY(fast_pair_pname_sync_req_t) \
    ENTRY(fast_pair_pname_sync_cfm_t)

/*! X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),

enum MARSHAL_TYPES_PNAME_SYNC
{
    /*! common types must be placed at the start of the enum */
    DUMMY_PNAME_SYNC = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /*! now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE_PNAME_SYNC(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_PNAME_SYNC_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

/*! Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const fp_pname_sync_marshal_type_descriptors[];

/*! \brief Fast Pair Personalized Name Sync Initialization
    This is used to initialize fast pair Personalized Name sync interface.
 */
void fastPair_PNameSync_Init(void);

#endif /*! FAST_PAIR_PNAME_SYNC_H */
