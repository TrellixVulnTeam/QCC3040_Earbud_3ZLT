/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Charger monitor module data
*/

#ifndef CHARGER_DATA_H_
#define CHARGER_DATA_H_

#include "charger_monitor.h"

/*! Internal message IDs used by the Charger module */
enum charger_monitor_internal_messages
{
    /*! Used to limit the time spent in some charge phases */
    CHARGER_INTERNAL_CHARGE_TIMEOUT  = INTERNAL_MESSAGE_BASE,
    /*! Attempt to re-enable the charger disabled due to error */
    CHARGER_INTERNAL_RE_ENABLE_TIMEOUT,
    /*! Periodic VCHG polling */
    CHARGER_INTERNAL_VCHG_MEASUREMENT,

    /*! This must be the final message */
    CHARGER_INTERNAL_MESSAGE_END
};

/*! List charger types that can be detected by the application */
typedef enum
{
    /*! Charger is not attached */
    CHARGER_TYPE_DETACHED,

    /*! Normal data-capable USB port */
    CHARGER_TYPE_SDP,

    /*! BC1.2 compliant wall brick */
    CHARGER_TYPE_DCP,

    /*! BC1.2 compliant data-capable USB port */
    CHARGER_TYPE_CDP,

    /*! USB-C charger allowing to draw 1.5A*/
    CHARGER_TYPE_USB_C_15,

    /*! USB-C charger allowing to draw 3.0A*/
    CHARGER_TYPE_USB_C_30,

    /*! Floating data lines charger */
    CHARGER_TYPE_FLOAT,

    /*! Charger that is not USB */
    CHARGER_TYPE_NON_USB,

    /*! Proprietary charger providing 500mA */
    CHARGER_TYPE_PROPRIETARY_500,

    /*! Proprietary charger providing 1000mA */
    CHARGER_TYPE_PROPRIETARY_1000,

    /*! Proprietary charger providing 2100mA */
    CHARGER_TYPE_PROPRIETARY_2100,

    /*! Proprietary charger providing 2400mA */
    CHARGER_TYPE_PROPRIETARY_2400,

    /*! Proprietary charger providing 2000mA */
    CHARGER_TYPE_PROPRIETARY_2000,

    /*! "safe" config that should work with any charger - used then we don't
     * know which charger is attached */
    CHARGER_TYPE_SAFE,

    /*! Charger type is not resolved */
    CHARGER_TYPE_NOT_RESOLVED,
} charger_detect_type;

/*! Charger config */
typedef struct
{
    /*! Charger type value from charger_detect_type enum */
    unsigned charger_type:6;

    /*! Adjust current in response to USB suspend and enumeration events */
    unsigned usb_events_apply:1;
    /*! Gradually increase current while monitoring voltage on VCHG to
     * detect charger limit. */
    unsigned current_limiting:1;
    /*! Maximum current in mA */
    unsigned current:12;
} charger_config_t;

/*! The charger module internal state */
typedef struct
{
    /*! Charger task */
    TaskData         task;
#define CHARGER_CONNECTION_UNKNOWN 2
    /*! Set when charger is connected */
    unsigned         is_connected:2;
    /*! Set when charger is enabled */
    unsigned         is_enabled:1;
    /*! When TRUE power off is always allowed. */
    unsigned         force_allow_power_off:1;
    /*! When TRUE earbud cannot enter dormant mode while in charger case*/
    unsigned         disallow_dormant:1;
    /*! Charger Detection is in the test mode with test
     * values overriding hardware states */
    unsigned         test_mode:1;
    /*! TRUE is external charging mode is enabled */
    unsigned         ext_mode_enabled:1;

    /*! Configured fast current */
    unsigned         fast_current:12;
    /*! Maximum current supported by the charger HW */
    unsigned         max_supported_current:12;

    /*! Power supply is switched to the battery */
    unsigned         power_source_vbat:1;

#ifdef INCLUDE_CHARGER_DETECT

    /*! USB enumeration status */
    unsigned         usb_enumerated:1;
    /*! USB suspend status */
    unsigned         usb_suspend:1;
    /*! LEDs powered from USB are forced off */
    unsigned         usb_leds_forced_off:1;

    /*! VCHG monitor is enabled */
    unsigned         vchg_monitor_enabled:1;
    /*! ADC reading is pending */
    unsigned         vchg_monitor_read_pending:1;

    /*! Whether limit was detected */
    unsigned         current_limit_detected:1;
    /*! Detection mode - increasing current monitoring VCHG drop or
     * decreasing monitoring VCHG restore. */
    unsigned         current_increasing:1;
    /*! Detected current limit */
    unsigned         current_limit:12;

    /*! Cached VREF value for ADC conversion */
    uint16           vref_reading;

    /*! Previous charger current reading */
    uint16           chg_mon_reading;

#endif /* INCLUDE_CHARGER_DETECT */

    /*! The current charger status */
    charger_status   status;
    /*! Current charger attached status */
    usb_attached_status test_attached_status;
    /*! Reasons the charger is disabled (bitfield). */
    ChargerDisableReason disable_reason;

#ifdef INCLUDE_CHARGER_DETECT

    const charger_config_t *charger_config;


#endif /* INCLUDE_CHARGER_DETECT */
} chargerTaskData;

extern chargerTaskData charger_data;
#define appGetCharger()     (&charger_data)


#endif /* CHARGER_DATA_H_ */
