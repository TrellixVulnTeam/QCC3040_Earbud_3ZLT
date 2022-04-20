/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Header file for the application state machine
*/

#ifndef EARBUD_SM_H_
#define EARBUD_SM_H_

#include "earbud_primary_rules.h"
#include "earbud_secondary_rules.h"
#include "earbud_sm_config.h"
#include "earbud_handover_typedef.h"

#include <phy_state.h>
#include <tws_topology.h>
#include <a2dp.h>

#include <marshal.h>

/*!
@startuml

    note "For clarity not all state transitions shown" as N1
    note "For clarity TERMINATING substate is not shown in all parent states" as N2
    note top of STARTUP
      _IDLE is any of the idle states
      * IN_CASE_IDLE
      * OUT_OF_CASE_IDLE
      * IN_EAR_IDLE
    end note

    [*] -down-> INITIALISING : Power On
    INITIALISING : App module and library init
    INITIALISING --> DFU_CHECK : Init Complete

    DFU_CHECK : Is DFU in progress?
    DFU_CHECK --> STARTUP : No DFU in progress

    STARTUP : Check for paired peer earbud
    STARTUP : Attempt peer synchronisation
    STARTUP : After attempt go to _IDLE
    STARTUP --> PEER_PAIRING : No paired peer

    FACTORY_RESET : Disconnect links, deleting all pairing, reboot
    FACTORY_RESET : Only entered from _IDLE
    FACTORY_RESET -r-> INITIALISING : Reboot

    HANDSET_PAIRING : Pair with handset
    HANDSET_PAIRING : Return to _IDLE state

    state IN_CASE #LightBlue {
        IN_CASE : Charger Active
        IN_CASE : Buttons Disabled
        DFU : Device Upgrade
        DFU --> IN_CASE_IDLE #LightGreen : DFU Complete
        DFU_CHECK --> DFU : DFU in progress
        IN_CASE_IDLE : May have BT connection(s)
        IN_CASE_IDLE -up-> DFU : Start DFU
	IN_CASE_IDLE : Start dormant timer
    }

    state OUT_OF_CASE #LightBlue {
        OUT_OF_CASE_IDLE : May have BT connection(s)
        OUT_OF_CASE_IDLE : Start dormant timer
        OUT_OF_CASE_SOPORIFIC : Allow sleep
        OUT_OF_CASE_SOPORIFIC_TERMINATING : Disconnect links
        OUT_OF_CASE_SOPORIFIC_TERMINATING : Inform power prepared to sleep
        OUT_OF_CASE_IDLE #LightGreen --> IN_CASE_IDLE : In Case
        IN_CASE_IDLE --> OUT_OF_CASE_IDLE : Out of Case
        OUT_OF_CASE_IDLE -u-> OUT_OF_CASE_SOPORIFIC : Idle timeout
        OUT_OF_CASE_SOPORIFIC -->  OUT_OF_CASE_SOPORIFIC_TERMINATING : POWER_SLEEP_PREPARE_IND
        OUT_OF_CASE_IDLE --> HANDSET_PAIRING : User or Auto pairing
        OUT_OF_CASE_BUSY : Earbud removed from ear
        OUT_OF_CASE_BUSY : Audio still playing
        OUT_OF_CASE_BUSY #LightGreen --> OUT_OF_CASE_IDLE : Out of ear audio timeout
        OUT_OF_CASE_BUSY --> OUT_OF_CASE_IDLE : Audio Inactive
    }

    state IN_EAR #LightBlue {
        IN_EAR_IDLE : May have BT connection(s)
        IN_EAR_IDLE #LightGreen -l-> OUT_OF_CASE_IDLE : Out of Ear
        IN_EAR_IDLE -u-> IN_EAR_BUSY : Audio Active
        OUT_OF_CASE_IDLE --> IN_EAR_IDLE : In Ear
        IN_EAR_BUSY : Streaming Audio Active (A2DP or SCO)
        IN_EAR_BUSY : Tones audio available in other states
        IN_EAR_BUSY #LightGreen -d-> IN_EAR_IDLE : Audio Inactive
        IN_EAR_BUSY -l-> OUT_OF_CASE_BUSY : Out of Ear
        OUT_OF_CASE_BUSY -l-> IN_EAR_BUSY : In Ear
        IN_EAR_IDLE --> HANDSET_PAIRING : User or Auto pairing
    }
@enduml
*/

/*! \brief The state machine substates, note that not all parent states support
           all substates. */
typedef enum sm_application_sub_states
{
    APP_SUBSTATE_TERMINATING           = 0x0001, /*!< Preparing to shutdown (e.g. disconnecting links) */
    APP_SUBSTATE_SOPORIFIC             = 0x0002, /*!< Allowing sleep */
    APP_SUBSTATE_SOPORIFIC_TERMINATING = 0x0004, /*!< Preparing to sleep (e.g. disconnecting links) */
    APP_SUBSTATE_IDLE                  = 0x0008, /*!< Audio is inactive */
    APP_SUBSTATE_BUSY                  = 0x0010, /*!< Audio is active */
    APP_SUBSTATE_DFU                   = 0x0020, /*!< Upgrading firmware */

    APP_SUBSTATE_INITIALISING          = 0x0040, /*!< App module and library initialisation in progress. */
    APP_SUBSTATE_DFU_CHECK             = 0x0080, /*!< Interim state, to see if DFU is in progress. */
    APP_SUBSTATE_FACTORY_RESET         = 0x0100, /*!< Resetting the earbud to factory defaults. */
    APP_SUBSTATE_STARTUP               = 0x0200, /*!< Startup, syncing with peer. */

    APP_SUBSTATE_HANDSET_PAIRING       = 0x0800, /*!< Pairing with a handset */
    APP_SUBSTATE_DEVICE_TEST_MODE      = 0x1000, /*!< Dedicated state for device test */

    APP_END_OF_SUBSTATES = APP_SUBSTATE_DEVICE_TEST_MODE, /*!< The last substate */
    APP_SUBSTATE_MASK    = ((APP_END_OF_SUBSTATES << 1) - 1), /*!< Bitmask to retrieve substate from full state */
} appSubState;

/*! \brief Application states.
 */
typedef enum sm_application_states
{
    /*! Initial state before state machine is running. */
    APP_STATE_NULL                = 0x0000,
    APP_STATE_INITIALISING        = APP_SUBSTATE_INITIALISING,
    APP_STATE_DFU_CHECK           = APP_SUBSTATE_DFU_CHECK,
    APP_STATE_FACTORY_RESET       = APP_SUBSTATE_FACTORY_RESET,
    APP_STATE_STARTUP             = APP_SUBSTATE_STARTUP,
    APP_STATE_HANDSET_PAIRING     = APP_SUBSTATE_HANDSET_PAIRING,
    APP_STATE_TERMINATING         = APP_SUBSTATE_TERMINATING,
    APP_STATE_DEVICE_TEST_MODE    = APP_SUBSTATE_DEVICE_TEST_MODE,

    /*! Earbud is in the case, parent state */
    APP_STATE_IN_CASE                   = APP_END_OF_SUBSTATES<<1,
        APP_STATE_IN_CASE_TERMINATING   = APP_STATE_IN_CASE + APP_SUBSTATE_TERMINATING,
        APP_STATE_IN_CASE_SOPORIFIC             = APP_STATE_IN_CASE + APP_SUBSTATE_SOPORIFIC,
	APP_STATE_IN_CASE_SOPORIFIC_TERMINATING = APP_STATE_IN_CASE + APP_SUBSTATE_SOPORIFIC_TERMINATING,
    APP_STATE_IN_CASE_IDLE          = APP_STATE_IN_CASE + APP_SUBSTATE_IDLE,
        APP_STATE_IN_CASE_DFU           = APP_STATE_IN_CASE + APP_SUBSTATE_DFU,

    /*!< Earbud is out of the case, parent state */
    APP_STATE_OUT_OF_CASE                           = APP_STATE_IN_CASE<<1,
        APP_STATE_OUT_OF_CASE_TERMINATING           = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_TERMINATING,
        APP_STATE_OUT_OF_CASE_SOPORIFIC             = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_SOPORIFIC,
        APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_SOPORIFIC_TERMINATING,
        APP_STATE_OUT_OF_CASE_IDLE                  = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_IDLE,
        APP_STATE_OUT_OF_CASE_BUSY                  = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_BUSY,

    /*!< Earbud in in ear, parent state */
    APP_STATE_IN_EAR                   = APP_STATE_OUT_OF_CASE<<1,
        APP_STATE_IN_EAR_TERMINATING   = APP_STATE_IN_EAR + APP_SUBSTATE_TERMINATING,
        APP_STATE_IN_EAR_IDLE          = APP_STATE_IN_EAR + APP_SUBSTATE_IDLE,
        APP_STATE_IN_EAR_BUSY          = APP_STATE_IN_EAR + APP_SUBSTATE_BUSY,

} appState;

/*! \brief Return TRUE if the state is in the case */
#define appSmStateInCase(state) (0 != ((state) & APP_STATE_IN_CASE))

/*! \brief Return TRUE if the state is out of case
    \note this means in one of the "out of case" states, and does not
    include "in ear".
*/
#define appSmStateOutOfCase(state) (0 != ((state) & APP_STATE_OUT_OF_CASE))

/*! \brief Return TRUE if the state is in the ear */
#define appSmStateInEar(state) (0 != ((state) & APP_STATE_IN_EAR))

/*! \brief Return TRUE if the sub-state is terminating */
#define appSmSubStateIsTerminating(state) \
    (0 != ((state) & (APP_SUBSTATE_TERMINATING | APP_SUBSTATE_SOPORIFIC_TERMINATING)))

/*! \brief Return TRUE if the state is idle */
#define appSmStateIsIdle(state) (0 != ((state) & (APP_SUBSTATE_IDLE | APP_SUBSTATE_SOPORIFIC)))

/*! \brief Check if the application is in a core state.
    \note Warning, ensure this macro is updated if enum #sm_application_states
    or #sm_application_sub_states is changed.
*/
#define appSmIsCoreState() (0 != (appGetSubState() & (APP_SUBSTATE_IDLE | \
                                                      APP_SUBSTATE_BUSY | \
                                                      APP_SUBSTATE_SOPORIFIC)))

/*! \brief SM UI Provider contexts */
typedef enum
{
    context_phy_state_in_case,
    context_phy_state_out_of_case

} phy_state_provider_context_t;

#ifdef PRODUCTION_TEST_MODE
/*! \brief Provide Production Test Mode state context */
typedef enum
{
    context_ptm_state_none,  /*!< Mission mode */
    context_ptm_state_ptm    /*!< Running production test mode */
} ptm_state_provider_context_t;
#endif

typedef enum
{
    context_app_sm_non_core_state,
    context_app_sm_in_case,
    context_app_sm_out_of_case_idle,
    context_app_sm_out_of_case_idle_connected,
    context_app_sm_out_of_case_busy,
    context_app_sm_in_ear

} app_sm_provider_context_t;

typedef enum
{
    EARBUD_ROLE_PRIMARY = EARBUD_ROLE_MESSAGE_BASE,
    EARBUD_ROLE_SECONDARY,
    EARBUD_ROLE_NO_ROLE,

    /*! This must be the final message */
    EARBUD_ROLE_MESSAGE_END
} earbud_sm_topology_role_changed_t;
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(EARBUD_ROLE, EARBUD_ROLE_MESSAGE_END)

/*! \brief Main application state machine task data. */
typedef struct
{
    TaskData task;                      /*!< SM task */
    appState state;                     /*!< Application state */
    phyState phy_state;                 /*!< Cache the current physical state */
    uint16 disconnect_lock;             /*!< Disconnect message lock */
    uint16 dfu_dynamic_role_lock;       /*!< DFU dynamic role selection message lock */

    bool user_pairing:1;                /*!< User initiated pairing */
                                        /*!  Flag used to restart handset pairing after peer role 
                                             confirmed as primary */
    bool restart_pairing_after_role_selection:1;

                                        /*! Flag used to record short-term status where the peer 
                                            device (primary at the time) has entered the case while
                                            it was handset pairing */
    bool peer_was_pairing_when_entered_case:1;

    tws_topology_role role;             /*!< Current primary/secondary/none role of the earbud */
    rule_set_t primary_rules;
    rule_set_t secondary_rules;

    earbud_sm_timers timers;            /*!< Remaining times on outstanding timers during handover. */
    /*! The clients of the application include the UI Indicators, which are sent messages on
     *  application state changes. */
    task_list_t client_tasks;
} smTaskData;

/*! \brief Used to specify which link needs to be disconnected */
typedef enum
{
    SM_DISCONNECT_HANDSET  = 1 << 0,
    SM_DISCONNECT_ALL = SM_DISCONNECT_HANDSET,
} smDisconnectBits;

/*!< Application state machine. */
extern smTaskData app_sm;

/*! Get pointer to application state machine task data struture. */
#define SmGetTaskData()          (&app_sm)

/*! Get pointer to state machine client tasks */
#define SmGetClientTasks()       (&(SmGetTaskData()->client_tasks))

/*! \brief Get the state machine disconnect lock. */
#define appSmDisconnectLockGet() (SmGetTaskData()->disconnect_lock)
/*! \brief Clear all bits in the disconnect lock */
#define appSmDisconnectLockClearAll() appSmDisconnectLockGet() = 0;
/*! \brief Set connected links (that will be disconnected) bits in the disconnect lock */
#define appSmDisconnectLockSetLinks(sm_disconnect_bits) appSmDisconnectLockGet() |= sm_disconnect_bits
/*! \brief Clear connected links (that will be disconnected) bits in the disconnect lock */
#define appSmDisconnectLockClearLinks(sm_disconnect_bits) appSmDisconnectLockGet() &= ~sm_disconnect_bits
/*! \brief Query if the handset is disconnecting */
#define appSmDisconnectLockHandsetIsDisconnecting() (0 != (appSmDisconnectLockGet() & SM_DISCONNECT_HANDSET))


/*! \brief Get DFU Dynamic Role selection lock. */
#define appSmDfuDynamicRoleLockGet() (SmGetTaskData()->dfu_dynamic_role_lock)
/*! \brief Set dfu_dynamic_role_lock=TRUE if Dynamic Role Selection is to be done for DFU 
           and dfu_dynamic_role_lock=FALSE if Fixed Role Known for DFU.
 */
#define appSmDfuDynamicRoleLockSet(hasDynamicRole) appSmDfuDynamicRoleLockGet() = hasDynamicRole
/*! \brief Clear dfu_dynamic_role_lock (i.e.= FALSE) if Dynamic Role Selection is completed for DFU */
#define appSmDfuDynamicRoleLockClear() appSmDfuDynamicRoleLockGet() = FALSE

/*! \brief provides state manager(sm) task to other components

    \param[in]  void

    \return     Task - sm task.
*/
Task SmGetTask(void);

/*! \brief Change application state.

    \param new_state State to move to.
 */
void appSmSetState(appState new_state);

/*! \brief Get current application state.

    \return appState Current application state.
 */
static inline appState appSmGetState(void)
{
    return SmGetTaskData()->state;
}

/*! \brief Initialise the main application state machine.
 */
bool appSmInit(Task init_task);

/*! \brief Application state machine message handler.
    \param task The SM task.
    \param id The message ID to handle.
    \param message The message content (if any).
*/
void appSmHandleMessage(Task task, MessageId id, Message message);

/* FUNCTIONS THAT CHECK THE STATE OF THE SM
 *******************************************/

/*! @brief Query if this earbud is in-ear.
    @return TRUE if in-ear, otherwise FALSE. */
bool appSmIsInEar(void);

/*! @brief Query if this earbud is out-of-ear.
    @return TRUE if out-of-ear, otherwise FALSE. */
bool appSmIsOutOfEar(void);

/*! @brief Query if this earbud is in-case.
    @return TRUE if in-case, otherwise FALSE. */
bool appSmIsInCase(void);

/*! @brief Query if this earbud is out-of-case.
    @return TRUE if out-of-case, otherwise FALSE. */
bool appSmIsOutOfCase(void);

/*! @brief Query if this earbud is intentionally disconnecting links. */
#define appSmIsDisconnectingLinks() \
    (0 != (appGetSubState() & (APP_SUBSTATE_SOPORIFIC_TERMINATING | \
                               APP_SUBSTATE_TERMINATING)))

/*! @brief Query if this earbud is pairing. */
#define appSmIsPairing() \
    (appSmGetState() == APP_STATE_HANDSET_PAIRING)

/*! @brief Query if we think the peer was pairing when it entered case */
#define appSmPeerWasPairingWhenItEnteredCase() \
    (app_sm.peer_was_pairing_when_entered_case)

/*! @brief Query if this state is a sleepy state. */
#define appSmStateIsSleepy(state) \
    (0 != ((state) & (APP_SUBSTATE_SOPORIFIC | \
                      APP_SUBSTATE_SOPORIFIC_TERMINATING)))

/*! Check whether we are allowed to create new BLE connections.

    New connections are allowed in the case and specifically when
    in a special DFU mode (Download Firmware Upgrade).

    Configuration options allow BLE to be used
    \li Out of the case

    And to block NEW BLE connections
    \li if in a call / playing music (not idle)
*/
#define appSmStateAreNewBleConnectionsAllowed(state) \
    (   appSmStateInCase(state) \
     || (   (appSmStateOutOfCase(state) || appSmStateInEar(state))\
         && (((state) & APP_SUBSTATE_IDLE) == APP_SUBSTATE_IDLE \
             || appConfigBleNewConnectionsWhenBusy()) \
         && appConfigBleAllowedOutOfCase()) \
     || (state) == APP_STATE_IN_CASE_DFU)

/*! Query if in DFU mode

    \return FALSE not in DFU mode, TRUE otherwise */
extern bool appSmIsInDfuMode(void);

/*! @brief Get the physical state as received from last update message. */
#define appSmGetPhyState() (SmGetTaskData()->phy_state)

/*! @brief Query if pairing has been initiated by the user. */
#define appSmIsUserPairing() (SmGetTaskData()->user_pairing)
/*! @brief Set user initiated pairing flag. */
#define appSmSetUserPairing()  (SmGetTaskData()->user_pairing = TRUE)
/*! @brief Clear user initiated pairing flag. */
#define appSmClearUserPairing()  (SmGetTaskData()->user_pairing = FALSE)

/*! Check if pairing should be restarted after role selection (if we are primary) */
#define appSmRestartPairingAfterRoleSelection() (SmGetTaskData()->restart_pairing_after_role_selection)

/*! Set whether pairing should be restarted after role selection */
#define appSmSetRestartPairingAfterRoleSelection(_restart) (SmGetTaskData()->restart_pairing_after_role_selection = (_restart))



/* FUNCTIONS THAT CHECK THE STATE OF BLE FUNCTIONALITY */
/* Defined here as this is application-level knowledge */

/*! Do we have a BLE connection */
#define appSmHasBleConnection()     (appGattHasBleConnection())

/*! \brief Determine if the Earbud is currently Primary. */
bool appSmIsPrimary(void);

/*! \brief Determine if an A2DP restart timer is pending. */
bool appSmIsA2dpRestartPending(void);

/*! \brief Determine if we have an broadcast restart timer pending. */
bool appSmIsBroadcastRestartPending(void);

/* FUNCTIONS TO INITIATE AN ACTION IN THE SM
 ********************************************/
/*! \brief Initiate pairing with a handset. */
void appSmPairHandset(void);
/*! \brief Delete paired handsets. */
void appSmDeleteHandsets(void);
/*! \brief Connect to paired handset. */
void appSmConnectHandset(void);
/*! \brief Disconnect the least recently used handset. */
void appSmDisconnectLruHandset(void);
/*! \brief appSmEnterDtsMode */
void appSmEnterDtsMode(uint16 mode);
/*! \brief Request a factory reset. */
void appSmFactoryReset(void);
/*! \brief Reboot the earbud. */
void appSmReboot(void);

/*! \brief Enter the special upgrade state.

    This should be called to enter the upgrade (or DFU) mode immediately.

    \note The upgrade mode may be time limited. This is based on the values
          in the configuration (for instance \ref appConfigDfuTimeoutToPlaceInCaseMs).
 */
void appSmEnterDfuMode(void);

/*! \brief Enable disable entry to upgarde mode when entering the case

    \param enable Enable or disable this mode
    \param inform_peer Inform peer to enable this mode */
extern void appSmEnterDfuModeInCase(bool enable, bool inform_peer);

/*! \brief Handler for unsolicited connection library messages.

    This function needs to be called by the task that is registered for
    connection library messages, as the majority of these are sent to
    a single registered task.

    \param      id      The identifier of the connection library message
    \param      message Pointer to the message content. Can be NULL.
    \param      already_handled Flag indicating if another task has already
                        dealt with this message

    \return TRUE if the handler has dealt with the message, FALSE otherwise
*/
bool appSmHandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled);

#endif /* EARBUD_SM_H_ */
