/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       touchpad_psoc4000s.h
\brief      Header file for Cypress Touch Pad PSoc 4000S
*/

#ifndef TOUCHPAD_PSOC4000S_PRIVATE_H
#define TOUCHPAD_PSOC4000S_H

#ifdef HAVE_TOUCHPAD_PSOC4000S

#include <types.h>
#include "touch.h"
/*! I2C communication addresses */
#define TOUCHPAD_I2C_ADDRESS                (0x08)

/*! @name Touch Events. */
//!@{
#define TOUCH_DATA_FIRST_BYTE                   (0xFF)
#define TOUCH_DATA_SECOND_BYTE                  (0x03)
#define TOUCH_DATA_THIRD_BYTE                   (0x01)
#define TOUCH_DATA_FOURTH_BYTE                  (0x00)
#define TOUCH_DATA_SLIDER_UP                    (0x04)
#define TOUCH_DATA_SLIDER_DOWN                  (0x05)
#define TOUCH_DATA_SLIDER_LEFT                  (0x06)
#define TOUCH_DATA_SLIDER_RIGHT                 (0x07)
#define TOUCH_DATA_SINGLE_PRESS                 (0x08)
#define TOUCH_DATA_LONG_PRESS                   (0x0F)
#define TOUCH_DATA_DOUBLE_PRESS                 (0x12)
#define TOUCH_DATA_TRIPLE_PRESS                 (0x14)
#define TOUCH_DATA_HANDCOVER                    (0x0A)
#define TOUCH_DATA_HANDCOVER_RELEASE            (0x0B)
#define TOUCH_DATA_VERY_LONG_PRESS              (0x10)
#define TOUCH_DATA_VERY_VERY_LONG_PRESS         (0x11)
#define TOUCH_DATA_VERY_VERY_VERY_LONG_PRESS    (0x13)
#define TOUCH_DATA_DOUBLE_PRESS_HOLD            (0x17)
#define TOUCH_DATA_TRIPLE_PRESS_HOLD            (0x15)
#define TOUCH_DATA_FOUR_PRESS_HOLD              (0x16)
#define TOUCH_DATA_FIVE_PRESS_HOLD              (0x18)
#define TOUCH_DATA_FOUR_PRESS                   (0x19)
#define TOUCH_DATA_FIVE_PRESS                   (0x1A)
#define TOUCH_DATA_SIX_PRESS                    (0x1B)
#define TOUCH_DATA_SEVEN_PRESS                  (0x1C)
#define TOUCH_DATA_EIGHT_PRESS                  (0x1D)
#define TOUCH_DATA_NINE_PRESS                   (0x1E)
#define TOUCH_DATA_PRESS                        (0x20)
#define TOUCH_DATA_RELEASE                      (0x21)
#define TOUCH_DATA_UNDEFINED                    (0xFF)

//!@}

#define touchConfigPressCancelMs()              (300)
#define touchConfigMaximumHeldTimeSeconds()     (65)
#define touchConfigClickTimeoutlMs()            (400)
#define touchConfigSensorResetHoldMs()          (50)

/*! The high level configuration of touch sensor */
struct __touch_config_t
{
    /*! The I2C clock frequency */
    uint16 i2c_clock_khz;
    /*! The PIOs used to control/communicate with the sensor */
    struct
    {
        /*! PIO used to power-on the sensor, or #VNCL3020_ON_PIO_UNUSED */
        uint8 xres;
        /*! Interrupt PIO driven by the sensor */
        uint8 interrupt;
        /*! I2C serial data PIO */
        uint8 i2c_sda;
        /*! I2C serial clock PIO */
        uint8 i2c_scl;
    } pios;
};
typedef struct
{
    touch_action_t   action;
    uint8     touch_data;

} touch_data_to_action_t;

enum
{
    TOUCH_INTERNAL_HELD_RELEASE,
    TOUCH_INTERNAL_HELD_CANCEL_TIMER,
    TOUCH_INTERNAL_HELD_TIMER,
    TOUCH_INTERNAL_CLICK_TIMER,
    TOUCH_INTERNAL_RESET_TIMER,
};

#endif /* HAVE_TOUCHPAD_PSOC4000S */
#endif /* TOUCHPAD_PSOC4000S_PRIVATE_H */
