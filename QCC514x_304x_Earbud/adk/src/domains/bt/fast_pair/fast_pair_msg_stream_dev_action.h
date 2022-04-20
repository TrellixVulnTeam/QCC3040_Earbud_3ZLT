/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_action.h
\brief      Header file for fast pair device action.
*/

#ifndef FASTPAIR_MSG_STREAM_DEV_ACTION_H
#define FASTPAIR_MSG_STREAM_DEV_ACTION_H

#include <marshal_common.h>
#include <marshal.h>
#include <message.h>
#include <task_list.h>

#include "kymera.h"

#define FP_STOP_RING_CURRENT    0
#define FP_STOP_RING_BOTH       1

#define FASTPAIR_DEVICEACTION_RING_RSP_ADD_DATA_LEN 1
#define FASTPAIR_DEVICEACTION_STOP_RING 0
#define FASTPAIR_DEVICEACTION_RING_RIGHT_MUTE_LEFT 1
#define FASTPAIR_DEVICEACTION_RING_LEFT_MUTE_RIGHT 2
#define FASTPAIR_DEVICEACTION_RING_RIGHT_LEFT 3

/* Message Code for Device action message group */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT = 0x01
} FASTPAIR_MESSAGESTREAM_DEVACTION_MESSAGE_CODE;

/* Device Action data structure*/
typedef struct{
    uint8 ring_component;
    uint8 ring_timeout;
}fast_pair_msg_stream_dev_action;

/* Ringtone volume level to ramp up volume for ringing device */
typedef enum
{
  ring_vol32,
  ring_vol64,
  ring_vol128,
  ring_volmax
} ringtone_volume;

typedef struct
{
    TaskData task;
    /* Indicates if device is currently ringing or not. */
    bool is_device_ring;
    /* Volume level indicator for the current ringtone being played. */
    ringtone_volume vol_level;
    /* Play a ringtone with a volume level for certain no of time
       before it is played to a max level. */
    uint16 ringtimes;
} fp_ring_device_task_data_t;

typedef enum
{
    fast_pair_ring_event = 0,
    fast_pair_ring_stop_event
} fast_pair_ring_device_event_id;

extern fp_ring_device_task_data_t ring_device;
extern fast_pair_msg_stream_dev_action dev_action_data;

#define fpRingDevice_GetTaskData() (&ring_device)
#define fpRingDevice_GetTask() (&ring_device.task)

typedef struct fast_pair_ring_device_req
{
    bool  ring_start_stop;
    uint8 ring_time;
} fast_pair_ring_device_req_t;

typedef struct fast_pair_ring_Device_cfm
{
    bool synced;
} fast_pair_ring_device_cfm_t;

/*! Create base list of marshal types ring device will use. */
#define MARSHAL_TYPES_TABLE_RING_DEVICE(ENTRY) \
    ENTRY(fast_pair_ring_device_req_t) \
    ENTRY(fast_pair_ring_device_cfm_t)

/*! X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),

enum MARSHAL_TYPES_RING_DEVICE_SYNC
{
    /*! common types must be placed at the start of the enum */
    DUMMY_DEVICE_ACTION_SYNC = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /*! now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE_RING_DEVICE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_DEVICE_ACTION_SYNC_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

/*! Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const fp_ring_device_marshal_type_descriptors[];

void fastPair_MsgStreamDevAction_Init(void);

#endif /* FASTPAIR_MSG_STREAM_DEV_ACTION_H */
