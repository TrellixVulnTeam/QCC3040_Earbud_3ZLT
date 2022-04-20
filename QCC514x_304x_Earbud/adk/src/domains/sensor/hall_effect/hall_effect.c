/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Support for the hall effect sensor
*/

#ifdef INCLUDE_HALL_EFFECT_SENSOR
#include <pio.h>
#include <pio_common.h>
#include <pio_monitor.h>
#include <stdlib.h>
#include <panic.h>
#include <logging.h>

#include "adk_log.h"
#include "hall_effect.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(hall_effect_messages_t)

#ifndef HOSTED_TEST_ENVIRONMENT
/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(HALL_EFFECT, HALL_EFFECT_MESSAGE_END)

#endif

/* Task information for hall effect sensor */
hallEffectTaskData app_hall_effect;


/*! Get the client message based on whether PIO is set

    The sensor signals magnet by setting the masked PIO low. */
static MessageId hallEffectSensor_MessageId(unsigned masked_bits)
{
    return masked_bits ? HALL_EFFECT_MESSAGE_MAGNET_NOT_PRESENT
                       : HALL_EFFECT_MESSAGE_MAGNET_PRESENT;
}


/*! \brief Handle the hall effect sensor interrupt */
static void hallEffectSensor_MessageHandler(Task task, MessageId id, Message msg)
{
    hallEffectTaskData *hall_effect = (hallEffectTaskData *) task;

    switch(id)
    {
        case MESSAGE_PIO_CHANGED:
            {
               /* When hall effect sensor is connected to hall_effect_sensor_pio
                    not around a magnet (meaning out of case), the pio will be high
                    covered by a magnet (meaning in case), the pio will be low
                */
                const MessagePioChanged *mpc = (const MessagePioChanged *)msg;
                bool pio_set;
                task_list_t *list = TaskList_GetFlexibleBaseTaskList((task_list_flexible_t *)&hall_effect->client_tasks);

                if (PioMonitorIsPioInMessage(mpc, hall_effect->hall_effect_sensor_pio, &pio_set))
                {
                    DEBUG_LOG_VERBOSE("hallEffectSensor_MessageHandler Magnet:%d", !pio_set);

                    TaskList_MessageSendId(list, hallEffectSensor_MessageId(pio_set));
                }
            }
            break;

        default:
            break;
    }
}

/*! \brief Enable the hall effec sensor */
static void hallEffectSensor_Enable(uint8 pio)
{
    uint16 bank;
    uint32 mask;

    /* Setup Interrupt as input with weak pull up */
    bank = PioCommonPioBank(pio);
    mask = PioCommonPioMask(pio);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));

}

/*! \brief Disable the hall effect sensor */
static void hallEffectSensor_Disable(uint8 pio)
{
    uint16 bank;
    uint32 mask;

    /* Disable interrupt and set weak pull down */
    bank = PioCommonPioBank(pio);
    mask = PioCommonPioBank(pio);
    PanicNotZero(PioSet32Bank(bank, mask, 0));
}

bool HallEffectSensor_ClientRegister(Task task)
{
    hallEffectTaskData *hall_effect = &app_hall_effect;

    hall_effect->hall_effect_sensor_pio = HALL_EFFECT_SENSOR_PIO;
    hallEffectSensor_Enable(hall_effect->hall_effect_sensor_pio);

    /* Register for interrupt events */
    hall_effect->task.handler = hallEffectSensor_MessageHandler;
    PioMonitorRegisterTask(&hall_effect->task, hall_effect->hall_effect_sensor_pio);

    /* Send a state message to the registering client: read the interrupt PIO state */
    uint16 bank = PioCommonPioBank(hall_effect->hall_effect_sensor_pio);
    uint32 mask = PioCommonPioMask(hall_effect->hall_effect_sensor_pio);
    MessageSend(task, hallEffectSensor_MessageId(PioGet32Bank(bank) & mask), NULL);

    return TaskList_AddTask(TaskList_GetFlexibleBaseTaskList((task_list_flexible_t *)(&hall_effect->client_tasks)), task);
}

void HallEffectSensor_ClientUnRegister(Task task)
{
    hallEffectTaskData *hall_effect = &app_hall_effect;
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList((task_list_flexible_t *)(&hall_effect->client_tasks)), task);

    /* Unregister for interrupt events */
    PioMonitorUnregisterTask(&hall_effect->task, hall_effect->hall_effect_sensor_pio);
    hallEffectSensor_Disable(hall_effect->hall_effect_sensor_pio);
}

#endif /* INCLUDE_HALL_EFFECT_SENSOR*/

