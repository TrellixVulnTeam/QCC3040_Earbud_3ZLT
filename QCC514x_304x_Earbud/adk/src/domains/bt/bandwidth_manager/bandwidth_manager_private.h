/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       bandwidth_manager_private.h
\brief      Declare/defines private data for bandwidth manager module.
*/

#ifndef BANDWIDTH_MANAGER_PRIVATE_H
#define BANDWIDTH_MANAGER_PRIVATE_H

#include <marshal_common.h>
#include "bandwidth_manager_typedef.h"


/*! \brief  An enumeration of bandwidth manager actions.
*/
typedef enum
{
    BANDWIDTH_MGR_ACTION_MSG_START,
    BANDWIDTH_MGR_ACTION_MSG_STOP
} bandwidth_manager_action_msg_t;

/*! \brief  The handle allocated to a feature by the BandwidthManager_RegisterFeature function.
 */
typedef void *bandwidth_manager_handle_t;

/*! \brief Structure to store BANDWIDTH_MGR_ACTION_MSG_START message content */
typedef struct
{
    bandwidth_manager_handle_t feature_handle;
}BANDWIDTH_MGR_ACTION_MSG_START_T;

/*! \brief Structure to store BANDWIDTH_MGR_ACTION_MSG_STOP message content */
typedef struct
{
    bandwidth_manager_handle_t feature_handle;
}BANDWIDTH_MGR_ACTION_MSG_STOP_T;

#define bandwidthManager_GetMessageTask()   &(bandwidth_manager_info.task)

extern bandwidth_manager_info_t bandwidth_manager_info;

#define FOR_EACH_REGISTERED_FEATURE(feature_info) \
    for (bandwidth_manager_feature_info_t *feature_info = bandwidth_manager_info.feature_info; \
        feature_info < (bandwidth_manager_info.feature_info + bandwidth_manager_info.registered_features_num); \
        feature_info++)

/*! \brief Callback function that returns about dynamic length of feature elements array.
 *         This callback gets invoked during marshal.
 */
uint32 BandwidthManager_ActiveFeaturesSize_cb(const void *parent,
                                     const marshal_member_descriptor_t *member_descriptor,
                                     uint32 array_element);

/*! \brief Get the number of running bandwidth features.
 *
 * \return Number of features which are running by BandwidthManager_FeatureStart API.
 */
uint8 BandwidthManager_GetActiveFeaturesNum(void);

/*! \brief Reset all the registered feature information, typically throttle and running status */
void BandwidthManager_ResetAllFeaturesInfo(void);

/*! \brief Update feature information with requested bitfields.
 *  Use feature identifier to access information of feature array element.
 *
 * \param bitfields Pointer to bitfield information which shall be updated with
 */
void BandwidthManager_UpdateFeatureInfo(feature_bitfields_t *bitfields);

/*! \brief Refresh throttle status of features that are running.
 *  Notify those feature clients about bandwidth throttle requirement
 */
void BandwidthManager_RefreshFeatureThrottleStatus(void);

/*! \brief Set the number of active features status
 *
 * \param active_features_num number of running features at given time.
 */
void BandwidthManager_SetActiveFeaturesNum(uint8 active_features_num);

#endif // BANDWIDTH_MANAGER_PRIVATE_H
