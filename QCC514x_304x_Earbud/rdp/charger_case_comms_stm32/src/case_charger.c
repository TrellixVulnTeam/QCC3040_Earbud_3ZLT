/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     Charger control
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"
#include "case_charger.h"
#include "charger.h"
#include "charger_detect.h"
#include "usb.h"
#include "power.h"
#include "battery.h"
#include "timer.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/**
 * Time in ticks to wait for a USB enumeration after a charger has been
 * connected.
 */
#define CASE_CHARGER_USB_ENUMERATION_TIMEOUT 200

/**
 * Period in ticks to check the battery temperature and adjust the charge
 * current as needed
 */
#define CASE_CHARGER_MONITOR_PERIOD 200

/**
 * NTC Thermistor values for 0, 15 and 45 degrees Celsius.
 * Measured with a ECTH100505 103F 3435 FST thermistor through 3.3V 10k
 * resistor ladder. Values are defined in millivolts.
 */
#define CASE_CHARGER_BATTERY_0_C 2360 
#define CASE_CHARGER_BATTERY_15_C 1920 
#define CASE_CHARGER_BATTERY_45_C 1070
#define CASE_CHARGER_BATTERY_MAX_TEMP 100

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CASE_CHARGER_IDLE,
    CASE_CHARGER_CONNECTED,
    CASE_CHARGER_DISCONNECTED,
    CASE_CHARGER_WAITING,
    CASE_CHARGER_MONITORING,
    CASE_CHARGER_FINISH,
}
CASE_CHARGER_STATE;

typedef struct
{
    CASE_CHARGER_STATE state;
    uint16_t delay_ticks;
}
CASE_CHARGER_STATUS;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static CASE_CHARGER_STATUS case_charger_status = { .state = CASE_CHARGER_IDLE};
static CHARGER_CURRENT_MODE requested_mode = CHARGER_CURRENT_MODE_100MA;
static uint8_t charger_reason = 0;
static bool charger_enabled_now = false;
static bool temperature_ok = true;
static bool battery_read = false;

/*
* Identifies which reasons are 'on' and which are 'off'. See CHARGER_REASON.
*/
static const bool charger_reason_on[CHARGER_NO_OF_REASONS] =
{
    true, false, true, false, false
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Returns whether the charger outputs can be trusted
 * \return True if the charger can be trusted, false otherwise.
 */ 
static bool case_charger_can_trust(void)
{
    /* Wait at least 10 ticks from power up before trusting the charger. */
    return ticks > 10;
}

static void charger_enable_evaluate(void)
{
    uint8_t i;
    bool on = false;

    for (i=0; i<CHARGER_NO_OF_REASONS; i++)
    {
        if (charger_reason & (1 << i))
        {
            on = charger_reason_on[i];
        }
    }

    if (on != charger_enabled_now)
    {
        charger_enabled_now = on;
        charger_enable(on);
    }
}

void charger_set_reason(CHARGER_REASON reason)
{
    if (reason==CHARGER_OFF_BATTERY_READ)
    {
        battery_read = true;

        if (charger_connected() && case_charger_can_trust())
        {
            charger_set_current(CHARGER_CURRENT_MODE_STANDBY);
        }
    }
    else
    {
        charger_reason |= (1 << reason);
        charger_enable_evaluate();
    }
}

void charger_clear_reason(CHARGER_REASON reason)
{
    if (reason==CHARGER_OFF_BATTERY_READ)
    {
        battery_read = false;
        charger_enable_evaluate();
    }
    else
    {
        charger_reason &= ~(1 << reason);
        charger_enable_evaluate();
    }
}

void case_charger_connected(void)
{
    case_charger_status.state = CASE_CHARGER_CONNECTED;

    usb_chg_detected();
}

void case_charger_disconnected(void)
{
    case_charger_status.state = CASE_CHARGER_DISCONNECTED;
}

bool case_charger_is_resolved(void)
{
    CASE_CHARGER_STATE state = case_charger_status.state;
    return state == CASE_CHARGER_FINISH || state == CASE_CHARGER_MONITORING;
}

/**
 * Set the charger current based on which type of charger we detected.
 *
 * This is done to conform to the Battery Charging Specification 1.2 (BC1.2).
 */
static void case_charger_set_charging_current(void)
{
    CHARGER_DETECT_TYPE charger_type = charger_detect_get_type();

    PRINTF_B("USB type = %u", charger_type);

    if (charger_type == CHARGER_DETECT_TYPE_SDP)
    {
        /* We can draw up to 500mA from a configured (enumerated) USB 2.0 port
         * and up to 100mA for an unconfigured port */
        if (usb_has_enumerated())
        {
            requested_mode = CHARGER_CURRENT_MODE_500MA;
        }
        else
        {
            requested_mode = CHARGER_CURRENT_MODE_100MA;
        }
    }
    else
    {
        /* We should be able to draw 1.5A from a DCP and CDP according to BC1.2
         * for USB 2.0 ports. We assume that a floating data line charger can
         * also supply the maximum current we need. */
        requested_mode = CHARGER_CURRENT_MODE_ILIM;
    }
}

/*
* Returns true if the temperature is too high or too low, false otherwise.
*/
bool case_charger_temperature_fault(void)
{
    return (temperature_ok) ? false:true;
}

/**
 * Read the current value of the battery thermistor and alter the charging
 * current based on reading.
 *
 * This respects the limits of the VDL 602045 545mA 3.7V lithium battery using 
 * the ECTH100505 103F 3435 FST thermistor.
 */
static void case_charger_monitor_battery_temp(void)
{
    uint16_t ntc = battery_read_ntc();
    CHARGER_CURRENT_MODE mode = requested_mode;

    temperature_ok = true;

    /*
     * If the thermistor reading is outside our permitted range we assume it
     * is not connected and select the requested current mode.
     * Usually this would yield a reading of 0, however if there is stray
     * capacitance this may result in a non-zero value.
     */
    if (ntc >= CASE_CHARGER_BATTERY_MAX_TEMP)
    {
        /* < 0C or > 45C */
        if (ntc > CASE_CHARGER_BATTERY_0_C || ntc < CASE_CHARGER_BATTERY_45_C)
        {
            mode = CHARGER_CURRENT_MODE_STANDBY;
            temperature_ok = false;
        }
        /* < 15C and > 0C */
        else if (ntc >= CASE_CHARGER_BATTERY_15_C && ntc < CASE_CHARGER_BATTERY_0_C)
        {
            mode = CHARGER_CURRENT_MODE_100MA;
        }
    }

    /* Force the charger off if taking a battery reading */
    if (battery_read && charger_connected() && case_charger_can_trust())
    {
        mode = CHARGER_CURRENT_MODE_STANDBY;
    }

    charger_set_current(mode);

    if (temperature_ok)
    {
        charger_clear_reason(CHARGER_OFF_TEMPERATURE);
    }
    else
    {
        charger_set_reason(CHARGER_OFF_TEMPERATURE);
    }
}

/**
 * Manage charger insertions and removals.
 * We dynamically enable/disable the charger and adjust the charging currents
 * based on what charger we believe is attached.
 */
static void charger_detect_inserted_periodic(void)
{
    switch(case_charger_status.state)
    {
        case CASE_CHARGER_IDLE:
        default:
            break;

        case CASE_CHARGER_CONNECTED:
            power_set_run_reason(POWER_RUN_CHG_CONNECTED);
            battery_monitor_set_reason(BATTERY_MONITOR_REASON_CHARGER_CONN);

            /* If we are connected to a real USB host we can draw up to 100mA
             * until we are configured temperature permitting. */
            requested_mode = CHARGER_CURRENT_MODE_100MA;
            case_charger_monitor_battery_temp();
            charger_set_reason(CHARGER_ON_CONNECTED);

            case_charger_status.delay_ticks = CASE_CHARGER_USB_ENUMERATION_TIMEOUT;
            case_charger_status.state = CASE_CHARGER_WAITING;
            break;

        case CASE_CHARGER_DISCONNECTED:
            charger_clear_reason(CHARGER_ON_CONNECTED);
            charger_clear_reason(CHARGER_ON_COMMAND);
            charger_clear_reason(CHARGER_OFF_COMMAND);
            case_charger_status.state = CASE_CHARGER_IDLE;

            charger_detect_cancel();

            power_clear_run_reason(POWER_RUN_CHG_CONNECTED);
            battery_monitor_clear_reason(BATTERY_MONITOR_REASON_CHARGER_CONN);
            break;

        case CASE_CHARGER_WAITING:
            if (!case_charger_status.delay_ticks || usb_has_enumerated())
            {
                case_charger_status.state = CASE_CHARGER_FINISH;
            }
            else 
            {
                case_charger_status.delay_ticks--;
            }
            break;

        case CASE_CHARGER_FINISH:
            
            if (charger_connected())
            {
                case_charger_set_charging_current();
                case_charger_status.state = CASE_CHARGER_MONITORING;
            }
            break;
        case CASE_CHARGER_MONITORING:
            if (!case_charger_status.delay_ticks)
            {
                /* Monitor the battery temperature and adjust the charger if needed. */
                case_charger_monitor_battery_temp();

                /* Stay in this state and repeat after a delay. */
                case_charger_status.delay_ticks = CASE_CHARGER_MONITOR_PERIOD;
            }
            else 
            {
                case_charger_status.delay_ticks--;
            }
            break;
    }
}

void case_charger_periodic(void)
{
    /* Handle tasks relating to when a charger is inserted/removed */
    charger_detect_inserted_periodic();
}

CLI_RESULT ats_charger(uint8_t cmd_source __attribute__((unused)))
{
    long int en;

    if (cli_get_next_parameter(&en, 10))
    {
        if (en)
        {
            long int mode;

            /*
            * Not bothering to check the input parameter, relying on the fact
            * that charger_set_current() will disregard anything invalid.
            */
            if (cli_get_next_parameter(&mode, 10))
            {
                requested_mode = (CHARGER_CURRENT_MODE)mode;
                charger_set_current((CHARGER_CURRENT_MODE)mode);
            }

            charger_set_reason(CHARGER_ON_COMMAND);
            charger_clear_reason(CHARGER_OFF_COMMAND);
        }
        else
        {
            charger_clear_reason(CHARGER_ON_COMMAND);
            charger_set_reason(CHARGER_OFF_COMMAND);
        }
    }
    else
    {
        charger_clear_reason(CHARGER_ON_COMMAND);
        charger_clear_reason(CHARGER_OFF_COMMAND);
    }

    return CLI_OK;
}

CLI_RESULT atq_charger(uint8_t cmd_source)
{
    PRINTF("%d,%d,%d",
        (charger_connected()) ? 1:0, (charger_is_charging()) ? 1:0,
        charger_current_mode());
    return CLI_OK;
}
