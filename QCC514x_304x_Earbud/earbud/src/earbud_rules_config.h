/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_rules_config.h
\brief      Configuration related definitions for connection rules.
*/

#ifndef EARBUD_RULES_CONFIG_H_
#define EARBUD_RULES_CONFIG_H_


/*! Only allow upgrades when the request has been made by the user (through the UI)
    and the device is in the case.
  */
#define appConfigDfuOnlyFromUiInCase()              FALSE

/*! Can BLE be used to perform upgrades when not in the case */
#define appConfigDfuAllowBleUpgradeOutOfCase()      TRUE

/*! Can BREDR be used to perform upgrades when not in the case */
#define appConfigDfuAllowBredrUpgradeOutOfCase()    TRUE

/*! Allow 2nd Earbud to connect to TWS+ Handset after pairing */
#define ALLOW_CONNECT_AFTER_PAIRING (TRUE)

/* The FORCE_LED_FLASHES macro preprocessor symbol is used to override Physical State
   and any configured LED filters from being able to mask LED flash pattern indications.
   This is used when targetting development board builds of the Earbud App (e.g. CF376)
   to ensure visibility of the LED flashes at all times. */
#if defined (FORCE_LED_FLASHES) || defined(CF133) || defined(CG437) || defined(CF020) || defined(CORVUS_YD300)
/*! Allow LED indications when Earbud is in ear */
#define appConfigInEarLedsEnabled() (TRUE)
#else
/*! Only enable LED indications when Earbud is out of ear */
#define appConfigInEarLedsEnabled() (FALSE)
#endif /* FORCE_LED_FLASHES */

    /********************************************
     *   SETTINGS for Bluetooth Low Energy (BLE)
     ********************************************/

/*! Define whether BLE is allowed when out of the case.

    Restricting to use in the case only will reduce power
    consumption and extend battery life. It will not be possible
    to start an upgrade or read battery information.

    \note Any existing BLE connections will not be affected
    when leaving the case.
 */
#define appConfigBleAllowedOutOfCase()          (FALSE)

#endif /* EARBUD_RULES_CONFIG_H_ */
