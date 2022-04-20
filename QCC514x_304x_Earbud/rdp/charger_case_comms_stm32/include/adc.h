/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      ADC
*/

#ifndef ADC_H_
#define ADC_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
#ifdef EARBUD_CURRENT_SENSES
    ADC_CURRENT_SENSE_R,
    ADC_CURRENT_SENSE_L,
#endif
    ADC_VBAT,
    ADC_NTC,
    ADC_VREF,
    NO_OF_ADCS
}
ADC_NO;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void adc_init(void);
void adc_sleep(void);
void adc_stop(void);
void adc_wake(void);
bool adc_start_measuring(void);
void adc_blocking_measure(void);
CLI_RESULT adc_cmd(uint8_t cmd_source);
uint16_t adc_read(ADC_NO adc_no);
uint16_t adc_read_mv(ADC_NO adc_no, uint16_t base_mv);
volatile uint16_t *adc_value_ptr(ADC_NO adc_no);

#endif /* ADC_H_ */
