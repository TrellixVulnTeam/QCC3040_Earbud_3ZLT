/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

#ifndef ST2_GPIO_H_
#define ST2_GPIO_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* A0: MAG_SENSOR
*/
#define GPIO_MAG_SENSOR                 GPIO_A0

/*
* A2: VBAT_MONITOR_ON_OFF
*/
#define GPIO_VBAT_MONITOR_ON_OFF        GPIO_A2

/*
* A4: VBAT_MONITOR
*/
#define GPIO_VBAT_MONITOR               GPIO_A4

/*
* A5: CHG_CE_N
*/
#define GPIO_CHG_CE_N                   (GPIO_A5 | GPIO_ACTIVE_LOW)

/*
* A6: NTC_Detect
*/
#define GPIO_NTC_MONITOR                GPIO_A6

/*
* A8: NTC_BIAS
*/
#define GPIO_NTC_MONITOR_ON_OFF         GPIO_A8

/*
* A9: CHG_TD_ON 
*/
#define GPIO_CHG_TD_ON                  GPIO_A9

/*
* A10: VREG_ISO 
*/
#define GPIO_VREG_ISO                   GPIO_A10

/*
* A11: USB_DN
*/
#define GPIO_USB_D_N                    GPIO_A11

/*
* A12: USB_DP
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
* B8: DOCK_PULL_EN
*/
#define GPIO_DOCK_PULL_EN               GPIO_B8

/*
* B10: Dock_Data_TX
*/
#define GPIO_DOCK_DATA_TX               GPIO_B10

/*
* B11: Dock_Data_RX
*/
#define GPIO_DOCK_DATA_RX               GPIO_B11

/*
* B14: VREG_EN
*/
#define GPIO_VREG_EN                    GPIO_B14

/*
* B15: VREG_SEL
*/
#define GPIO_VREG_SEL                   GPIO_B15

/*
* C13: VCHG_Detect
*/
#define GPIO_CHG_SENSE                  GPIO_C13

#endif /* ST2_GPIO_H_ */
