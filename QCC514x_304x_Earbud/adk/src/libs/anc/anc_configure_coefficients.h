/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_configure_coefficients.h

DESCRIPTION

*/

#ifndef ANC_CONFIGURE_COEFFICIENTS_H_
#define ANC_CONFIGURE_COEFFICIENTS_H_

#include <app/audio/audio_if.h>
#include "anc_config_data.h"
#include "anc_data.h"

/* TODO - Below defines are temporary. These will be available in stream_if.h */
#define STREAM_ANC_CONTROL_1 0x1112
#define STREAM_ANC_RX_MIX_FFA_GAIN   0x1113
#define STREAM_ANC_RX_MIX_FFA_SHIFT  0x1114
#define STREAM_ANC_RX_MIX_FFB_GAIN   0x1115
#define STREAM_ANC_RX_MIX_FFB_SHIFT  0x1116

anc_instance_config_t * getInstanceConfig(audio_anc_instance instance);
void ancConfigureMutePathGains(void);
void ancConfigureFilterCoefficients(void);
void ancConfigureFilterPathGains(void);
void ancConfigureFilterPathCoarseGains(void);
void ancConfigureFilterPathFineGains(void);
bool ancConfigureGainForFFApath(audio_anc_instance instance, uint8 gain);
bool ancConfigureGainForFFBpath(audio_anc_instance instance, uint8 gain);
bool ancConfigureGainForFBpath(audio_anc_instance instance, uint8 gain);
bool ancConfigureParallelGainForFFApath(uint8 gain_instance_zero, uint8 gain_instance_one);
bool ancConfigureParallelGainForFFBpath(uint8 gain_instance_zero, uint8 gain_instance_one);
bool ancConfigureParallelGainForFBpath(uint8 gain_instance_zero, uint8 gain_instance_one);
void ancOverWriteWithUserPathGains(void);
void ancConfigureParallelFilterMutePathGains(void);
void ancConfigureParallelFilterCoefficients(void);
void ancConfigureParallelFilterPathGains(bool enable_coarse_gains, bool enable_fine_gains);
void ancEnableOutMix(void);
void ancEnableAnc1UsesAnc0RxPcmInput(void);
uint32 getAncInstanceMask(anc_instance_mask_t instance_mask);

void setRxMixGains(anc_filter_topology_t topology);
void setRxMixEnables(anc_filter_topology_t topology);

#endif /* ANC_CONFIGURE_COEFFICIENTS_H_ */

