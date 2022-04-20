/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       connection_manager_config.h
\brief      Configuration related definitions for the connection manager state machine.
*/

#ifndef CONNECTION_MANAGER_CONFIG_H_
#define CONNECTION_MANAGER_CONFIG_H_

#include <rtime.h>

/*! Page timeout in BT SLOTS to use as one earbud attempting connection to the other Earbud. */
#define appConfigEarbudPageTimeout()    (MS_TO_BT_SLOTS(10000))

/*! Page timeout in BT SLOTS to use for connecting to any AG. */
#define appConfigPageTimeout()          con_manager.page_timeout

/*! The page timeout multiplier for Handsets after link-loss.
    Multiplier should be chosen carefully to make sure total page timeout doesn't exceed 0xFFFF */
#define appConfigHandsetLinkLossPageTimeoutMultiplier()    (4)

/* Connection manager timer of to apply BLE connection parameters after GATT service discovery */
#define appConfigDelayApplyBleParamsOnPairingSecs()   (2)

/* Connection manager timer to allow BLE parameter update if no response received */
#define appConfigDelayBleParamUpdateTimeout()   (15)

/*! Maximum number of Handsets can be connected to earbud at the same time */
#define appConfigMaxNumOfHandsetsCanConnect()  (1)
#endif /* CONNECTION_MANAGER_CONFIG_H_ */
