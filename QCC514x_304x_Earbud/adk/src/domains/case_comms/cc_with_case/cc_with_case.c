/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case domain, managing interactions with a product case.
*/
/*! \addtogroup case_comms
@{
*/

#include "cc_with_case.h"
#include "cc_with_case_private.h"
#include "cc_with_case_state_client_msgs.h"
#include "cc_protocol.h"
#include "cc_case_channel.h"

#include <phy_state.h>
#include <charger_monitor.h>
#include <power_manager_action.h>

#include <task_list.h>
#include <logging.h>

#include <message.h>

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_EARBUDS

#include <peer_pairing.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(case_message_t)

/*! State for case comms with the case. */
cc_with_case_t cc_with_case;

/* \brief Utility function to validate a battery state value.
    \param battery_state Battery and charging state in combined format.
    \return TRUE state is valid, FALSE otherwise.
*/
static bool ccWithCase_BatteryStateIsValid(uint8 battery_state)
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
static void ccWithCase_HandlePhyStateChangedInd(const PHY_STATE_CHANGED_IND_T *ind)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();

    DEBUG_LOG_INFO("ccWithCase_HandlePhyStateChangedInd state enum:phyState:%d enum:phy_state_event:%d", ind->new_state, ind->event);

    /* if we just came out of the case, we can no longer trust the last lid_state */
    if (ind->event == phy_state_event_out_of_case)
    {
        case_td->lid_state = CASE_LID_STATE_UNKNOWN;
        CcWithCase_ClientMsgLidState(case_td->lid_state);

        /* coming out of the case cancels any pending shipping mode */
        if (case_td->shipping_mode_pending)
        {
            case_td->shipping_mode_pending = FALSE;
            Charger_ClientUnregister(CcWithCase_GetTask());
        }
    }
}

static void ccWithCase_HandleChargerDetached(void)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();
    
    DEBUG_LOG_INFO("ccWithCase_HandleChargerDetached shipping mode pending:%d", case_td->shipping_mode_pending);

    if (case_td->shipping_mode_pending)
    {
        appPowerDoPowerOff();
    }
}

/* \brief Case message handler.
 */
static void ccWithCase_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case PHY_STATE_CHANGED_IND:
            ccWithCase_HandlePhyStateChangedInd((const PHY_STATE_CHANGED_IND_T*)message);
            break;

        case CHARGER_MESSAGE_DETACHED:
            ccWithCase_HandleChargerDetached();
            break;

        case CHARGER_MESSAGE_ATTACHED:
            /* fall-through */
        case CHARGER_MESSAGE_COMPLETED:
            /* fall-through */
        case CHARGER_MESSAGE_CHARGING_OK:
            /* fall-through */
        case CHARGER_MESSAGE_CHARGING_LOW:
            /* fall-through */
        case CHARGER_MESSAGE_DISABLED:
            /* fall-through */
        case CHARGER_MESSAGE_ERROR:
            /* ignore these messages, only interested in detach
               to complete shipping mode */
            break;

        default:
            DEBUG_LOG_WARN("ccWithCase_HandleMessage. Unhandled message MESSAGE:0x%x",id);
            break;
    }
}

/*! \brief Initialise the Case domain component.
*/
bool CcWithCase_Init(Task init_task)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("CcWithCase_Init");
    
    /* initialise domain state */
    memset(case_td, 0, sizeof(cc_with_case_t));
    case_td->task.handler = ccWithCase_HandleMessage;
    case_td->lid_state = CASE_LID_STATE_UNKNOWN;
    case_td->case_battery_state = BATTERY_STATUS_UNKNOWN;
    case_td->peer_battery_state = BATTERY_STATUS_UNKNOWN;
    /* setup client task list */
    TaskList_InitialiseWithCapacity(CcWithCase_GetStateClientTasks(), STATE_CLIENTS_TASK_LIST_INIT_CAPACITY);

    /* register for phy state notifications */
    appPhyStateRegisterClient(CcWithCase_GetTask());

    /* initialise case comms protocol and transport and the case channel */
    CcProtocol_Init(CASECOMMS_MODE_EARBUD, CC_TRANSPORT);
    CcCaseChannel_Init();

    /* initialisation completed already, so indicate done */
    return TRUE;
}

/*! \brief Register client task to receive Case state messages.
*/
void CcWithCase_RegisterStateClient(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("CcWithCase_RegisterStateClient client task 0x%x", client_task);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(CcWithCase_GetStateClientTasks()), client_task);
}

/*! \brief Unregister client task to stop receiving Case state messages.
*/
void CcWithCase_UnregisterStateClient(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("CcWithCase_UnregisterStateClient client task 0x%x", client_task);
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(CcWithCase_GetStateClientTasks()), client_task);
}

/*! \brief Get the current state of the case lid.
*/
case_lid_state_t CcWithCase_GetLidState(void)
{
    DEBUG_LOG_VERBOSE("CcWithCase_GetLidState enum:case_lid_state_t:%d", CcWithCase_GetTaskData()->lid_state);
    return CcWithCase_GetTaskData()->lid_state;
}

/*! \brief Get the battery level of the case.
*/
uint8 CcWithCase_GetCaseBatteryState(void)
{
    return CcWithCase_GetTaskData()->case_battery_state;
}

/*! \brief Get the battery level of the peer earbud.
*/
uint8 CcWithCase_GetPeerBatteryState(void)
{
    return CcWithCase_GetTaskData()->peer_battery_state;
}

/*! \brief Determine if the case has the charger connected.
*/
bool CcWithCase_IsCaseChargerConnected(void)
{
    return CcWithCase_GetTaskData()->case_charger_connected;
}

/*! \brief Handle new lid state event from the case.
*/
void CcWithCase_LidEvent(case_lid_state_t new_lid_state)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();

    DEBUG_LOG_INFO("CcWithCase_LidEvent case lid state %u", new_lid_state);

    /* if valid, save last known state and notify clients */
    if (   (new_lid_state >= CASE_LID_STATE_CLOSED)
        && (new_lid_state <= CASE_LID_STATE_UNKNOWN))
    {
        /* only update and notify clients if state has changed */
        if (new_lid_state != case_td->lid_state)
        {
            case_td->lid_state = new_lid_state;
            CcWithCase_ClientMsgLidState(case_td->lid_state);
        }
    }
    else
    {
        DEBUG_LOG_WARN("CcWithCase_LidEvent invalid state %d", new_lid_state);
    }
}

/*! \brief Handle new power state message from the case.
*/
void CcWithCase_PowerEvent(uint8 case_battery_state, 
                     uint8 peer_battery_state, uint8 local_battery_state,
                     bool case_charger_connected)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();

    DEBUG_LOG_INFO("CcWithCase_PowerEvent Case [%d%% Chg:%d ChgConn:%d] Peer [%d%% Chg:%d] Local [%d%% Chg:%d]",
                    BATTERY_STATE_PERCENTAGE(case_battery_state),
                    BATTERY_STATE_IS_CHARGING(case_battery_state),
                    case_charger_connected,
                    BATTERY_STATE_PERCENTAGE(peer_battery_state),
                    BATTERY_STATE_IS_CHARGING(peer_battery_state),
                    BATTERY_STATE_PERCENTAGE(local_battery_state),
                    BATTERY_STATE_IS_CHARGING(local_battery_state));

    /* if valid, save last known state and notify clients 
       don't save local battery state, we always get latest */
    if (   ccWithCase_BatteryStateIsValid(case_battery_state)
        && ccWithCase_BatteryStateIsValid(peer_battery_state)
        && ccWithCase_BatteryStateIsValid(local_battery_state))
    {
        case_td->case_battery_state = case_battery_state;
        case_td->peer_battery_state = peer_battery_state;
        case_td->case_charger_connected = case_charger_connected;

        CcWithCase_ClientMsgPowerState(case_td->case_battery_state,
                                 case_td->peer_battery_state, local_battery_state,
                                 case_td->case_charger_connected);
    }
    else
    {
        DEBUG_LOG_WARN("CcWithCase_PowerEvent invalid battery state");
    }
}

void CcWithCase_PeerPairCmdRx(const bdaddr* peer_address)
{
    if (PeerPairing_PeerPairToAddress(CcWithCase_GetTask(), peer_address))
    {
        DEBUG_LOG_INFO("CcWithCase_PeerPairCmdRx 0x%06x 0x%x 0x%04x", peer_address->lap, peer_address->uap, peer_address->nap);
        CcCaseChannel_PeerPairCmdRespTx(TRUE);
    }
    else
    {
        CcCaseChannel_PeerPairCmdRespTx(FALSE);
    }
}

void CcWithCase_ShippingModeCmdRx(void)
{
    cc_with_case_t* case_td = CcWithCase_GetTaskData();

    /* invalidate any previous pending shipping mode, if another shipping
       mode command received before completing a previous one */
    if (case_td->shipping_mode_pending)
    {
        case_td->shipping_mode_pending = FALSE;
        Charger_ClientUnregister(CcWithCase_GetTask());
    }

    /* register to see charger disconnect event (VCHG removed by case), which will
       be the trigger to go to dormant, and check that charger is currently connected
       so that we will receive a CHARGER_MESSAGE_DETACHED message. */
    if (Charger_IsConnected() && Charger_ClientRegister(CcWithCase_GetTask()))
    {
        /* ok to confirm shipping mode request, set the flag to enter dormant
           when charger is detached */
        case_td->shipping_mode_pending = TRUE;
    }

    /* send back response that this device will enter shipping mode when VCHG is removed */
    CcCaseChannel_ShippingModeCmdRespTx(case_td->shipping_mode_pending);
}

#endif /* HAVE_CC_MODE_EARBUDS */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */
