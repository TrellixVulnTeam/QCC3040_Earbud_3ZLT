/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definitions of audio capability IDs
*/

#ifndef ULTRA_QUIET_DAC_H
#define ULTRA_QUIET_DAC_H

#include <message.h>

/*
NAME:
    LoopbackTest_SetEnableUltraQuietMode

DESCRIPTION:
    Enable ultra-quiet mode at DAC outputs
    <Note> This feature is available only in QCC516x audio firmware and afterwards.
*/
bool LoopbackTest_SetEnableUltraQuietMode(void);

/******************************************************************************
NAME:
    LoopbackTest_SetDisableUltraQuietMode

DESCRIPTION:
    Disable ultra-quiet mode at DAC output
    <Note> This feature is available only in QCC516x audio firmware and afterwards.
*/
bool LoopbackTest_SetDisableUltraQuietMode(void);

void setupUltraQuietDac(void);

#endif // ULTRA_QUIET_MODE_H
