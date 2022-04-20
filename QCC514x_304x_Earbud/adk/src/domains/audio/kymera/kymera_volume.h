/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Kymera Volume Helpers
*/

#include "csrtypes.h"

/*! Kymera requires gain specified in unit of 1/60th db */
#define KYMERA_DB_SCALE (60)

/*! Volume level in dB equivalent to muting */
#define VOLUME_MUTE_IN_DB  (-90)

/*!@{ \name Useful gains in kymera operators format */
#define GAIN_HALF (-6 * KYMERA_DB_SCALE)
#define GAIN_FULL (0)
#define GAIN_MIN (-90 * KYMERA_DB_SCALE)
/*!@} */

/*! \brief Convert volume in dB Kymera internal gain format.
    \param volume The volume in dB.
    \return The kymera internal gain equivalent to the specified volume.
 */
int32 Kymera_VolDbToGain(int16 volume);
