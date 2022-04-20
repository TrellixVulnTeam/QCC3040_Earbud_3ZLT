/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header with lock related definitions
*/

#ifndef KYMERA_LOCK_H_
#define KYMERA_LOCK_H_

/*!@{ \name Macros to set and clear bits in the lock. */
#define appKymeraSetToneLock(theKymera) (theKymera)->lock |= 1U
#define appKymeraClearToneLock(theKymera) (theKymera)->lock &= ~1U
#define appKymeraSetA2dpStartingLock(theKymera) (theKymera)->lock |= 2U
#define appKymeraClearA2dpStartingLock(theKymera) (theKymera)->lock &= ~2U
#define appKymeraSetScoStartingLock(theKymera) (theKymera)->lock |= 4U
#define appKymeraClearScoStartingLock(theKymera) (theKymera)->lock &= ~4U
#define appKymeraSetLeStartingLock(theKymera) (theKymera)->lock |= 8U
#define appKymeraClearLeStartingLock(theKymera) (theKymera)->lock &= ~8U
#define appKymeraSetAncStartingLock(theKymera) (theKymera)->lock |= 16U
#define appKymeraClearAncStartingLock(theKymera) (theKymera)->lock &= ~16U
#define appKymeraSetAdaptiveAncStartingLock(theKymera) (theKymera)->lock |= 32U
#define appKymeraClearAdaptiveAncStartingLock(theKymera) (theKymera)->lock &= ~32U
/*!@}*/

#endif /* KYMERA_LOCK_H_ */
