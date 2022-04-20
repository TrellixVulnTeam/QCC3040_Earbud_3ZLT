/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger case application for 20-1114
*/

#ifndef MAIN_H_
#define MAIN_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*#define CHARGER_COMMS_FAKE*/
/*#define CHARGER_COMMS_FAKE_U*/

/*#ifdef HSE_VALUE
#undef HSE_VALUE
#endif
#define HSE_VALUE 8000000*/

/*
* FAST_TIMER_INTERRUPT: Enables interrupts on the fast timer, allowing
* the very convenient global_time_us to exist. Probably can't use it
* with the clock speed set to 8MHz as it causes the charger comms
* transmit to take too long.
*/
/*#define FAST_TIMER_INTERRUPT*/

#ifdef VARIANT_CB
#define VARIANT_NAME "CB"
#define SCHEME_A
#define EARBUD_CURRENT_SENSES
#endif

#ifdef VARIANT_ST2
#define VARIANT_NAME "ST2"
#define SCHEME_B
#define FORCE_48MHZ_CLOCK
#endif

#define CHARGER_BQ24230
#define USB_ENABLED

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

#endif /* MAIN_H_ */
