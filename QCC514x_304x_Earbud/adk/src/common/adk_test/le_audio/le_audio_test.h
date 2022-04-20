/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common Test APIs
\brief      Interface for common LE Audio specifc testing functions.
*/
#ifndef LE_AUDIO_TEST_H
#define LE_AUDIO_TEST_H

/*! @{ */

/*! \brief Check if LE extended advertsing is enabled.

    Check the state of the le_advertising_manager to see if extended
    advertising is currently active.

    \return TRUE if extended advertising is active, FALSE othrwise.
*/
bool leAudioTest_IsExtendedAdvertisingActive(void);

/*! \brief Check if any LE Broadcast source is active.

    Active means are synced to, or in the process of syncing to,
    a PA and / or BIS.

    \return TRUE if any source is active; FALSE otherwise.
*/
bool leAudioTest_IsBroadcastReceiveActive(void);

/*! \brief Check if any LE Broadcast source is pa synced.

    \return TRUE if any source is pa synced; FALSE otherwise.
*/
bool leAudioTest_IsAnyBroadcastSourceSyncedToPa(void);

/*! \brief Check if any LE Broadcast source is synced to a BIS.

    \return TRUE if any source is BIS synced; FALSE otherwise.
*/
bool leAudioTest_IsAnyBroadcastSourceSyncedToBis(void);

/*! \brief Set the volume during LEA broadcast

    \param volume 0-255
    \return bool TRUE if the volume set request was initiated
                 else FALSE
*/
bool leAudioTest_SetVolumeForBroadcast(uint8 volume);

/*! \brief Set the mute state during LEA broadcast

    \param mute_state TRUE for mute, FALSE for unmute
    \return bool TRUE if the mute request was initiated
                 else FALSE
*/
bool leAudioTest_SetMuteForBroadcast(bool mute_state);

/*! \brief Pause receiving the broadcast stream
    \return bool TRUE if the request was initiated else FALSE.
*/
bool leAudioTest_PauseBroadcast(void);

/*! \brief Resume receiving the broadcast stream.
    \return bool TRUE if the request was initiated else FALSE.
*/
bool leAudioTest_ResumeBroadcast(void);

/*! \brief Query if the broadcast is paused.
    \return TRUE if paused.
*/
bool leAudioTest_IsBroadcastPaused(void);

/*! \brief Set the volume during LEA unicast music

    \param volume 0-255
    \return bool TRUE if the volume set request was initiated
                 else FALSE
*/
bool leAudioTest_SetVolumeForUnicastMusic(uint8 volume);

/*! \brief Set the mute state during LEA unicast music

    \param mute_state TRUE for mute, FALSE for unmute
    \return bool TRUE if the mute request was initiated
                 else FALSE
*/
bool leAudioTest_SetMuteForUnicastMusic(bool mute_state);

/*! \brief Set the volume during LEA unicast voice

    \param volume 0-255
    \return bool TRUE if the volume set request was initiated
                 else FALSE
*/
bool leAudioTest_SetVolumeForUnicastVoice(uint8 volume);

/*! \brief Set the mute state during LEA unicast voice

    \param mute_state TRUE for mute, FALSE for unmute
    \return bool TRUE if the mute request was initiated
                 else FALSE
*/
bool leAudioTest_SetMuteForUnicastVoice(bool mute_state);

/*! \brief Get the VCP volume of the current audio source

    \return VCP volume of current source, 0-255
*/
int leAudioTest_GetCurrentVcpAudioVolume(void);


/*! \brief Check if any handset is connected both BREDR and LE

    \return bool TRUE if a handset is found that has both a BREDR 
                 and LE connection
*/
bool leAudioTest_AnyHandsetConnectedBothBredrAndLe(void);

/*! @} */

#endif // LE_AUDIO_TEST_H
