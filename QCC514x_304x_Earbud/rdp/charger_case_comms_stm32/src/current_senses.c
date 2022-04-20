/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Code for handling the use of current senses for the pogo pins supplying
            each earbud.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"
#include "current_senses.h"
#include "cli.h"
#include "adc.h"
#include "gpio.h"
#include "power.h"
#include "cmsis.h"
#include "timer.h"
#include "config.h"
#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ DEFINES ----------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * Current sense ADC value below which we consider there to be no
 * current senses.
 * This is non-zero as there may be some stray capacitance on the ADC lines
 * when they are floating.
 */
#define CURRENT_SENSE_NO_COMMS 50

/**
 * The voltage that the current sense circuit is biased by in millivolts.
 * When no current is drawn we expect to see this bias on the current sense
 * output.
 */
#define CURRENT_SENSE_BIAS_MV 190

/**
 * The ratio between the voltage output from the current senses and the current
 * it represents.
 * This depends on the value of the shunt resistor used to detect current and the
 * gain of the amplifier.
 *
 * E.g:
 * 0.1 Ohm shunt
 * x200 gain current sense amplifier
 *
 * If 1mA (0.001A) flows, there is 0.001*0.1 = 0.0001V potential across the resistor, 
 * With 200 gain, 0.0001V * 200 = 0.02A (20mA)
 */
#define CURRENT_SENSE_MV_PER_MA 20

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint8_t current_sense_amp_reason = 0;
static uint16_t current_sense_bias_mv = CURRENT_SENSE_BIAS_MV;
static uint16_t current_sense_mv_per_ma = CURRENT_SENSE_MV_PER_MA;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/
void current_senses_init(void)
{
    /* The 20-17759-H2 board uses a x100 gain amplifier and half the bias
     * compared to other boards. */
    if (config_get_board_id() == 20177593)
    {
        current_sense_bias_mv = 95;
        current_sense_mv_per_ma = 10;
    }

    /* Monitor the VBUS load by default while active. */
    current_senses_set_sense_amp(CURRENT_SENSE_AMP_MONITORING);
}

bool current_senses_are_present(void)
{
    return (adc_read(ADC_CURRENT_SENSE_L) >= CURRENT_SENSE_NO_COMMS) &&
           (adc_read(ADC_CURRENT_SENSE_R) >= CURRENT_SENSE_NO_COMMS);
}

void current_senses_set_sense_amp(uint8_t reason)
{
    DISABLE_IRQ();
    current_sense_amp_reason |= reason;
    gpio_enable(GPIO_CURRENT_SENSE_AMP);
    ENABLE_IRQ();
}

void current_senses_clear_sense_amp(uint8_t reason)
{
    DISABLE_IRQ();
    current_sense_amp_reason &= ~reason;
    if (!current_sense_amp_reason)
    {
        gpio_disable(GPIO_CURRENT_SENSE_AMP);
    }
    ENABLE_IRQ();
}

volatile uint16_t *current_senses_left_adc_value()
{
    return adc_value_ptr(ADC_CURRENT_SENSE_L);
}

volatile uint16_t *current_senses_right_adc_value()
{
    return adc_value_ptr(ADC_CURRENT_SENSE_R);
}

/**
 * \brief Calculate the total load on VBUS in milliamps.
 * \param left_sense_mv The reading of the left current sense in millivolts.
 * \param right_sense_mv The reading of the right current sense in millivolts.
 * \return The total load on VBUS in milliamps.
 */
static uint32_t battery_total_load_ma(uint16_t left_sense_mv, uint16_t right_sense_mv)
{
    /* Remove the bias and clamp each reading to 0. The total load is calculated from the left and right reading. */
    uint32_t total_cur_mv = (left_sense_mv >= current_sense_bias_mv ? left_sense_mv - current_sense_bias_mv : 0) +
                            (right_sense_mv >= current_sense_bias_mv ? right_sense_mv - current_sense_bias_mv : 0);

    uint32_t total_ma = total_cur_mv/current_sense_mv_per_ma;

    return total_ma;
}

/**
 * \brief Calculate the load from an earbud in milliamps.
 * \param sense_mv The reading of the left current sense in millivolts.
 * \return The load from an an earbud in milliamps.
 */
static uint32_t current_senses_to_milliamps(uint16_t sense_mv)
{
    /* Remove the bias and clamp the reading to 0. */
    uint32_t cur_mv = (sense_mv >= current_sense_bias_mv ? sense_mv - current_sense_bias_mv : 0);
    uint32_t calculated = cur_mv/current_sense_mv_per_ma;

    /* Scale to 95% - we seem to be overreading the current on this board
     * so we adjust the reading. */
    if (config_get_board_id() == 20177593)
    {
        return (calculated * 19)/20;
    }
    else
    {
        return calculated;
    }
}

void battery_fetch_load_ma(uint32_t *left_mA, uint32_t *right_mA)
{
    uint16_t left_sense = adc_read_mv(ADC_CURRENT_SENSE_L,3300);
    uint16_t right_sense = adc_read_mv(ADC_CURRENT_SENSE_R,3300);

    *left_mA = current_senses_to_milliamps(left_sense);
    *right_mA = current_senses_to_milliamps(right_sense);
}

uint32_t battery_fetch_total_load_ma(void)
{
    uint32_t left_ma;
    uint32_t right_ma;
    battery_fetch_load_ma(&left_ma, &right_ma);
    return left_ma + right_ma;
}

CLI_RESULT atq_sense(uint8_t cmd_source)
{
    current_senses_set_sense_amp(CURRENT_SENSE_AMP_COMMAND);
    delay_ms(200);
    adc_blocking_measure();
    PRINTF("%u,%u",
        adc_read_mv(ADC_CURRENT_SENSE_L,3300), adc_read_mv(ADC_CURRENT_SENSE_R,3300));
    current_senses_clear_sense_amp(CURRENT_SENSE_AMP_COMMAND);
    return CLI_OK;
}
