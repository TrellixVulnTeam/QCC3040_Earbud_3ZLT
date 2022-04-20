/*!
\copyright  Copyright (c) 2017 - 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       a2dp_profile_caps.c
\brief      Defines the A2DP capabilities
*/

#include "a2dp_profile_caps.h"

#include <a2dp.h>
#include <panic.h>

#include "av.h"

/*! \brief Update sample rate in an SBC capability

    \param  caps        Pointer to a capability record. Such as retrieved by appA2dpFindServiceCategory(),
                        \b see a2dp_profile.c.
    \param  sample_rate Rate we want to set.
 */
void appAvUpdateSbcCapabilities(uint8 *caps, uint32 sample_rate)
{
    caps[4] &= ~(SBC_SAMPLING_FREQ_48000 | SBC_SAMPLING_FREQ_44100);
    switch (sample_rate)
    {
        case 48000:
            caps[4] |= SBC_SAMPLING_FREQ_48000;
            break;

        case 44100:
            caps[4] |= SBC_SAMPLING_FREQ_44100;
            break;

        default:
            Panic();
            break;
    }
}
