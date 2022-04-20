/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Reports battery level over AT+BIEV command.

*/

#ifndef HAVE_NO_BATTERY

#include "hfp_profile_battery_level.h"

#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_states.h"
#include "hfp_profile_instance.h"
#include <hfp_profile.h>
#include <state_of_charge.h>
#include <bt_device.h>
#ifdef INCLUDE_TWS
#include <state_proxy.h>
#endif

#include <hfp.h>

#include <logging.h>


#ifdef INCLUDE_TWS
/*! \brief Determines if only local battery level should be taken into consideration */
static uint8 use_local_only;

#else
/*! \brief Configuration of battery monitor notifications. */
static const soc_registration_form_t hfp_battery_form = {
        .task = (Task)&hfp_profile_task_data.task,
        .hysteresis = 1
};
#endif

/*! \brief Variable used by tests to check what has been sent to hfp library*/
static uint8 last_biev_battery_level;

void HfpProfile_BatteryLevelInit(void)
{
#ifdef INCLUDE_TWS
    ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, (Task)&hfp_profile_task_data.task);
    StateProxy_EventRegisterClient((Task)&hfp_profile_task_data.task, state_proxy_event_type_battery_voltage);
    use_local_only = TRUE;
#else
    (void)Soc_Register((soc_registration_form_t *)&hfp_battery_form);
#endif

    last_biev_battery_level = 0;
}

/*! \brief Determine battery level to be sent to a remote device.

    For standalone device it just returns local battery level.
    For peer devices:
    If peer battery level is invalid then it returns local battery level.
    Otherwise it returns lower battery level out of two peers.

    \return Battery level in percent.
*/
inline static uint8 hfpProfile_GetBatteryLevel(void)
{
#ifdef INCLUDE_TWS
    uint16 battery_level_1;
    uint16 battery_level_2;
    uint16 lower_battery_level;

    StateProxy_GetLocalAndRemoteBatteryLevels(&battery_level_1, &battery_level_2);

    if(use_local_only)
    {
        lower_battery_level = battery_level_1;
    }
    else
    {
        lower_battery_level = battery_level_1 < battery_level_2 ? battery_level_1 : battery_level_2;
    }

    DEBUG_LOG_VERBOSE("hfpProfile_GetBatteryLevel %d mV, %d mV -> %d mV", battery_level_1, battery_level_2, lower_battery_level);

    last_biev_battery_level = Soc_ConvertLevelToPercentage(lower_battery_level);
#else
    last_biev_battery_level =  Soc_GetBatterySoc();
#endif

    return last_biev_battery_level;
}

/*! \brief Send AT+BIEV battery update to specific instance

    \param instance HFP instance to which AT+BIEV should be sent.
*/
static void hfpProfile_SendBievCommandToInstance(hfpInstanceTaskData * instance)
{
    uint8 percent = hfpProfile_GetBatteryLevel();
    hfp_link_priority link_priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
    DEBUG_LOG_VERBOSE("hfpProfile_SendBievCommandToInstance sending %d percent to link enum:hfp_link_priority:0x%x",
            percent, link_priority);
    HfpBievIndStatusRequest(link_priority, hf_battery_level, percent);
}

/*! \brief Send AT+BIEV battery update to all relevant devices, when HFP state is correct. */
inline static void hfpProfile_SendBievCommand(void)
{
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    DEBUG_LOG_VERBOSE("hfpProfile_SendBievCommand");

    for_all_hfp_instances(instance, &iterator)
    {
        hfpState state = appHfpGetState(instance);
        
        DEBUG_LOG_VERBOSE("hfpProfile_SendBievCommand instance %d lap 0x%x state enum:hfpState:%d hf_ind enum:hfp_indicators_assigned_id:%d",
                iterator.index, instance->ag_bd_addr.lap, state, instance->bitfields.hf_indicator_assigned_num);
        if(instance->bitfields.hf_indicator_assigned_num == hf_battery_level)
        {
            if(HfpProfile_StateIsSlcConnected(state))
            {
                hfpProfile_SendBievCommandToInstance(instance);
            }
        }
    }
}

#ifdef INCLUDE_TWS
/*! \brief Determine if peer battery is valid

    \return TRUE if peer battery is valid.
*/
inline static uint8 hfpProfile_IsPeerBatteryStateValid(void)
{
    uint8 is_valid;
    battery_region_state_t local_battery_state;
    battery_region_state_t peer_battery_state;

    StateProxy_GetLocalAndRemoteBatteryStates(&local_battery_state, &peer_battery_state);

    is_valid = peer_battery_state == battery_region_unknown ? FALSE : TRUE;

    DEBUG_LOG_VERBOSE("hfpProfile_IsPeerBatteryStateValid %d", is_valid);

    return is_valid;
}
#endif

void HfpProfile_HandleBatteryMessages(MessageId id, Message message)
{
    uint8 update_battery = TRUE;
    UNUSED(message);

    switch(id)
    {
#ifdef INCLUDE_TWS
        case STATE_PROXY_EVENT:
            {
                STATE_PROXY_EVENT_T *event = (STATE_PROXY_EVENT_T *)message;
                DEBUG_LOG_VERBOSE("HfpProfile_HandleBatteryMessages STATE_PROXY_EVENT enum:state_proxy_event_type:%d", event->type);
                if(event->type == state_proxy_event_type_battery_voltage)
                {
                    use_local_only = !hfpProfile_IsPeerBatteryStateValid();
                }
            }
            break;

        case CON_MANAGER_TP_DISCONNECT_IND:
            {
                CON_MANAGER_TP_DISCONNECT_IND_T *ind = (CON_MANAGER_TP_DISCONNECT_IND_T *)message;
                if(appDeviceIsPeer(&ind->tpaddr.taddr.addr))
                {
                    DEBUG_LOG_VERBOSE("HfpProfile_HandleBatteryMessages CON_MANAGER_TP_DISCONNECT_IND received and it is peer");
                    use_local_only = TRUE;
                }
            }
            break;
#else
        case SOC_UPDATE_IND:
            DEBUG_LOG_VERBOSE("HfpProfile_HandleBatteryMessages MESSAGE_SOC_UPDATE");
            break;
#endif
        default:
            update_battery = FALSE;
            break;
    }

    if(update_battery)
    {
        hfpProfile_SendBievCommand();
    }
}


void HfpProfile_EnableBatteryHfInd(hfpInstanceTaskData *instance, uint8 indicator_is_enabled)
{
    DEBUG_LOG_VERBOSE("HfpProfile_HandleBatteryHfInd 0x%x", indicator_is_enabled);

    if(indicator_is_enabled)
    {
        instance->bitfields.hf_indicator_assigned_num |= hf_battery_level;
        hfpProfile_SendBievCommandToInstance(instance);
    }
    else
    {
        instance->bitfields.hf_indicator_assigned_num &= ~hf_battery_level;
    }
}


#endif /* !HAVE_NO_BATTERY */
