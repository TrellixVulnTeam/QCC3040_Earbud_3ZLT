/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia anc framework plugin
*/

#ifndef ANC_GAIA_PLUGIN_H_
#define ANC_GAIA_PLUGIN_H_

#include <gaia_features.h>
#include <gaia_framework.h>
#include <anc.h>


/*! \brief Gaia ANC plugin version
*/
#define ANC_GAIA_PLUGIN_VERSION 1

#define GAIA_FEATURE_ANC        1

#define ANC_GAIA_GET_AC_STATE_PAYLOAD_LENGTH                    0x01
#define ANC_GAIA_SET_AC_STATE_PAYLOAD_LENGTH                    0x02
#define ANC_GAIA_SET_MODE_PAYLOAD_LENGTH                        0x01
#define ANC_GAIA_SET_GAIN_PAYLOAD_LENGTH                        0x02
#define ANC_GAIA_GET_TOGGLE_CONFIGURATION_PAYLOAD_LENGTH        0x01
#define ANC_GAIA_SET_TOGGLE_CONFIGURATION_PAYLOAD_LENGTH        0x02
#define ANC_GAIA_GET_SCENARIO_CONFIGURATION_PAYLOAD_LENGTH      0x01
#define ANC_GAIA_SET_SCENARIO_CONFIGURATION_PAYLOAD_LENGTH      0x02
#define ANC_GAIA_SET_DEMO_STATE_PAYLOAD_LENGTH                  0x01
#define ANC_GAIA_SET_ADAPTATION_STATUS_PAYLOAD_LENGTH           0x01

#define ANC_GAIA_GET_AC_STATE_RESPONSE_PAYLOAD_LENGTH                       0x02
#define ANC_GAIA_GET_NUM_OF_MODES_RESPONSE_PAYLOAD_LENGTH                   0x01
#define ANC_GAIA_GET_CURRENT_MODE_RESPONSE_PAYLOAD_LENGTH                   0x04
#define ANC_GAIA_GET_GAIN_RESPONSE_PAYLOAD_LENGTH                           0x04
#define ANC_GAIA_GET_TOGGLE_CONFIGURATION_COUNT_RESPONSE_PAYLOAD_LENGTH     0x01
#define ANC_GAIA_GET_TOGGLE_CONFIGURATION_RESPONSE_PAYLOAD_LENGTH           0x02
#define ANC_GAIA_GET_SCENARIO_CONFIGURATION_RESPONSE_PAYLOAD_LENGTH         0x02
#define ANC_GAIA_GET_DEMO_SUPPORT_RESPONSE_PAYLOAD_LENGTH                   0x01
#define ANC_GAIA_GET_DEMO_STATE_RESPONSE_PAYLOAD_LENGTH                     0x01
#define ANC_GAIA_ADAPTATION_STATUS_RESPONSE_PAYLOAD_LENGTH                  0x01

#define ANC_GAIA_AC_STATE_NOTIFICATION_PAYLOAD_LENGTH                       0x02
#define ANC_GAIA_MODE_CHANGE_NOTIFICATION_PAYLOAD_LENGTH                    0x04
#define ANC_GAIA_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH                    0x04
#define ANC_GAIA_TOGGLE_CONFIGURATION_NOTIFICATION_PAYLOAD_LENGTH           0x02
#define ANC_GAIA_SCENARIO_CONFIGURATION_NOTIFICATION_PAYLOAD_LENGTH         0x02
#define ANC_GAIA_DEMO_STATE_NOTIFICATION_PAYLOAD_LENGTH                     0x01
#define ANC_GAIA_ADAPTATION_STATUS_NOTIFICATION_PAYLOAD_LENGTH              0x01

#define ANC_GAIA_MIN_VALID_SCENARIO_ID      0x01
#define ANC_GAIA_MAX_VALID_SCENARIO_ID      0x04
#define ANC_GAIA_MIN_VALID_TOGGLE_WAY       0x01
#define ANC_GAIA_MAX_VALID_TOGGLE_WAY       0x03

#define ANC_GAIA_AC_FEATURE_OFFSET                  0x00
#define ANC_GAIA_AC_STATE_OFFSET                    0x01
#define ANC_GAIA_CURRENT_MODE_OFFSET                0x00
#define ANC_GAIA_CURRENT_MODE_TYPE_OFFSET           0x01
#define ANC_GAIA_ADAPTATION_CONTROL_OFFSET          0x02
#define ANC_GAIA_GAIN_CONTROL_OFFSET                0x03
#define ANC_GAIA_LEFT_GAIN_OFFSET                   0x02
#define ANC_GAIA_RIGHT_GAIN_OFFSET                  0x03
#define ANC_GAIA_SET_LEFT_GAIN_OFFSET               0x00
#define ANC_GAIA_SET_RIGHT_GAIN_OFFSET              0x01
#define ANC_GAIA_TOGGLE_OPTION_NUM_OFFSET           0x00
#define ANC_GAIA_TOGGLE_OPTION_VAL_OFFSET           0x01
#define ANC_GAIA_SCENARIO_OFFSET                    0x00
#define ANC_GAIA_SCENARIO_BEHAVIOUR_OFFSET          0x01


#define ANC_GAIA_SET_ANC_STATE_DISABLE      0x00
#define ANC_GAIA_SET_ANC_STATE_ENABLE       0x01
#define ANC_GAIA_STATE_DISABLE              0x00
#define ANC_GAIA_STATE_ENABLE               0x01

#define ANC_GAIA_DEMO_NOT_SUPPORTED         0x00
#define ANC_GAIA_DEMO_SUPPORTED             0x01
#define ANC_GAIA_DEMO_STATE_INACTIVE        0x00
#define ANC_GAIA_DEMO_STATE_ACTIVE          0x01

#define ANC_GAIA_AANC_ADAPTIVITY_PAUSED     0x00
#define ANC_GAIA_AANC_ADAPTIVITY_RESUMED    0x01
#define ANC_GAIA_AANC_ADAPTIVITY_PAUSE      ANC_GAIA_AANC_ADAPTIVITY_PAUSED
#define ANC_GAIA_AANC_ADAPTIVITY_RESUME     ANC_GAIA_AANC_ADAPTIVITY_RESUMED

#define ANC_GAIA_TOGGLE_WAY_1               0x01
#define ANC_GAIA_TOGGLE_WAY_2               0x02
#define ANC_GAIA_TOGGLE_WAY_3               0x03

#define ANC_GAIA_SCENARIO_IDLE              0x01
#define ANC_GAIA_SCENARIO_PLAYBACK          0x02
#define ANC_GAIA_SCENARIO_SCO               0x03
#define ANC_GAIA_SCENARIO_VA                0x04

#define ANC_GAIA_CONFIG_OFF                     0x00
#define ANC_GAIA_CONFIG_MODE_1                  0x01
#define ANC_GAIA_CONFIG_MODE_2                  0x02
#define ANC_GAIA_CONFIG_MODE_3                  0x03
#define ANC_GAIA_CONFIG_MODE_4                  0x04
#define ANC_GAIA_CONFIG_MODE_5                  0x05
#define ANC_GAIA_CONFIG_MODE_6                  0x06
#define ANC_GAIA_CONFIG_MODE_7                  0x07
#define ANC_GAIA_CONFIG_MODE_8                  0x08
#define ANC_GAIA_CONFIG_MODE_9                  0x09
#define ANC_GAIA_CONFIG_MODE_10                 0x0A
#define ANC_GAIA_CONFIG_SAME_AS_CURRENT         0xFF
#define ANC_GAIA_TOGGLE_OPTION_NOT_CONFIGURED   0xFF

#define ANC_GAIA_STATIC_MODE        0x01
#define ANC_GAIA_LEAKTHROUGH_MODE   0x02
#define ANC_GAIA_ADAPTIVE_MODE      0x03

#define ANC_GAIA_ADAPTATION_CONTROL_NOT_SUPPORTED   0x00
#define ANC_GAIA_ADAPTATION_CONTROL_SUPPORTED       0x01
#define ANC_GAIA_GAIN_CONTROL_NOT_SUPPORTED         0x00
#define ANC_GAIA_GAIN_CONTROL_SUPPORTED             0x01


/*! \brief These are the ANC commands provided by the GAIA framework
*/
typedef enum
{
    /*! To provide state of Audio Curation(AC) of Primary earbud(AC state is always synchronized between earbuds) */
    anc_gaia_get_ac_state = 0,
    /*! Enables/Disables  Audio Curation and state will be synchronized between earbuds */
    anc_gaia_set_ac_state,
    /*! Returns number of mode configurations supported */
    anc_gaia_get_num_modes,
    /*! Returns current mode configuration of primary earbud */
    anc_gaia_get_current_mode,
    /*! Configures Audio Curation with particular configuration of parameters, mode will be synchronoized between earbuds */
    anc_gaia_set_mode,
    /*! Returns configured gain for current mode on primary earbud */
    anc_gaia_get_gain,
    /*! Sets gain for current mode, gain will be synchronized between earbuds */
    anc_gaia_set_gain,
    /*! Returns number of toggle configurations supported */
    anc_gaia_get_toggle_configuration_count,
    /*! Returns current toggle configuration of primary earbud */
    anc_gaia_get_toggle_configuration,
    /*! Configures a toggle way, configuration will be synchronized between earbuds */
    anc_gaia_set_toggle_configuration,
    /*! Returns current scenario configuration of primary earbud */
    anc_gaia_get_scenario_configuration,
    /*! Configures a scenario behaviour, configuration will be synchronized between earbuds */
    anc_gaia_set_scenario_configuration,
    /*! To identify if demo mode is supported by devie */
    anc_gaia_get_demo_support,
    /*! Returns current state of demo mode on primary earbud */
    anc_gaia_get_demo_state,
    /*! Enables/disables demo mode and state will be communicated to peer device */
    anc_gaia_set_demo_state,
    /*! Returns adaptation status of primary earbud */
    anc_gaia_get_adaptation_control_status,
    /*! Enables/disables adaptation and control will be synchronized between earbuds */
    anc_gaia_set_adaptation_control_status,
    /*! Total number of commands.*/
    number_of_anc_commands
} anc_gaia_plugin_command_ids_t;


/*! \brief These are the ANC notifications provided by the GAIA framework
*/
typedef enum
{
    /*! The device sends the notification when AC state gets updated on the device */
    anc_gaia_ac_state_notification = 0,
    /*! The device sends the notification when mode gets updated on the device */
    anc_gaia_mode_change_notification,
    /*! The device sends the notification when gain gets updated on the device */
    anc_gaia_gain_change_notification,
    /*! The device sends the notification when toggle configuration gets updated on the device */
    anc_gaia_toggle_configuration_notification,
    /*! The device sends the notification when scenario configuration gets updated on the device */
    anc_gaia_scenario_configuration_notification,
    /*! The device sends the notification when demo state gets updated on the device */
    anc_gaia_demo_state_notification,
    /*! The device sends the notification when adaptation status gets updated on the device */
    anc_gaia_adaptation_status_notification,
    /*! Total number of notifications */
    number_of_anc_notifications
} anc_gaia_plugin_notification_ids_t;


/*! \brief Gaia Anc plugin init function
*/
void AncGaiaPlugin_Init(void);


#endif /* ANC_GAIA_PLUGIN_H_ */
