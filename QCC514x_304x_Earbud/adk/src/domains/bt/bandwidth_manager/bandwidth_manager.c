/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       bandwidth_manager.c
\brief      Implementation of the bandwidth manager module.
*/

#include "bandwidth_manager.h"
#include "bandwidth_manager_private.h"
#include <logging.h>
#include <message.h>
#include <panic.h>

/*=========== Local definitions and types ===========*/

#define FOR_EACH_FEATURE_PRIORITY(priority) \
    for (bandwidth_manager_priority_t priority = high_bandwidth_manager_priority; \
        priority < unused_bandwidth_manager_priority; \
        priority++)

/*=========== Local data ===========*/

/*! \brief  A boolean flag indicating whether the bandwidth manager has been initialised.
*/
#ifndef HOSTED_TEST_ENVIRONMENT
static
#endif
bool bandwidth_manager_initialised = FALSE;

/*! \brief  A structure containing the bandwidth manager information.
*/
bandwidth_manager_info_t bandwidth_manager_info = {0};

/*=========== Local functions ===========*/

/*! \brief  Get the highest priority running feature.
 *
 * \param running_feature_prios - Array of running feature priorities
 * \param default_prio - Default priority of running feature to be returned if no higer feature priority is found
 *
 * \return bandwidth_manager_priority_t - HighestPriority running feature.
*/
static bandwidth_manager_priority_t bandwidthManager_getHighestRunningFeaturePrio(const uint8 *running_feature_prios, bandwidth_manager_priority_t default_prio)
{
    bandwidth_manager_priority_t highest_priority = default_prio;
    FOR_EACH_FEATURE_PRIORITY(priority)
    {
        if(running_feature_prios[priority] && (priority < highest_priority))
        {
            highest_priority = priority;
        }
    }
    return highest_priority;
}

/*! \brief  Inform all feature clients to throttle their bandwidth usage running below than requested priority.
 *
 * \param higher_feature_prio - Priority below which all the feature clients to be informed to throttle their bandwidth usage
*/
static void bandwidthManager_throttleBandwidthOfAllLowPrioFeatures(bandwidth_manager_priority_t higher_feature_prio)
{
    DEBUG_LOG_DEBUG("bandwidthManager_throttleBandwidthOfAllLowPrioFeatures: enum:bandwidth_manager_priority_t:higher_feature_prio[%d]", higher_feature_prio);
    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        if ((feature_info->bitfields.priority > higher_feature_prio) && (feature_info->bitfields.running))
        {            
            if (!feature_info->bitfields.throttle_required)
            {
                DEBUG_LOG_DEBUG("bandwidthManager_throttleBandwidthOfAllLowPrioFeatures, enum:bandwidth_manager_feature_id_t:%d at enum:bandwidth_manager_priority_t:priority[%d] requesting to throttle",
                    feature_info->bitfields.identifier, feature_info->bitfields.priority);
                feature_info->bitfields.throttle_required = TRUE;
                feature_info->callback(feature_info->bitfields.throttle_required);
            }
            else
            {
                DEBUG_LOG_DEBUG("bandwidthManager_throttleBandwidthOfAllLowPrioFeatures, enum:bandwidth_manager_feature_id_t:%d at enum:bandwidth_manager_priority_t:priority[%d] already throttling",
                    feature_info->bitfields.identifier, feature_info->bitfields.priority);
            }

        }
    }
}

/*! \brief  Inform all feature clients to unthrottle their bandwidth usage running at requested priority.
 *
 * \param feature_prio - Priority of which all the features to be informed to unthrottle bandwidth usage
*/
static void bandwidthManager_unthrottleBandwidthOfFeatures(bandwidth_manager_priority_t feature_prio)
{
    DEBUG_LOG_DEBUG("bandwidthManager_unthrottleBandwidthOfFeatures: enum:bandwidth_manager_priority_t:feature_prio[%d]", feature_prio);

    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        if ((feature_prio == feature_info->bitfields.priority) && (feature_info->bitfields.running))
        {
            if (feature_info->bitfields.throttle_required)
            {
                DEBUG_LOG_DEBUG("bandwidthManager_unthrottleBandwidthOfFeatures, enum:bandwidth_manager_feature_id_t:%d at  enum:bandwidth_manager_priority_t:priority[%d] requesting to unthrottle",
                    feature_info->bitfields.identifier, feature_info->bitfields.priority);
                feature_info->bitfields.throttle_required = FALSE;
                feature_info->callback(feature_info->bitfields.throttle_required);
            }
            else
            {
                DEBUG_LOG_DEBUG("bandwidthManager_unthrottleBandwidthOfFeatures, enum:bandwidth_manager_feature_id_t:%d at  enum:bandwidth_manager_priority_t:priority[%d] was not throttled",
                    feature_info->bitfields.identifier, feature_info->bitfields.priority);
            }
        }
    }
}

/*! \brief  A function to handle bandwidth manager actions */
static void bandwidthManager_handleAction(bandwidth_manager_feature_info_t *acting_feature_info,
    bandwidth_manager_action_msg_t action_msg)
{
    DEBUG_LOG_DEBUG("bandwidthManager_handleAction(%p, enum:bandwidth_manager_action_msg_t:%d)", acting_feature_info, action_msg);
    uint8 running_at_priority[unused_bandwidth_manager_priority] = {0};
    uint8 total_running = 0;

    /* Determine what features at the various priorities are currently running */
    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        if (feature_info->bitfields.running)
        {
            running_at_priority[feature_info->bitfields.priority]++;
        }
    }

    FOR_EACH_FEATURE_PRIORITY(priority)
    {
        DEBUG_LOG_DEBUG("bandwidthManager_handleAction: features[uint8:%d] running at enum:bandwidth_manager_priority_t:priority[%d]", running_at_priority[priority], priority);
        total_running += running_at_priority[priority];
    }

    switch (action_msg)
    {
        case BANDWIDTH_MGR_ACTION_MSG_START:
            /*
             * A feature has started to use Bluetooth bandwidth.
             */
            if (total_running > 1)
            {
                switch(acting_feature_info->bitfields.priority)
                {
                    case high_bandwidth_manager_priority:
                        /*
                         * A high priority feature has started and there are one or more lower priority features running.
                         * Tell all running lower priority features to throttle bandwidth usage.
                         */
                        bandwidthManager_throttleBandwidthOfAllLowPrioFeatures(high_bandwidth_manager_priority);
                        break;

                    case medium_bandwidth_manager_priority:
                    case low_bandwidth_manager_priority:
                    {
                        /* Determine higest priority running feature and inform other lower priority features to throttle bandwidth usage */
                        bandwidth_manager_priority_t highest_running_prio = bandwidthManager_getHighestRunningFeaturePrio(running_at_priority, acting_feature_info->bitfields.priority);
                        bandwidthManager_throttleBandwidthOfAllLowPrioFeatures(highest_running_prio);
                    }
                    break;

                    default:
                        DEBUG_LOG_ERROR("bandwidthManager_handleAction: unknown enum:bandwidth_manager_priority_t:priority[%d]", acting_feature_info->bitfields.priority);
                        break;
                }
            }
            else
            {
                DEBUG_LOG_DEBUG("bandwidthManager_handleAction: only one feature running");
            }
            break;

        case BANDWIDTH_MGR_ACTION_MSG_STOP:
            /*
             * A feature has stopped using Bluetooth bandwidth.
             * Any features of lower priority that are running can increase their bandwidth.
             */
            if (total_running > 0)
            {
                /* Unthrottle lower priority features if there is no higher priority features are running */
                bandwidth_manager_priority_t highest_running_prio = bandwidthManager_getHighestRunningFeaturePrio(running_at_priority, low_bandwidth_manager_priority);
                if (highest_running_prio > acting_feature_info->bitfields.priority)
                {
                    bandwidthManager_unthrottleBandwidthOfFeatures(highest_running_prio);
                }
            }
            else
            {
                DEBUG_LOG_DEBUG("bandwidthManager_handleAction: last feature stopped");
            }
            break;

        default:
            DEBUG_LOG_ERROR("bandwidthManager_handleAction: unknown enum:bandwidth_manager_action_msg_t:action_msg[%d]", action_msg);
            break;
    }
}

/*! Handler of messages sent to bandwidth manager task */
static void bandwidthManager_handleMessage(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    switch(id)
    {
        case BANDWIDTH_MGR_ACTION_MSG_START:
        {
            /* Extract feature information from message payload */
            BANDWIDTH_MGR_ACTION_MSG_START_T* action_info = (BANDWIDTH_MGR_ACTION_MSG_START_T*)msg;
            bandwidth_manager_feature_info_t *feature_info = (bandwidth_manager_feature_info_t*)(action_info->feature_handle);
            bandwidthManager_handleAction(feature_info, id);
        }
        break;
        case BANDWIDTH_MGR_ACTION_MSG_STOP:
        {
            /* Extract feature information from message payload */
            BANDWIDTH_MGR_ACTION_MSG_STOP_T* action_info = (BANDWIDTH_MGR_ACTION_MSG_STOP_T*)msg;
            bandwidth_manager_feature_info_t *feature_info = (bandwidth_manager_feature_info_t*)(action_info->feature_handle);
            bandwidthManager_handleAction(feature_info, id);
        }
        break;
        default:
        {
            DEBUG_LOG_DEBUG("bandwidthManager_handleMessage: unhandle MessageId:msg[%u]", id);
        }
        break;
    }
}

/*! \brief Get the feature information using identifier if feature is registered before */
static bandwidth_manager_feature_info_t* bandwidthManager_GetFeatureInfoUsingIdentifier(bandwidth_manager_feature_id_t identifier)
{
    if (identifier < BANDWIDTH_MGR_FEATURE_MAX)
    {
        FOR_EACH_REGISTERED_FEATURE(feature_info)
        {
            if (feature_info->bitfields.identifier == identifier)
            {
                return feature_info;
            }
        }
    }
    return NULL;
}

/*! \brief Get the pointer to first non-running feature, which will be the slot to place in running feature info
 *
 * \return Pointer to first non-running feature info
 */
static bandwidth_manager_feature_info_t *bandwidthManager_GetFirstNonRunningFeature(void)
{
    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        if (!feature_info->bitfields.running)
        {
            return feature_info;
        }
    }
    return NULL;
}

/*! \brief Re-arrange all running features to take first contagious slots of feature elements array */
static bandwidth_manager_feature_info_t* bandwidthManager_ArrangeFeatureSlot(bandwidth_manager_feature_info_t *current_feature_slot)
{
    bandwidth_manager_feature_info_t *arranged_feature_slot = NULL;

    DEBUG_LOG("bandwidthManager_ArrangeFeatureSlot: feature_running[%d]", current_feature_slot->bitfields.running);
    if (current_feature_slot->bitfields.running)
    {
        /* Get the first non-running feature slot and check if it's taking early slot than this feature slot */
        arranged_feature_slot = bandwidthManager_GetFirstNonRunningFeature();
        if (arranged_feature_slot && (arranged_feature_slot < current_feature_slot))
        {
            /* Exchange the feature info */
            bandwidth_manager_feature_info_t temp = *current_feature_slot;
            *current_feature_slot = *arranged_feature_slot;
            *arranged_feature_slot = temp;
        }
        else
        {
            DEBUG_LOG_DEBUG("bandwidthManager_ArrangeFeatureSlot: No need to arrange");
            arranged_feature_slot = current_feature_slot;
        }
    }
    else
    {
        /* Check if this feature info is not the last element and the following features are actually running to move the feature elements' info.
            Otherwise, there is no need of re-arrangement of elements */
        if (
                (current_feature_slot != (bandwidth_manager_info.feature_info + (bandwidth_manager_info.registered_features_num - 1))) &&
                ((current_feature_slot + 1)->bitfields.running)
            )
        {
            bandwidth_manager_feature_info_t temp = *current_feature_slot;
            uint8 move_num_of_elements = ABS((bandwidth_manager_info.feature_info + (bandwidth_manager_info.registered_features_num -1)) - current_feature_slot);
            DEBUG_LOG_DEBUG("bandwidthManager_ArrangeFeatureSlot: move number of elements[%d]", move_num_of_elements);

            /*Shift all feature elements info by one slot and move this feature to last registered feature slot */
            memmove((void*)current_feature_slot, (const void*)(current_feature_slot + 1), move_num_of_elements * sizeof(bandwidth_manager_feature_info_t));
            arranged_feature_slot = &bandwidth_manager_info.feature_info[bandwidth_manager_info.registered_features_num -1];
            *arranged_feature_slot = temp;
        }
        else
        {
            DEBUG_LOG_DEBUG("bandwidthManager_ArrangeFeatureSlot: No need to arrange");
            arranged_feature_slot = current_feature_slot;
        }
    }

    DEBUG_LOG_DEBUG("bandwidthManager_ArrangeFeatureSlot: feature[%p]-before arrange, feature[%p]-after arrange",
                                current_feature_slot,
                                arranged_feature_slot);
    return arranged_feature_slot;
}

/*=========== Public functions ===========*/

bool BandwidthManager_Init(Task init_task)
{
    DEBUG_LOG_DEBUG("BandwidthManager_Init");
    UNUSED(init_task);
    if (bandwidth_manager_initialised == FALSE)
    {
        bandwidth_manager_initialised = TRUE;
        memset(&bandwidth_manager_info, 0, sizeof(bandwidth_manager_info_t));
        bandwidth_manager_info.task.handler = bandwidthManager_handleMessage;

        /* Make all the features to have invalid identifier initially */
        for (bandwidth_manager_feature_info_t *feature_info = bandwidth_manager_info.feature_info;
             feature_info < bandwidth_manager_info.feature_info + BANDWIDTH_MGR_FEATURE_MAX;
             feature_info++)
        {
            feature_info->bitfields.identifier = BANDWIDTH_MGR_FEATURE_INVALID_ID;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("BandwidthManager_Init: already initialised");
    }

    return TRUE;
}

bool BandwidthManager_RegisterFeature(bandwidth_manager_feature_id_t identifier, bandwidth_manager_priority_t priority,
    bandwidth_manager_callback_t callback)
{
    bool registered = FALSE;
    DEBUG_LOG_DEBUG("BandwidthManager_RegisterFeature(enum:bandwidth_manager_feature_id_t:%d, enum:bandwidth_manager_priority_t:%d, bandwidth_manager_callback_t:%p)", identifier, priority, callback);

    if ((identifier < BANDWIDTH_MGR_FEATURE_MAX) && (bandwidth_manager_info.registered_features_num < BANDWIDTH_MGR_FEATURE_MAX))
    {
        if (bandwidth_manager_initialised)
        {
            if (priority >= unused_bandwidth_manager_priority)
            {
                DEBUG_LOG_ERROR("BandwidthManager_RegisterFeature: invalid enum:bandwidth_manager_priority_t:priority[%d]", priority);
            }
            else if ((priority != high_bandwidth_manager_priority) && (callback == NULL))
            {
                DEBUG_LOG_ERROR("BandwidthManager_RegisterFeature: enum:bandwidth_manager_priority_t:priority[%d], no callback", priority);
            }
            else
            {
                /* Make sure this feature identifier is not yet registered, otherwise overwrite the feature info with new attributes */
                bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);
                if (feature_info == NULL)
                {
                    feature_info = &bandwidth_manager_info.feature_info[bandwidth_manager_info.registered_features_num++];
                }
                memset(feature_info, 0, sizeof(bandwidth_manager_feature_info_t));
                feature_info->bitfields.priority  = priority;
                feature_info->bitfields.identifier = identifier;
                feature_info->callback  = callback;
                registered = TRUE;

                DEBUG_LOG_DEBUG("BandwidthManager_RegisterFeature: enum:bandwidth_manager_feature_id_t:%d, feature_handle[%p]",
                    identifier, feature_info);
            }
        }
        else
        {
            DEBUG_LOG_ERROR("BandwidthManager_RegisterFeature: called before BandwidthManager_Init");
        }        
    }
    else
    {
        DEBUG_LOG_ERROR("BandwidthManager_RegisterFeature: invalid feature identifier registration");
    }

    return registered;
}

bool BandwidthManager_FeatureStart(bandwidth_manager_feature_id_t identifier)
{
    bool successful = FALSE;
    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);

    if (feature_info)
    {
        if (feature_info->bitfields.running)
        {
            DEBUG_LOG_ERROR("BandwidthManager_FeatureStart(enum:bandwidth_manager_feature_id_t:%d): already running",
                                                    feature_info->bitfields.identifier);
        }
        else
        {
            /* Post back message to bandwidth manager task to handle start action.
             * Otherwise, it form synchronous call to handleAction and runs in context of feature client */
            feature_info->bitfields.running   = TRUE;
            feature_info->bitfields.throttle_required = FALSE;
            bandwidth_manager_info.active_features_num++;

            bandwidth_manager_feature_info_t *arranged_slot = bandwidthManager_ArrangeFeatureSlot(feature_info);
            PanicNull((void*)arranged_slot);

            MESSAGE_MAKE(msg, BANDWIDTH_MGR_ACTION_MSG_START_T);
            msg->feature_handle = arranged_slot;
            MessageSend(bandwidthManager_GetMessageTask(), BANDWIDTH_MGR_ACTION_MSG_START, msg);
            successful = TRUE;
            DEBUG_LOG_ERROR("BandwidthManager_FeatureStart(enum:bandwidth_manager_feature_id_t:%d, feature_handle:%p): started",
                                                    arranged_slot->bitfields.identifier,
                                                    arranged_slot);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("BandwidthManager_FeatureStart(enum:bandwidth_manager_feature_id_t:%d): not registered", identifier);
    }

    return successful;
}

bool BandwidthManager_FeatureStop(bandwidth_manager_feature_id_t identifier)
{
    bool successful = FALSE;
    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);

    if (feature_info)
    {
        if (!feature_info->bitfields.running)
        {
            DEBUG_LOG_ERROR("BandwidthManager_FeatureStop(enum:bandwidth_manager_feature_id_t:%d): not running",
                                                    feature_info->bitfields.identifier);
        }
        else
        {
            /* Post back message to bandwidth manager task to handle stop action.
             * Otherwise, it form synchronous call to handleAction and runs in context of feature client */
            feature_info->bitfields.running   = FALSE;
            feature_info->bitfields.throttle_required  = FALSE;
            bandwidth_manager_info.active_features_num--;

            bandwidth_manager_feature_info_t *arranged_slot = bandwidthManager_ArrangeFeatureSlot(feature_info);
            PanicNull((void*)arranged_slot);

            MESSAGE_MAKE(msg, BANDWIDTH_MGR_ACTION_MSG_STOP_T);
            msg->feature_handle = arranged_slot;
            MessageSend(bandwidthManager_GetMessageTask(), BANDWIDTH_MGR_ACTION_MSG_STOP, msg);
            successful = TRUE;
            DEBUG_LOG_ERROR("BandwidthManager_FeatureStop(enum:bandwidth_manager_feature_id_t:%d, feature_handle:%p): stopped",
                                                    arranged_slot->bitfields.identifier,
                                                    arranged_slot);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("BandwidthManager_FeatureStop(enum:bandwidth_manager_feature_id_t:%d): not registered", identifier);
    }

    return successful;
}

bool BandwidthManager_IsThrottleRequired(bandwidth_manager_feature_id_t identifier)
{
    bool throttle_required = TRUE;
    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);

    if (feature_info)
    {
        /* Making sure the actual status of throttle requirement is valid only when feature is running.
         * By default making feature to throttle, when invalid/non-running fearure throttle status inquired */
        if(feature_info->bitfields.running)
        {
            throttle_required = feature_info->bitfields.throttle_required;
        }
    }
    else
    {
        Panic();
    }
    return throttle_required;
}

bool BandwidthManager_IsFeatureRunning(bandwidth_manager_feature_id_t identifier)
{
    bool running = FALSE;

    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);

    if (feature_info)
    {
        running = feature_info->bitfields.running;
    }
    else
    {
        Panic();
    }
    return running;
}

bandwidth_manager_priority_t BandwidthManager_GetFeaturePriority(bandwidth_manager_feature_id_t identifier)
{
    bandwidth_manager_priority_t priority = unused_bandwidth_manager_priority;

    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(identifier);

    if (feature_info)
    {
        priority = feature_info->bitfields.priority;
    }
    else
    {
        Panic();
    }
    return priority;
}

uint8 BandwidthManager_GetActiveFeaturesNum(void)
{
    return bandwidth_manager_info.active_features_num;
}

void BandwidthManager_ResetAllFeaturesInfo(void)
{
    /* Reset only throttle and running status of feature */
    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        feature_info->bitfields.running = FALSE;
        feature_info->bitfields.throttle_required = FALSE;
    }
    BandwidthManager_SetActiveFeaturesNum(0);
}

void BandwidthManager_UpdateFeatureInfo(feature_bitfields_t *bitfields)
{
    bandwidth_manager_feature_info_t *feature_info = bandwidthManager_GetFeatureInfoUsingIdentifier(bitfields->identifier);
    if (feature_info)
    {
        feature_info->bitfields = *bitfields;
        PanicNull(bandwidthManager_ArrangeFeatureSlot(feature_info));
    }
}

void BandwidthManager_RefreshFeatureThrottleStatus(void)
{
    /* Notify all the clients if they are required to throttle their bandwidth usage */
    FOR_EACH_REGISTERED_FEATURE(feature_info)
    {
        if (feature_info->bitfields.running && feature_info->bitfields.throttle_required)
        {
            feature_info->callback(feature_info->bitfields.throttle_required);
        }
    }
}

void BandwidthManager_SetActiveFeaturesNum(uint8 active_features_num)
{
    DEBUG_LOG("BandwidthManager_SetActiveFeaturesNum: active_features_num[%d]", active_features_num);
    bandwidth_manager_info.active_features_num = active_features_num;
}

