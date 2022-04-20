/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Detection Config
*/
#include "logging.h"

#include "charger_detect.h"
#include "charger_detect_config.h"

#include "usb_device.h"
#include "panic.h"

/*! Normal data-capable USB port */
const charger_config_t config_sdp = {
        .charger_type = CHARGER_TYPE_SDP,
        .usb_events_apply = 1,
        .current          = 500
};

/*! BC1.2 compliant wall brick */
const charger_config_t config_dcp = {
        .charger_type = CHARGER_TYPE_DCP,
        .current_limiting = 1,
        .current          = 1500
};

/*! BC1.2 compliant data-capable USB port */
const charger_config_t config_cdp = {
        .charger_type = CHARGER_TYPE_CDP,
        .current_limiting = 1,
        .current          = 1500
};

/*! USB-C charger allowing to draw 1.5A*/
const charger_config_t config_usbc_15 = {
        .charger_type = CHARGER_TYPE_USB_C_15,
        .current          = 1500
};

/*! USB-C charger allowing to draw 3.0A*/
const charger_config_t config_usbc_30 = {
        .charger_type = CHARGER_TYPE_USB_C_30,
        .current          = 1800
};

/*! Floating data lines charger */
const charger_config_t config_floating = {
        .charger_type = CHARGER_TYPE_FLOAT,
        .current          = 500
};

/*! Charger that is not USB */
const charger_config_t config_non_usb = {
        .charger_type = CHARGER_TYPE_NON_USB,
        .current          = 500
};

/*! "safe" config that should work with any charger - used then we don't
 * know which charger is attached */
const charger_config_t config_safe = {
        .charger_type = CHARGER_TYPE_SAFE,
        .current          = 100
};

/*! Charger is not attached */
const charger_config_t config_detached = {
        .charger_type = CHARGER_TYPE_DETACHED,
        .current          = 0
};

/*! Structure for describing a proprietary charger with positive
 * biases on USB DP and USB DM lines. */
typedef struct
{
    /*! Voltage on USB DP line in mV */
    uint16 dp_mV;
    /*! Voltage on USB DM line in mV */
    uint16 dm_mV;
    /*! Tolerance for USB DP and USB DM voltages in mV */
    uint16 tolerance_mV;
    /*! Charger config */
    charger_config_t config;
} proprietary_charger_config_t;

/*! Proprietary chargers configuration table */
const proprietary_charger_config_t proprietary_chargers[] =
{
        /* Apple 2.4A */
        {.dp_mV = 2700, .dm_mV = 2700, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_2400,
                 .current = 1800}},

        /* Apple 2.1A */
        {.dp_mV = 2700, .dm_mV = 2000, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_2100,
                 .current = 1800}},

        /* Apple 1.0A */
        {.dp_mV = 2000, .dm_mV = 2700, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_1000,
                 .current = 1000}},

        /* Apple 0.5A */
        {.dp_mV = 2000, .dm_mV = 2000, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_500,
                 .current = 500}},

        /* Samsung 2.0A */
        {.dp_mV = 1200, .dm_mV = 1200, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_2000,
                 .current = 1800}},

        /* HTC 1.0A */
        {.dp_mV = 3100, .dm_mV = 3100, .tolerance_mV = 300,
         .config = {
                 .charger_type = CHARGER_TYPE_PROPRIETARY_1000,
                 .current = 1000}},
};

const charger_config_t *ChargerDetect_GetConfig(MessageChargerDetected *msg)
{
    switch (msg->attached_status)
    {
        case DETACHED:
            return &config_detached;

        case NON_USB_CHARGER:
            DEBUG_LOG_INFO("ChargerDetect: detected NON-USB charger");
            return &config_non_usb;

        case UNKNOWN_STATUS:
        case CHARGING_PORT:
            /* Ignore */
            return NULL;

        default:
            break;
    }

    switch (msg->cc_status)
    {
        case CC_CURRENT_1500:
            DEBUG_LOG_INFO("ChargerDetect: detected USB-C 1.5A");
            return &config_usbc_15;
        case CC_CURRENT_3000:
            DEBUG_LOG_INFO("ChargerDetect: detected USB-C 3.0A");
            return &config_usbc_30;

        default:
            break;
    }

    switch (msg->attached_status)
    {
        case HOST_OR_HUB:
            DEBUG_LOG_INFO("ChargerDetect: detected SDP");
            return &config_sdp;

        case DEDICATED_CHARGER:
            DEBUG_LOG_INFO("ChargerDetect: detected DCP");
            return &config_dcp;

        case HOST_OR_HUB_CHARGER:
            DEBUG_LOG_INFO("ChargerDetect: detected CDP");
            return &config_cdp;

        case FLOATING_CHARGER:
            DEBUG_LOG_INFO("ChargerDetect: detected floating data lines");
            return &config_floating;

        case NON_COMPLIANT_CHARGER:
        {

            int i;
            const proprietary_charger_config_t *cfg;
            for (i=0; i < ARRAY_DIM(proprietary_chargers); i++)
            {
                cfg = &proprietary_chargers[i];
                if (cfg->dp_mV - cfg->tolerance_mV <= msg->charger_dp_millivolts &&
                    cfg->dp_mV + cfg->tolerance_mV >= msg->charger_dp_millivolts &&
                    cfg->dm_mV - cfg->tolerance_mV <= msg->charger_dm_millivolts &&
                    cfg->dm_mV + cfg->tolerance_mV >= msg->charger_dm_millivolts)
                {
                    DEBUG_LOG_INFO("ChargerDetect: detected proprietary config #%d", i);
                    return &cfg->config;
                }
            }

            DEBUG_LOG_INFO("ChargerDetect: detected unknown proprietary");

            /* No match in proprietary chargers, should be safe to treat
             * as Floating Data Lines. */
            return &config_floating;
        }

        default:
            break;
    }

    return NULL;
}

const charger_config_t *ChargerDetect_GetConnectedConfig(bool charger_connected)
{
    return charger_connected ?
            &config_safe :
            &config_detached;
}
