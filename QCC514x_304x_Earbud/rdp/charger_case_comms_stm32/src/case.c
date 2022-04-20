/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Case status
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "main.h"
#include "gpio.h"
#include "power.h"
#include "ccp.h"
#include "clock.h"
#include "usb.h"
#include "battery.h"
#include "adc.h"
#include "case.h"
#include "charger.h"
#include "earbud.h"
#include "case_charger.h"
#include "timer.h"
#include "config.h"
#include "charger_comms_device.h"
#include "vreg.h"
#include "debug.h"

#ifdef EARBUD_CURRENT_SENSES  
#include "current_senses.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* CASE_RESET_DELAY_TIME: Time in ticks that we wait following the earbud's
* acknowledgement of a reset command before starting to poll it.
*/
#define CASE_RESET_DELAY_TIME 100

/*
* CASE_RESET_POLLS: Number of times we attempt to poll an earbud following
* a reset command.
*/
#define CASE_RESET_POLLS 3

/*
* CASE_LOW_BATTERY_THRESHOLD: Battery level below which we indicate to the
* earbuds to use low current mode.
*/
#define CASE_LOW_BATTERY_THRESHOLD 10

/*
* CASE_HIGH_BATTERY_THRESHOLD: Level at which we consider the battery to be
* fully charged.
*/
#define CASE_HIGH_BATTERY_THRESHOLD 100

/*
* CASE_LOOPBACK_SEED: Seed for 'randomly' generated loopback payload data.
*/
#define CASE_LOOPBACK_SEED 1234

/*
* CASE_SHIPPING_TIME: Time in ticks for which the lid must be open in order
* to leave shipping mode.
*/
#define CASE_SHIPPING_TIME 25

/*
* CASE_RUN_TIME_BEFORE_STOP: Time in seconds for which we run before using
* stop mode (instead of sleep).
*/
#define CASE_RUN_TIME_BEFORE_STOP 30

/**
 * The time in seconds that must elapse before we consider the case lid to be
 * have been opened for a long time.
 */
#define CASE_STATUS_MAX_OPEN_TIME 600

/*
* CASE_STATUS_TIME_CHARGED: Status period in seconds that applies when the
* lid is closed and both earbuds are fully charged.
*/
#define CASE_STATUS_TIME_CHARGED 86400

/*
* CASE_BATTERY_UNKNOWN: Special value for when we don't know the battery
* level.
*/
#define CASE_BATTERY_UNKNOWN 0xFF

/**
 * The minimum current, measured in milliamps that we expect an earbud 
 * to draw from VBUS if it's not responding to comms.
 * Otherise we consider the case to be empty.
 */
#define CASE_EARBUD_MINIMUM_CURRENT_MA 10

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CS_IDLE,
    CS_ALERT,
    CS_SENT_STATUS_REQUEST,
    CS_STATUS_BROADCAST,
    CS_SENT_RESET,
    CS_RESET_DELAY,
    CS_RESETTING,
    CS_SENT_LOOPBACK,
    CS_SENT_SHIPPING_MODE,
    CS_SENT_XSTATUS_REQUEST,
    CS_SHIPPING_DONE
}
CASE_STATE;

typedef struct
{
    CASE_STATE state;
    uint16_t state_time;
    bool status_wanted;
    bool xstatus_wanted;
    bool reset_wanted;
    bool loopback_wanted;
    bool ship_wanted;
    bool valid;
    uint8_t pp;
    uint8_t charge_rate;
    uint8_t battery;
    uint8_t charging;
    uint8_t info_type;
    bool ack;
    bool give_up;
    bool abort;
    bool present;
    CLI_SOURCE cmd_source;
    uint8_t reset_poll_attempts;
    uint16_t loopback_count;
    uint16_t loopback_nack_count;
    uint16_t loopback_iterations;
    uint32_t loopback_start_time;
    bool loopback_generated_data;
    uint16_t loopback_data_len;
    uint8_t loopback_data[CCP_MAX_PAYLOAD_SIZE];
}
CASE_EARBUD_STATUS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static void case_rx_earbud_status(
    uint8_t earbud,
    uint8_t pp,
    uint8_t chg_rate,
    uint8_t battery,
    uint8_t charging);

static void case_rx_bt_address(
    uint8_t earbud, uint16_t nap, uint8_t uap, uint32_t lap);

static void case_rx_loopback(uint8_t earbud, uint8_t *data, uint16_t len);
static void case_ack(uint8_t earbud);
static void case_nack(uint8_t earbud);
static void case_give_up(uint8_t earbud);
static void case_no_response(uint8_t earbud);
static void case_abort(uint8_t earbud);
static void case_broadcast_finished(void);
static void case_rx_shipping(uint8_t earbud, uint8_t sm);
static CLI_RESULT case_cmd_info(uint8_t cmd_source);
static CLI_RESULT case_cmd_status(uint8_t cmd_source);
static CLI_RESULT case_cmd_reset(uint8_t cmd_source);
static CLI_RESULT case_cmd_loopback(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const uint8_t case_earbud_rr[NO_OF_EARBUDS] =
    { POWER_RUN_STATUS_L, POWER_RUN_STATUS_R };

static bool lid_now = false;
static bool lid_before = false;
static bool chg_now = false;
static bool chg_before = false;
static volatile bool case_event = true;
static bool case_dfu_planned = false;
static bool case_status_on_timer = false;
static bool case_debug_mode = false;
static uint16_t lid_open_time = 0;
static uint32_t case_status_countdown;
static CASE_EARBUD_STATUS case_earbud_status[NO_OF_EARBUDS] = {0};
static bool in_shipping_mode = false;
static uint8_t shipping_mode_lid_open_count = 0;
static uint32_t run_time = 0;
static bool stop_set = false;
bool comms_enabled = false;

static const CCP_USER_CB case_ccp_cb =
{
    case_rx_earbud_status,
    case_rx_bt_address,
    case_ack,
    case_nack,
    case_give_up,
    case_no_response,
    case_abort,
    case_broadcast_finished,
    case_rx_loopback,
    case_rx_shipping
};

static const CLI_COMMAND case_command[] =
{
    { "",           case_cmd_info,       2 },
    { "status",     case_cmd_status,     2 },
    { "reset",      case_cmd_reset,      2 },
    { "loopback",   case_cmd_loopback,   2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Enable case debug mode.
*/
void case_enable_debug(void)
{
    case_debug_mode = true;
}

/*
* Disable case debug mode.
*/
void case_disable_debug(void)
{
    case_debug_mode = false;
}

/*
* Enable periodic status messages.
*/
static void case_enable_status_timer(void)
{
    case_status_on_timer = true;
}

/*
* Disable periodic status messages.
*/
static void case_disable_status_timer(void)
{
    case_status_on_timer = false;
}

void case_event_occurred(void)
{
    power_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event = true;
}

/*
* Start charger comms related activity.
*/
static void case_start(void)
{
    in_shipping_mode = false;

#ifdef SCHEME_A
    /*
    * Using the current senses to determine whether this hardware supports
    * charger comms or not. At this point we will have a stored value for the
    * ADCs so we don't need to perform another measurement.
    */
    if (current_senses_are_present())
#endif
    {
        comms_enabled = true;
        ccp_init(&case_ccp_cb);
        charger_comms_device_init();
    }

    case_enable_status_timer();
    case_event_occurred();
    battery_read_request(false);
}

void case_init(void)
{
    case_earbud_status[EARBUD_LEFT].battery = CASE_BATTERY_UNKNOWN;
    case_earbud_status[EARBUD_LEFT].cmd_source = CLI_SOURCE_NONE;
    case_earbud_status[EARBUD_RIGHT].battery = CASE_BATTERY_UNKNOWN;
    case_earbud_status[EARBUD_RIGHT].cmd_source = CLI_SOURCE_NONE;

    /* Force a status request on power up */
    case_status_countdown = 0;

    if (config_get_shipping_mode())
    {
        in_shipping_mode = true;
        power_set_standby_reason(POWER_STANDBY_SHIPPING_MODE);
        vreg_off_set_reason(VREG_REASON_OFF_SHIPPING_MODE);
    }
    else
    {
        case_start();
    }
}

/**
 * \brief Schedule the next time that the case sends a status request to both
 *        earbuds.
 * \param number_of_ticks The number of ticks to wait till the next status request.
 */
static void case_schedule_next_status_req(uint32_t number_of_ticks)
{
    if (number_of_ticks)
    {
        case_enable_status_timer();
        case_status_countdown = number_of_ticks;
    }
    else
    {
        /* If no number of ticks are defined, disable the timer so we will not
         * schedule a status request */
        case_disable_status_timer();
    }
}

/*
* An earbud has responded to our status request.
*/
static void case_rx_earbud_status(
    uint8_t earbud,
    uint8_t pp,
    uint8_t chg_rate,
    uint8_t battery,
    uint8_t charging)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];
    uint8_t cmd_source = ces->cmd_source;

    if (cmd_source != CLI_SOURCE_NONE)
    {
        PRINTF("EBSTATUS (%c): %d", earbud_letter[earbud], battery);
    }

    ces->valid = true;
    ces->pp = pp;
    ces->charge_rate = chg_rate;
    ces->battery = battery;
    ces->charging = charging;
    ces->present = true;
}

/*
* An earbud has responded to our request for its bluetooth address.
*/
static void case_rx_bt_address(
    uint8_t earbud, uint16_t nap, uint8_t uap, uint32_t lap)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];
    uint8_t cmd_source = ces->cmd_source;

    if (cmd_source != CLI_SOURCE_NONE)
    {
        PRINTF("EBSTATUS (%c): %04X,%02X,%06X",
            earbud_letter[earbud], nap, uap, lap);
    }

    ces->valid = true;
    ces->present = true;
}

/*
* Shipping mode response received.
*/
static void case_rx_shipping(uint8_t earbud, uint8_t sm)
{
    if (sm)
    {
        case_earbud_status[earbud].valid = true;
    }
    else
    {
        case_earbud_status[earbud].give_up = true;
    }
}

/*
* Loopback response received.
*/
static void case_rx_loopback(uint8_t earbud, uint8_t *data, uint16_t len)
{
    uint8_t cmd_source = case_earbud_status[earbud].cmd_source;
    bool ok;

    if ((len==case_earbud_status[earbud].loopback_data_len) &&
        !memcmp(case_earbud_status[earbud].loopback_data, data, len))
    {
        ok = true;
    }
    else
    {
        ok = false;
    }

    PRINTF("LOOPBACK (%c): %s", earbud_letter[earbud], (ok) ? "OK":"ERROR");

    case_earbud_status[earbud].valid = true;
    case_earbud_status[earbud].present = true;
}

/*
* Send a loopback message.
*/
bool case_tx_loopback(uint8_t earbud)
{
    if (case_earbud_status[earbud].loopback_generated_data)
    {
        uint8_t i;

        /*
        * If it's the first message, initialise the random number generator
        * to give a predictable data pattern. When running unit tests, for
        * convenience, we do this for every message.
        */
#ifndef TEST
        if (!case_earbud_status[earbud].loopback_count)
#endif
        {
            srand(CASE_LOOPBACK_SEED);
        }

        for (i=0; i<CCP_MAX_PAYLOAD_SIZE; i++)
        {
            case_earbud_status[earbud].loopback_data[i] = (uint8_t)rand();
        }
    }

    return ccp_tx_loopback(
        earbud,
        case_earbud_status[earbud].loopback_data,
        case_earbud_status[earbud].loopback_data_len);
}

/*
* Call to change state.
*/
static void case_new_state(uint8_t earbud, CASE_STATE new_state)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];

    /*PRINTF_B("case state (%c) %d->%d", earbud_letter[earbud], ces->state, new_state);*/

    if (new_state==CS_IDLE)
    {
        power_clear_run_reason(case_earbud_rr[earbud]);
    }
    else
    {
        if (ces->state==CS_IDLE)
        {
            power_set_run_reason(case_earbud_rr[earbud]);
        }
    }

    ces->state = new_state;
    ces->state_time = 0;
}

/*
* Start the shipping mode request sequence on the specified earbud, if
* possible.
*/
static void case_start_earbud_shipping_mode_sequence(uint8_t earbud)
{
    case_earbud_status[earbud].ship_wanted = true;
    if (case_earbud_status[earbud].state==CS_IDLE)
    {
        case_new_state(earbud, CS_ALERT);
    }
}

/*
* Start the status sequence on the specified earbud, if possible.
*/
static void case_start_earbud_status_sequence(uint8_t earbud)
{
    case_earbud_status[earbud].status_wanted = true;
    if (case_earbud_status[earbud].state==CS_IDLE)
    {
        case_new_state(earbud, CS_ALERT);
    }
}

/*
* Start the extended status sequence on the specified earbud, if possible.
*/
static void case_start_earbud_xstatus_sequence(uint8_t earbud, uint8_t info_type)
{
    case_earbud_status[earbud].xstatus_wanted = true;
    case_earbud_status[earbud].info_type = info_type;
    if (case_earbud_status[earbud].state==CS_IDLE)
    {
        case_new_state(earbud, CS_ALERT);
    }
}

/*
* Returns true if both earbuds are present and fully charged, false otherwise.
*/
static bool case_all_charged(void)
{
    bool ret = true;
    uint8_t e;

    for (e=0; e<NO_OF_EARBUDS; e++)
    {
        if (!case_earbud_status[e].present ||
            (case_earbud_status[e].battery < CASE_HIGH_BATTERY_THRESHOLD) ||
            (case_earbud_status[e].battery == CASE_BATTERY_UNKNOWN))
        {
            ret = false;
            break;
        }
    }

    return ret;
}

static void case_end_earbud_status_sequence(uint8_t earbud, bool success)
{
    uint8_t cmd_source = case_earbud_status[earbud].cmd_source;

    if (!success && (cmd_source != CLI_SOURCE_NONE))
    {
        PRINTF("EBSTATUS (%c): Failed", earbud_letter[earbud]);
    }

    case_earbud_status[earbud].status_wanted = false;
    case_earbud_status[earbud].cmd_source = CLI_SOURCE_NONE;

    power_clear_reset_stop_reason(POWER_STOP_CASE_EMPTY);

    if (!case_earbud_status[EARBUD_LEFT].status_wanted &&
        !case_earbud_status[EARBUD_RIGHT].status_wanted)
    {
        if (!case_earbud_status[EARBUD_LEFT].valid &&
            !case_earbud_status[EARBUD_RIGHT].valid)
        {
            if (!case_earbud_status[EARBUD_LEFT].present && 
                !case_earbud_status[EARBUD_RIGHT].present)
            {
#ifdef EARBUD_CURRENT_SENSES  
                uint32_t left_ma;
                uint32_t right_ma;
                battery_fetch_load_ma(&left_ma, &right_ma);

                if(vreg_is_enabled() &&
                   left_ma < CASE_EARBUD_MINIMUM_CURRENT_MA &&
                   right_ma < CASE_EARBUD_MINIMUM_CURRENT_MA)
#endif
                {
                    /*
                     * Neither earbud has responsded to comms and neither is
                     * drawing a significant current from VBUS. It is very likely
                     * that the case is empty, go to STOP mode.
                     */
                    power_set_reset_stop_reason(POWER_STOP_CASE_EMPTY);
                }
            }
            case_new_state(earbud, CS_ALERT);
        }
        else
        {
            /* If at least one earbud responded, broadcast
             * the battery levels */
            case_new_state(earbud, CS_STATUS_BROADCAST);
        }
    }
    else
    {
        case_new_state(earbud, CS_ALERT);
    }

    if (!lid_now && case_all_charged())
    {
        case_status_countdown = CASE_STATUS_TIME_CHARGED;
        power_set_reset_stop_reason(POWER_STOP_FULLY_CHARGED);
    }
    else
    {
        power_clear_reset_stop_reason(POWER_STOP_FULLY_CHARGED);
    }
}

static void case_end_earbud_xstatus_sequence(uint8_t earbud, bool success)
{
    uint8_t cmd_source = case_earbud_status[earbud].cmd_source;

    if (!success)
    {
        PRINTF("EBSTATUS (%c): Failed", earbud_letter[earbud]);
    }

    case_earbud_status[earbud].xstatus_wanted = false;
    case_earbud_status[earbud].cmd_source = CLI_SOURCE_NONE;
    case_new_state(earbud, CS_ALERT);
}

static void case_end_earbud_loopback_sequence(uint8_t earbud, bool success)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];
    uint8_t cmd_source = ces->cmd_source;

    if (!success)
    {
        PRINTF("LOOPBACK (%c): Failed", earbud_letter[earbud]);
    }

    /*
    * Performance report if we were sending multiple loopbacks.
    */
    if (ces->loopback_iterations)
    {
        PRINTF("LOOPBACK (%c): Data rate = %d, NACKs = %d",
            earbud_letter[earbud],
            (ces->loopback_iterations * ces->loopback_data_len * 2 * TIMER_FREQUENCY_HZ) /
            (ticks - ces->loopback_start_time),
            ces->loopback_nack_count);
    }

    case_new_state(earbud, CS_ALERT);
    ces->cmd_source = CLI_SOURCE_NONE;
    ces->loopback_wanted = false;
}

void case_end_earbud_reset_sequence(uint8_t earbud)
{
    case_earbud_status[earbud].reset_wanted = false;
    case_new_state(earbud, CS_ALERT);
}

/*
* Should be called every second.
*/
void case_tick(void)
{
    run_time++;

    if (case_status_countdown)
    {
        case_status_countdown--;
    }

    /*
    * Time how long the lid has been open for (up to the maximum length of time
    * that we care about).
    */
    if (lid_now)
    {
        if (lid_open_time < CASE_STATUS_MAX_OPEN_TIME)
        {
            lid_open_time++;
        }
    }
    else
    {
        lid_open_time = 0;
    }

    if (case_status_on_timer && !case_debug_mode)
    {
        /*
        * Set the flag to go to stop mode if we have been running for long
        * enough.
        */
        if (!stop_set)
        {
            if (run_time > CASE_RUN_TIME_BEFORE_STOP)
            {
                stop_set = true;
                power_set_stop_reason(POWER_STOP_RUN_TIME);
            }
        }

        if (!case_status_countdown)
        {
            /*
            * Kick off an exchange of status information.
            */
            case_start_status_sequence(false);

            /*
            * Restart the countdown, the length of which depends on whether
            * or not the lid is open.
            */
            if (lid_now && lid_open_time >= CASE_STATUS_MAX_OPEN_TIME)
            {
                case_schedule_next_status_req(config_get_status_time_open());
            }
            else
            {
                case_schedule_next_status_req(config_get_status_time_closed());
            }
        }
    }
}

/*
* Initiate requesting/sending status messages.
*/
void case_start_status_sequence(bool led)
{
    if (!case_dfu_planned)
    {
        battery_read_request(led);

        if (comms_enabled)
        {
            case_start_earbud_status_sequence(EARBUD_LEFT);
            case_start_earbud_status_sequence(EARBUD_RIGHT);
        }
    }
}

static void case_ack(uint8_t earbud)
{
    case_earbud_status[earbud].ack = true;
    case_earbud_status[earbud].present = true;
}

static void case_nack(uint8_t earbud)
{
    case_earbud_status[earbud].loopback_nack_count++;
    case_earbud_status[earbud].present = true;
}

static void case_give_up(uint8_t earbud)
{
    PRINTF_B("Give up (%c)", earbud_letter[earbud]);
    case_earbud_status[earbud].give_up = true;
    case_earbud_status[earbud].present = true;
}

static void case_no_response(uint8_t earbud)
{
    PRINTF_B("No response (%c)", earbud_letter[earbud]);
    case_earbud_status[earbud].give_up = true;
    case_earbud_status[earbud].present = false;
}

static void case_abort(uint8_t earbud)
{
    PRINTF_B("Abort (%c)", earbud_letter[earbud]);
    case_earbud_status[earbud].abort = true;
}

static void case_broadcast_finished(void)
{
    power_clear_run_reason(POWER_RUN_BROADCAST);
}

bool case_allow_dfu(void)
{
    bool ret = false;

    if ((case_earbud_status[EARBUD_LEFT].state == CS_IDLE) &&
        (case_earbud_status[EARBUD_RIGHT].state == CS_IDLE))
    {
        ret = true;
    }

    case_dfu_planned = true;
    return ret;
}

void case_dfu_finished(void)
{
    case_dfu_planned = false;
}

/*
* Set the command source for both earbuds if neither is busy. This is for CLI
* commands that initiate activity on both earbuds and are required to output a
* result some time later.
*/
static bool case_set_cmd_source(uint8_t cmd_source)
{
    bool ret = false;

    if ((case_earbud_status[EARBUD_LEFT].cmd_source==CLI_SOURCE_NONE) &&
        (case_earbud_status[EARBUD_RIGHT].cmd_source==CLI_SOURCE_NONE))
    {
        case_earbud_status[EARBUD_LEFT].cmd_source = cmd_source;
        case_earbud_status[EARBUD_RIGHT].cmd_source = cmd_source;
        ret = true;
    }

    return ret;
}

/*
* Clear the command source for both earbuds.
*/
static void case_clear_cmd_source(void)
{
    case_earbud_status[EARBUD_LEFT].cmd_source = CLI_SOURCE_NONE;
    case_earbud_status[EARBUD_RIGHT].cmd_source = CLI_SOURCE_NONE;
}

/*
* Enter shipping mode.
*/
static void case_enter_shipping_mode(void)
{
    uint8_t cmd_source = case_earbud_status[EARBUD_LEFT].cmd_source;

    config_set_shipping_mode(true);
    power_set_standby_reason(POWER_STANDBY_SHIPPING_MODE);
    case_new_state(EARBUD_LEFT, CS_IDLE);
    case_new_state(EARBUD_RIGHT, CS_IDLE);
    case_disable_status_timer();
    debug_enable_test_mode(false, cmd_source);
}

void case_periodic(void)
{
    case_earbud_status[EARBUD_LEFT].state_time++;
    case_earbud_status[EARBUD_RIGHT].state_time++;

    if (case_event)
    {
        case_event = false;
        lid_now = gpio_active(GPIO_MAG_SENSOR);
        chg_now = charger_connected();

        if (lid_now)
        {
            if (!lid_before)
            {
                if (in_shipping_mode)
                {
                    power_set_run_reason(POWER_RUN_SHIP);
                }
                else
                {
                    /*
                    * Lid is opened. Initiate a status exchange and request an
                    * LED indication of the battery level.
                    */
                    case_start_status_sequence(true);
                }
                
                /* If the lid has not been open for at least
                 * CASE_STATUS_MAX_OPEN_TIME then we consider it closed.
                 */
                if(lid_open_time < CASE_STATUS_MAX_OPEN_TIME)
                {
                    case_schedule_next_status_req(config_get_status_time_closed());
                }
                else
                {
                    case_schedule_next_status_req(config_get_status_time_open());
                }
            }
        }
        else
        {
            if (in_shipping_mode)
            {
                shipping_mode_lid_open_count = 0;
                power_clear_run_reason(POWER_RUN_SHIP);
            }

            if (lid_before)
            {
                case_schedule_next_status_req(config_get_status_time_closed());

                if (!in_shipping_mode)
                {
                    /*
                    * Lid is closed.  Initiate a status exchange and LED
                    * indication
                    */
                    case_start_status_sequence(true);
                }
            }

            lid_open_time = 0;
        }

#ifdef USB_ENABLED
        if (chg_now)
        {
            if (!chg_before)
            {
                PRINT_B("Charger connected");
                case_charger_connected();
            }
        }
        else
        {
            if (chg_before)
            {
                PRINT_B("Charger disconnected");
                usb_disconnected();
                case_charger_disconnected();
            }
        }
#endif

        if (comms_enabled)
        {
            if (ccp_tx_short_status(lid_now, chg_now,
                (battery_percentage_current() < CASE_LOW_BATTERY_THRESHOLD) ? true:false))
            {
                power_set_run_reason(POWER_RUN_BROADCAST);
            }
            else
            {
                case_event = true;
            }
        }

        lid_before = lid_now;
        chg_before = chg_now;

        /*
        * It's possible that another event might have occurred in the meantime,
        * so check the event flag before clearing the run reason.
        */
        if (!case_event)
        {
            power_clear_run_reason(POWER_RUN_CASE_EVENT);
        }
    }
    else
    {
        uint8_t e;

        for (e=0; e<NO_OF_EARBUDS; e++)
        {
            CASE_EARBUD_STATUS *ces = &case_earbud_status[e];

            switch (ces->state)
            {
                default:
                case CS_IDLE:
                    break;

                case CS_ALERT:
                    if (ces->ship_wanted)
                    {
                        if (ccp_tx_shipping_mode(e))
                        {
                            ces->valid = false;
                            case_new_state(e, CS_SENT_SHIPPING_MODE);
                        }
                    }
                    else if (ces->status_wanted)
                    {
                        ces->valid = false;
                        ces->battery = CASE_BATTERY_UNKNOWN;

                        if (ccp_tx_status_request(e))
                        {
                            case_new_state(e, CS_SENT_STATUS_REQUEST);
                        }
                    }
                    else if (ces->xstatus_wanted)
                    {
                        ces->valid = false;

                        if (ccp_tx_xstatus_request(e, ces->info_type))
                        {
                            case_new_state(e, CS_SENT_XSTATUS_REQUEST);
                        }
                    }
                    else if (ces->reset_wanted)
                    {
                        if (ccp_tx_reset(e, true))
                        {
                            case_new_state(e, CS_SENT_RESET);
                        }
                    }
                    else if (ces->loopback_wanted)
                    {
                        ces->valid = false;

                        if (case_tx_loopback(e))
                        {
                            case_new_state(e, CS_SENT_LOOPBACK);
                        }
                    }
                    else
                    {
                        case_new_state(e, CS_IDLE);
                    }
                    break;

                case CS_SENT_RESET:
                    if (ces->ack)
                    {
                        /*
                        * The reset command was accepted. Clear any previously
                        * received status information.
                        */
                        ces->valid = false;
                        ces->battery = CASE_BATTERY_UNKNOWN;
                        ces->reset_poll_attempts = 0;
                        case_new_state(e, CS_RESET_DELAY);
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_ALERT);
                    }
                    else if (ces->give_up)
                    {
                        case_end_earbud_reset_sequence(e);
                    }
                    break;

                case CS_RESET_DELAY:
                    /*
                    * After initiating a reset, wait for a bit before polling
                    * the earbud.
                    */
                    if (ces->state_time > CASE_RESET_DELAY_TIME)
                    {
                        if (ccp_tx_status_request(e))
                        {
                            case_new_state(e, CS_RESETTING);
                        }
                    }
                    break;

                case CS_RESETTING:
                    if (ces->valid)
                    {
                        /*
                        * The valid flag being set means that we got a response
                        * from the earbud, so the reset is complete.
                        */
                        case_end_earbud_reset_sequence(e);
                    }
                    else if (ces->give_up)
                    {
                        if (++ces->reset_poll_attempts >= CASE_RESET_POLLS)
                        {
                            case_end_earbud_reset_sequence(e);
                        }
                        else
                        {
                            case_new_state(e, CS_RESET_DELAY);
                        }
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_RESET_DELAY);
                    }
                    break;

                case CS_SENT_STATUS_REQUEST:
                    if (ces->valid)
                    {
                        case_end_earbud_status_sequence(e, true);
                    }
                    else if (ces->give_up)
                    {
                        case_end_earbud_status_sequence(e, false);
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_ALERT);
                    }
                    break;

                case CS_SENT_XSTATUS_REQUEST:
                    if (ces->valid)
                    {
                        case_end_earbud_xstatus_sequence(e, true);
                    }
                    else if (ces->give_up)
                    {
                        case_end_earbud_xstatus_sequence(e, false);
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_ALERT);
                    }
                    break;

                case CS_STATUS_BROADCAST:
                    if (battery_read_done())
                    {
                        uint8_t bpc = battery_percentage_current();

                        if (ccp_tx_status(lid_now, chg_now, charger_is_charging(),
                            (bpc < CASE_LOW_BATTERY_THRESHOLD) ? true:false, bpc,
                            case_earbud_status[EARBUD_LEFT].battery,
                            case_earbud_status[EARBUD_RIGHT].battery,
                            case_earbud_status[EARBUD_LEFT].charging,
                            case_earbud_status[EARBUD_RIGHT].charging))
                        {
                            power_set_run_reason(POWER_RUN_BROADCAST);
                            case_new_state(e, CS_ALERT);
                        }
                    }
                    break;

                case CS_SENT_LOOPBACK:
                    if (ces->valid)
                    {
                        ces->loopback_count++;

                        if (ces->loopback_count >= ces->loopback_iterations)
                        {
                            case_end_earbud_loopback_sequence(e, true);
                        }
                        else
                        {
                            ces->valid = false;

                            if (!case_tx_loopback(e))
                            {
                                case_new_state(e, CS_ALERT);
                            }
                        }
                    }
                    else if (ces->give_up)
                    {
                        if (ces->loopback_iterations)
                        {
                            case_new_state(e, CS_ALERT);
                        }
                        else
                        {
                            case_end_earbud_loopback_sequence(e, false);
                        }
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_ALERT);
                    }
                    break;

                case CS_SENT_SHIPPING_MODE:
                    {
                        uint8_t cmd_source = ces->cmd_source;

                        if (ces->valid)
                        {
                            PRINTF("Shipping mode (%c)", earbud_letter[e]);
                            case_new_state(e, CS_SHIPPING_DONE);
                        }
                        else if (ces->give_up)
                        {
                            case_new_state(e, CS_SHIPPING_DONE);
                        }
                        else if (ces->abort)
                        {
                            case_new_state(e, CS_ALERT);
                        }
                    }
                    break;

                case CS_SHIPPING_DONE:
                    break;
            }

            ces->ack = false;
            ces->abort = false;
            ces->give_up = false;
        }
    }

    /*
    * Handle startup in shipping mode.
    */
    if (in_shipping_mode)
    {
        if (lid_now)
        {
            if (++shipping_mode_lid_open_count > CASE_SHIPPING_TIME)
            {
                PRINTF_B("Leaving shipping mode");
                config_set_shipping_mode(false);
                power_clear_standby_reason(POWER_STANDBY_SHIPPING_MODE);
                lid_before = false;
                case_start();
                power_clear_run_reason(POWER_RUN_SHIP);
            }
        }
    }

    /*
    * Case enters shipping mode after the earbuds have been dealt with.
    */
    if ((case_earbud_status[EARBUD_LEFT].state==CS_SHIPPING_DONE) &&
        (case_earbud_status[EARBUD_RIGHT].state==CS_SHIPPING_DONE))
    {
        uint8_t cmd_source = case_earbud_status[EARBUD_LEFT].cmd_source;

        case_earbud_status[EARBUD_LEFT].ship_wanted = false;
        case_earbud_status[EARBUD_RIGHT].ship_wanted = false;

        if ((case_earbud_status[EARBUD_LEFT].valid) &&
            (case_earbud_status[EARBUD_RIGHT].valid))
        {
            PRINT("OK");
            case_enter_shipping_mode();
        }
        else
        {
            PRINT("ERROR");
            case_new_state(EARBUD_LEFT, CS_ALERT);
            case_new_state(EARBUD_RIGHT, CS_ALERT);
        }

        case_clear_cmd_source();
    }
}

static CLI_RESULT case_cmd_info(uint8_t cmd_source)
{
    uint8_t e;

    PRINTF("Earbud  Present  Battery");

    for (e=0; e<NO_OF_EARBUDS; e++)
    {
        PRINTF("%-6c  %-7s  %d", earbud_letter[e],
            (case_earbud_status[e].present) ? "Yes":"No",
            case_earbud_status[e].battery);
    }

    PRINT("");

    if (lid_now)
    {
        PRINTF("Lid : Open (%ds)", lid_open_time);
    }
    else
    {
        PRINT("Lid : Closed");
    }
    PRINTF("Next status in %u seconds", case_status_countdown);

    return CLI_OK;
}

static CLI_RESULT case_cmd_status(uint8_t cmd_source __attribute__((unused)))
{
    /*
    * Initiate a status exchange and display the case battery level using LEDs.
    */
    case_start_status_sequence(true);
    return CLI_OK;
}

/*
* Handle AT+EBSTATUS
*/
CLI_RESULT ats_ebstatus(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;

    if (comms_enabled)
    {
        uint8_t earbud;

        if (cli_get_earbud(&earbud))
        {
            long int i;

            if (cli_get_next_parameter(&i, 10))
            {
                if (case_earbud_status[earbud].cmd_source==CLI_SOURCE_NONE)
                {
                    case_earbud_status[earbud].cmd_source = cmd_source;
                    case_start_earbud_xstatus_sequence(earbud, (uint8_t)i);
                    ret = CLI_OK;
                }
            }
        }
        else
        {
            /*
            * Initiate a status exchange only.
            */
            if (case_set_cmd_source(cmd_source))
            {
                case_start_status_sequence(false);
                ret = CLI_OK;
            }
        }
    }

    return ret;
}

static CLI_RESULT case_cmd_reset(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;

    if (!case_dfu_planned)
    {
        uint8_t earbud;
        /*
        * Initiate earbud reset.
        */
        if (cli_get_earbud(&earbud))
        {
            case_earbud_status[earbud].reset_wanted = true;

            if (case_earbud_status[earbud].state==CS_IDLE)
            {
                case_new_state(earbud, CS_ALERT);
            }
            ret = CLI_OK;
        }
    }

    return ret;
}

static CLI_RESULT case_cmd_loopback(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;
    uint8_t earbud;

    /*
    * Initiate earbud loopback.
    */
    if (cli_get_earbud(&earbud))
    {
        CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];

        if (ces->cmd_source==CLI_SOURCE_NONE)
        {
            long int i;

            ces->loopback_data_len = CCP_MAX_PAYLOAD_SIZE;
            ces->loopback_iterations = 0;
            ces->loopback_generated_data = true;

            if (cli_get_next_parameter(&i, 10))
            {
                ces->loopback_iterations = (uint16_t)i;

                if (cli_get_next_parameter(&i, 10))
                {
                    uint8_t data_len;

                    ces->loopback_data_len = (uint8_t)i;

                    if (cli_get_hex_data(
                        ces->loopback_data, &data_len,
                        sizeof(ces->loopback_data)))
                    {
                        ces->loopback_generated_data = false;

                        if (data_len < ces->loopback_data_len)
                        {
                            for (i=data_len; i < ces->loopback_data_len; i++)
                            {
                                ces->loopback_data[i] =
                                    ces->loopback_data[i % data_len];
                            }
                        }
                        else
                        {
                            ces->loopback_data_len = data_len;
                        }
                    }
                }
            }

            ces->loopback_wanted = true;
            ces->loopback_start_time = ticks;
            ces->loopback_nack_count = 0;
            ces->loopback_count = 0;

            /*
            * Make a note of the command source so that we can use it to
            * report the outcome later on.
            */
            ces->cmd_source = cmd_source;

            if (ces->state==CS_IDLE)
            {
                case_new_state(earbud, CS_ALERT);
            }

            ret = CLI_OK;
        }
    }

    return ret;
}

CLI_RESULT case_cmd(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;

    if (comms_enabled)
    {
        ret = cli_process_sub_cmd(case_command, cmd_source);
    }

    return ret;
}

CLI_RESULT ats_loopback(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;

    if (comms_enabled)
    {
        ret = case_cmd_loopback(cmd_source);
    }

    return ret;
}

/*
* Handle AT+SHIP
*/
CLI_RESULT ats_ship(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;

    /*
    * Only allow the command if the lid is closed.
    */
    if (!lid_now)
    {
        if (comms_enabled)
        {
            /*
            * Only allow the command if both earbuds are present.
            */
            if (case_earbud_status[EARBUD_LEFT].present &&
                case_earbud_status[EARBUD_RIGHT].present)
            {
                if (case_set_cmd_source(cmd_source))
                {
                    case_start_earbud_shipping_mode_sequence(EARBUD_LEFT);
                    case_start_earbud_shipping_mode_sequence(EARBUD_RIGHT);
                    ret = CLI_WAIT;
                }
            }
        }
        else
        {
            /*
            * Without comms we can't tell if the earbuds are present, nor can
            * we send them a shipping mode request, so we just put the case
            * into shipping mode immediately.
            */
            case_enter_shipping_mode();
            ret = CLI_OK;
        }
    }

    return ret;
}

CLI_RESULT atq_lid(uint8_t cmd_source)
{
    PRINTF("%d", (gpio_active(GPIO_MAG_SENSOR)) ? 1:0);
    return CLI_OK;
}
