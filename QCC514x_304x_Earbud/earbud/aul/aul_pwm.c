#include <led.h>

#include "aul_private.h"
#include "aul_common.h"
#include "aul_pwm.h"

#include "aul_debug.h"

/*
        QCC3040 module and DK board LED pin configure

        DK        Module
------------------------------------------
        LD1 ----- LED0 -> Used in ref app
        LD2 ----- LED1 -> Available
        LD3 ----- LED3 -> Available
        LD4 ----- Not connected
*/

#define AUL_PWM_CH0			LED_0
#define AUL_PWM_CH1			LED_1
#define AUL_PWM_CH2			LED_2


static void aul_pwm_read_start(uint32 period)
{
    MessageCancelAll(aul_stateTaskGet(), AUL_PWM_START);

    MessageSendLater(aul_stateTaskGet(), AUL_PWM_START, NULL, period);
}

void aulPwm_init(void)
{
    aul_pwm_read_start(2000);
}

void aul_msg_pwm_start_handle(void)
{
    LedConfigure(AUL_PWM_CH0, LED_PERIOD, 0);
    LedConfigure(AUL_PWM_CH0, LED_DUTY_CYCLE, 0x3FF/2);
    LedConfigure(AUL_PWM_CH0, LED_ENABLE, 1);

    LedConfigure(AUL_PWM_CH1, LED_PERIOD, 0);
    LedConfigure(AUL_PWM_CH1, LED_DUTY_CYCLE, 0x7FF/2);
    LedConfigure(AUL_PWM_CH1, LED_ENABLE, 1);

    LedConfigure(AUL_PWM_CH2, LED_PERIOD, 0);
    LedConfigure(AUL_PWM_CH2, LED_DUTY_CYCLE, 0xFFF/2);
    LedConfigure(AUL_PWM_CH2, LED_ENABLE, 1);
}

