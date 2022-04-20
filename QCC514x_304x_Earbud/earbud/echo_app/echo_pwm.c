#include <led.h>

#include "echo_private.h"
#include "echo_common.h"
#include "echo_pwm.h"

#include "echo_debug.h"

/*
        QCC3040 module and DK board LED pin configure

        DK        Module
------------------------------------------
        LD1 ----- LED0 -> Used in ref app
        LD2 ----- LED1 -> Available
        LD3 ----- LED3 -> Available
        LD4 ----- Not connected
*/

#define ECHO_PWM_CH0			LED_0
#define ECHO_PWM_CH1			LED_1
#define ECHO_PWM_CH2			LED_2


static void Echo_Pwm_Read_Start(uint32 period)
{
    MessageCancelAll(ECHO_Get_Task_State(), ECHO_PWM_START);

    MessageSendLater(ECHO_Get_Task_State(), ECHO_PWM_START, NULL, period);
}

void Echo_Pwm_Init(void)
{
    Echo_Pwm_Read_Start(2000);
}

void Echo_Pwm_Start_Handle(void)
{
    LedConfigure(ECHO_PWM_CH0, LED_PERIOD, 0);
    LedConfigure(ECHO_PWM_CH0, LED_DUTY_CYCLE, 0x3FF/2);
    LedConfigure(ECHO_PWM_CH0, LED_ENABLE, 1);

    LedConfigure(ECHO_PWM_CH1, LED_PERIOD, 0);
    LedConfigure(ECHO_PWM_CH1, LED_DUTY_CYCLE, 0x7FF/2);
    LedConfigure(ECHO_PWM_CH1, LED_ENABLE, 1);

    LedConfigure(ECHO_PWM_CH2, LED_PERIOD, 0);
    LedConfigure(ECHO_PWM_CH2, LED_DUTY_CYCLE, 0xFFF/2);
    LedConfigure(ECHO_PWM_CH2, LED_ENABLE, 1);
}
