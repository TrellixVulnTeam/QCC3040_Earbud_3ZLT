/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Battery
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include "main.h"
#include "adc.h"
#include "gpio.h"
#include "led.h"
#include "timer.h"
#include "case_charger.h"
#include "config.h"
#include "power.h"
#include "current_senses.h"
#include "battery.h"
#include "vreg.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/**
 * Time in ticks that we must wait for the VBAT monitor to settle before taking
 * a measurement from it.
 */
#define BATTERY_READ_DELAY_TIME 20

/**
 * Time in ticks to wait for the ADC reading to take place.
 */
#define BATTERY_ADC_DELAY_TIME 2

/*
* BATTERY_NO_OF_CUTOFF_READS: Number of reads we will make if the voltage is
* below the cutoff voltage.
*/
#define BATTERY_NO_OF_CUTOFF_READS 3

/**
 * Maximum current that can be drawn in total from the VBUS pogo pins.
 * If this value is exceeded we must switch off VBUS to protect the battery.
 */
#define BATTERY_MAX_LOAD_MA 330

/**
 * Maximum current that can be drawn from a single earbud.
 * If this value is exceeded we must switch off VBUS to protect the battery.
 */
#define BATTERY_MAX_LOAD_PER_EARBUD_MA 200

/**
 * The number of ticks that the VBUS output will be disabled if we detect it is overloaded
 * with current.
 */
#define BATTERY_OVERLOAD_TICKS 300

/**
 * The battery voltage at which this battery cannot support running the
 * voltage regualtor.
 * If too much current is drawn from the regualtor, it risks the battery
 * voltage to drop too far and cause a brownout.
 */
#define BATTERY_CUT_VREG_MV (3500)

/**
 * The battery voltage at which it is deemed safe to re-enable the voltage
 * regulator if it had been disabled before.
 * It is recommended this value is greater than BATTERY_CUT_VREG_MV to provide
 * hysterisis for if the regulator can be enabled or not.
 */
#define BATTERY_REENABLE_VREG_MV (3900)

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    BATTERY_IDLE,
    BATTERY_START_READING,
    BATTERY_READING,
    BATTERY_STOP_READING,
    BATTERY_DONE
}
BATTERY_STATE;

typedef enum
{
    BATTERY_LOAD_READING,
    BATTERY_LOAD_OVERLOADED
}
BATTERY_LOAD_STATE;

typedef struct
{
    BATTERY_STATE state;
    uint8_t current_battery_percent;
    uint16_t current_battery_mv;
    uint16_t delay_ticks;
    bool led;
    uint8_t cmd_source;
    uint32_t cutoff_mv;
    uint8_t read_ctr;
}
BATTERY_STATUS;

typedef struct
{
    /* The voltage of the battery at this level in millivolts. */
    uint16_t voltage_mv;

    /* The percentage of charge this level represents from 0% to 100%. */
    uint8_t percentage;
}
BATTERY_LEVEL;

typedef struct
{
    /* The load on VBUS supplying the earbuds. This is assumed to be total load
     * on the case battery so regulator efficiency and other peripheral power
     * consumption is negligible. */
    uint16_t load_mA;

    /* The expected drop in battery voltage under the load. Measured in millivolts. */
    uint16_t battery_drop_mv;
}
BATTERY_DROP;

typedef struct
{
    /* The voltage these drops apply to */
    uint16_t battery_voltage_mv;

    /* A series of voltage drops we expect at different loads.*/
    const BATTERY_DROP *drops;

    /* The number of drops */
    size_t num_drops;
}
BATTERY_DROPS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*
 * Measured battery capacity levels of the VDL 602045 545mA
 * 3.7V lithium battery.
 * These were determined using the B2281S-20-6 Battery Simulator in mode 9.
 */
static const BATTERY_LEVEL battery_levels[] =
{   
    {3500, 0},
    {3627, 1},
    {3784, 5},
    {3803, 10},
    {3834, 15},
    {3860, 20},
    {3884, 25},
    {3897, 30},
    {3920, 40},
    {3949, 50},
    {3981, 60},
    {4018, 70},
    {4063, 80},
    {4115, 90},
    {4139, 95},
    {4160, 100},
};
static const size_t battery_levels_num = sizeof(battery_levels) / sizeof(BATTERY_LEVEL);

#ifdef EARBUD_CURRENT_SENSES
/*
 * Measured battery voltage drops of the VDL 602045 545mA
 * 3.7V lithium battery.
 * These were determined using the B2281S-20-6 Battery Simulator in mode 9.
 */
static const BATTERY_DROP DROPS_3V5[] =  {{0,0}, {50,38}, {100,73}, {200,161}, {280,215}};
static const BATTERY_DROP DROPS_3V8[] =  {{0,0}, {50,31}, {100,65}, {200,134}, {280,187}};
static const BATTERY_DROP DROPS_3V86[] = {{0,0}, {50,29}, {100,64}, {200,126}, {280,179}};
static const BATTERY_DROP DROPS_4V[] =   {{0,0}, {50,31}, {100,58}, {200,114}, {280,161}};
static const BATTERY_DROP DROPS_4V1[] =  {{0,0}, {50,34}, {100,63}, {200,123}, {280,172}};
static const BATTERY_DROP DROPS_4V16[] = {{0,0}, {50,33}, {100,64}, {200,125}, {280,172}};

static const BATTERY_DROPS battery_drops[] = {
    {3500, DROPS_3V5,  sizeof(DROPS_3V5)/sizeof(BATTERY_DROP)},
    {3803, DROPS_3V8,  sizeof(DROPS_3V8)/sizeof(BATTERY_DROP)},
    {3860, DROPS_3V86, sizeof(DROPS_3V86)/sizeof(BATTERY_DROP)},
    {4063, DROPS_4V,   sizeof(DROPS_4V)/sizeof(BATTERY_DROP)},
    {4115, DROPS_4V1,  sizeof(DROPS_4V1)/sizeof(BATTERY_DROP)},
    {4160, DROPS_4V16, sizeof(DROPS_4V16)/sizeof(BATTERY_DROP)}
};
static const size_t battery_drops_num = sizeof(battery_drops) / sizeof(BATTERY_DROPS);
#endif

static BATTERY_STATUS battery_status =
{
    .state = BATTERY_IDLE,
    .cmd_source = CLI_SOURCE_NONE
};


static BATTERY_LOAD_STATE battery_load_state = BATTERY_LOAD_READING;
static uint16_t battery_load_timer = 0;

static uint16_t battery_monitor_reason = 0;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void battery_monitor_enable_evaluate(void)
{
    if (battery_monitor_reason) 
    {
        gpio_enable(GPIO_VBAT_MONITOR_ON_OFF);
    }
    else
    {
        gpio_disable(GPIO_VBAT_MONITOR_ON_OFF);
    }
}

void battery_monitor_set_reason(BATTERY_MONITOR_REASON reason)
{
    battery_monitor_reason |= (1 << reason);
    battery_monitor_enable_evaluate();
}

void battery_monitor_clear_reason(BATTERY_MONITOR_REASON reason)
{
    battery_monitor_reason &= ~(1 << reason);
    battery_monitor_enable_evaluate();
}


static uint8_t battery_percentage(uint16_t mv)
{
    size_t i;
    uint8_t ret = 0;

    /* Any voltage less than the minimum level is considered 0% */
    if (mv > battery_levels[0].voltage_mv)
    {
        /* Any voltage larger than the maximum is considered 100% */
        ret = 100;

        /* Find the battery levels which the measured voltage sits between and
         * linearly interpolate the resulting percentage */
        for (i = 1; i < battery_levels_num; i++)
        {
            if (mv < battery_levels[i].voltage_mv)
            {
                /* Linearly interpolate a percentage based on this level and the previous */
                uint16_t range_mv = battery_levels[i].voltage_mv - battery_levels[i - 1].voltage_mv;
                uint8_t  range_pc = battery_levels[i].percentage - battery_levels[i - 1].percentage;
                uint8_t  d_mv = mv - battery_levels[i - 1].voltage_mv;

                ret = battery_levels[i - 1].percentage +
                    ((10 * range_pc * d_mv) / range_mv + 5) / 10;
                break;
            }
        }
    }

    return ret;
}

uint8_t battery_percentage_current(void)
{
   return battery_status.current_battery_percent;
}

#ifdef EARBUD_CURRENT_SENSES
static uint32_t battery_calculate_drop(uint16_t total_ma, BATTERY_DROP *drops, size_t drops_num)
{
    size_t i;
    uint32_t drop_mv = 0;

    /* Only compensate if there is a load. */
    if (total_ma > drops[0].load_mA)
    {
        /* Any load larger than the maximum is considered to impose a 200mV drop. */
        drop_mv = 200;

        /* Find the battery load which the measured load sits between and
         * linearly interpolate the resulting battery drop*/
        for (i = 1; i < drops_num; i++)
        {
            if (total_ma < drops[i].load_mA)
            {
                /* Linearly interpolate a battery_drop_mv based on this level and the previous */
                uint16_t range_ma = drops[i].load_mA - drops[i - 1].load_mA;
                uint8_t  range_mv = drops[i].battery_drop_mv - drops[i - 1].battery_drop_mv;
                uint8_t  d_ma = total_ma - drops[i - 1].load_mA;

                drop_mv = drops[i - 1].battery_drop_mv +
                    ((10 * range_mv * d_ma) / range_ma + 5) / 10;
                break;
            }
        }
    }

    return drop_mv; 
}

static uint32_t battery_compensated_voltage(uint16_t raw_mv, uint32_t total_ma)
{
    size_t i;
    uint32_t drop_mv = 0;

    /* Any load larger than the maximum is considered to impose a 200mV drop. */
    drop_mv = 200;

    /* Find the battery load which the measured load sits between and
     * linearly interpolate the resulting battery drop*/
    for (i = 1; i < battery_drops_num; i++)
    {
        if (raw_mv < battery_drops[i].battery_voltage_mv)
        {
            /* Linearly interpolate a battery_drop_mv based on this level and the previous */
            uint16_t range_ma = battery_drops[i].battery_voltage_mv - battery_drops[i - 1].battery_voltage_mv;
            uint32_t drop_a = battery_calculate_drop(total_ma, battery_drops[i - 1].drops, battery_drops[i - 1].num_drops);
            uint32_t drop_b = battery_calculate_drop(total_ma, battery_drops[i].drops, battery_drops[i].num_drops);
            uint8_t  range_mv = drop_a > drop_b ? drop_a - drop_b : drop_b - drop_a;
            uint8_t  d_ma = raw_mv - battery_drops[i - 1].battery_voltage_mv;

            drop_mv = (drop_a > drop_b ? drop_a : drop_b) -
                ((10 * range_mv * d_ma) / range_ma + 5) / 10;
            break;
        }
    }

    return raw_mv + drop_mv; 
}
#endif

static uint16_t battery_mv(void)
{
    return adc_read_mv(ADC_VBAT, 6600);
}


#ifdef EARBUD_CURRENT_SENSES
/**
 * Monitor the approximate load on the battery by measuring the load
 * on VBUS.
 * If the total load exceeds the maximum then temporarily disable VBUS.
 * Once re-enabled continue to monitor and disable VBUS again if necessary.
 */
static void battery_current_monitoring(void)
{
    switch(battery_load_state)
    {
        case BATTERY_LOAD_READING:
        {
            uint32_t left_ma;
            uint32_t right_ma;
            battery_fetch_load_ma(&left_ma, &right_ma);
            uint32_t total_load_ma = left_ma + right_ma;

            /* Too much current from one earbud or the total load. */
            if (!charger_comms_is_active() &&
                (left_ma > BATTERY_MAX_LOAD_PER_EARBUD_MA ||
                 right_ma > BATTERY_MAX_LOAD_PER_EARBUD_MA ||
                 total_load_ma > BATTERY_MAX_LOAD_MA))
            {
                power_set_run_reason(POWER_RUN_CURRENT_MON);
                PRINTF_B("VBUS load l=%umA r=%umA exceeds max, switch off VBUS", left_ma, right_ma);
                battery_load_state = BATTERY_LOAD_OVERLOADED;
                battery_load_timer = BATTERY_OVERLOAD_TICKS;
                charger_comms_vreg_reset();
                vreg_off_set_reason(VREG_REASON_OFF_OVERCURRENT);
            }
            else
            {
                power_clear_run_reason(POWER_RUN_CURRENT_MON);
            }

            /* TODO: This is necessary to keep the battery voltage monitoring happy.
             * This could be improved. */
            if (battery_status.state != BATTERY_READING)
            {
                adc_start_measuring();
            }
            break;
        }
        case BATTERY_LOAD_OVERLOADED:
            if (!battery_load_timer--)
            {
                /* Once the timer expires, we re-enable VBUS and start
                 * monitoring the current again. If it still exceeds
                 * the permitted maximum, we will switch VBUS off again.
                 */
                charger_comms_vreg_high();
                vreg_off_clear_reason(VREG_REASON_OFF_OVERCURRENT);
                battery_load_state = BATTERY_LOAD_READING;
            }
            break;

        default:
            break;
    }
}
#endif

/**
 * \brief Handle events based on battery voltage
 * \param battery_mv The most recent battery voltage in millivolts.
 */
static void battery_handle_voltage_events(uint16_t battery_mv)
{
    if (case_charger_is_resolved() && charger_connected() && charger_is_charging() && battery_mv < BATTERY_CUT_VREG_MV)
    {
        vreg_off_set_reason(VREG_REASON_OFF_LOW_BATTERY);
    }
    else 
    {
        if (!charger_connected() || battery_mv >= BATTERY_REENABLE_VREG_MV)
        {
            vreg_off_clear_reason(VREG_REASON_OFF_LOW_BATTERY);
        }
    }
}

/**
 * \brief Perform monitoring of the battery voltage with the most recent battery reading.
 */
static void battery_voltage_monitoring(void)
{
    battery_handle_voltage_events(charger_connected() ? battery_mv() : battery_status.current_battery_mv);
}


void battery_periodic(void)
{
    battery_voltage_monitoring();

#ifdef EARBUD_CURRENT_SENSES
    battery_current_monitoring();
#endif

    /* Ensure that we read the case battery before initiating the status
     * sequence with the earbuds. */
    switch (battery_status.state)
    {
        case BATTERY_START_READING:
            /* We must enable the VBAT monitor circuit and then wait for it
             * to settle before taking a measurement. */
            battery_monitor_set_reason(BATTERY_MONITOR_REASON_READING);
            charger_set_reason(CHARGER_OFF_BATTERY_READ);
#ifdef EARBUD_CURRENT_SENSES
            current_senses_set_sense_amp(CURRENT_SENSE_AMP_BATTERY);
#endif
            battery_status.delay_ticks = BATTERY_READ_DELAY_TIME;
            battery_status.state = BATTERY_READING;
            battery_status.read_ctr = 0;
            battery_status.cutoff_mv = config_get_battery_cutoff_mv();
            power_set_run_reason(POWER_RUN_BATTERY_READ);
            break;

        case BATTERY_READING:
            /* Wait and then instruct the ADC to take the measurement. */
            if (!battery_status.delay_ticks)
            {
                if (adc_start_measuring())
                {
                    battery_status.delay_ticks = BATTERY_ADC_DELAY_TIME;
                    battery_status.state = BATTERY_STOP_READING;
                }
            }
            else
            {
                battery_status.delay_ticks--;
            }
            break;

        case BATTERY_STOP_READING:
            /* Wait for the ADC to take the measurement. */
            if (!battery_status.delay_ticks)
            {
                uint16_t raw_mv = battery_mv();
                battery_status.current_battery_mv = raw_mv;

#ifdef EARBUD_CURRENT_SENSES
                {
                    uint32_t total_load_ma = battery_fetch_total_load_ma();
                    battery_status.current_battery_percent =
                        battery_percentage(battery_compensated_voltage(raw_mv, total_load_ma));
                }
#else
                battery_status.current_battery_percent = battery_percentage(raw_mv);
#endif

                if (battery_status.current_battery_mv < battery_status.cutoff_mv)
                {
                    /*
                     * Reading was below the configured cutoff threshold.
                     */
                    if (++battery_status.read_ctr < BATTERY_NO_OF_CUTOFF_READS)
                    {
                        battery_status.state = BATTERY_READING;
                        break;
                    }
                    else
                    {
                        /*
                         * Readings persistently low, so go to standby if no
                         * charger is present.
                         */
                        if (!charger_connected())
                        {
                            power_set_standby_reason(POWER_STANDBY_LOW_BATTERY);
                        }
                    }
                }
                else
                {
                    power_clear_standby_reason(POWER_STANDBY_LOW_BATTERY);
                }

                /* We no longer need the VBAT monitor HW to be powered */
                battery_monitor_clear_reason(BATTERY_MONITOR_REASON_READING);
#ifdef EARBUD_CURRENT_SENSES
                current_senses_clear_sense_amp(CURRENT_SENSE_AMP_BATTERY);
#endif
                charger_clear_reason(CHARGER_OFF_BATTERY_READ);
                power_clear_run_reason(POWER_RUN_BATTERY_READ);
                battery_status.state = BATTERY_DONE;
                battery_status.delay_ticks = BATTERY_READ_DELAY_TIME;

                if (battery_status.led)
                {
                    led_indicate_battery(battery_status.current_battery_percent);
                }

                /*
                * If a command to read the battery is in progress, display
                * the result.
                */
                if (battery_status.cmd_source != CLI_SOURCE_NONE)
                {
                    uint8_t cmd_source = battery_status.cmd_source;

                    PRINTF("%u,%u",
                        battery_status.current_battery_mv,
                        battery_status.current_battery_percent);
                    PRINTF("OK");
                    battery_status.cmd_source = CLI_SOURCE_NONE;
                }
            }
            else
            {
                battery_status.delay_ticks--;
            }
            break;

        case BATTERY_IDLE:
        case BATTERY_DONE:
        default:
            break;
    }
}

void battery_read_request(bool led)
{
    if ((battery_status.state==BATTERY_IDLE) ||
        (battery_status.state==BATTERY_DONE))
    {
        /*
        * No battery read in progress, so start one.
        */
        battery_status.state = BATTERY_START_READING;
        battery_status.led = led;
    }
    else if (led)
    {
        /*
        * Set the led flag, so that the battery read already in progress
        * will report to the LED module at the end.
        */
        battery_status.led = true;
    }
}

bool battery_read_done(void)
{
    return (battery_status.state == BATTERY_DONE) ? true:false;
}

uint16_t battery_read_ntc(void)
{
    uint16_t ntc_mv;

    gpio_enable(GPIO_NTC_MONITOR_ON_OFF);
    adc_blocking_measure();
    ntc_mv = adc_read_mv(ADC_NTC, 3300);
    gpio_disable(GPIO_NTC_MONITOR_ON_OFF);

    return ntc_mv;
}

CLI_RESULT atq_ntc(uint8_t cmd_source)
{
    PRINTF("%u", battery_read_ntc());
    return CLI_OK;
}

CLI_RESULT atq_battery(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;

    if (battery_status.cmd_source==CLI_SOURCE_NONE)
    {
        battery_read_request(false);
        battery_status.cmd_source = cmd_source;
        ret = CLI_WAIT;
    }

    return ret;
}
