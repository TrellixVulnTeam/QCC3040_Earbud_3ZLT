/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       bt_device_handover.c
\brief      BT Device Handover related interfaces

*/
#ifdef INCLUDE_MIRRORING

#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "bt_device.h"
#include "bt_device_handover_typedef.h"
#include "device_properties.h"
#include "device_db_serialiser.h"
#include "mirror_profile.h"
#include <logging.h>
#include <panic.h>
#include <device_list.h>
#include <stdlib.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool btDevice_Veto(void);

static bool btDevice_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type,
                               void **marshal_obj);

static app_unmarshal_status_t btDevice_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type,
                                 void *unmarshal_obj);

static void btDevice_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t bt_device_marshal_types[] = {
    MARSHAL_TYPE_INFO(bt_device_handover_t, MARSHAL_TYPE_CATEGORY_PER_INSTANCE),
};

const marshal_type_list_t bt_device_marshal_types_list = {bt_device_marshal_types, ARRAY_DIM(bt_device_marshal_types)};
REGISTER_HANDOVER_INTERFACE(BT_DEVICE, &bt_device_marshal_types_list, btDevice_Veto, btDevice_Marshal, btDevice_Unmarshal, btDevice_Commit);

static bt_device_handover_t marshal_data;


/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! 
    \brief Handle Veto check during handover
    \return bool
*/
static bool btDevice_Veto(void)
{
    bool i_veto = FALSE;
    return i_veto;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled.
    \param[in] type         Type of the data to be marshalled.
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: The component has another object to marshal of that type.
            FALSE: The component does not have another object to marshal of that type.
            The function can set marshal_obj to NULL if it has no objects to 
            marshal (and return FALSE)

*/
static bool btDevice_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type, 
                               void **marshal_obj)
{
    DEBUG_LOG("btDevice_Marshal");
    *marshal_obj = NULL;
    bool status = FALSE;

    switch (type)
    {
        case MARSHAL_TYPE(bt_device_handover_t):
        {
            device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
            PanicNull(device);

            BtDevice_GetDeviceData(device, &marshal_data.pdd);

            marshal_data.connected_profiles = BtDevice_GetConnectedProfiles(device);
            marshal_data.audio_source = DeviceProperties_GetAudioSource(device);
            marshal_data.voice_source = DeviceProperties_GetVoiceSource(device);

            *marshal_obj = &marshal_data;
            status = TRUE;
        }
        break;
 
        default:
            /* Do nothing */
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
static app_unmarshal_status_t btDevice_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type, 
                                 void *unmarshal_obj)
{
    DEBUG_LOG("btDevice_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    PanicNull(unmarshal_obj);

    switch (type)
    {
        case MARSHAL_TYPE(bt_device_handover_t):
        {
            device_t device;
            bt_device_handover_t *unmarshalled = NULL;

            device = BtDevice_GetDeviceForBdAddr(bd_addr);
            PanicNull(device);

            /* Unmarshal device data for handset address */
            unmarshalled = (bt_device_handover_t*)unmarshal_obj;
            BtDevice_SetDeviceData(device, &unmarshalled->pdd);
            BtDevice_SetConnectedProfiles(device, unmarshalled->connected_profiles);

            if (unmarshalled->voice_source != voice_source_none)
            {
                DeviceProperties_SetVoiceSource(device, unmarshalled->voice_source);
            }
            if (unmarshalled->audio_source != audio_source_none)
            {
                DeviceProperties_SetAudioSource(device, unmarshalled->audio_source);
            }

            /* Set the mru device property for only focused/mirroring device */
            if(BdaddrIsSame(bd_addr, MirrorProfile_GetMirroredDeviceAddress()))
            {
                appDeviceUpdateMruDevice(bd_addr);
            }

            result = UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        break;

        
        default:
            /* Do nothing */
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
static void btDevice_Commit(bool is_primary)
{
    DEBUG_LOG("btDevice_Commit");

    bdaddr self_addr, peer_addr;

    /* Swap the self and peer earbud address */
    appDeviceGetMyBdAddr(&self_addr);
    appDeviceGetPeerBdAddr(&peer_addr);
    BtDevice_SwapAddresses(&self_addr, &peer_addr);

    if (!is_primary)
    {
        /* Secondary cannot have profiles connected */
        deviceType type = DEVICE_TYPE_HANDSET;
        device_t *devices = NULL;
        unsigned num_devices = 0;
        /* just making sure that no handset devices are having connected profiles as it's going to be new secondary earbud */
        DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
        if( devices && num_devices)
        {
            for(unsigned index=0; index < num_devices; index++)
            {
                BtDevice_SetConnectedProfiles(devices[index], 0);
            }
        }
        free(devices);
    }

    /* Store device data in ps after some delay */
    BtDevice_StorePsDeviceDataWithDelay();
}

#endif /* INCLUDE_MIRRORING */
