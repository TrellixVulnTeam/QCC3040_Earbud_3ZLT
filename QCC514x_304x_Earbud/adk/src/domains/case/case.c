/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Case domain, managing interactions with a product case.
*/
/*! \addtogroup case_domain
@{
*/

#include "case.h"
#include "case_private.h"
#include "case_state_client_msgs.h"
#include "case_comms.h"

#include <phy_state.h>

#include <task_list.h>
#include <logging.h>

#include <message.h>

#ifdef INCLUDE_CASE

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(case_message_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(CASE, CASE_MESSAGE_END)

#endif

/*! Main Case domain data structure. */
case_state_t case_state;

/* \brief Utility function to validate a battery state value.
    \param battery_state Battery and charging state in combined format.
    \return TRUE state is valid, FALSE otherwise.
*/
static bool case_BatteryStateIsValid(uint8 battery_state)
{
    if (   (battery_state == BATTERY_STATUS_UNKNOWN)
        || (BATTERY_STATE_PERCENTAGE(battery_state) <= 100))
    {
        return TRUE;
    }

    return FALSE;
}

/* \brief If taken out of the case then update lid state to unknown (and notify).
*/
static void Case_HandlePhyStateChangedInd(const PHY_STATE_CHANGED_IND_T *ind)
{
    case_state_t* case_td = Case_GetTaskData();

    DEBUG_LOG_INFO("Case_HandlePhyStateChangedInd state enum:phyState:%d enum:phy_state_event:%d", ind->new_state, ind->event);

    /* if we just came out of the case, we can no longer trust the last lid_state */
    if (ind->event == phy_state_event_out_of_case)
    {
        case_td->lid_state = CASE_LID_STATE_UNKNOWN;
        Case_ClientMsgLidState(case_td->lid_state);
    }
}

/* \brief Case message handler.
 */
static void case_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case MESSAGE_CHARGERCOMMS_IND:
            Case_CommsHandleMessageChargerCommsInd((const MessageChargerCommsInd*)message);
            break;

        case MESSAGE_CHARGERCOMMS_STATUS:
            Case_CommsHandleMessageChargerCommsStatus((const MessageChargerCommsStatus*)message);
            break;

        case PHY_STATE_CHANGED_IND:
            Case_HandlePhyStateChangedInd((const PHY_STATE_CHANGED_IND_T*)message);
            break;

        default:
            DEBUG_LOG_WARN("case_HandleMessage. Unhandled message MESSAGE:0x%x",id);
            break;
    }
}

/*! \brief Initialise the Case domain component.
*/
bool Case_Init(Task init_task)
{
    case_state_t* case_td = Case_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("Case_Init");
    
    /* initialise domain state */
    memset(case_td, 0, sizeof(case_state_t));
    case_td->task.handler = case_HandleMessage;
    case_td->lid_state = CASE_LID_STATE_UNKNOWN;
    case_td->case_battery_state = BATTERY_STATUS_UNKNOWN;
    case_td->peer_battery_state = BATTERY_STATUS_UNKNOWN;
    /* setup client task list */
    TaskList_InitialiseWithCapacity(Case_GetStateClientTasks(), STATE_CLIENTS_TASK_LIST_INIT_CAPACITY);

    /* register for phy state notifications */
    appPhyStateRegisterClient(Case_GetTask());

    Case_CommsInit();

    /* initialisation completed already, so indicate done */
    return TRUE;
}

/*! \brief Register client task to receive Case state messages.
*/
void Case_RegisterStateClient(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("Case_RegisterStateClient client task 0x%x", client_task);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(Case_GetStateClientTasks()), client_task);
}

/*! \brief Unregister client task to stop receiving Case state messages.
*/
void Case_UnregisterStateClient(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("Case_UnregisterStateClient client task 0x%x", client_task);
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(Case_GetStateClientTasks()), client_task);
}

/*! \brief Get the current state of the case lid.
*/
case_lid_state_t Case_GetLidState(void)
{
    DEBUG_LOG_VERBOSE("Case_GetLidState enum:case_lid_state_t:%d", Case_GetTaskData()->lid_state);
    return Case_GetTaskData()->lid_state;
}

/*! \brief Get the battery level of the case.
*/
uint8 Case_GetCaseBatteryState(void)
{
    return Case_GetTaskData()->case_battery_state;
}

/*! \brief Get the battery level of the peer earbud.
*/
uint8 Case_GetPeerBatteryState(void)
{
    return Case_GetTaskData()->peer_battery_state;
}

/*! \brief Determine if the case has the charger connected.
*/
bool Case_IsCaseChargerConnected(void)
{
    return Case_GetTaskData()->case_charger_connected;
}

/*! \brief Handle new lid state event from the case.
*/
void Case_LidEvent(case_lid_state_t new_lid_state)
{
    case_state_t* case_td = Case_GetTaskData();

    DEBUG_LOG_INFO("Case_LidEvent case lid state %u", new_lid_state);

    /* if valid, save last known state and notify clients */
    if (   (new_lid_state >= CASE_LID_STATE_CLOSED)
        && (new_lid_state <= CASE_LID_STATE_UNKNOWN))
    {
        /* only update and notify clients if state has changed */
        if (new_lid_state != case_td->lid_state)
        {
            case_td->lid_state = new_lid_state;
            Case_ClientMsgLidState(case_td->lid_state);
        }
    }
    else
    {
        DEBUG_LOG_WARN("Case_LidEvent invalid state %d", new_lid_state);
    }
}

/*! \brief Handle new power state message from the case.
*/
void Case_PowerEvent(uint8 case_battery_state, 
                     uint8 peer_battery_state, uint8 local_battery_state,
                     bool case_charger_connected)
{
    case_state_t* case_td = Case_GetTaskData();

    DEBUG_LOG_INFO("Case_PowerEvent Case [%d%% Chg:%d ChgConn:%d] Peer [%d%% Chg:%d] Local [%d%% Chg:%d]",
                    BATTERY_STATE_PERCENTAGE(case_battery_state),
                    BATTERY_STATE_IS_CHARGING(case_battery_state),
                    case_charger_connected,
                    BATTERY_STATE_PERCENTAGE(peer_battery_state),
                    BATTERY_STATE_IS_CHARGING(peer_battery_state),
                    BATTERY_STATE_PERCENTAGE(local_battery_state),
                    BATTERY_STATE_IS_CHARGING(local_battery_state));

    /* if valid, save last known state and notify clients 
       don't save local battery state, we always get latest */
    if (   case_BatteryStateIsValid(case_battery_state)
        && case_BatteryStateIsValid(peer_battery_state)
        && case_BatteryStateIsValid(local_battery_state))
    {
        case_td->case_battery_state = case_battery_state;
        case_td->peer_battery_state = peer_battery_state;
        case_td->case_charger_connected = case_charger_connected;

        Case_ClientMsgPowerState(case_td->case_battery_state,
                                 case_td->peer_battery_state, local_battery_state,
                                 case_td->case_charger_connected);
    }
    else
    {
        DEBUG_LOG_WARN("Case_PowerEvent invalid battery state");
    }
}

#endif /* INCLUDE_CASE */
/*! @} End of group documentation */
