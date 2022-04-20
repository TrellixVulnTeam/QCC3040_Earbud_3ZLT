/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Private header file for the TWS topology.
*/

#ifndef TWS_TOPOLOGY_PRIVATE_H_
#define TWS_TOPOLOGY_PRIVATE_H_

#include "tws_topology_sm.h"
#include "tws_topology_config.h"
#include "hdma.h"
#include <rules_engine.h>
#include <goals_engine.h>
#include <task_list.h>
#include <stdlib.h>

#include <message.h>
#include <bdaddr.h>

/*! Type used to indicate the stages of stopping, triggered by TwsTopology_Stop() */
typedef enum 
{
    twstop_state_stopped,  /*!< Topology is stopped (default) */
    twstop_state_stopping, /*!< Topology is stopping */
    twstop_state_started,  /*!< Topology is started */
} tws_topology_stopping_state_t;

/*! Defines the roles changed task list initalc capacity */
#define MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY 1

typedef enum
{
    /*! Message sent internally to action the TwsTopology_Start function */
    TWSTOP_INTERNAL_START = INTERNAL_MESSAGE_BASE,

    /*! Message sent internally to action the TwsTopology_Stop function */
    TWSTOP_INTERNAL_STOP,
    TWSTOP_INTERNAL_HANDLE_PENDING_GOAL,
    TWSTOP_INTERNAL_ALL_ROLE_CHANGE_CLIENTS_PREPARED,
    TWSTOP_INTERNAL_ROLE_CHANGE_CLIENT_REJECTION,
    TWSTOP_INTERNAL_CLEAR_HANDOVER_PLAY,

    /*! Internal message sent if the topology stop command times out */
    TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP,

    /*! Result of the peer pairing procedure. */
    TWSTOP_INTERNAL_PROC_PAIR_PEER_RESULT,
    /*! Indication that system stop procedure has completed. */
    TWSTOP_INTERNAL_PROC_SEND_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED,

    TWSTOP_INTERNAL_MESSAGE_END
} tws_topology_internal_message_t;

/* Validate that internal message range has not been breached. */
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(TWSTOP_INTERNAL_MESSAGE_END)

/*! Message content for TWSTOP_INTERNAL_START internal message */
typedef struct
{
    /*! Application task requesting start */
    Task app_task;
} TWSTOP_INTERNAL_START_T;

/*! Message content for TWSTOP_INTERNAL_STOP internal message */
typedef struct
{
    /*! Application task requesting stop */
    Task app_task;
} TWSTOP_INTERNAL_STOP_T;


/*! Structure describing handover data */
typedef struct {
    /*! Reason for the handover decision #hdma_handover_reason_t.
     * This reason will also be used as conditional lock for posting
     * Handover retry cancel message #TWS_TOP_PROC_HANDOVER_INTERNAL_CANCEL_RETRY,
     * therefore forcing data type to uint16 */
    uint16 reason;
}handover_data_t;

/*! Result of the peer pairing procedure, sent by the procedure
    to the TWS Topology core. */
typedef struct
{
    bool success;
} TWSTOP_INTERNAL_PROC_PAIR_PEER_RESULT_T;

/*! Structure holding information for the TWS Topology task */
typedef struct
{
    /*! Task for handling messages */
    TaskData                task;

    /*! Task for handling goal messages (from the rules engine) */
    TaskData                goal_task;

    /*! Task to be sent all outgoing messages */
    Task                    app_task;

    /*! Current primary/secondary role */
    tws_topology_role       role;

    /*! Whether we are acting in a role until a firm role is determined. */
    bool                    acting_in_role;

    /*! Internal state */
    tws_topology_state      state;

    /*! Whether we have sent a start confirm yet */
    bool                    start_cfm_needed;

    /*! List of clients registered to receive TWS_TOPOLOGY_ROLE_CHANGED_IND_T
     * messages */
    TASK_LIST_WITH_INITIAL_CAPACITY(MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY)   message_client_tasks;

    /*! Task handler for pairing activity notification */
    TaskData                pairing_notification_task;

    /*! Queue of goals already decided but waiting to be run. */
    TaskData                pending_goal_queue_task;

    /*! The TWS topology goal set */
    goal_set_t              goal_set;

    /*! Whether hdma is created or not.TRUE if created. FALSE otherwise */
    bool                    hdma_created;
    
    /*! Whether Handover is allowed or prohibited. controlled by APP */
    bool                    app_prohibit_handover;

    /*! handover related information */
    handover_data_t         handover_info;

    /*! Can be used to control whether topology attempts handset connection */
    bool                    prohibit_connect_to_handset;

    /*! Flag used to track topology stop commands */
    tws_topology_stopping_state_t stopping_state;

    /*! For in-case DFU; earbuds are first put in DFU mode to retain profiles */
    bool enter_dfu_mode;

    /*! If device is put in-case after enabling DFU mode */
    bool enter_dfu_in_case;

    /*! The currently selected advertising parameter set */
    tws_topology_le_adv_params_set_type_t advertising_params;

    /*! Profile mask of the peer profiles which topology has been requested to connect 
        despite earbud is going in the case. */
    uint32                  peer_profile_connect_mask;

    /*! Flag to indicate topology to remain active for handset despite earbud is going in the case. */
    bool                   remain_active_for_handset:1;

    /*! Flag to indicate topology to remain active for peer despite earbud is going in the case. */
    bool                   remain_active_for_peer:1;

    /*! Flag to remember the last phy_state transition was going
        into the case. This is used when running rules based on the
        peer BREDR link being disconnected, to differentiate opening
        the lid or going in the case. This enables PFR/Connect logic
        on lid open, but prevents it when having just gone in the case. */
    bool                    just_went_in_case:1;

    /*! Flag to remember if tws_topology_goal_connect_handset was underway
        and handover has been triggered which resulated in cancelling the 
        connect_handset goal.
        This is used when running the rule to resume the connect_handset once
        handover completes.
        If handover fails old primary will resume the connect_handset.
        If handover succeed new primary will resume the connect_handset. */
    bool                    reconnect_post_handover:1;

} twsTopologyTaskData;

/* Make the tws_topology instance visible throughout the component. */
extern twsTopologyTaskData tws_topology;

/*! Get pointer to the task data */
#define TwsTopologyGetTaskData()         (&tws_topology)

/*! Get pointer to the TWS Topology task */
#define TwsTopologyGetTask()             (&tws_topology.task)

/*! Get pointer to the TWS Topology task */
#define TwsTopologyGetGoalTask()         (&tws_topology.goal_task)

/*! Get pointer to the TWS Topology role changed tasks */
#define TwsTopologyGetMessageClientTasks() (task_list_flexible_t *)(&tws_topology.message_client_tasks)


/*! Macro to create a TWS topology message. */
#define MAKE_TWS_TOPOLOGY_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))

void twsTopology_SetRole(tws_topology_role role);
tws_topology_role twsTopology_GetRole(void);
void twsTopology_SetActingInRole(bool acting);
void twsTopology_RulesSetEvent(rule_events_t event);
void twsTopology_RulesResetEvent(rule_events_t event);
void twsTopology_RulesMarkComplete(MessageId message);
void twsTopology_CreateHdma(void);
void twsTopology_DestroyHdma(void);

/*! Private API used to implement the stop functionality */
void twsTopology_StopHasStarted(void);

/*! Private API used for test functionality

    \return TRUE if topology has been started, FALSE otherwise
 */
bool twsTopology_IsRunning(void);

/*! \brief Check if DFU mode is enabled (used for in-case DFU)

    \return TRUE if DFU mode is enabled.
 */
bool TwsTopology_IsDfuMode(void);

/*! \brief function to enable/disable in-case DFU flag (used for in-case DFU)

    Request the topology to enable/disable in-case DFU mode flag.

    \param val Boolean variable to enable/disable in-case DFU flag
 */
void TwsTopology_SetDfuInCase(bool val);

/*! \brief Check if in-case DFU flag is enabled (used for in-case DFU)

    \return TRUE if in-case DFU flag is enabled.
 */
bool TwsTopology_IsDfuInCase(void);

/*! \brief Check which peer profile to connect.

    Tell the topology that which peer profiles have to be connected, even if
    Earbud is going in case.

    \return profile mask of peer profiles which have to be connected.
 */
uint32 TwsTopology_GetPeerProfileConnectMask(void);

/*! \brief Check whether to remain active or not.

    Tell the topology to remain active for handset even if Earbud is going in the case.

    \return TRUE when remain_active_for_handset enabled, FALSE otherwise.
 */
bool TwsTopology_IsRemainActiveForHandsetEnabled(void);

/*! \brief Check whether to remain active or not.

    Tell the topology to remain active for peer even if Earbud is going in the case.

    \return TRUE when remain_active_for_peer enabled, FALSE otherwise.
 */
bool TwsTopology_IsRemainActiveForPeerEnabled(void);

/*! \brief Was the last phy state transition going into the case.
    \return TRUE Earbud just went into the case, FALSE otherwise.
*/
bool twsTopology_JustWentInCase(void);

/*! \brief function to Set/Reset the reconnect_post_handover flag.

    \param reconnect_post_handover bool variable to Set/Reset the reconnect_post_handover flag
 */
void twsTopology_SetReconnectPostHandover(bool reconnect_post_handover);

#endif /* TWS_TOPOLOGY_H_ */
