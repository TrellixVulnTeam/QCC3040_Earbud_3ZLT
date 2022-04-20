/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       qualcomm_connection_manager_private.h
\brief      Private header file for Qualcomm Connection Manager
*/

#ifndef __QCOM_CON_MANAGER_PRIVATE_H
#define __QCOM_CON_MANAGER_PRIVATE_H

#ifdef INCLUDE_QCOM_CON_MANAGER

/*! QHS modes 2,3,4,5 octet */
#define QHS_MODE_2345_OCTET 0
/*! QHS modes 2,3,4,5 mask */
#define QHS_MODE_2345_MASK 0xF0

/*! QHS mode 6 octet*/
#define QHS_MODE_6_OCTET 1
/*! QHS mode 6 mask */
#define QHS_MODE_6_MASK 0x01

/*! Fast exit sniff subrate octet */
#define FAST_EXIT_SNIFF_SUBRATE_OCTET 2
/*! Fast exit sniff subrate mask */
#define FAST_EXIT_SNIFF_SUBRATE_MASK 0x40

/*! Delay from QLM connection to requesting remote feature read */
#define REQUEST_REMOVE_FEATURES_DELAY_MS 100

/*! Qualcomm Connection Manager module task data. */
typedef struct
{
    /*! The task information for the qualcomm connection manager */
    TaskData task;

    /*! List of tasks registered for notifications from qcom connection manager module */
    task_list_t client_tasks;

    /*! Set when the local device supports any QHS mode */
    unsigned qhs : 1;

    /*! Set when the local device supports fast exit from sniff subrate */
    unsigned fast_exit_sniff_subrate : 1;

} qcomConManagerTaskData;

/*! Internal messages */
typedef enum
{
    /*! Message to trigger reading remote features */
    QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES = INTERNAL_MESSAGE_BASE,

    /*! This must be the final message */
    QCOM_CON_MANAGER_INTERNAL_MESSAGE_END
} qualcomm_connection_manager_internal_message_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(QCOM_CON_MANAGER_INTERNAL_MESSAGE_END)

/*! Message content for QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES */
typedef struct
{
    /*! Address of remote device */
    BD_ADDR_T bd_addr;
} QCOM_CON_MANAGER_INTERNAL_READ_QLM_REMOTE_FEATURES_T;

/*!< Qualcomm connection manager task data */
qcomConManagerTaskData  qcom_con_manager;

#define QcomConManagerGet() (&qcom_con_manager)

#define QcomConManagerGetTask() (&qcom_con_manager.task)

/*! Construct a VSDM prim of the given type */
#define MAKE_VSDM_PRIM_T(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->type = TYPE;

#endif /* INCLUDE_QCOM_CON_MANAGER */

#endif /*__QCOM_CON_MANAGER_PRIVATE_H*/
