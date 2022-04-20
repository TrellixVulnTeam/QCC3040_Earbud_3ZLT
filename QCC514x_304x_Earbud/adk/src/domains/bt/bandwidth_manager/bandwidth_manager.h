/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       bandwidth_manager.h
\brief      Interface of the bandwidth manager module.

    These apis are used to register different Bluetooth using features with the bandwidth manager
    for those features to tell the bandwdth manager when they start and stop using Bluetooth bandwidth
    and for the bandwidth manager to tell lower priority features when they should reduce or increase
    their use of Bluetooth bandwidth.
*/

#ifndef BANDWIDTH_MANAGER_H_
#define BANDWIDTH_MANAGER_H_


/*! \brief  This is the type definition of a callback function that the lower priority features must
            register when they call the BandwidthManager_RegisterFeature function. (The high priority
            features are not told to adjust their Bluetooth bandwidth and hence do not need to register
            a callback function and can give a NULL function pointer instead.) The bandwidth manager
            will call a feature's callback function to set that feature's Bluetooth bandwidth
            restriction level.
    \param  throttle_required - Bluetooth bandwidth throttling required or not.
            TRUE, indicates that other higher prioroty features are currently running, so reduce bandwidth.
            FALSE, indicates that throttling may not be required as no other higher priority features are runniing.

    \return void
*/
typedef void (*bandwidth_manager_callback_t)(bool throttle_required);

/*! \brief  The enumeration of the levels of bandwidth manager priorities.
            high    - The application ensures all High Priority features are mutually exclusive
                    - The Feature shall use large amounts of bandwidth e.g. > 30% such as A2DP and HFP
            medium  - Used for short-lived transient features (otherwise use Low Priority), such as VA WuW
                    - The feature may use large amounts of bandwidth but it can vary the amount if requested
            low     - Used for longer lived features, such as DFU
                    - The feature may use large amounts of bandwidth but it can vary the amount (down to zero) if requested
            unused  - for internal use only
 */
typedef enum
{
    high_bandwidth_manager_priority,
    medium_bandwidth_manager_priority,
    low_bandwidth_manager_priority,
    unused_bandwidth_manager_priority,
} bandwidth_manager_priority_t;


/*! \brief Enumeration of feature identifiers */
typedef enum
{
    BANDWIDTH_MGR_FEATURE_A2DP_LL,
    BANDWIDTH_MGR_FEATURE_PAGE_SCAN,
    BANDWIDTH_MGR_FEATURE_DFU,
    BANDWIDTH_MGR_FEATURE_VA,
    BANDWIDTH_MGR_FEATURE_ESCO,
    BANDWIDTH_MGR_FEATURE_A2DP_HIGH_BW,
    BANDWIDTH_MGR_FEATURE_LAST = BANDWIDTH_MGR_FEATURE_A2DP_HIGH_BW,
    BANDWIDTH_MGR_FEATURE_MAX,
    BANDWIDTH_MGR_FEATURE_INVALID_ID
}bandwidth_manager_feature_id_t;

/*! \brief Initialse the Bandwidth Manager module. */
bool BandwidthManager_Init(Task init_task);

/*! \breif  The function to register a feature with the bandwidth manager.
   \param identifier - the feature identifier as defined by bandwidth_manager_feature_id_t

    \param  priority - The bandwidth manager priority for this feature.

    \param  callback - The callback provided by this feature for the bandwith manager to use to set the level of
                       Bluetooth bandwith usage.

    \return bandwidth_manager_handle_t to be used with subsequent calls to the bandwidth manager from this feature.
            This is NULL on failure.
*/
bool BandwidthManager_RegisterFeature(bandwidth_manager_feature_id_t identifier, bandwidth_manager_priority_t priority,
    bandwidth_manager_callback_t callback);

/*! \brief  A feature is notifying the bandwidth manager that it is about to start using Bluetooth bandwidth.

    \param  identifier - the feature identifier as defined by bandwidth_manager_feature_id_t.

    \return bool - TRUE if successful, FALSE if not
*/
bool BandwidthManager_FeatureStart(bandwidth_manager_feature_id_t identifier);

/*! \brief  A feature is notifying the bandwidth manager that it has stopped using Bluetooth bandwidth.

\param  handle - the handle of the feature as returned by BandwidthManager_RegisterFeature.

\return bool - TRUE if successful, FALSE if not
*/
bool BandwidthManager_FeatureStop(bandwidth_manager_feature_id_t identifier);

/*! \brief A feature can query the Bandwidth Manager to know about its throttle requirement.
 *
 *  \param identifier - the feature identifier as defined by bandwidth_manager_feature_id_t
 *
 *  \return bool - TRUE if bandwidth throttling required, FALSE otherwise.
*/
bool BandwidthManager_IsThrottleRequired(bandwidth_manager_feature_id_t identifier);

/*! \brief A feature can query the Bandwidth Manager to know about its running status.
 *
 *  \param identifier - the feature identifier as defined by bandwidth_manager_feature_id_t
 *
 *  \return bool - TRUE, if the feature handle is valid and feature is running, FALSE otherwise.
*/
bool BandwidthManager_IsFeatureRunning(bandwidth_manager_feature_id_t identifier);

/*! \brief A feature can query the Bandwidth Manager to know about its priority.
 *
 *  \param identifier - the feature identifier as defined by bandwidth_manager_feature_id_t
 *
 *  \return bandwidth_manager_priority_t - Return priority of registered feature, otherwise unused_bandwidth_manager_priority.
*/
bandwidth_manager_priority_t BandwidthManager_GetFeaturePriority(bandwidth_manager_feature_id_t identifier);

#endif /* BANDWIDTH_MANAGER_H_*/

