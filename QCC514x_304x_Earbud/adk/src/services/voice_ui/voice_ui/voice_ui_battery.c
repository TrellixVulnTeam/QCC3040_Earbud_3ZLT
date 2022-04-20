/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_battery.c
\brief      Implementation of the voice UI battery handling.
*/

#ifndef HAVE_NO_BATTERY

#include <logging.h>
#include "voice_ui_battery.h"
#include "voice_ui_container.h"
#include "state_proxy.h"
#include "state_of_charge.h"

typedef struct
{
    TaskData task;
} voiceui_battery_task;

static voiceui_battery_task voiceui_battery_data;

static void voiceui_BatteryMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch(id)
    {
#ifdef INCLUDE_TWS
        case STATE_PROXY_EVENT:
            {
                voice_ui_handle_t* handle = VoiceUi_GetActiveVa();
                if (handle && handle->voice_assistant->BatteryUpdate)
                {
                    handle->voice_assistant->BatteryUpdate(Soc_GetBatterySoc());
                }
            }
            break;
#else
        case SOC_UPDATE_IND:
        {
            const MESSAGE_SOC_UPDATE_T *battery_level = message;
            voice_ui_handle_t* handle = VoiceUi_GetActiveVa();
            if (handle && handle->voice_assistant->BatteryUpdate)
            {
                handle->voice_assistant->BatteryUpdate(battery_level->percent);
            }
            break;
        }
#endif /* INCLUDE_TWS */
        default:
            break;
    }
}

void VoiceUi_BatteryInit(void)
{
    DEBUG_LOG("VoiceUi_BatteryInit");
    voiceui_battery_data.task.handler = voiceui_BatteryMessageHandler;

#ifdef INCLUDE_TWS
    StateProxy_EventRegisterClient(&voiceui_battery_data.task, state_proxy_event_type_battery_voltage);
#else
    soc_registration_form_t battery_registration_form;
    battery_registration_form.task =&voiceui_battery_data.task;
    battery_registration_form.hysteresis = 1;
    (void)Soc_Register(&battery_registration_form);
#endif /* INCLUDE_TWS */
}

#endif /* !HAVE_NO_BATTERY */
