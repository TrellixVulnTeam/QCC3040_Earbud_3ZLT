/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      List of supported voice sources.
*/

#ifndef VOICE_SOURCES_LIST_H_
#define VOICE_SOURCES_LIST_H_

typedef enum
{
    voice_source_none,
    voice_source_hfp_1,
    voice_source_hfp_2,
    voice_source_usb,
    voice_source_le_audio_unicast,
    max_voice_sources
} voice_source_t;

/*! \brief Query if a voice source is for HFP.
    \param source The source to query.
    \return TRUE if the source is an HFP source.
*/
static inline bool VoiceSource_IsHfp(voice_source_t source)
{
    return (source == voice_source_hfp_1 || source == voice_source_hfp_2);
}

#define VoiceSource_IsValid(source) ((source > voice_source_none) && (source < max_voice_sources))

#endif /* VOICE_SOURCES_LIST_H_ */
