/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the voice enhancement gaia framework plugin
*/

#ifndef VOICE_ENHANCEMENT_GAIA_PLUGIN_H_
#define VOICE_ENHANCEMENT_GAIA_PLUGIN_H_

#if defined(INCLUDE_GAIA) && defined(INCLUDE_CVC_DEMO)
#include <gaia_features.h>
#include <gaia_framework.h>

/*! \brief Voice Enhancement gaia plugin version */
#define VOICE_ENHANCEMENT_GAIA_PLUGIN_VERSION 0

#define CVC_SEND_MODE_CHANGE_PAYLOAD_LENGTH 4
#define CVC_SEND_GET_SUPPORTED_PAYLOAD_LENGTH 2
#define CVC_SEND_GET_CONFIG_PAYLOAD_LENGTH 4

/*! \brief These are the voice enhancement commands provided by the GAIA framework */
typedef enum
{
    /*! Command to find out which enhancements are supported by the device */
    get_supported_voice_enhancements = 0,
    /*! Command to set a mode */
    set_config_voice_enhancement = 1,
    /*! Command to get a mode */
    get_config_voice_enhancement = 2,
    /*! Total number of commands */
    number_of_voice_enhancement_commands,
} voice_enhancement_gaia_plugin_pdu_ids_t;

/*! \brief These are the voice enhancement notifications provided by the GAIA framework */
typedef enum
{
    /*! Gaia Client will be informed about a mode change */
    voice_enhancement_mode_change = 0,
    /*! Total number of notifications */
    number_of_voice_enhancement_notifications,
} voice_enhancement_gaia_plugin_notifications_t;

/*! Capabilities supported */
typedef enum
{
    VOICE_ENHANCEMENT_CAP_NONE = 0,
    VOICE_ENHANCEMENT_CAP_CVC_3MIC = 1,
} voice_enhancement_cap_t;

/*! Supported enhancements */
typedef enum
{
    VOICE_ENHANCEMENT_NO_MORE_DATA = 0,
    VOICE_ENHANCEMENT_MORE_DATA = 1,
} voice_enhancement_supported_t;

/*! \brief Voice enhancement plugin init function */
bool VoiceEnhancementGaiaPlugin_Init(Task init_task);

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_CVC_DEMO) */
#endif /* VOICE_ENHANCEMENT_GAIA_PLUGIN_H_ */
