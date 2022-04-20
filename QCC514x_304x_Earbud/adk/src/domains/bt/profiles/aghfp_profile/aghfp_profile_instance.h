/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief  Application domain HFP dynamic instance management.
*/


#ifndef AGHFP_PROFILE_INSTANCE_H
#define AGHFP_PROFILE_INSTANCE_H

#include "aghfp_profile_typedef.h"
#include "voice_sources.h"
#include "device.h"

#define AGHFP_MAX_NUM_INSTANCES 1

#define for_all_aghfp_instances(aghfp_instance, iterator) for(aghfp_instance = AghfpInstance_GetFirst(iterator); aghfp_instance != NULL; aghfp_instance = AghfpInstance_GetNext(iterator))

typedef struct
{
    aghfpInstanceTaskData* instances[AGHFP_MAX_NUM_INSTANCES];
    unsigned index;
} aghfp_instance_iterator_t;

aghfpInstanceTaskData * AghfpInstance_GetFirst(aghfp_instance_iterator_t * iterator);
aghfpInstanceTaskData * AghfpInstance_GetNext(aghfp_instance_iterator_t * iterator);

aghfpInstanceTaskData * AghfpProfileInstance_Create(const bdaddr *bd_addr, bool allocate_source);
void AghfpProfileInstance_Destroy(aghfpInstanceTaskData * instance);

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForDevice(device_t device);
aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForBdaddr(const bdaddr *bd_addr);
aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForAghfp(AGHFP *aghfp);

voice_source_t AghfpProfileInstance_GetVoiceSourceForInstance(aghfpInstanceTaskData * instance);

device_t AghfpProfileInstance_FindDeviceFromInstance(aghfpInstanceTaskData* instance);

void AghfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source_t voice_source);
void AghfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_t voice_source);

void AghfpProfileInstance_SetLock(aghfpInstanceTaskData* instance, uint16 lock);
uint16 * AghfpProfileInstance_GetLock(aghfpInstanceTaskData *instance);

void AghfpProfileInstance_SetAghfp(AGHFP *aghfp_inst);

bool AghfpProfileInstance_ReachedMaxInstances(void);

#endif // AGHFP_PROFILE_INSTANCE_H
