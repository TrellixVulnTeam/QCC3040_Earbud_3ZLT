/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_eq.h
\brief      File consists of function decalration for Amazon Voice Service's EQ handling.
*/
#ifndef AMA_EQ_H
#define AMA_EQ_H

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
/*! \brief Initialize the AMA EQ module.
*/
bool Ama_EqInit(void);

/*! \brief Update the EQ gains
*/
void Ama_EqUpdate(void);

/*! \brief Get the bass EQ gain as a percentage
*/
uint32 Ama_EqGetEqualizerBass(void);

/*! \brief Get the mid EQ gain as a percentage
*/
uint32 Ama_EqGetEqualizerMid(void);

/*! \brief Get the treble EQ gain as a percentage
*/
uint32 Ama_EqGetEqualizerTreble(void);

/*! \brief Set the bass EQ gain as a percentage
*/
void Ama_EqSetEqualizerBass(uint32 bass_gain);

/*! \brief Set the mid EQ gain as a percentage
*/
void Ama_EqSetEqualizerMid(uint32 mid_gain);

/*! \brief Set the treble EQ gain as a percentage
*/
void Ama_EqSetEqualizerTreble(uint32 treble_gain);
#else
#define Ama_EqInit() ((void)0)
#define Ama_EqUpdate    (NULL)
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */
#endif // AMA_EQ_H

