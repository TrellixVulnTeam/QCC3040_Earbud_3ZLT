/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The av c type definitions. This file is generated by C:/Qualcomm_Prog/qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _AV_TYPEDEF_H__
#define _AV_TYPEDEF_H__

#include <csrtypes.h>
#include <marshal_common.h>
#include <task_list.h>

#include "avrcp_typedef.h"
#include "a2dp_typedef.h"
#include "bandwidth_manager.h"
#include "av_callback_interface.h"

/*! AV Instance task data structure */
typedef struct 
{
    /*! Task/Message information for this instance */
    TaskData av_task;
    /*! Bluetooth Address of remote device */
    bdaddr bd_addr;
    /*! Delay timer for establishing AVRCP connection post handover */
    uint8 connect_avrcp_post_handover;
    /*! Delay timer for sending internal AVRCP pause for unrouted source post handover */
    uint8 send_avrcp_unrouted_pause_post_handover;
    /*! AVRCP task data */
    avrcpTaskData avrcp;
    /*! A2DP task data */
    a2dpTaskData a2dp;
    /*! The AV volume */
    uint8 volume;
    /*! Is a rejected connection expected */
    uint8 avrcp_reject_pending;
    /*! Flag indicating if the instance is about to be detatched */
    uint8 detach_pending;
    /*! A pointer to the plugin interface */
    const av_callback_interface_t *av_callbacks;
} avInstanceTaskData;

/*! AV task bitfields data structure */
typedef struct 
{
    /*! Current state of AV state machine */
    unsigned state:2;
    /*! Volume repeat */
    unsigned volume_repeat:1;
} avTaskDataBitfields;

/*! AV Task data structure */
typedef struct 
{
    /*! Task for messages */
    TaskData task;
    /*! AV Task data bitfields */
    avTaskDataBitfields bitfields;
    /*! Bitmap of active suspend reasons */
    avSuspendReason suspend_state;
    /*! Enable play on connect if connect to this device */
    bool play_on_connect;
    /*! List of tasks registered via \ref appAvAvrcpClientRegister */
    task_list_t avrcp_client_list;
    /*! List of tasks registered via \ref appAvStatusClientRegister. These receive indications for AVRCP, A2DP and A2DP streaming */
    task_list_t av_status_client_list;
    /*! List of tasks registered for UI events */
    task_list_t av_ui_client_list;
    /*! A2DP connect request clients */
    task_list_with_data_t a2dp_connect_request_clients;
    /*! A2DP disconnect request clients */
    task_list_with_data_t a2dp_disconnect_request_clients;
    /*! AVRCP connect request clients */
    task_list_with_data_t avrcp_connect_request_clients;
    /*! AVRCP disconnect request clients */
    task_list_with_data_t avrcp_disconnect_request_clients;
} avTaskData;

#endif /* _AV_TYPEDEF_H__ */
