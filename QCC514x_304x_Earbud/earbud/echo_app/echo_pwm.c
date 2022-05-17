#include <led.h>
#include <panic.h>

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
#define PIO2BANK(pio) ((uint16)((pio) / 32))
/*
 * 1. PIO BANK DATA LOAD
 * USER Definition
*/
//#define PioCommonPioBank(_pio) ((_pio) / PIOS_PER_BANK)
/*
 * 1-2. SDK Provided
*/
//#define PIO2MASK(pio) (1UL << ((pio) % 32))
/*
 * 2. PIO MASK LOAD
 * USER Definition
*/
//#define PioCommonPioMask(_pio) (1UL << ((_pio) % PIOS_PER_BANK))
/*
 * 2-2. SDK Provided
*/

//uint32 PioSetMapPins32Bank(uint16 bank, uint32 mask, uint32 bits);
/*
 * 3. Binding BANK AND MASK
*/

//uint32 PioSetDir32Bank(uint16 bank, uint32 mask, uint32 dir);
/*
 * 4. IO Mode Configuration
 * MASK : OUTPUT
 * 0    : INPUT
*/

//uint32 PioSet32Bank(uint16 bank, uint32 mask, uint32 bits);
/*
 * 5. PIN LEVEL Configuration
 * MASK : HIGH LEVEL
 * 0    : LOW LEVEL
*/

#if 0
#define ECHO_PWM_CH0		LED_0
#define ECHO_PWM_CH1		LED_1
#define ECHO_PWM_CH2        LED_2
#endif

#if 1
#define ECHO_PWM_CH0            2
#define ECHO_PWM_CH1            3
#define ECHO_PWM_CH2            4
#endif

#define ECHO_PIO_MASK0          PioCommonPioMask(ECHO_PWM_CH0)
#define ECHO_PIO_MASK1          PioCommonPioMask(ECHO_PWM_CH1)
#define ECHO_PIO_MASK2          PioCommonPioMask(ECHO_PWM_CH2)

#define ECHO_PIO_BANK0          PioCommonPioBank(ECHO_PWM_CH0)
#define ECHO_PIO_BANK1          PioCommonPioBank(ECHO_PWM_CH1)
#define ECHO_PIO_BANK2          PioCommonPioBank(ECHO_PWM_CH2)

#define ECHO_PIO_SET_MAP0       PioSetMapPins32Bank(ECHO_PIO_BANK0,ECHO_PIO_MASK0,ECHO_PWM_CH0)
#define ECHO_PIO_SET_MAP1       PioSetMapPins32Bank(ECHO_PIO_BANK1,ECHO_PIO_MASK1,ECHO_PWM_CH1)
#define ECHO_PIO_SET_MAP2       PioSetMapPins32Bank(ECHO_PIO_BANK2,ECHO_PIO_MASK2,ECHO_PWM_CH2)

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
    //PanicNotZero(PioSetMapPins32Bank(ECHO_PIO_BANK2,ECHO_PIO_MASK2,ECHO_PWM_CH2));
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
