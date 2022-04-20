/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_configure.h

DESCRIPTION
    Functions required to configure the ANC Sinks/Sources.
*/

#ifndef ANC_CONFIGURE_H_
#define ANC_CONFIGURE_H_

#include "anc.h"

#include <csrtypes.h>

/******************************************************************************
NAME
    ancConfigure

DESCRIPTION
    Configure the ANC hardware
*/
bool ancConfigure(bool enable);
/******************************************************************************
NAME
    ancConfigureEnableParallelAnc

DESCRIPTION
    Enables the parallel filter configuration for ANC.
*/
bool ancConfigureEnableParallelAnc(void);
/******************************************************************************
NAME
    ancEnableWithPathGainsMuted

DESCRIPTION
    Enable the ANC hardware with filter path gains (FFA, FFB and FB paths) muted
*/
bool ancEnableWithPathGainsMuted(void);

/******************************************************************************
NAME
    ancConfigureAfterModeChange

DESCRIPTION
    (Re)Configure following an ANC mode change
*/
bool ancConfigureAfterModeChange(void);

/******************************************************************************
NAME
    ancConfigureFilterCoefficientsAfterModeChange

DESCRIPTION
    (Re)Configure filter coefficients following an ANC mode change.
*/
bool ancConfigureFilterCoefficientsAfterModeChange(void);

/******************************************************************************
NAME
    ancConfigureFilterCoefficientsPathGainsAfterModeChange

DESCRIPTION
    (Re)Configure filter coefficients and path gains following an ANC mode change.
*/
bool ancConfigureFilterCoefficientsPathGainsAfterModeChange(bool enable_coarse_gains, bool enable_fine_gains);


/******************************************************************************
NAME
    ancConfigureFilterPathGain

DESCRIPTION
    (Re)Configure filter path (FFA or FFB or FB) gain for a given ANC hardware instance

    @param instance The audio ANC hardware instance number.
    @param path The ANC filter path (valid range: FFA or FFB or FB) to set
    @param gain The ANC filter path FFA/FFb/FB gain to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool ancConfigureFilterPathGain(audio_anc_instance instance, audio_anc_path_id path, uint8 gain);

/******************************************************************************
NAME
    anConfigureEnableParallelAncPathGainsMuted
    Enable the parallel ANC hardware with filter path gains (FFA, FFB and FB paths) muted

DESCRIPTION
*/

bool ancConfigureEnableParallelAncPathGainsMuted(void);
/******************************************************************************

NAME
    ancConfigureDisableParallelAnc

DESCRIPTION
    Disables the parallel ANC hardware after muting the filter path gains (FFA,FFB and FB Path)

*/

bool ancConfigureDisableParallelAnc(void);
/******************************************************************************

NAME
    ancConfigureParallelFilterAfterModeChange

DESCRIPTION
    (Re)Configure following an parallel ANC mode change
*/

bool ancConfigureParallelFilterAfterModeChange(void);

/******************************************************************************
NAME
    ancConfigureParallelFilterCoefAfterModeChange

DESCRIPTION
    (Re)Configure filter coefficients following an Parallel ANC mode change.
*/
bool ancConfigureParallelFilterCoefAfterModeChange(void);

/******************************************************************************
NAME
    ancConfigureParallelFilterCoefPathGainsAfterModeChange

DESCRIPTION
    (Re)Configure filter coefficients and path gains following an Parallel ANC mode change.
*/
bool ancConfigureParallelFilterCoefPathGainsAfterModeChange(bool enable_coarse_gains, bool enable_fine_gains);


/******************************************************************************
NAME
    ancConfigureParallelFilterPathGain

DESCRIPTION
    (Re)Configure filter path (FFA or FFB or FB) gain for a given ANC hardware instance.
    Applicable only for parallel ANC.

    @param path The ANC filter path (valid range: FFA or FFB or FB) to set
    @param gain_instance_zero The ANC filter path FFA/FFb/FB gain to for AUDIO_ANC_INSTANCE_0 to set.
    @param gain_instance_one The ANC filter path FFA/FFb/FB gain to for AUDIO_ANC_INSTANCE_1 to set.

    Note:- For parallel path filter configuration gains for both ANC instances needs to be sent. In case the
    gain value is same a single trap will be use to update both the ANC instances, in case the gain value is different
    two different traps will be used to send the gain update to audio subsystem.

*/
bool ancConfigureParallelFilterPathGain(audio_anc_path_id path, uint8 gain_instance_zero,uint8 gain_instance_one);

#endif
