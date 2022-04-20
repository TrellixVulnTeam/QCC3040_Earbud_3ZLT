/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Device specific code for charger comms.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"
#include "charger_comms.h"
#include "cli.h"
#include "adc.h"
#include "gpio.h"
#include "power.h"
#include "timer.h"
#include "wire.h"
#include "cmsis.h"
#include "charger_comms_device.h"
#include "earbud.h"
#include "current_senses.h"
#include "vreg.h"
#include "config.h"
#ifdef TEST
#include "test_st.h"
#endif

#ifdef CHARGER_COMMS_FAKE
#include "fake_earbud.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static void on_complete_impl(void);
static void on_tx_start_impl(uint8_t *buf, uint8_t num_tx_octets);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

earbud_channel left_earbud;
earbud_channel right_earbud;
uint16_t adc_buf_left[CHARGER_COMMS_ADC_BUFFER_SIZE];
uint16_t adc_buf_right[CHARGER_COMMS_ADC_BUFFER_SIZE];

charger_comms_cfg cfg =
{
    .on_complete = on_complete_impl,
    .on_tx_start = on_tx_start_impl,
    .packet_reply_timeout_ms = 20,
    .adc_threshold = 110
};

bool received_charger_comm_packet = false;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void init_earbuds(void)
{
    /* All other fields should be zero initialised by the CRT. */

    left_earbud.current_adc_val = current_senses_left_adc_value();
    left_earbud.adc_buf = adc_buf_left;

    right_earbud.current_adc_val = current_senses_right_adc_value();
    right_earbud.adc_buf = adc_buf_right;
}

void charger_comms_device_init(void)
{
    vreg_init();

    /* The 20-17759-H2 uses a different current sense and hence the threshold
     * for comms must be adjusted.
     */
    if (config_get_board_id() == 20177593)
    {
        cfg.adc_threshold = 55;
    }
    charger_comms_init(&cfg);
    init_earbuds();
    current_senses_init();
}

static void on_complete_impl(void)
{
    timer_comms_tick_stop();
    power_clear_run_reason(POWER_RUN_CHARGER_COMMS);

    /* No need to listen any more. */

    current_senses_clear_sense_amp(CURRENT_SENSE_AMP_COMMS);
    vreg_pfm();

    if ((left_earbud.data_valid) || (right_earbud.data_valid))
    {
        received_charger_comm_packet = true;
    }
}

static void on_tx_start_impl(uint8_t *buf, uint8_t num_tx_octets)
{
    cli_tx_hex(CLI_BROADCAST, "WIRE->COMMS", buf, num_tx_octets);

    power_set_run_reason(POWER_RUN_CHARGER_COMMS);

    /* Must have the current senses switched to receive charger comms messages
     * and the regulator must be in PWM to transmit charger comms. */
    current_senses_set_sense_amp(CURRENT_SENSE_AMP_COMMS);
    vreg_pwm();

    timer_comms_tick_start();

#ifdef CHARGER_COMMS_FAKE
    earbud_rx(buf, num_tx_octets);
#endif
}

void charger_comms_periodic(void)
{
    if (received_charger_comm_packet)
    {
        uint8_t result[CHARGER_COMMS_MAX_MSG_LEN];
        received_charger_comm_packet = false;

        if(left_earbud.data_valid)
        {
            charger_comms_fetch_rx_data(&left_earbud, result);
            wire_rx(EARBUD_LEFT, result, left_earbud.num_rx_octets);
        }

        if(right_earbud.data_valid)
        {
            charger_comms_fetch_rx_data(&right_earbud, result);
            wire_rx(EARBUD_RIGHT, result, right_earbud.num_rx_octets);
        }
    }

    if(charger_comms_should_read_header())
    {
        charger_comms_read_header();
    }
}
