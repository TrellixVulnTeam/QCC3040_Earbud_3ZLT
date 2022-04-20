/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief  AGHFP Profile private types.
*/


#ifndef AGHFP_PROFILE_PRIVATE_H
#define AGHFP_PROFILE_PRIVATE_H

#define MAKE_AGHFP_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);
#include "aghfp.h"
#include "task_list.h"
#include "aghfp_profile_typedef.h"
#include "hfp_abstraction.h"

#define RING_PERIOD_IN_SECONDS 4

#define REMOTE_FEATURE_HV2 0x1000
#define REMOTE_FEATURE_HV3 0x2000

#define REMOTE_FEATURE_EV3 0x8000
#define REMOTE_FEATURE_EV4 0x0001
#define REMOTE_FEATURE_EV5 0x0002
#define REMOTE_FEATURE_2EV3 0x2000
#define REMOTE_FEATURE_2EV5 0x8000
#define REMOTE_FEATURE_3EV3 0x4000
#define REMOTE_FEATURE_3EV5 0x8000

#define PSKEY_LOCAL_SUPPORTED_FEATURES (0x00EF)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE (4)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS { 0xFEEF, 0xFE8F, 0xFFDB, 0x875B }

#define appAgHfpGetStatusNotifyList() (task_list_flexible_t *)(&(aghfp_profile_task_data.status_notify_list))

/*! \brief Get SLC status notify list */
#define aghfpProfile_GetSlcStatusNotifyList() (task_list_flexible_t *)(&(aghfp_profile_task_data.slc_status_notify_list))

/*! \brief Get status notify list */
#define aghfpProfile_GetStatusNotifyList() (task_list_flexible_t *)(&(aghfp_profile_task_data.status_notify_list))

typedef struct
{
    TaskData task;
    /*! List of tasks to notify of SLC connection status. */
    TASK_LIST_WITH_INITIAL_CAPACITY(AGHFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY) slc_status_notify_list;
    /*! List of tasks to notify of general HFP status changes */
    TASK_LIST_WITH_INITIAL_CAPACITY(AGHFP_STATUS_NOTIFY_LIST_INIT_CAPACITY) status_notify_list;
    /*! List of tasks requiring confirmation of HFP connect requests */
    task_list_with_data_t connect_request_clients;
    /*! List of tasks requiring confirmation of HFP disconnect requests */
    task_list_with_data_t disconnect_request_clients;
    /*! Task to handle TWS+ AT commands. */
    Task at_cmd_task;
    /*! The task to send SCO sync messages */
    Task sco_sync_task;
} agHfpTaskData;

extern agHfpTaskData aghfp_profile_task_data;

enum aghfp_profile_internal_messages
{
    AGHFP_INTERNAL_CONFIG_WRITE_REQ,              /*!< Internal message to store the HFP device config */
    AGHFP_INTERNAL_HSP_INCOMING_TIMEOUT,          /*!< Internal message to indicate timeout from incoming call */
    AGHFP_INTERNAL_HFP_CONNECT_REQ,               /*!< Internal message to connect to HFP */
    AGHFP_INTERNAL_HFP_DISCONNECT_REQ,            /*!< Internal message to disconnect HFP */
    AGHFP_INTERNAL_HFP_RING_REQ,                  /*!< Internal message to disconnect HFP */
    AGHFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ,    /*!< Internal message to request last number redial */
    AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ,            /*!< Internal message to request voice dial */
    AGHFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ,    /*!< Internal message to disable voice dial */
    AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ,           /*!< Internal message to accept an incoming call */
    AGHFP_INTERNAL_HFP_CALL_REJECT_REQ,           /*!< Internal message to reject an incoming call */
    AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ,           /*!< Internal message to hang up an active call */
    AGHFP_INTERNAL_HFP_MUTE_REQ,                  /*!< Internal message to mute an active call */
    AGHFP_INTERNAL_HFP_TRANSFER_REQ,              /*!< Internal message to transfer active call between AG and device */
    AGHFP_INTERNAL_HFP_HOLD_CALL_REQ,             /*!< Internal message to hold active call */
    AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ,     /*!< Internal message to release the held call */
    AGHFP_INTERNAL_NUMBER_DIAL_REQ
};

/*! Internal connect request message */
typedef struct
{
    bdaddr addr;            /*!< Address of HF */
} AGHFP_INTERNAL_HFP_CONNECT_REQ_T;

typedef struct
{
    bdaddr addr;
} AGHFP_INTERNAL_HFP_RING_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_CALL_REJECT_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_DISCONNECT_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_HOLD_CALL_REQ_T;

typedef struct
{
    aghfpInstanceTaskData *instance;
} AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ_T;

#endif // AGHFP_PROFILE_PRIVATE_H
