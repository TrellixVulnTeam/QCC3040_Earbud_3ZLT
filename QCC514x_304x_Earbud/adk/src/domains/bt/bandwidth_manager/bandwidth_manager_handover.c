/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       bandwidth_manager_handover.c
\brief      Bandwidth Manager Handover related interfaces

*/
#ifdef INCLUDE_MIRRORING

#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "bandwidth_manager_private.h"
#include "bandwidth_manager_marshal_typedef.h"
#include <logging.h>
#include <stdlib.h>
#include <panic.h>
#include <message.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool bandwidthManager_Veto(void);

static bool bandwidthManager_Marshal(const bdaddr *bd_addr,
                               marshal_type_t type,
                               void **marshal_obj);

static app_unmarshal_status_t bandwidthManager_Unmarshal(const bdaddr *bd_addr,
                                 marshal_type_t type,
                                 void *unmarshal_obj);

static void bandwidthManager_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t bandwidth_manager_marshal_types[] = {
    MARSHAL_TYPE_INFO(bandwidth_manager_info_t, MARSHAL_TYPE_CATEGORY_GENERIC)
};

const marshal_type_list_t bandwidth_manager_marshal_type_list = {bandwidth_manager_marshal_types, ARRAY_DIM(bandwidth_manager_marshal_types)};
REGISTER_HANDOVER_INTERFACE(BANDWIDTH_MANAGER, &bandwidth_manager_marshal_type_list, bandwidthManager_Veto, bandwidthManager_Marshal, bandwidthManager_Unmarshal, bandwidthManager_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*!
    \brief Handle Veto check during handover
    \return TRUE to veto handover.
*/
static bool bandwidthManager_Veto(void)
{
    bool veto = FALSE;

    if (MessagesPendingForTask(bandwidthManager_GetMessageTask(), NULL))
    {
        DEBUG_LOG("bandwidthManager_Veto: pending messages, vetoed");
        veto = TRUE;
    }
    return veto;
}

/*! \brief Restore running features' bitfield information which is unmarshalled.
 * \param Pointer to unmarshalled feature bitfield array
 */
static void bandwidthManager_UnmarshalActiveFeaturesInfo(bandwidth_manager_info_t* unmarshal_data)
{
    /* Make sure to reset all features info before updating to that of old primary */
    BandwidthManager_ResetAllFeaturesInfo();
    for (uint8 index =0; index < unmarshal_data->active_features_num; index++)
    {
        DEBUG_LOG("bandwidthManager_UnmarshalActiveFeaturesInfo: unmarshalling enum:bandwidth_manager_feature_id_t:%d info", unmarshal_data->feature_info[index].bitfields.identifier);
        BandwidthManager_UpdateFeatureInfo(&unmarshal_data->feature_info[index].bitfields);
    }
    BandwidthManager_SetActiveFeaturesNum(unmarshal_data->active_features_num);
}

/*!
    \brief The function shall set marshal_obj to the address of the object to
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled.
    \param[in] type         Type of the data to be marshalled.
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool bandwidthManager_Marshal(const bdaddr *bd_addr,
                               marshal_type_t type,
                               void **marshal_obj)
{
    bool status = FALSE;
    UNUSED(bd_addr);

    DEBUG_LOG("bandwidthManager_Marshal");

    switch(type)
    {
        case MARSHAL_TYPE(bandwidth_manager_info_t):
        {
           if (BandwidthManager_GetActiveFeaturesNum() > 0)
           {
               *marshal_obj = &bandwidth_manager_info;
               status = TRUE;
           }
        }
        break;
        default:
        break;
    }
    return status;
}

/*!
    \brief The function shall copy the unmarshal_obj associated to specific
            marshal type

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled.
    \param[in] type         Type of the unmarshalled data.
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t bandwidthManager_Unmarshal(const bdaddr *bd_addr,
                                 marshal_type_t type,
                                 void *unmarshal_obj)
{
    DEBUG_LOG("bandwidthManager_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    UNUSED(bd_addr);

    switch(type)
    {
        case MARSHAL_TYPE(bandwidth_manager_info_t):
        {
           bandwidthManager_UnmarshalActiveFeaturesInfo(unmarshal_obj);
           result = UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        break;
        default:
        break;
    }

    return result;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if new role is primary, else secondary

*/
static void bandwidthManager_Commit(bool is_primary)
{
    DEBUG_LOG("bandwidthManager_Commit");

    if (!is_primary)
    {
        BandwidthManager_ResetAllFeaturesInfo();
    }
    /* New primary shall refresh it's registered bandwidth features' throttle status */
    else
    {
        BandwidthManager_RefreshFeatureThrottleStatus();
    }
}

uint32 BandwidthManager_ActiveFeaturesSize_cb(const void *parent,
                                     const marshal_member_descriptor_t *member_descriptor,
                                     uint32 array_element)
{
    UNUSED(parent);
    UNUSED(member_descriptor);
    UNUSED(array_element);
    uint8 active_features_num = BandwidthManager_GetActiveFeaturesNum();
    DEBUG_LOG("BandwidthManager_ActiveFeaturesSize_cb: number of active features[%d]", active_features_num);
    return active_features_num;
}

#endif /* INCLUDE_MIRRORING */
