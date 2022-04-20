/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       charger_monitor_config.h
\brief      Configuration related definitions for charger monitoring.
*/

#ifndef CHARGER_MONITOR_CONFIG_H_
#define CHARGER_MONITOR_CONFIG_H_


/*! The time to debounce charger state changes (ms).
    The charger hardware will have a more limited range. */
#define appConfigChargerStateChangeDebounce()          (128)

/*! Trickle-charge current (mA) */
#ifdef QCC3020_FF_ENTRY_LEVEL_AA
#define appConfigChargerTrickleCurrent()               (8)    /* Adjust for Aura LC RDP to meet battery spec */
#else
#if (defined HAVE_RDP_HW_YE134) || (defined HAVE_RDP_HW_18689)
#define appConfigChargerTrickleCurrent()               (30)    /* Adjust for Auraplus ANC RDP to meet battery spec */
#else
#define appConfigChargerTrickleCurrent()               (10)
#endif
#endif
/*! Pre-charge current (mA)*/
#define appConfigChargerPreCurrent()                   (MIN(PRE_CHARGE_CURRENT, FAST_CHARGE_CURRENT))

/*! Pre-charge to fast-charge threshold */
#define appConfigChargerPreFastThresholdVoltage()      (3000)

/*! Critical battery threshold  */
#define appConfigChargerCriticalThresholdVoltage()     (3300)

/*! Fast-charge current (mA). Limited to 500mA if charger detection is not enabled */
#ifdef INCLUDE_CHARGER_DETECT
#define appConfigChargerFastCurrent()                  (FAST_CHARGE_CURRENT)
#else
#define appConfigChargerFastCurrent()                  (MIN(FAST_CHARGE_CURRENT, 500))
#endif

/*! Fast-charge (constant voltage) to standby transition point.
    Percentage of the fast charge current */
#define appConfigChargerTerminationCurrent()           (10)

/*! Fast-charge Vfloat voltage */
#define appConfigChargerTerminationVoltage()           (4200)

/*! Standby to fast charge hysteresis (mV) */
#define appConfigChargerStandbyFastVoltageHysteresis() (250)

/* Enable short timeouts for charger/battery platform testing */
#ifdef CF133_BATT
#define CHARGER_PRE_CHARGE_TIMEOUT_MS D_MIN(5)
#define CHARGER_FAST_CHARGE_TIMEOUT_MS D_MIN(15)
#else
#define CHARGER_PRE_CHARGE_TIMEOUT_MS D_MIN(0)
#define CHARGER_FAST_CHARGE_TIMEOUT_MS D_MIN(0)
#endif

/*! The charger will be disabled if the pre-charge time exceeds this limit.
    Following a timeout, the charger will be re-enabled when the charger is detached.
    Set to zero to disable the timeout. */
#define appConfigChargerPreChargeTimeoutMs() CHARGER_PRE_CHARGE_TIMEOUT_MS

/*! The charger will be disabled if the fast-charge time exceeds this limit.
    Following a timeout, the charger will be re-enabled when the charger is detached.
    Set to zero to disable the timeout. */
#define appConfigChargerFastChargeTimeoutMs() CHARGER_FAST_CHARGE_TIMEOUT_MS

/*! Timeout before re-enabling the charger after an error. */
#define appConfigChargerReEnableTimeoutMs() 1000

/*! Maximum current for internal charging mode */
#define appConfigChargerInternalMaxCurrent() 200

/*! Maximum current from USB compliant host when not configured */
#define appConfigChargerUsbUnconfiguredCurrent() 100

/*! IDCP_min, supported by any DCP */
#define appConfigChargerDCPMinCurrent() 500
/*! VCHG poll period for detecting charger limit */
#define appConfigChargerVchgPollingPeriodMs() 100
/*! Voltage level when to stop increasing current */
#define appConfigChargerVchgLowThreshold() 4550
/*! Voltage level when to stop recovery (decreasing current) */
#define appConfigChargerVchgRecoveryThreshold() 4650
/*! Current step in mA for increasing/decreasing current */
#define appConfigChargerVchgStep() 10

#endif /* CHARGER_MONITOR_CONFIG_H_ */
