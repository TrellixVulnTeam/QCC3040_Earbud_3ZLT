/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Music processing component
*/

#if defined(INCLUDE_MUSIC_PROCESSING)

#include <kymera.h>

#include "music_processing.h"

#if defined(INCLUDE_MUSIC_PROCESSING_PEER)
#include "music_processing_peer_sig.h"
#endif /* INCLUDE_MUSIC_PROCESSING_PEER */

#ifdef INCLUDE_GAIA
#include "music_processing_gaia_plugin.h"
#endif /* INCLUDE_GAIA */

bool MusicProcessing_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_VERBOSE("MusicProcessing_Init");

#if defined(INCLUDE_MUSIC_PROCESSING_PEER)
    MusicProcessingPeerSig_Init();
#endif /* INCLUDE_MUSIC_PROCESSING_PEER */

    return TRUE;
}

bool MusicProcessing_SetPreset(uint8 preset)
{
    uint32 delay = 0;
    bool preset_set = FALSE;

    DEBUG_LOG_VERBOSE("MusicProcessing_SetPreset %d", preset);


#if defined(INCLUDE_MUSIC_PROCESSING_PEER)
    MusicProcessingPeerSig_SetPreset(&delay, preset);
#endif /* INCLUDE_MUSIC_PROCESSING_PEER */

    preset_set = Kymera_SelectEqBank(delay, preset);

#ifdef INCLUDE_GAIA
    if (preset_set)
    {
        DEBUG_LOG_VERBOSE("MusicProcessing_SetPreset, send gaia notification");
        MusicProcessingGaiaPlugin_PresetChanged(preset);
    }
#endif /* INCLUDE_GAIA */

    return preset_set;
}

uint8 MusicProcessing_IsEqActive(void)
{
    return Kymera_UserEqActive();
}

uint8 MusicProcessing_GetActiveEqType(void)
{
    uint8 selected_bank = Kymera_GetSelectedEqBank();
    DEBUG_LOG_VERBOSE("MusicProcessing_GetActiveEqType %d", selected_bank);

    return selected_bank;
}

uint8 MusicProcessing_GetNumberOfActiveBands(void)
{
    uint8 num_of_bands = Kymera_GetNumberOfEqBands();
    DEBUG_LOG_VERBOSE("MusicProcessing_GetNumberOfActiveBands %d", num_of_bands);

    return num_of_bands;
}

bool MusicProcessing_SetUserEqBands(uint8 start_band, uint8 end_band, int16 *gains)
{
    uint32 delay = 0;
    bool user_eq_bands_set = FALSE;

    DEBUG_LOG_VERBOSE("MusicProcessing_SetUserEqBands start band %d, end band %d, first gain %d",
            start_band, end_band, gains[0]);

#if defined(INCLUDE_MUSIC_PROCESSING_PEER)
    MusicProcessingPeerSig_SetUserEqBands(&delay, start_band, end_band, gains);
#endif /* INCLUDE_MUSIC_PROCESSING_PEER */

    user_eq_bands_set = Kymera_SetUserEqBands(delay, start_band, end_band, gains);

    return user_eq_bands_set;
}

#endif /* defined(INCLUDE_MUSIC_PROCESSING) */
