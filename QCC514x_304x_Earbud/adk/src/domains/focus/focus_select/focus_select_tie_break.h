/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Focus Select tie break API
*/
#ifndef FOCUS_SELECT_TIE_BREAK_H
#define FOCUS_SELECT_TIE_BREAK_H

#include "focus_select_status.h"

/*! \brief Initialise tie break module. Resets tie break priorities to default.
*/
void FocusSelect_TieBreakInit(void);

/*! \brief Handle a tie break between audio sources

    \param focus_status - The cached focus rankings compiled by the caller
*/
void FocusSelect_HandleTieBreak(focus_status_t * focus_status);

/*! \brief Handle a tie break between voice sources

    \param focus_status - The cached focus rankings compiled by the caller
*/
void FocusSelect_HandleVoiceTieBreak(focus_status_t * focus_status);

#endif /* FOCUS_SELECT_TIE_BREAK_H */
