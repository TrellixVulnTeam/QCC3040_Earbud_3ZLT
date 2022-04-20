/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    media_player
\brief      Header file for music processing

The Media Player uses \ref audio_domain Audio domain.
*/

#ifndef MUSIC_PROCESSING_H_
#define MUSIC_PROCESSING_H_

#if defined(INCLUDE_MUSIC_PROCESSING)

/*\{*/

/*! \brief Media processing init function

    \param init_task Unused
*/
bool MusicProcessing_Init(Task init_task);

/*! \brief Gets the number of available presets (including off and user)

    \param preset  Preset ID

    \return TRUE if the preset ID is valid
*/
bool MusicProcessing_SetPreset(uint8 preset);

/*! \brief Checks if the equaliser is active

    \return TRUE if the preset ID is valid
*/
uint8 MusicProcessing_IsEqActive(void);

/*! \brief Gets active EQ type

    \return Active EQ type
*/
uint8 MusicProcessing_GetActiveEqType(void);

/*! \brief Gets the number of active bands for the user eq

    \return Number of active bands
*/
uint8 MusicProcessing_GetNumberOfActiveBands(void);

/*! \brief Sets a specific set of bands of the user EQ

    \param band_number  Start band

    \param band_number  End band

    \param band_number  Gain value
*/
bool MusicProcessing_SetUserEqBands(uint8 start_band, uint8 end_band, int16 *gains);


/*\}*/

#endif /* defined(INCLUDE_MUSIC_PROCESSING) */
#endif /* MUSIC_PROCESSING_H_ */
