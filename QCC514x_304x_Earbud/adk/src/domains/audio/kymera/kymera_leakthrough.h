/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_leakthrough.h
\brief      Kymera implementation to accommodate software leak-through.
*/

#ifndef KYMERA_LEAKTHROUGH_H
#define KYMERA_LEAKTHROUGH_H

/*! \brief Initialises leakthrough kymera task*/
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughInit(void);
#else
#define Kymera_LeakthroughInit() ((void)(0))
#endif

/*! \brief Returns the status of leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
bool Kymera_IsLeakthroughActive(void);
#else
#define Kymera_IsLeakthroughActive() (FALSE)
#endif

/*! \brief Stops the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughStopChainIfRunning(void);
#else
#define Kymera_LeakthroughStopChainIfRunning() ((void)(0))
#endif

/*! \brief Resumes the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughResumeChainIfSuspended(void);
#else
#define Kymera_LeakthroughResumeChainIfSuspended() ((void)(0))
#endif

/*! \brief Creates the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_CreateLeakthroughChain(void);
#else
#define Kymera_CreateLeakthroughChain() ((void)(0))
#endif

/*! \brief Destroys the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_DestroyLeakthroughChain(void);
#else
#define Kymera_DestroyLeakthroughChain() ((void)(0))
#endif

/*! \brief Set up the leakthrough sidetone gain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughSetupSTGain(void);
#else
#define Kymera_LeakthroughSetupSTGain() ((void)(0))
#endif

/*! \brief Step up the leakthrough sidetone gain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughStepupSTGain(void);
#else
#define Kymera_LeakthroughStepupSTGain() ((void)(0))
#endif

#endif /* KYMERA_LEAKTHROUGH_H */
