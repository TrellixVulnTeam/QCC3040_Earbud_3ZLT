/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      ADC
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include "stm32f0xx_adc.h"
#include "stm32f0xx_dma.h"
#include "main.h"
#include "timer.h"
#include "gpio.h"
#include "adc.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* ADC_CAL_WAIT_CTR: Number of times we will check the ADCAL bit at start-up
* before giving up.
*/
#define ADC_CAL_WAIT_CTR 100

#define VREFINT_CAL_ADDR (0x1FFFF7BA)

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    char *name;
    uint32_t channel;
}
ADC_CONFIG;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT adc_cmd_status(uint8_t cmd_source);
static CLI_RESULT adc_cmd_timing(uint8_t cmd_source);
static CLI_RESULT adc_cmd_fake(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static volatile bool adc_in_progress = false;

static const ADC_CONFIG adc_config[NO_OF_ADCS] =
{
#ifdef EARBUD_CURRENT_SENSES 
    { "R",       ADC_Channel_1 },
    { "L",       ADC_Channel_3 },
#endif
    { "VBAT",    ADC_Channel_4 },
    { "NTC",     ADC_Channel_6 },
    { "VREFINT", ADC_Channel_17 }
};

static volatile uint16_t adc_value[NO_OF_ADCS] = {0};
static uint16_t adc_fake_value[NO_OF_ADCS];

static const CLI_COMMAND adc_command[] =
{
    { "",       adc_cmd_status, 2 },
    { "fake",   adc_cmd_fake,   2 },
    { "timing", adc_cmd_timing, 2 },
    { NULL }
};

TIMER_DEBUG_VARIABLES(adc)

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void adc_sleep(void)
{
    /*
    * Disable ADC peripheral clock.
    */
    RCC->APB2ENR &= ~RCC_APB2Periph_ADC1;
    RCC->AHBENR &= ~RCC_AHBPeriph_DMA1;
}

void adc_stop(void)
{
    /* Disable the reference bandgap. */
    ADC->CCR &= ~((uint32_t)ADC_CCR_VREFEN);

    /* Set the ADDIS to Disable the ADC peripheral */
    ADC1->CR |= (uint32_t)ADC_CR_ADDIS;

    RCC->APB2RSTR |= RCC_APB2Periph_ADC1;
    RCC->APB2RSTR &= ~RCC_APB2Periph_ADC1;
}

void adc_wake(void)
{
    /*
    * Enable ADC and DMA peripheral clocks.
    */
    RCC->APB2ENR |= RCC_APB2Periph_ADC1;
    RCC->AHBENR |= RCC_AHBPeriph_DMA1;

    adc_in_progress = false;
}

void adc_init(void)
{
    uint8_t n=0;

    memset(adc_fake_value, 0xFF, sizeof(adc_fake_value));

    adc_wake();

    /*
    * Calibration.
    */
    ADC1->CR |= ADC_CR_ADCAL;
    while ((ADC1->CR & ADC_CR_ADCAL) && (++n < ADC_CAL_WAIT_CTR));

    /*
    * DMA initialisation.
    */
    DMA1_Channel1->CPAR = (uint32_t)(&ADC1->DR);
    DMA1_Channel1->CMAR = (uint32_t)adc_value;
    DMA1_Channel1->CNDTR = NO_OF_ADCS;
    DMA1_Channel1->CCR =  DMA_CCR_CIRC | DMA_CCR_MINC |
        DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 | DMA_CCR_EN;

    /*
    * ADC initialisation.
    */
    ADC1->CFGR1 = ADC_CFGR1_DMAEN;

    /*
    * Enable VREFINT.
    */
    ADC->CCR |= (uint32_t)ADC_CCR_VREFEN;

    /*
    * Clear and enable EOS interrupt.
    */
    ADC1->ISR = 0xFFFFFFFF;
    ADC1->IER |= ADC_IT_EOSEQ;

    /*
    * Channel and sampling time configuration.
    */
    for (n=0; n<NO_OF_ADCS; n++)
    {
        ADC1->CHSELR |= adc_config[n].channel;
    }
    ADC1->SMPR = (uint32_t)ADC_SampleTime_71_5Cycles;

    /*
    * Enable ADC.
    */
    ADC1->CR |= (uint32_t)ADC_CR_ADEN;

    /*
     * Read ADCs now, so that we at least have stored measurements for the
     * channels that don't require any additional setup.
     */
    adc_blocking_measure();
}

bool adc_start_measuring(void)
{
    bool ret = false;

    if (!adc_in_progress)
    {
        ret = true;
        adc_in_progress = true;
        TIMER_DEBUG_START(adc)

        /*
        * Start conversion.
        */
        ADC1->CR |= (uint32_t)ADC_CR_ADSTART;
    }

    return ret;
}

void adc_blocking_measure(void)
{
    while (!adc_start_measuring());
    while (adc_in_progress);
}

static CLI_RESULT adc_cmd_status(uint8_t cmd_source)
{
    uint8_t n;

    adc_blocking_measure();

    for (n=0; n<NO_OF_ADCS; n++)
    {
        PRINTF("%-7s  %03x%s", adc_config[n].name, adc_read(n),
            (adc_fake_value[n]==0xFFFF) ? "":" (FAKE)");
    }

    return CLI_OK;
}

static CLI_RESULT adc_cmd_fake(uint8_t cmd_source __attribute__((unused)))
{
    bool ret = CLI_ERROR;
    char *tok = cli_get_next_token();

    if (tok)
    {
        uint8_t n;

        for (n=0; n<NO_OF_ADCS; n++)
        {
            if (!strcasecmp(tok, adc_config[n].name))
            {
                long int x;

                if (cli_get_next_parameter(&x, 16))
                {
                    adc_fake_value[n] = (uint16_t)x;
                }
                else
                {
                    adc_fake_value[n] = 0xFFFF;
                }

                ret = CLI_OK;
                break;
            }
        }
    }

    return ret;
}

static CLI_RESULT adc_cmd_timing(uint8_t cmd_source)
{
    PRINTF("Measurements : %d", adc_no_of_measurements);
    PRINTF("Total time   : %dms", (uint32_t)(adc_total_time_taken / 1000));
    PRINTF("Average time : %dus",
        (uint32_t)(adc_total_time_taken / adc_no_of_measurements));
    PRINTF("Slowest time : %dus", (uint32_t)adc_slowest_time);

    return CLI_OK;
}

CLI_RESULT adc_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(adc_command, cmd_source);
}

volatile uint16_t *adc_value_ptr(ADC_NO adc_no)
{
    return &adc_value[adc_no];
}

uint16_t adc_read(ADC_NO adc_no)
{
    return (adc_fake_value[adc_no]==0xFFFF) ?
        adc_value[adc_no] : adc_fake_value[adc_no];
}

uint16_t adc_read_mv(ADC_NO adc_no, uint16_t base_mv)
{
#ifdef TEST
    uint16_t cal = 0x600;
#else
    uint16_t cal = (*((uint16_t*)VREFINT_CAL_ADDR));
#endif
    uint16_t raw_adc = adc_read(adc_no);
    uint16_t ref = adc_read(ADC_VREF);

    /*
    * Prevent a possible divide by zero in the upcoming voltage calculation.
    */
    if (ref==0)
    {
        ref = cal;
    }

    return (uint16_t)((((10ull * base_mv * cal * raw_adc) / (ref * 4095)) + 5) / 10);
}

void ADC1_IRQHandler(void)
{
    volatile uint32_t isr = ADC1->ISR;

    ADC1->ISR = isr;

    if (isr & ADC_ISR_EOS)
    {
        TIMER_DEBUG_STOP(adc);
        adc_in_progress = false;
    }
}

