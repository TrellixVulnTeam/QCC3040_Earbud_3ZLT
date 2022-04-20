/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Handles music processing chain

*/

#ifndef KYMERA_MUSIC_PROCESSING_H_
#define KYMERA_MUSIC_PROCESSING_H_


/*@{*/

#define PS_KEY_USER_EQ_PARAMS    9
#define PS_KEY_USER_EQ_PRESET_INDEX    0
#define PS_KEY_USER_EQ_START_GAINS_INDEX    1

/*! \brief The KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK message content. */
typedef struct
{
    /*! Preset ID for the new user EQ */
    uint8 preset;
} KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK_T;

/*! \brief The KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS message content. */
typedef struct
{
    /*! Start band of gain changes */
    uint8 start_band;
    /*! End band of gain hanges */
    uint8 end_band;
    /*! Gains list for the bands */
    int16 *gain;
} KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS_T;

void Kymera_InitMusicProcessing(void);

/*! \brief Check if music processing chain is present and can be used.

    \return TRUE if chain is present
*/
bool Kymera_IsMusicProcessingPresent(void);

/*! \brief Create music processing operators.

    Currently only Speaker EQ is implemented, which doesn't require any parameters.
*/
void Kymera_CreateMusicProcessingChain(void);

/*! \brief Configure music processing operators.

    Currently only Speaker EQ is implemented, which doesn't require any parameters.
*/
void Kymera_ConfigureMusicProcessing(uint32 sample_rate);

void Kymera_StartMusicProcessingChain(void);

void Kymera_StopMusicProcessingChain(void);

void Kymera_DestroyMusicProcessingChain(void);

bool Kymera_SelectEqBankNow(uint8 bank);

bool Kymera_SetUserEqBandsNow(uint8 start_band, uint8 end_band, int16 * gains);

/*@}*/

#endif /* KYMERA_MUSIC_PROCESSING_H_ */
