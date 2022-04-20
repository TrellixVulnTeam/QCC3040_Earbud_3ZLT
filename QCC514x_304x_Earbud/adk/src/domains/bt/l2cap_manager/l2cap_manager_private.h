/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       l2cap_manager_private.h
\brief      Header file for L2CAP Manager internal data types.
*/

#ifndef L2CAP_MANAGER_PRIVATE_H_
#define L2CAP_MANAGER_PRIVATE_H_

#ifdef INCLUDE_L2CAP_MANAGER

#include "l2cap_manager.h"

#include <csrtypes.h>
#include <bdaddr.h>
#include <task_list.h>


/*! \brief The initial capacity of the task list statically allocated.
           NB: Do not enclose the nuber with parenthesises as the number
               will be used as a input to create a typedef symbol. */
#define L2CAP_MANAGER_STATIC_TASKS_LIST_INIT_CAPACITY       3

/*! \brief Flag that indicates a link is in a transitional state. */
#define L2CAP_MANAGER_LINK_STATE_TRANSITIONAL               (0x80)


/*! \brief States of a PSM instance.
           Each PSM instance has the state machine with these states. */
typedef enum
{
    L2CAP_MANAGER_PSM_STATE_NULL,                       /*!< Initial state. */

    L2CAP_MANAGER_PSM_STATE_PSM_REGISTRATION,           /*!< L2CAP registration. */
    L2CAP_MANAGER_PSM_STATE_SDP_REGISTRATION,           /*!< SDP record registration. */
    L2CAP_MANAGER_PSM_STATE_SDP_SEARCH,                 /*!< SDP search in progress. */
    L2CAP_MANAGER_PSM_STATE_READY,                      /*!< PSM is connectable/connected state. */

    L2CAP_MANAGER_PSM_STATE_CONNECTING,                 /*!< Establishing a connection in progress. */
    L2CAP_MANAGER_PSM_STATE_CONNECTED                   /*!< An L2CAP connection is live. */

} l2cap_manager_psm_state_t;


/*! \brief States of an L2CAP link.
           Each L2CAP connection has the state machine with these states. */
typedef enum
{
    L2CAP_MANAGER_LINK_STATE_NULL                       = 0,                                             /*!< Initial state */

    L2CAP_MANAGER_LINK_STATE_DISCONNECTED               = 1,                                             /*!< No connection. */
    L2CAP_MANAGER_LINK_STATE_LOCAL_INITIATED_SDP_SEARCH = 2 + L2CAP_MANAGER_LINK_STATE_TRANSITIONAL,     /*!< SDP search to get remote PSM (if not known). */
    L2CAP_MANAGER_LINK_STATE_LOCAL_INITIATED_CONNECTING = 3 + L2CAP_MANAGER_LINK_STATE_TRANSITIONAL,     /*!< Establishing connection initiated by local device. */
    L2CAP_MANAGER_LINK_STATE_CONNECTING_BY_REMOTE       = 4 + L2CAP_MANAGER_LINK_STATE_TRANSITIONAL,     /*!< Establishing connection initiated by remote device. */
    L2CAP_MANAGER_LINK_STATE_CONNECTED                  = 5,                                             /*!< Connnected. */
    L2CAP_MANAGER_LINK_STATE_DISCONNECTING              = 6 + L2CAP_MANAGER_LINK_STATE_TRANSITIONAL      /*!< Disconnection in progress. */

} l2cap_manager_link_state_t;


/*! \brief Linked-list data types. */
typedef enum
{
    l2cap_manager_linked_list_type_invalid = 0,
    
    l2cap_manager_linked_list_type_psm_instance,
    l2cap_manager_linked_list_type_l2cap_link_instance,

} l2cap_manager_linked_list_type_t;


/*! \brief A unique key of a linked list. */
typedef uint16 linked_list_key;


#define L2CAP_MANAGER_INSTANCE_ID_INVALID               (0x0000)
#define L2CAP_MANAGER_INSTANCE_ID_FLAG_ID_FIELD_MASK    (0x0FFF)
#define L2CAP_MANAGER_INSTANCE_ID_FLAG_PSM              (0x1000)
#define L2CAP_MANAGER_INSTANCE_ID_FLAG_L2CAP_LINK       (0x2000)

#define L2CAP_MANAGER_INVALID_SINK                      ((Sink)(0x0000))
#define L2CAP_MANAGER_INVALID_SOURCE                    ((Source)(0x0000))



/*! \brief An L2CAP instance data held per link. */
typedef struct __l2cap_manager_l2cap_link_instance_t
{
    /*! Instance ID of an L2CAP link instance. */
    linked_list_key instance_id;

    /*! Pointer to the next instance. This may NULL. */
    struct __l2cap_manager_l2cap_link_instance_t *next;

    /*! The status of this connection. */
    l2cap_manager_link_state_t          link_status;

    /*! Local PSM that the remote device connects to. */
    uint16                              local_psm;
    /*! The Bluetooth device address of the remote device. */
    tp_bdaddr                           remote_dev;
    /*! Unique signal identifier for an L2CAP connection. */
    uint16                              connection_id;
    /*! A single octet identifier to be used to match responses with requests.
        Note that this variable is just a place holder for the latest value
        used as a different Identifier must be used for each successive command. */
    uint8                               identifier;

    /*! The MTU advertised by the remote device. */
    uint16                              mtu_remote;
    /*! The flush timeout in use by the remote device. */
    uint16                              flush_timeout_remote;
    /*! The Quality of Service settings of the remote device. */
    l2cap_manager_qos_flow              qos_remote;
    /*! The flow mode agreed with the remote device */
    uint8                               mode;

    /*! The sink that is used to send data to the remote device. */
    Sink                                sink;
    /*! The source that is used to receive data from the remote device. */
    Source                              source;

    /*! A pointer to the context data the client can use at its discretion. */
    void                               *context;

} l2cap_manager_l2cap_link_instance_t;


/*! \brief A PSM instance data held per client task for marshalled message channels. */
typedef struct __l2cap_manager_psm_instance_t
{
    /*! Instance ID of a PSM instance. */
    linked_list_key                         instance_id;

    /*! Pointer to the next instance. This may NULL. */
    struct __l2cap_manager_psm_instance_t  *next;

    /*! Current state of this PSM instance. */
    l2cap_manager_psm_state_t               state;
    /*! L2CAP PSM registered by the local device. */
    uint16                                  local_psm;
    /*! L2CAP PSM registered by the remote device. */
    uint16                                  remote_psm;             
    /*! Pointer to the SDP record. */
    uint8                                   *sdp_record;
    /*! SDP service record handle. */
    uint32                                  service_handle;
    /*! The maximum number of SDP search retries. */
    uint8                                   sdp_search_max_retries;
    /*! Count of failed SDP searches. */
    uint8                                   sdp_search_attempts;

    /*! The number of L2CAP connections on this PSM. */
    uint8                                   num_of_links;           
    /*! Pointer to the linked-list of the L2CAP link instances. */
    l2cap_manager_l2cap_link_instance_t     *l2cap_instances;                 

    /*!< Pointer to the table of the callback functions. */
    const l2cap_manager_functions_t         *functions;


} l2cap_manager_psm_instance_t;


/*! \brief Task data for the L2CAP Manager. */
typedef struct
{
    TaskData                        task;                   /*!< L2CAP Manager module task. */

    TASK_LIST_WITH_INITIAL_CAPACITY(L2CAP_MANAGER_STATIC_TASKS_LIST_INIT_CAPACITY)  client_tasks;   /*!< List of tasks registered. */
 
    uint8                           num_of_psm_instances;   /*!< The number of the PSM instances. */
    uint8                           pending_connections;    /*!< The number of pending L2CAP connections. */
    l2cap_manager_psm_instance_t    *psm_instances;         /*!< Pointer to the linked-list of the PSM instances. */

} l2cap_manager_task_data_t;


extern l2cap_manager_task_data_t l2cap_manager_task_data;


/*! Get pointer to the L2CAP Manager task data structure. */
#define l2capManagerGetTaskData()           (&l2cap_manager_task_data)

/*! Get pointer to the peer signalling modules data structure */
#define l2capManagerGetClientTasks()        (task_list_flexible_t *)(&l2cap_manager_task_data.client_tasks)

/*! Get pointer to the linked-list of the PSM instances. */
#define l2capManagerGetPsmInstanceList()    (&l2cap_manager_task_data.psm_instances)


#endif /* INCLUDE_L2CAP_MANAGER */
#endif /* L2CAP_MANAGER_PRIVATE_H_ */
