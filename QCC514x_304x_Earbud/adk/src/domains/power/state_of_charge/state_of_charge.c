/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_of_charge.c
\brief      
*/

#ifndef HAVE_NO_BATTERY

#define DEBUG_LOG_MODULE_NAME state_of_charge
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <ps.h>
#include <panic.h>
#include <pio.h>
#include <vm.h>
#include <hydra_macros.h>
#include <limits.h>
#include <task_list.h>
#include <logging.h>

#include "adk_log.h"
#include "state_of_charge_private.h"
#include "battery_monitor.h"
#include "charger_monitor.h"
#include "battery_monitor_config.h"
#include "unexpected_message.h"

#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(soc_messages)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(STATE_OF_CHARGE, STATE_OF_CHARGE_MESSAGE_END)

typedef struct
{
    const soc_lookup_t* soc_config_table;
    unsigned soc_config_size;
}soc_ctx_t;

static soc_ctx_t soc_ctx;

soc_data_t app_battery_charge;

/* TRUE if the current value is less than the threshold taking into account hysteresis */
static bool soc_ltThreshold(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return current < (threshold - hysteresis);
}

/* TRUE if the current value is greater than the threshold taking into account hysteresis. */
static bool soc_gtThreshold(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return current > (threshold + hysteresis);
}

/* TRUE if the current value is outside the threshold taking into account hysteresis */
static bool soc_thresholdExceeded(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return soc_ltThreshold(current, threshold, hysteresis) ||
           soc_gtThreshold(current, threshold, hysteresis);
}

/*! 
    \brief Send battery state of charge update message to all registered clients.
    \param battery_region pointer to battery_region_data_t structure
*/
static void soc_ServiceClients(soc_data_t *battery_charge)
{
    socRegisteredClient *client = NULL;
    for (client = battery_charge->client_list; client != NULL; client = client->next)
    {
        uint16 hysteresis = client->form.hysteresis;
        
        uint8 percent = (uint8)battery_charge->state_of_charge;
        if (soc_thresholdExceeded(percent, client->percent, hysteresis))
        {
            MESSAGE_MAKE(msg, MESSAGE_SOC_UPDATE_T);
            msg->percent = percent;
            client->percent = percent;
            MessageSend(client->form.task, SOC_UPDATE_IND, msg);
        }
    }
}

/*! Add a client to the list of clients */
static bool soc_ClientAdd(soc_data_t *battery_charge, soc_registration_form_t *form)
{
    socRegisteredClient *new = calloc(1, sizeof(*new));
    if (new)
    {
        new->form = *form;
        new->next = battery_charge->client_list;
        battery_charge->client_list = new;
        return TRUE;
    }
    return FALSE;
}

/*! Remove a client from the list of clients */
static void soc_ClientRemove(soc_data_t *battery_charge, Task task)
{
    socRegisteredClient **head;
    for (head = &battery_charge->client_list; *head != NULL; head = &(*head)->next)
    {
        if ((*head)->form.task == task)
        {
            socRegisteredClient *toremove = *head;
            *head = (*head)->next;
            free(toremove);
            break;
        }
    }
}

static void soc_StateOfChargeUpdate(soc_data_t *battery_charge_state, uint16 battery_voltage)
{
    uint32 prev_index, index;
    uint16 prev_mv, this_mv;

    if (battery_voltage < appConfigBatteryVoltageCritical())
        battery_voltage = appConfigBatteryVoltageCritical();
    else if (battery_voltage > appConfigBatteryFullyCharged())
        battery_voltage = appConfigBatteryFullyCharged();

    prev_index = battery_charge_state->config_index;
    prev_mv = soc_ctx.soc_config_table[prev_index].voltage;

    if (battery_voltage >= prev_mv)
    {
        /* SoC increase, search forward through higher voltages / higher SoC */
        for (index = prev_index + 1; index < soc_ctx.soc_config_size; index++)
        {
            this_mv = prev_mv;
            prev_mv = soc_ctx.soc_config_table[index].voltage;

            DEBUG_LOG("soc_StateOfChargeUpdate, search forwards index=%d, voltage=%d, this_mv=%d, prev_mv=%d",
                       index, battery_voltage, this_mv, prev_mv);

            if (battery_voltage < prev_mv && 
                battery_voltage >= this_mv)
            {
                battery_charge_state->config_index = index - 1;
                return;
            }            
        }
        battery_charge_state->config_index = soc_ctx.soc_config_size - 1;
    }
    else /* voltage < prev_mv */
    {
        /* SoC decrease, search backwards through lower voltages / lower soc */
        for (index = prev_index; index > 0; index--)
        {
            this_mv = prev_mv;
            prev_mv = soc_ctx.soc_config_table[index-1].voltage;

            DEBUG_LOG("soc_StateOfChargeUpdate, search backwards index=%d, voltage=%d, this_mv=%d",
                       index, battery_voltage, this_mv);

            if (battery_voltage < this_mv && 
                battery_voltage >= prev_mv)
            {
                battery_charge_state->config_index = index - 1;
                return;
            }            
        }
        battery_charge_state->config_index = SOC_MIN_INDEX;
    }
}

/*! \brief Handle temperature messages */
static void soc_HandleMessage(Task task, MessageId id, Message message)
{
    soc_data_t *soc_data = GetBatteryChargeData();
    uint16 battery_voltage;

    UNUSED(task);
    
    switch (id)
    {
        case MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE:
            battery_voltage = ((const MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T*)message)->voltage_mv;
            DEBUG_LOG("soc_HandleMessage, battery voltage update message received. voltage: %d",
                    battery_voltage);
            soc_StateOfChargeUpdate(soc_data, battery_voltage);
            break;
                
        case CHARGER_MESSAGE_ATTACHED:
            soc_data->charger_connected = TRUE;
            DEBUG_LOG("soc_HandleMessage, charger attached message received.");
            break;

        case CHARGER_MESSAGE_DETACHED:
            soc_data->charger_connected = FALSE;
            DEBUG_LOG("soc_HandleMessage, charger detached message received.");
            break;

        default:            
            break;
    }

    if (soc_data->charger_connected)
    {
        if (soc_ctx.soc_config_table[soc_data->config_index].percentage != (uint8)soc_data->state_of_charge)
        {
            soc_data->state_of_charge = soc_ctx.soc_config_table[soc_data->config_index].percentage;
            DEBUG_LOG_INFO("soc_StateOfChargeUpdate, charging %u%%", soc_data->state_of_charge);
            if (!PsStore(BATTERY_STATE_OF_CHARGE_KEY, &soc_data->state_of_charge, 1))
            {
                DEBUG_LOG_WARN("soc_StateOfChargeUpdate, PS Store update for Battery SoC key failed.");
            }
            soc_ServiceClients(soc_data);
        }
    }
    else
    {/* disconnected state so compare to last value. any value which is higher than last value to be ignored. */
        if (soc_ctx.soc_config_table[soc_data->config_index].percentage < (uint8)soc_data->state_of_charge)
        {
            soc_data->state_of_charge = soc_ctx.soc_config_table[soc_data->config_index].percentage;
            DEBUG_LOG_INFO("soc_StateOfChargeUpdate, discharging %u%%", soc_data->state_of_charge);
            /* in disconnected state this value seems ok as reduction or being same expected */            
            if (!PsStore(BATTERY_STATE_OF_CHARGE_KEY, &soc_data->state_of_charge, 1))
            {
                DEBUG_LOG_WARN("soc_StateOfChargeUpdate, PS Store update for Battery SoC key failed.");
            }
            soc_ServiceClients(soc_data);
        }
        else
        {
            DEBUG_LOG_INFO("soc_StateOfChargeUpdate, In charger disconnect state "
                    "battery charge going up.");
        }
    }
}

static uint8 soc_getIndex(uint16 battery_voltage)
{
    uint8 i;
    uint16 prev_mv, this_mv;

    prev_mv = soc_ctx.soc_config_table[SOC_MIN_INDEX].voltage;

    for (i = 1; i < soc_ctx.soc_config_size; i++)
    {
        this_mv = prev_mv;
        prev_mv = soc_ctx.soc_config_table[i].voltage;

         if (battery_voltage < prev_mv && 
             battery_voltage >= this_mv)
         {
             return (i - 1);
         }
    }
    return (soc_ctx.soc_config_size - 1);
}

void Soc_Init(void)
{
    soc_data_t *soc_data = GetBatteryChargeData();
    batteryRegistrationForm form;
    uint16 soc;

    DEBUG_LOG("SoC_Init");
    memset(soc_data, 0, sizeof(*soc_data));

    /* Set up task handler */
    soc_data->task.handler = soc_HandleMessage;
    
    /* read PS Key */
    if(PsRetrieve(BATTERY_STATE_OF_CHARGE_KEY, &soc, 1) != 0)
    {
        DEBUG_LOG("SoC_Init: PSRetrieve returned last value of Battery Charge: %u%%", soc);
        soc_data->state_of_charge = soc; 
    }
    else
    {
        DEBUG_LOG("SoC_Init: PSRetrieve Failed");
		
		/* If SOC is not recorded previously, then the default value of 100
		   is required in order to get the correct values of SOC when the
		   battery indication is handled. */
        soc_data->state_of_charge = 100;
    }
    
    soc_data->charger_connected = Charger_IsConnected();

    /* register for battery voltage */
    form.task = SoC_GetTask();
    form.representation = battery_level_repres_voltage;
    form.hysteresis = 6;
    (void)appBatteryRegister(&form);

    /* register for conected state change update */
    Charger_ClientRegister(SoC_GetTask());
}
 
bool Soc_Register(soc_registration_form_t *client)
{
    soc_data_t *soc_data = GetBatteryChargeData();
    if (soc_ClientAdd(soc_data, client))
    {
        soc_ServiceClients(soc_data);
        return TRUE;
    }
    return FALSE;
}
 
void Soc_Unregister(Task task)
{
    soc_data_t *soc_data = GetBatteryChargeData();
    soc_ClientRemove(soc_data, task);
}

uint8 Soc_GetBatterySoc(void)
{
    soc_data_t *soc_data = GetBatteryChargeData();
    return (uint8)soc_data->state_of_charge;
}

void Soc_SetConfigurationTable(const soc_lookup_t* config_table,
                              unsigned config_size)
{
    DEBUG_LOG("SoC_SetConfigurationTable, set voltage->percentage lookup configuration table");

    soc_ctx.soc_config_table = config_table;
    soc_ctx.soc_config_size = config_size;
}

uint8 Soc_ConvertLevelToPercentage(uint16 battery_level)
{
    uint8 index;
    
    if (battery_level < appConfigBatteryVoltageCritical())
    {
        battery_level = appConfigBatteryVoltageCritical();
    }
    else if (battery_level > appConfigBatteryFullyCharged())
    {
        battery_level = appConfigBatteryFullyCharged();
    }
    index = soc_getIndex(battery_level);

    return soc_ctx.soc_config_table[index].percentage;
}

#endif /* !HAVE_NO_BATTERY */
