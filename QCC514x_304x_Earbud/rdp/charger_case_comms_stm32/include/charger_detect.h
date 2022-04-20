/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     USB charger detection on STM32F0xx devices.
           Distinguish between SDP, CDP, DCP and floating data line chargers. 
*/

#ifndef CHARGER_DETECT_H_
#define CHARGER_DETECT_H_

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CHARGER_DETECT_TYPE_PENDING,
    CHARGER_DETECT_TYPE_SDP,
    CHARGER_DETECT_TYPE_DCP,
    CHARGER_DETECT_TYPE_CDP,
    CHARGER_DETECT_TYPE_FLOATING
}
CHARGER_DETECT_TYPE;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Start the charger detection sequence
 */
void charger_detect_start(void);

/**
 * \brief The periodic function which drives the detection/delays
 */
void charger_detect_periodic(void);

/**
 * \brief Cancel any pending charger detection.
 */
void charger_detect_cancel(void);

/**
 * \brief Return what type of charger was last detected.
 * If CHARGER_DETECT_TYPE_PENDING is returned, the detection is still in
 * progress.
 */
CHARGER_DETECT_TYPE charger_detect_get_type(void);

#endif /* CHARGER_DETECT_H_ */

