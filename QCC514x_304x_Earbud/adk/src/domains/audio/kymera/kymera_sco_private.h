/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera private header for sco audio use case handler
*/

#ifndef KYMERA_SCO_PRIVATE_H_
#define KYMERA_SCO_PRIVATE_H_

#include "kymera.h"
#include "kymera_internal_msg_ids.h"

/*! \brief The KYMERA_INTERNAL_SCO_START message content. */
typedef struct
{
    /*! The SCO audio sink. */
    Sink audio_sink;
    /*! Pointer to SCO chain information. */
    const appKymeraScoChainInfo *sco_info;
    /*! The link Wesco. */
    uint8 wesco;
    /*! The starting volume. */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself before starting kymera SCO. */
    uint8 pre_start_delay;
    /* If TRUE, the chain will be started muted. It will unmute at the time set
    by the function #Kymera_ScheduleScoSyncUnmute, or after a timeout if that
    function isn't called. */
    bool synchronised_start;
    /*! Function to call when SCO chain is started */
    Kymera_ScoStartedHandler started_handler;
} KYMERA_INTERNAL_SCO_START_T;

/*! \brief The KYMERA_INTERNAL_SCO_MIC_MUTE message content. */
typedef struct
{
    /*! TRUE to enable mute, FALSE to disable mute. */
    bool mute;
} KYMERA_INTERNAL_SCO_MIC_MUTE_T;

/*! \brief The KYMERA_INTERNAL_SCO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_SCO_SET_VOL_T;

/*! \brief Init SCO component
 */
void Kymera_ScoInit(void);

/*! \brief Handle request to start SCO.
    \param audio_sink The BT SCO audio sink (source of SCO audio).
    \param codec The HFP codec type.
    \param wesco WESCO parameter.
    \param volume_in_db Initial volume.
    \param synchronised_start If TRUE, the chain will be started muted.
    \return TRUE if successfully able to start SCO else FALSE.
*/
bool appKymeraHandleInternalScoStart(Sink sco_sink, const appKymeraScoChainInfo *info,
                                     uint8 wesco, int16 volume_in_db, bool synchronised_start);

/*! \brief Handle request to stop SCO. */
void appKymeraHandleInternalScoStop(void);

/*! \brief Handle request to set SCO volume.
    \param volume_in_db The requested volume.
*/
void appKymeraHandleInternalScoSetVolume(int16 volume_in_db);

/*! \brief Handle request to mute the SCO microphone.
    \param mute Set to TRUE to mute the microphone.
*/
void appKymeraHandleInternalScoMicMute(bool mute);

#endif /* KYMERA_SCO_PRIVATE_H_ */
