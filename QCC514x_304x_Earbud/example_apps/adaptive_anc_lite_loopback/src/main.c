/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version
\file       main.c
\brief      Main application task
*/

#include <os.h>
#include <panic.h>
#include <vmal.h>
#include <microphones_config.h>
#include <anc_loopback_test.h>
#include <app_task.h>

/*! Application data structure */
appTaskData globalApp;

static void appHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case APP_SETUP_ANC:
            Ancloopback_Setup();
        break;

        case APP_ENABLE_ADAPTIVITY:
            Ancloopback_EnableAdaptivity();
        break;

        case APP_DISABLE_ADAPTIVITY:
            Ancloopback_DisableAdaptivity();
        break;

        case APP_RESTART_ADAPTIVITY:
            Ancloopback_RestartAdaptivity();
        break;

        default:
        break;

    }

}

int main(void)
{
    OsInit();

    /* Set up task handlers */
    appGetApp()->task.handler = appHandleMessage;

    MessageSend(appGetAppTask(),APP_SETUP_ANC,NULL);

    /* Start the message scheduler loop */
    MessageLoop();

    /* We should never get here, keep compiler happy */
    return 0;
}

