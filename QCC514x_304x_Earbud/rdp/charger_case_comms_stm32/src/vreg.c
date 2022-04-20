/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Regulator.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"
#include "gpio.h"
#include "vreg.h"
#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/** Reasons to switch the voltage regulator OFF. */
static uint8_t vreg_off_reason = 0;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void vreg_enable_evaluate(void)
{
    if (vreg_off_reason) 
    {
        vreg_disable(); 
    }
    else
    {
        vreg_enable();
    }
}

void vreg_off_set_reason(VREG_REASON_OFF reason)
{
    vreg_off_reason |= (1 << reason);
    vreg_enable_evaluate();
}

void vreg_off_clear_reason(VREG_REASON_OFF reason)
{
    vreg_off_reason &= ~(1 << reason);
    vreg_enable_evaluate();
}

#ifdef SCHEME_A

void charger_comms_vreg_high(void)
{
    GPIO_RESET(GPIO_VREG_MOD);
    GPIO_OUTPUT(GPIO_VREG_MOD);
}

void charger_comms_vreg_low(void)
{
    GPIO_INPUT(GPIO_VREG_MOD);
}

void charger_comms_vreg_reset(void)
{
    GPIO_SET(GPIO_VREG_MOD);
    GPIO_OUTPUT(GPIO_VREG_MOD);
}

void vreg_init(void)
{
    /* Enable the regulator so earbuds will be charging by default.  */
    vreg_off_reason = 0;
    vreg_pfm();
    vreg_enable();
    charger_comms_vreg_high();
}

void vreg_pwm(void)
{
    gpio_enable(GPIO_VREG_PFM_PWM);
}

void vreg_pfm(void)
{
    gpio_disable(GPIO_VREG_PFM_PWM);
}

void vreg_enable(void)
{
    gpio_enable(GPIO_VREG_EN);
}

void vreg_disable(void)
{
    gpio_disable(GPIO_VREG_EN);
}

#else

void vreg_init(void)
{
    /* Enable the regulator so earbuds will be charging by default.  */
    gpio_disable(GPIO_VREG_SEL);
    vreg_enable();
}

void vreg_enable(void)
{
    gpio_disable(GPIO_DOCK_PULL_EN);
    gpio_enable(GPIO_VREG_ISO);
    gpio_enable(GPIO_VREG_EN);
}

void vreg_disable(void)
{
    gpio_disable(GPIO_VREG_ISO);
    gpio_disable(GPIO_VREG_EN);
}

#endif /*SCHEME_A*/


bool vreg_is_enabled(void)
{
    return gpio_active(GPIO_VREG_EN);
}

CLI_RESULT ats_regulator(uint8_t cmd_source __attribute__((unused)))
{
    bool ret = CLI_ERROR;
    long int en;

    if (cli_get_next_parameter(&en, 10))
    {
        if (en)
        {
            long int level;

            if (cli_get_next_parameter(&level, 10))
            {
#ifdef SCHEME_A
                vreg_pwm();
                switch (level)
                {
                    default:
                    case 0:
                        charger_comms_vreg_high();
                        break;

                    case 1:
                        charger_comms_vreg_low();
                        break;

                    case 2:
                        charger_comms_vreg_reset();
                        break;
                }
#else
                switch (level)
                {
                    case 0:
                        gpio_disable(GPIO_VREG_SEL);
                        break;

                    case 1:
                        gpio_enable(GPIO_VREG_SEL);
                        break;

                    default:
                        return CLI_ERROR;
                }
#endif
            }

            vreg_off_clear_reason(VREG_REASON_OFF_COMMAND);
            vreg_enable();
        }
        else
        {
            vreg_disable();
            vreg_off_set_reason(VREG_REASON_OFF_COMMAND);
        }

        ret = CLI_OK;
    }
    return ret;
}
