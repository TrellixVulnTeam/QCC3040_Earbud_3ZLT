/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power manager integration with UI

*/

#include "power_manager_ui.h"
#include "power_manager.h"

#include "ui.h"

#include <panic.h>

const message_group_t power_ui_inputs[] = { UI_INPUTS_DEVICE_STATE_MESSAGE_GROUP };

/*! \brief provides power module current context to ui module

    \return     ui_provider_ctxt_t - current context of power module.
*/
static unsigned appPowerCurrentContext(void)
{
    power_provider_context_t context = BAD_CONTEXT;
    switch(app_power.state)
    {
    case POWER_STATE_INIT:
        break;
    case POWER_STATE_OK:
        context = context_power_on;
        break;
    case POWER_STATE_TERMINATING_CLIENTS_NOTIFIED:
    case POWER_STATE_TERMINATING_CLIENTS_RESPONDED:
        context = context_powering_off;
        break;
    case POWER_STATE_SOPORIFIC_CLIENTS_NOTIFIED:
    case POWER_STATE_SOPORIFIC_CLIENTS_RESPONDED:
        context = context_entering_sleep;
        break;
    default:
        break;
    }
    return (unsigned)context;

}

void appPowerRegisterWithUi(void)
{
    /* Register power module as ui provider*/
    Ui_RegisterUiProvider(ui_provider_power,appPowerCurrentContext);

    Ui_RegisterUiInputConsumer(PowerGetTask(), power_ui_inputs, ARRAY_DIM(power_ui_inputs));
}

static void powerManager_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == POWER_APP_MESSAGE_GROUP);
    appPowerClientRegister(task);
    appPowerClientAllowSleep(task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(POWER_APP, powerManager_RegisterMessageGroup, NULL);
