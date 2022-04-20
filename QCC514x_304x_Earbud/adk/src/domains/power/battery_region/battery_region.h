/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file        battery_region.h
\brief      Header file for Battery region
*/

#ifndef BATTERY_REGION_H_
#define BATTERY_REGION_H_

#include "domain_message.h"
#include <marshal.h>

/*! Operating region classification based on battery voltage */
typedef enum
{    
    NORMAL_REGION,    
    CRITICAL_REGION,    
    SAFETY_REGION
} charger_region_type_t;

extern bool charging_timer_timeout;
#define BatteryRegion_GetChargerTimerTimeoutValue() charging_timer_timeout

/*! Battery region change messages.  */
enum battery_region_messages
{
    /*! Message signalling the battery module initialisation is complete */
    MESSAGE_BATTERY_REGION_INIT_CFM = BATTERY_REGION_MESSAGE_BASE,
     /*! Message signalling the battery state has changed. */
    MESSAGE_BATTERY_REGION_UPDATE,

    /*! This must be the final message */
    MESSAGE_BATTERY_REGION_MESSAGE_END
};
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(BATTERY_REGION, MESSAGE_BATTERY_REGION_MESSAGE_END)

/*! battery states */
typedef enum
{
    battery_region_unknown,
    battery_region_unsafe,
    battery_region_critical,
    battery_region_ok,
} battery_region_state_t;
#define MARSHAL_TYPE_battery_region_state_t MARSHAL_TYPE_uint8

/*! Message #MESSAGE_BATTERY_LEVEL_UPDATE_STATE content. */
typedef struct
{
    /*! The updated battery region. */
    battery_region_state_t state;
} MESSAGE_BATTERY_REGION_UPDATE_STATE_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_MESSAGE_BATTERY_REGION_UPDATE_STATE_T;

typedef struct
{    
    /* Desired charge current in mA */    
    uint16 current;    

    /* Min and max voltage in mV */    
    uint16 voltage_min;    
    uint16 voltage_max;    
    /* Voltage hysteresis in mV */    
    uint8 voltage_hysteresis;    

    /* Min and max temperature in degrees C*/    
    int8 temp_min;    
    int8 temp_max;    
    /*Temperature hysteresis in degrees C */    
    uint8 temp_hysteresis;    

    charger_region_type_t region_type;
 
    /* timer restarted when charging region is entered. 
     * defined in minutes in range 1 to 1,080 minutes 
     */
    uint16 charging_timer;
} charge_region_t; 
 
typedef struct
{
    /* handler when safety region is entered */
    void (*safety_handler)(uint8 old_region_enum, uint8 new_region_enum);
 
    /* handler when charging_timer expires */
    void (*charging_timeout_handler)(void);   
 
    /* handler when transition happens from one operating region to another */
    void (*transition_handler)(uint8 old_region_enum, uint8 new_region_enum);
 
} battery_region_handlers_t;

typedef enum
{
    DISCHARGE_MODE,
    CHARGE_MODE,
}charge_mode_t;

#ifdef HAVE_NO_BATTERY

#define BatteryRegion_Init()
#define BatteryRegion_SetChargeRegionConfigTable(mode, table, size)
#define BatteryRegion_SetHandlerStructure(config_table)
/* make sure it returns TRUE and dereferences the parameters */
#define BatteryRegion_Register(task) (task == task)
#define BatteryRegion_Unregister(task)
#define BatteryRegion_GetState() (battery_region_unknown)
#define BatteryRegion_GetCurrent() (0)

#else

/*! 
    \brief Initialisation function for battery_region module
*/
void BatteryRegion_Init(void);

/*! 
    \brief Initialize Battery Operating Region Config tables for charge and discharge mode.
    \param mode battery charge/discharge state
    \param config_table pointer to charge_region_t structure array 
    \param config_size size of charge_region_t structure array
*/
void BatteryRegion_SetChargeRegionConfigTable(charge_mode_t mode, 
                              const charge_region_t* config_table,
                              unsigned config_size);

/*! 
    \brief Initialize Battery Region various state handlers
    \param config_table pointer to battery_region_handlers_t structure
*/
void BatteryRegion_SetHandlerStructure(
                              const battery_region_handlers_t* config_table);

/*! 
    \brief Register for receiving updates from battery_region module.
    \param task Handler task.
    \return Returns TRUE if register successful.
*/
bool BatteryRegion_Register(Task task);

/*! 
    \brief Unregister task from receiving updates.
    \param task Handler task.
*/
void BatteryRegion_Unregister(Task task);

/*! 
    \brief Get current battery state from operating region it is in.
    \return Returns current battery state.
*/
battery_region_state_t BatteryRegion_GetState(void);

/*! 
    \brief Get current for the operating region battery is in currentlt.
    \return Returns current value in mA.
*/
uint16 BatteryRegion_GetCurrent(void);

#endif /* !HAVE_NO_BATTERY */

#endif /* BATTERY_REGION_H_ */
