/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

#ifndef CB_GPIO_H_
#define CB_GPIO_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* A0: MAG_SENSOR
*/
#define GPIO_MAG_SENSOR                 GPIO_A0

/*
* A1: R-CURRENT_SENSE
*/
#define GPIO_R_CURRENT_SENSE            GPIO_A1

/*
* A2: VBAT_MONITOR_ON_OFF
*/
#define GPIO_VBAT_MONITOR_ON_OFF        GPIO_A2

/*
* A3: L-CURRENT_SENSE
*/
#define GPIO_L_CURRENT_SENSE            GPIO_A3

/*
* A4: VBAT_MONITOR
*/
#define GPIO_VBAT_MONITOR               GPIO_A4

/*
* A5: CHG_CE_N
*/
#define GPIO_CHG_CE_N                   (GPIO_A5 | GPIO_ACTIVE_LOW)

/*
* A6: CHG_TD
* A6: CHG_NTC_MONITOR
*/
#define GPIO_CHG_TD                     GPIO_A6
#define GPIO_NTC_MONITOR                GPIO_A6

/*
* A7: VREG_EN
*/
#define GPIO_VREG_EN                    GPIO_A7

/*
* A8: NTC_MONITOR_ON_OFF
*/
#define GPIO_NTC_MONITOR_ON_OFF         GPIO_A8

/*
* A11: USB_D_N
*/
#define GPIO_USB_D_N                    GPIO_A11

/*
* A12: USB_D_P
*/
#define GPIO_USB_D_P                    GPIO_A12

/*
* B0: CHG_EN1
*/
#define GPIO_CHG_EN1                    GPIO_B0

/*
* B1: CHG_EN2
*/
#define GPIO_CHG_EN2                    GPIO_B1

/*
* B2: CHG_Status_N
*/
#define GPIO_CHG_STATUS_N               (GPIO_B2 | GPIO_ACTIVE_LOW)

/*
* B3: LED_RED
*/
#define GPIO_LED_RED                    (GPIO_B3 | GPIO_ACTIVE_LOW)

/*
* B4: LED_GREEN
*/
#define GPIO_LED_GREEN                  (GPIO_B4 | GPIO_ACTIVE_LOW)

/*
* B5: LED_BLUE
*/
#define GPIO_LED_BLUE                   (GPIO_B5 | GPIO_ACTIVE_LOW)

/*
* B6: UART_TX
*/
#define GPIO_UART_TX                    GPIO_B6

/*
* B7: UART_RX
*/
#define GPIO_UART_RX                    GPIO_B7

/*
* B9: VREG_MOD
*/
#define GPIO_VREG_MOD                   GPIO_B9

/*
* B10: VREG_PFM_PWM
*/
#define GPIO_VREG_PFM_PWM               GPIO_B10

/*
* B11: VREG_PG_STATUS
*/
#define GPIO_VREG_PG                    GPIO_B11

/*
* B12: Current-Sense_Amp
*/
#define GPIO_CURRENT_SENSE_AMP          GPIO_B12

/*
* C13: VCHG_Detect
*/
#define GPIO_CHG_SENSE                  GPIO_C13

#endif /* CB_GPIO_H_ */
