/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      LEDs
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "stm32f0xx.h"
#include "main.h"
#include "timer.h"
#include "gpio.h"
#include "power.h"
#include "battery.h"
#include "charger.h"
#include "case_charger.h"
#include "led.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define LED_NO_OF_PRIMARY_COLOURS 3

#define LED_SEQ_FOREVER 0xFFFF
#define LED_PHASE_FOREVER 0xFF

#define LED_EVENT_QUEUE_SIZE 4

/*
* LED_BATTERY_HIGH - percentage at and above which we consider the battery
* level to be high.
*/
#define LED_BATTERY_HIGH 95

/*
* LED_BATTERY_MEDIUM - percentage at and above which we consider the battery
* level to be medium.
*/
#define LED_BATTERY_MEDIUM 30

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    /** The colour of this phase. See LED_COLOUR_* defines. */
    uint8_t colour;
    /** The duration of this phase in periodic function ticks */
    uint8_t duration;
}
LED_PHASE;

typedef struct
{
    /** The duration of the sequence in periodic function ticks. */
    uint16_t duration;
    /** Number of phases in "phases" */
    uint8_t no_of_phases;
    /** A list of phases which will be executed in order for \a duration time. */
    LED_PHASE phase[];
}
LED_SEQUENCE;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT led_cmd_colour(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const uint16_t led_rgb[] = { GPIO_LED_RED, GPIO_LED_GREEN, GPIO_LED_BLUE };

static const LED_SEQUENCE led_seq_battery_medium =
{
    500, 1,
    {
        { LED_COLOUR_AMBER, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_battery_low =
{
    500, 1,
    {
        { LED_COLOUR_RED, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_battery_high =
{
    500, 1,
    {
        { LED_COLOUR_GREEN, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_battery_charging_low =
{
    LED_SEQ_FOREVER, 2,
    {
        { LED_COLOUR_RED, 50 },
        { LED_COLOUR_OFF, 50 },
    }
};

static const LED_SEQUENCE led_seq_battery_charging_medium =
{
    LED_SEQ_FOREVER, 2,
    {
        { LED_COLOUR_GREEN, 50 },
        { LED_COLOUR_OFF,   50 },
    }
};

static const LED_SEQUENCE led_seq_battery_charged =
{
    LED_SEQ_FOREVER, 1,
    {
        { LED_COLOUR_GREEN, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_error_condition =
{
    LED_SEQ_FOREVER, 2,
    {
        { LED_COLOUR_RED, 10 },
        { LED_COLOUR_OFF, 10 },
    }
};

static uint8_t led_ctr;
static uint16_t led_overall_ctr;
static uint8_t led_phase_ctr;
static uint8_t led_colour = LED_COLOUR_OFF;
static const LED_SEQUENCE *led_seq = NULL;
static const LED_SEQUENCE *led_event_seq = NULL;
static const LED_SEQUENCE *led_event_queue[LED_EVENT_QUEUE_SIZE] = {0};
static uint8_t led_queue_head = 0;
static uint8_t led_queue_tail = 0;
static bool led_running = true;

static const CLI_COMMAND led_command[] =
{
    { "colour", led_cmd_colour, 2 }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void led_init(void)
{
}

void led_set_colour(uint8_t colour)
{
    uint8_t n;

    if (colour != led_colour)
    {
        for (n=0; n<LED_NO_OF_PRIMARY_COLOURS; n++)
        {
            uint16_t pin = led_rgb[n];

            if (pin != GPIO_NULL)
            {
                if (colour & (1<<n))
                {
                    gpio_enable(pin);
                }
                else
                {
                    gpio_disable(pin);
                }
            }
        }

        led_colour = colour;
    }
}

void led_sleep(void)
{
    led_set_colour(LED_COLOUR_OFF);
}

void led_wake(void)
{
    led_ctr = 0;
    led_seq = NULL;
    led_event_seq = NULL;
    led_queue_head = 0;
    led_queue_tail = 0;
}

/*
* An event that we need to provide an indication for has occurred.
*/
static void led_indicate_event(const LED_SEQUENCE *seq)
{
    if (led_event_seq)
    {
        /*
        * We are already indicating an event so put this one in the queue
        * unless an identical indication is already there or currently being
        * displayed.
        */
        if (led_event_seq != seq)
        {
            uint8_t next_head = ((led_queue_head + 1) & (LED_EVENT_QUEUE_SIZE-1));
            bool already_there = false;
            uint8_t e;

            if (next_head != led_queue_tail)
            {
                /*
                * Queue is not full. Check the queue to see if the requested
                * indication is already there.
                */
                for (e = led_queue_tail; e != next_head; e = (uint8_t)((e + 1) & (LED_EVENT_QUEUE_SIZE-1)))
                {
                    if (led_event_queue[e] == seq)
                    {
                        /*
                        * This indication is already in the queue.
                        */
                        already_there = true;
                        break;
                    }
                }

                if (!already_there)
                {
                    led_event_queue[led_queue_head] = seq;
                    led_queue_head = next_head;
                }
            }
        }
    }
    else
    {
        /*
        * No indication currently in progress, so start displaying this one
        * immediately.
        */
        power_set_run_reason(POWER_RUN_LED);
        led_event_seq = seq;
    }
}

/*
* Called when the battery level has been read to initiate an indication
* of the result.
*/
void led_indicate_battery(uint8_t percent)
{
    if (led_running)
    {
        if (percent >= LED_BATTERY_HIGH)
        {
            led_indicate_event(&led_seq_battery_high);
        }
        else if (percent >= LED_BATTERY_MEDIUM)
        {
            led_indicate_event(&led_seq_battery_medium);
        }
        else
        {
            led_indicate_event(&led_seq_battery_low);
        }
    }
}

void led_periodic(void)
{
    if (led_running)
    {
        const LED_SEQUENCE *wanted_seq = led_event_seq;

        led_ctr++;
        led_overall_ctr++;

        if (!wanted_seq)
        {
            /*
            * No event indication at the moment, so work out what the
            * background indication should be.
            */
            if (charger_connected())
            {
                if (case_charger_temperature_fault())
                {
                    wanted_seq = &led_seq_error_condition;
                }
                else
                {
                    uint8_t bpc = battery_percentage_current();

                    if (charger_is_charging())
                    {
                        if (bpc >= LED_BATTERY_HIGH)
                        {
                            wanted_seq = &led_seq_battery_charged;
                        }
                        else if (bpc >= LED_BATTERY_MEDIUM)
                        {
                            wanted_seq = &led_seq_battery_charging_medium;
                        }
                        else
                        {
                            wanted_seq = &led_seq_battery_charging_low;
                        }
                    }
                    else
                    {
                        if (bpc >= LED_BATTERY_HIGH)
                        {
                            wanted_seq = &led_seq_battery_charged;
                        }
                    }
                }
            }
        }

        /*
        * Sequence changes.
        */
        if (wanted_seq != led_seq)
        {
            led_seq = wanted_seq;
            led_ctr = 0;
            led_overall_ctr = 0;
            led_phase_ctr = 0;
            led_set_colour(led_seq->phase[led_phase_ctr].colour);
        }

        if (led_seq)
        {
            if (led_overall_ctr >= led_seq->duration)
            {
                /*
                * Indication finished.
                */
                led_seq = NULL;
                led_set_colour(LED_COLOUR_OFF);

                /*
                * Get the next event sequence out of the queue if there is one.
                */
                if (led_queue_head != led_queue_tail)
                {
                    led_event_seq = led_event_queue[led_queue_tail];
                    led_queue_tail = (uint8_t)((led_queue_tail + 1) & (LED_EVENT_QUEUE_SIZE-1));
                }
                else
                {
                    led_event_seq = NULL;
                    power_clear_run_reason(POWER_RUN_LED);
                }
            }
            else if (led_ctr >= led_seq->phase[led_phase_ctr].duration)
            {
                /*
                * Next phase of the current sequence.
                */
                led_phase_ctr = (led_phase_ctr + 1) % led_seq->no_of_phases;
                led_set_colour(led_seq->phase[led_phase_ctr].colour);
                led_ctr = 0;
            }
        }
    }
}

static CLI_RESULT led_cmd_colour(uint8_t cmd_source __attribute__((unused)))
{
    long int col;

    if (cli_get_next_parameter(&col, 10) && (col <= LED_COLOUR_WHITE))
    {
        led_running = false;
        led_set_colour(col);
    }
    else
    {
        led_running = true;
        led_seq = NULL;
        led_set_colour(LED_COLOUR_OFF);
    }

    return CLI_OK;
}

CLI_RESULT led_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(led_command, cmd_source);
}

CLI_RESULT ats_led(uint8_t cmd_source __attribute__((unused)))
{
    return led_cmd_colour(cmd_source);
}
