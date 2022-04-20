/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation for Feature Manager APIs
*/

#include "feature_manager.h"

#include <logging.h>
#include <panic.h>

typedef struct {
    feature_id_t id;
    const feature_interface_t * interface;
} feature_manager_client_t;

static const feature_manager_priority_list_t * feature_manager_priority_list = NULL;
static feature_manager_handle_t feature_manager_handles[feature_id_max];

static bool featureManager_IsHighestPriorityFeature(feature_id_t id)
{
    return (feature_manager_priority_list->id[0] == id);
}

static void featureManager_VerifyClient(feature_manager_client_t * client)
{
    PanicNull((void *)client);
    PanicNull((void *)client->interface);
    PanicNull((void *)client->interface->GetState);

    if(!featureManager_IsHighestPriorityFeature(client->id))
    {
        PanicNull((void *)client->interface->Suspend);
        PanicNull((void *)client->interface->Resume);
    }
}

static unsigned featureManager_GetIndexForFeatureInPriorityList(feature_id_t id)
{
    unsigned index = 0xFF;

    for(unsigned i=0; i<feature_manager_priority_list->number_of_features; i++)
    {
        if(feature_manager_priority_list->id[i] == id)
        {
            index = i;
            break;
        }
    }

    PanicFalse(index != 0xFF);

    return index;
}

static feature_manager_client_t * featureManager_GetClientFromId(feature_id_t id)
{
    feature_manager_client_t * client = NULL;

    for(unsigned i=0; i<feature_id_max; i++)
    {
        feature_manager_client_t * current_client = (feature_manager_client_t *)feature_manager_handles[i];
        PanicNull(current_client);

        if(current_client->id == id)
        {
            client = current_client;
        }
    }

    PanicNull(client);

    return client;
}

static bool featureManager_IsHigherPriorityFeatureRunning(feature_manager_client_t * client_requesting_to_start)
{
    bool higher_priority_feature_running = FALSE;

    unsigned index_for_feature_in_priority_list = featureManager_GetIndexForFeatureInPriorityList(client_requesting_to_start->id);

    for(int index = index_for_feature_in_priority_list-1; index >= 0; index--)
    {
        feature_manager_client_t * client = featureManager_GetClientFromId(feature_manager_priority_list->id[index]);

        if(client->interface->GetState() == feature_state_running)
        {
            DEBUG_LOG("featureManager_IsHigherPriorityFeatureRunning enum:feature_id_t:%d is running", feature_manager_priority_list->id[index]);
            higher_priority_feature_running = TRUE;
            break;
        }
    }

    return higher_priority_feature_running;
}

static void featureManager_SuspendClient(feature_manager_client_t * client_to_suspend)
{
    feature_state_t state = client_to_suspend->interface->GetState();

    if(client_to_suspend->interface->Suspend)
    {
        if(state != feature_state_suspended)
        {
            DEBUG_LOG("featureManager_SuspendLowerPriorityFeatures suspending enum:feature_id_t:%d", client_to_suspend->id);
            client_to_suspend->interface->Suspend();
        }
        else
        {
            DEBUG_LOG("featureManager_SuspendLowerPriorityFeatures enum:feature_id_t:%d is already suspended", client_to_suspend->id);
        }
    }
}

static void featureManager_ResumeClient(feature_manager_client_t * client_to_resume)
{
    feature_state_t state = client_to_resume->interface->GetState();

    if(client_to_resume->interface->Resume)
    {
        if(state == feature_state_suspended)
        {
            DEBUG_LOG("featureManager_ResumeLowerPriorityFeatures resuming enum:feature_id_t:%d", client_to_resume->id);
            client_to_resume->interface->Resume();
        }
        else
        {
            DEBUG_LOG("featureManager_ResumeLowerPriorityFeatures enum:feature_id_t:%d was not suspended so no need to resume", client_to_resume->id);
        }
    }
}

static void featureManager_PerformActionOnLowerPriorityFeatures(feature_manager_client_t * client_requesting_to_start, void(*action)(feature_manager_client_t * client))
{
    PanicNull((void *)action);

    unsigned index_for_feature_in_priority_list = featureManager_GetIndexForFeatureInPriorityList(client_requesting_to_start->id);

    for(unsigned index = index_for_feature_in_priority_list+1; index < feature_manager_priority_list->number_of_features; index++)
    {
        feature_manager_client_t * client = featureManager_GetClientFromId(feature_manager_priority_list->id[index]);
        action(client);
    }
}

void FeatureManager_SetPriorities(const feature_manager_priority_list_t * priority_list)
{
    feature_manager_priority_list = priority_list;
}

feature_manager_handle_t FeatureManager_Register(feature_id_t feature_id, const feature_interface_t * feature_interface)
{
    DEBUG_LOG_FN_ENTRY("FeatureManager_Register enum:feature_id_t:%d", feature_id);

    PanicNull((void *)feature_manager_priority_list);
    PanicNotNull((void*) feature_manager_handles[feature_id]);

    feature_manager_client_t * handle = (feature_manager_client_t *)PanicUnlessMalloc(sizeof(feature_manager_client_t));
    handle->id = feature_id;
    handle->interface = feature_interface;

    featureManager_VerifyClient(handle);
    feature_manager_handles[feature_id] = (feature_manager_handle_t)handle;

    return feature_manager_handles[feature_id];
}

bool FeatureManager_StartFeatureRequest(feature_manager_handle_t handle)
{
    bool can_start = TRUE;
    feature_manager_client_t * client_requesting_to_start = (feature_manager_client_t *) handle;

    PanicNull((void *)feature_manager_priority_list);
    PanicNull((void *)client_requesting_to_start);

    DEBUG_LOG_FN_ENTRY("FeatureManager_StartFeatureRequest enum:feature_id_t:%d", client_requesting_to_start->id);

    if(featureManager_IsHigherPriorityFeatureRunning(client_requesting_to_start))
    {
        can_start = FALSE;
    }
    else
    {
        featureManager_PerformActionOnLowerPriorityFeatures(client_requesting_to_start, featureManager_SuspendClient);
    }

    return can_start;
}

void FeatureManager_StopFeatureIndication(feature_manager_handle_t handle)
{
    feature_manager_client_t * client_which_stopped = (feature_manager_client_t *) handle;

    PanicNull((void *)feature_manager_priority_list);
    PanicNull((void *)client_which_stopped);

    DEBUG_LOG_FN_ENTRY("FeatureManager_StopFeatureIndication enum:feature_id_t:%d", client_which_stopped->id);
    featureManager_PerformActionOnLowerPriorityFeatures(client_which_stopped, featureManager_ResumeClient);
}

#ifdef HOSTED_TEST_ENVIRONMENT
#include <stdlib.h>
void FeatureManager_Reset(void)
{
    feature_manager_priority_list = NULL;

    for(unsigned i=0; i<feature_id_max; i++)
    {
        free(feature_manager_handles[i]);
        feature_manager_handles[i] = NULL;
    }
}
#endif /* HOSTED_TEST_ENVIRONMENT */
