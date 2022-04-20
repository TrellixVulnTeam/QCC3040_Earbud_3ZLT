/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   media_player Music Player
\ingroup    services
\brief      Header file for music processing peer signalling

The Media Player uses \ref audio_domain Audio domain.
*/

#ifndef MEDIA_PROCESSING_PEER_SIG_H_
#define MEDIA_PROCESSING_PEER_SIG_H_

#if defined(INCLUDE_MUSIC_PROCESSING) && defined(INCLUDE_MUSIC_PROCESSING_PEER)

/*\{*/

#include <rtime.h>


/*! \brief Music processing init function
*/
void MusicProcessingPeerSig_Init(void);

/*! \brief Sets a specific preset to the peer bud

    \param timestamp        Timestamp to delay

    \param payload_length   Payload Length

    \param payload          Payload

    \return TRUE if set
*/
bool MusicProcessingPeerSig_SetPreset(uint32 *timestamp, uint8 preset);

/*! \brief Sets a specific set of bands of the user EQ to the peer bud

    \param timestamp        Timestamp to delay

    \param payload_length   Payload Length

    \param payload          Payload

    \return TRUE if set
*/
bool MusicProcessingPeerSig_SetUserEqBands(uint32 *timestamp, uint8 start_band, uint8 end_band, int16 * gain);


/*\}*/

#endif /* INCLUDE_MUSIC_PROCESSING && INCLUDE_MUSIC_PROCESSING_PEER */
#endif /* MEDIA_PROCESSING_PEER_SIG_H_ */
