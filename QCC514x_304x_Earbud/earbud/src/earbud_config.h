/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_config.h
\brief      Application configuration file
\defgroup   configuration All configurable items
*/

#ifndef EARBUD_CONFIG_H_
#define EARBUD_CONFIG_H_

#include <earbud_init.h>

/*! @{ */
/*! Left and Right device selection.

Only devices that have fixed Left and Right roles are supported. This can be 
selected using the Bluetooth Device Address, see #USE_BDADDR_FOR_LEFT_RIGHT,
or by using an external input.

If an alternative mechanism is used for Left / Right, then the  macros used 
here should be replaced (possibly by functions) that return the correct
role selection. */

#ifdef USE_BDADDR_FOR_LEFT_RIGHT
/* Left and right earbud roles are selected from Bluetooth address. */
/*! TRUE if this is the left earbud (Bluetooth address LAP is odd). */
#define appConfigIsLeft()           (InitGetTaskData()->appInitIsLeft)
/*! TRUE if this is the right earbud (Bluetooth address LAP is even). */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#else
/*! Left and right earbud roles are selected from the state of this PIO */
#define appConfigHandednessPio()    (2)
/*! TRUE if this is the left earbud (the #appConfigHandednessPio state is 1) */
#define appConfigIsLeft()           ((PioGet32Bank(appConfigHandednessPio() / 32) & (1UL << appConfigHandednessPio())) ? 1 : 0)
/*! TRUE if this is the right earbud (the #appConfigHandednessPio state is 0) */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#endif
/*! Number of trusted devices supported */
#define appConfigEarbudMaxDevicesSupported()     (6)

/*! @} */

/*! Default state proxy events to register */
#define appConfigStateProxyRegisteredEventsDefault()            \
                   (state_proxy_event_type_phystate |           \
                    state_proxy_event_type_is_pairing)

#ifdef INCLUDE_FAST_PAIR
/*! User will need to change this in Project DEFS (default is BOARD_TX_POWER_PATH_LOSS=236)as per the hardware used. */
#define appConfigBoardTxPowerPathLoss        (BOARD_TX_POWER_PATH_LOSS)
#endif

/*! place the audio types in the order you wish the audio_router to prioritise their routing,
    with the highest priority first */
#define AUDIO_TYPE_PRIORITIES {source_type_voice, source_type_audio}

/*! Initialize major and minor upgrade version information*/
#define UPGRADE_INIT_VERSION_MAJOR (1)
#define UPGRADE_INIT_VERSION_MINOR (0)

/*! The factory-set PS config version. After a successful upgrade the values from
    the upgrade header will be written to the upgrade PS key and used in future.*/
#define UPGRADE_INIT_CONFIG_VERSION (1)

/*! Initialize silent commit supported information*/
#define UPGRADE_SILENT_COMMIT_SUPPORTED (1)

/*!
\defgroup   configurable Typically configured items
\ingroup    configuration
*/

#define PIO_UNUSED                   (255)

#endif /* EARBUD_CONFIG_H_ */
