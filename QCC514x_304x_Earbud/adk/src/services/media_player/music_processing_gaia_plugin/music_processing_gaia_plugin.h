/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the music processing gaia framework plugin
*/

#ifndef MUSIC_PROCESSING_GAIA_PLUGIN_H_
#define MUSIC_PROCESSING_GAIA_PLUGIN_H_

#if defined(INCLUDE_GAIA) && defined(INCLUDE_MUSIC_PROCESSING)

#include <gaia_features.h>

#include <gaia_framework.h>


/*! \brief Music processing gaia plugin version
*/
#define MUSIC_PROCESSING_GAIA_PLUGIN_VERSION 1


/*! \brief These are the music processing commands provided by the GAIA framework
*/
typedef enum
{
    /*! Command to decide whether the user can interact with the User EQ settings (predefined or user set) */
    get_eq_state = 0,
    /*! Command to find out the IDs of the supported presets. Each preset is identified by a number which you’ll convert to a string and present to the user */
    get_available_eq_presets,
    /*! Command to find out what the currently selected preset (or User or off) is */
    get_eq_set,
    /*! Command to set the new preset value or user set or Off */
    set_eq_set,
    /*! Command to find out how many frequency bands the User set supports */
    get_user_set_number_of_bands,
    /*! Command to find out how many frequency bands the User set supports and the current gain value of each band */
    get_user_eq_set_configuration,
    /*! Command to set the gains of a specific set of bands */
    set_user_eq_set_configuration,
    /*! Total number of commands */
    number_of_music_processing_commands,
} music_processing_gaia_plugin_pdu_ids_t;

/*! \brief These are the media processing notifications provided by the GAIA framework
*/
typedef enum
{
    /*! Gaia Client will be told if the User EQ is not present */
    eq_state_change = 0,
    /*! Gaia Client will be told if the User EQ set (preset, User set or Off) changes */
    eq_set_change,
    /*! Gaia Client will be told if there are User EQ band changes */
    user_eq_band_change,
    /*! Total number of notifications */
    number_of_music_processing_notifications,
} music_processing_gaia_plugin_notifications_t;


/*! \brief Music processing plugin init function
*/
bool MusicProcessingGaiaPlugin_Init(Task init_task);

/*! \brief Public notification API for activating and deactivating eq

    \param eq_active    Indicator of the eq state
*/
void MusicProcessingGaiaPlugin_EqActiveChanged(uint8 eq_active);

/*! \brief Public notification API for preset change

    \param preset_id    Preset id
*/
void MusicProcessingGaiaPlugin_PresetChanged(uint8 preset_id);

/*! \brief Public notification API for band gains changed

    \param num_bands    Number of bands changed

    \param start_band   Start band
*/
void MusicProcessingGaiaPlugin_BandGainsChanged(uint8 num_bands, uint8 start_band);

#endif /* defined(INCLUDE_GAIA) && defined(INCLUDE_MUSIC_PROCESSING) */
#endif /* MUSIC_PROCESSING_GAIA_PLUGIN_H_ */
