/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       pio_proximity.c
\brief      Support for a PIO driven proximity sensor
*/

#ifdef INCLUDE_PROXIMITY
#ifdef HAVE_PIO_PROXIMITY
#include <panic.h>
#include <pio.h>
#include <pio_monitor.h>
#include <pio_common.h>
#include <stdlib.h>


#include "adk_log.h"
#include "proximity.h"
#include "proximity_config.h"
#include "pio_proximity.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(proximity_messages)

/*! \brief The threshold low and high should be changed based on the enclosure for the earbud.
*/
const struct __proximity_config proximity_config = {
    .pios = {
        /* The PROXIMITY_PIO definitions are defined in the platform x2p file */
        .on = PROXIMITY_PIO_ON,
        .interrupt = PROXIMITY_PIO_INT,
    },
};

/*!< Task information for proximity sensor */
proximityTaskData app_proximity;

/*! \brief Handle the proximity interrupt */
static void pioProximityInterruptHandler(Task task, MessageId id, Message msg)
{
    proximityTaskData *proximity = (proximityTaskData *) task;

    switch(id)
    {
        case MESSAGE_PIO_CHANGED:
        {
            const MessagePioChanged *mpc = (const MessagePioChanged *)msg;
            const proximityConfig *config = proximity->config;
            bool pio_set;

            if (PioMonitorIsPioInMessage(mpc, config->pios.interrupt, &pio_set))
            {
                /* High is in-ear, Low is out-of-ear */
                if (!pio_set)
                {
                    DEBUG_LOG_VERBOSE("pioProximityInterruptHandler not in proximity");
                    TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY);
                }
                else
                {
                    DEBUG_LOG_VERBOSE("pioProximityInterruptHandler in proximity");
                    TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_IN_PROXIMITY);
                }
            }
        }
        break; 

        case PIO_MONITOR_ENABLE_CFM:
        {
            DEBUG_LOG_VERBOSE("pioProximityInterruptHandler Received event: PIO_MONITOR_ENABLE_CFM");
        }
        break;

        default:
        break;
    }
}

/*! \brief Enable the pio proximity sensor */
static void pioProximity_Enable(const proximityConfig *config)
{
    uint16 bank;
    uint32 mask;

    DEBUG_LOG_VERBOSE("pioProximity_Enable %d", config->pios.interrupt);

    if (config->pios.on != PROXIMITY_ON_PIO_UNUSED)
    {
        /* Setup power PIO then power-on the sensor */
        bank = PioCommonPioBank(config->pios.on);
        mask = PioCommonPioMask(config->pios.on);
        PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
        PanicNotZero(PioSetDir32Bank(bank, mask, mask));
        PanicNotZero(PioSet32Bank(bank, mask, mask));
    }

    /* Setup Interrupt as input with weak pull up */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PioSetDeepSleepEitherLevelBank(bank, mask, mask);
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
}

/*! \brief Disable the pio proximity sensor */
static void pioProximity_Disable(const proximityConfig *config)
{
    DEBUG_LOG_VERBOSE("pioProximity_Disable %d", config->pios.interrupt);

    if (config->pios.on != PROXIMITY_ON_PIO_UNUSED)
    {
        /* Power off the proximity sensor */
        PanicNotZero(PioSet32Bank(PioCommonPioBank(config->pios.on),
                                  PioCommonPioMask(config->pios.on),
                                  0));
    }
}

bool appProximityClientRegister(Task task)
{
    proximityTaskData *prox = ProximityGetTaskData();

    const proximityConfig *config = appConfigProximity();

    DEBUG_LOG_VERBOSE("appProximityClientRegister");

    if (NULL == prox->clients)
    {
        prox->config = config;
        prox->state = PanicUnlessNew(proximityState);
        prox->state->proximity = proximity_state_unknown;
        prox->clients = TaskList_Create();

        DEBUG_LOG_VERBOSE("appProximityClientRegister %d", config->pios.interrupt);
		
        /* Set the handler and PIO for sensor interrupt events. */
        /* Sensor interrupts are not configured until #PIO_MONITOR_ENABLE_CFM */
        prox->task.handler = pioProximityInterruptHandler;
        pioProximity_Enable(config);
        PioMonitorRegisterTask(&prox->task, config->pios.interrupt);
    }

    /* Send initial message to client by reading from sensor. Post app init, the state is updated via interrupts */
    if(prox->state->proximity == proximity_state_unknown)
    {        
        if(PioCommonGetPio(config->pios.interrupt))
        {
            DEBUG_LOG_VERBOSE("appProximityClientRegister pioProximity Initial State: IN PROXIMITY");
            prox->state->proximity = proximity_state_in_proximity;
        }
        else
        {
            DEBUG_LOG_VERBOSE("appProximityClientRegister pioProximity Initial State: NOT IN PROXIMITY");
            prox->state->proximity = proximity_state_not_in_proximity;
        }
    }

    /* Must now be either "in" or "not_in state" */
    if (prox->state->proximity == proximity_state_in_proximity)
    {
        MessageSend(task, PROXIMITY_MESSAGE_IN_PROXIMITY, NULL);
    }
    else
    {
        MessageSend(task, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY, NULL);
    }

    return TaskList_AddTask(prox->clients, task);
}

void appProximityClientUnregister(Task task)
{
    proximityTaskData *prox = ProximityGetTaskData();
    TaskList_RemoveTask(prox->clients, task);
    if (0 == TaskList_Size(prox->clients))
    {
        TaskList_Destroy(prox->clients);
        prox->clients = NULL;
        free(prox->state);
        prox->state = NULL;		        

        /* Unregister for interrupt events */
        pioProximity_Disable(prox->config);
        PioMonitorUnregisterTask(&prox->task, prox->config->pios.interrupt);
    }
}

/* This function is to switch on/off sensor for power saving*/
void appProximityEnableSensor(Task task, bool enable)
{
    UNUSED(task);
    UNUSED(enable);
}
#endif /* HAVE_PIO_PROXIMITY */
#endif /* INCLUDE_PROXIMITY */
