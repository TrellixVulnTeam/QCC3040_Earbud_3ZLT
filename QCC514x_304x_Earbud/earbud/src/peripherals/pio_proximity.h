/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       pio_proximity.h
\brief      Header file for pio_proximity 
*/

#ifndef PIO_PROXIMITY_PRIVATE_H
#define PIO_PROXIMITY_PRIVATE_H

#ifdef HAVE_PIO_PROXIMITY

#include <types.h>

/*! The 'on' PIO will not be controlled when __proximity_config.pio.on is set to
    this value */
#define PROXIMITY_ON_PIO_UNUSED 255


/*! The high level configuration for taking measurement */
struct __proximity_config
{
    struct
    {
        /*! PIO used to power-on the sensor, or #PROXIMITY_ON_PIO_UNUSED */
        uint8 on;
        /*! Interrupt PIO driven by the sensor */
        uint8 interrupt;

    } pios;
};

/*! Internal representation of proximity state */
enum proximity_states
{
    proximity_state_unknown,
    proximity_state_in_proximity,
    proximity_state_not_in_proximity
};

/*! Trivial state for storing in-proximity state */
struct __proximity_state
{
    /*! The sensor proximity state */
    enum proximity_states proximity;
};

#endif /* HAVE_PIO_PROXIMITY */
#endif /* PIO_PROXIMITY_PRIVATE_H */
