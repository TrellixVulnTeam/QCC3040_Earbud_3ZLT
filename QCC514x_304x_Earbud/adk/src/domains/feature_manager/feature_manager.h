/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Feature Manager module APIs
*/

#ifndef FEATURE_MANAGER_H
#define FEATURE_MANAGER_H

typedef enum {
    feature_id_sco,
    feature_id_va,
#ifdef HOSTED_TEST_ENVIRONMENT
    feature_id_fit_test,
#endif
    feature_id_max
} feature_id_t;

typedef enum {
    feature_state_idle,
    feature_state_running,
    feature_state_suspended
} feature_state_t;

/*! A list of mutually exclusive features in priority order. */
typedef struct {
    const feature_id_t * id;
    unsigned number_of_features;
} feature_manager_priority_list_t;

/*! Interface to be implemented by clients and passed in at registration. */
typedef struct {
    feature_state_t(*GetState)(void);
    void(*Suspend)(void);
    void(*Resume)(void);
} feature_interface_t;

typedef struct feature_manager_client_t * feature_manager_handle_t;

/*! \brief Set the priority list which will govern the feature manager's behaviour.
    \param const feature_manager_priority_list_t * Pointer to the priority list.
 */
void FeatureManager_SetPriorities(const feature_manager_priority_list_t * priority_list);

/*! \brief Registers a feature with the feature manager.
    \param feature_id_t The feature identifier.
    \param const feature_interface_t * Pointer to the features implementation of the feature interface.
    \return feature_manager_handle_t A unique handle which the feature will store and use to trigger actions in the feature manager.
 */
feature_manager_handle_t FeatureManager_Register(feature_id_t feature_id, const feature_interface_t * feature_interface);

/*! \brief Requests to start the feature.
    \param feature_manager_handle_t The feature's unique handle acquired upon registration
    \return TRUE if the feature can start, otherwise FALSE.
 */
bool FeatureManager_StartFeatureRequest(feature_manager_handle_t handle);

/*! \brief Notifies the feature manager that the feature has stopped.
    \param feature_manager_handle_t The feature's unique handle acquired upon registration
 */
void FeatureManager_StopFeatureIndication(feature_manager_handle_t handle);

#ifdef HOSTED_TEST_ENVIRONMENT
void FeatureManager_Reset(void);
#endif /* HOSTED_TEST_ENVIRONMENT */

#endif /* FEATURE_MANAGER_H */
