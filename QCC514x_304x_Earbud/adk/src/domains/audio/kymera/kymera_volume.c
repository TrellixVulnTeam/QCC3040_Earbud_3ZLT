/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Kymera Volume Helpers
*/

#include "kymera_config.h"
#include "kymera_volume.h"

int32 Kymera_VolDbToGain(int16 volume_in_db)
{
    int32 gain = VOLUME_MUTE_IN_DB;
    if (volume_in_db > appConfigMinVolumedB())
    {
        gain = volume_in_db;
        if(gain > appConfigMaxVolumedB())
        {
            gain = appConfigMaxVolumedB();
        }
    }
    return (gain * KYMERA_DB_SCALE);
}

