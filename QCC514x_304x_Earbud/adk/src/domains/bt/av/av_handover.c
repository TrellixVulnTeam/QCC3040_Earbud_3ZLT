/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      AV Handover related interfaces

*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV
#ifdef INCLUDE_MIRRORING
#include "av.h"
#include "av_instance.h"
#include "a2dp_profile.h"
#include "a2dp_profile_sync.h"
#include "avrcp_profile.h"
#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "adk_log.h"
#include "kymera.h"
#include <panic.h>
#include <stdlib.h>

#include <device_properties.h>
#include <device_list.h>
#include <focus_audio_source.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool av_Veto(void);

static bool av_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type,
                       void **marshal_obj);

static app_unmarshal_status_t av_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type,
                         void *unmarshal_obj);

static void av_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t av_marshal_types[] = {
    MARSHAL_TYPE_INFO(avTaskData, MARSHAL_TYPE_CATEGORY_GENERIC),
    MARSHAL_TYPE_INFO(avInstanceTaskData, MARSHAL_TYPE_CATEGORY_PER_INSTANCE),
};

const marshal_type_list_t av_marshal_types_list = {av_marshal_types, ARRAY_DIM(av_marshal_types)};

REGISTER_HANDOVER_INTERFACE(AV, &av_marshal_types_list, av_Veto, av_Marshal, av_Unmarshal, av_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! 
    \brief Handle Veto check during handover
    \return TRUE to veto handover.
*/
static bool av_Veto(void)
{
    bool veto = FALSE;

    /* Handle any pending volume store requests immediately */
    if (MessageCancelFirst(appGetAvPlayerTask(), AV_INTERNAL_VOLUME_STORE_REQ))
    {
        Av_UpdateStoredVolumeForFocussedHandset();
    }

    /* Check for pending messages of AV task */
    if (MessagesPendingForTask(appGetAvPlayerTask(), NULL))
    {
        DEBUG_LOG_INFO("av_Veto, Messages pending for AV task");
        veto = TRUE;
    }
    else
    {
        avInstanceTaskData *the_inst;
        av_instance_iterator_t iterator;
        
        /*Check for all instances of AV */
        for_all_av_instances(the_inst, &iterator)
        {
            if (the_inst)
            {
                if ((the_inst->a2dp.state != A2DP_STATE_DISCONNECTED) && the_inst->detach_pending)
                {
                    /* We are not yet disconnected, but we have a "detach pending", i.e. ACL has been disconnected
                     * and now we wait for profile disconnection event from Stack. Veto untill the state
                     * is A2DP_STATE_DISCONNECTED.
                     */
                    DEBUG_LOG_INFO("av_Veto, detach_pending");
                    veto =  TRUE;
                    break;
                }
                else if (A2dpProfile_Veto(the_inst))
                {
                    veto =  TRUE;
                    break;
                }
                else if (AvrcpProfile_Veto(the_inst))
                {
                    veto =  TRUE;
                    break;
                }
            }
        }
    }
       
    return veto;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool av_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type, 
                       void **marshal_obj)
{
    bool status = FALSE;
    *marshal_obj = NULL;
    avTaskData *the_av = AvGetTaskData();

    switch (type)
    {
        case MARSHAL_TYPE(avTaskData):
            *marshal_obj = the_av;
            status = TRUE;
            break;

        case MARSHAL_TYPE(avInstanceTaskData):
        {
            *marshal_obj = appAvInstanceFindFromBdAddr(bd_addr);
            status = (*marshal_obj != NULL);
            break;
        }
        default:
            break;
    }

    return status;
}

/*! 
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type \ref marshal_type_t

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled
                            \ref bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t av_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type, 
                         void *unmarshal_obj)
{
    avTaskData *the_av = AvGetTaskData();
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    
    switch (type)
    {
        case MARSHAL_TYPE(avTaskData):
            {
                avTaskData *recAv = unmarshal_obj;
                the_av->suspend_state = recAv->suspend_state;
                result = UNMARSHAL_SUCCESS_FREE_OBJECT;
            }
            break;

        case MARSHAL_TYPE(avInstanceTaskData):
            {
                device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

                if(device)
                {
                    /* Realloc so pmalloc debug tracks the owner of this pool correctly */
                    avInstanceTaskData *av_inst = PanicNull(realloc(unmarshal_obj, sizeof(*av_inst)));

                    PanicFalse(AvInstance_GetInstanceForDevice(device) == NULL);

                    /* Set the AV instance for the handset device */
                    AvInstance_SetInstanceForDevice(device, av_inst);

                    av_inst->av_task.handler = appAvInstanceHandleMessage;
                    av_inst->bd_addr = *bd_addr;

                    if(av_inst->avrcp.state == AVRCP_STATE_CONNECTED)
                    {
                        /* The AV task is added automatically to the instance's AVRCP
                        task list, other components will need to register when committing
                        to the new role as primary */
                        av_inst->avrcp.client_list = TaskList_Create();
                        av_inst->avrcp.client_lock = 1;
                        av_inst->avrcp.client_responses = 0;
                        TaskList_AddTask(av_inst->avrcp.client_list, &av_inst->av_task);
                    }

                    /* Initialize Sync parameters */
                    appA2dpSyncInitialise(av_inst);
                    /* register to receive kymera notifications */
                    Kymera_ClientRegister(AvGetTask());
                    result = UNMARSHAL_SUCCESS_DONT_FREE_OBJECT;
                }
            }
            break;
        default:
            /* Do Nothing */
            break;
    }

    return result;
}

static void commit_device(device_t device, void *data)
{
    bool is_primary = (bool)data;
    avInstanceTaskData *av_instance = AvInstance_GetInstanceForDevice(device);
    if (av_instance)
    {
        A2dpProfile_Commit(av_instance, is_primary);
        AvrcpProfile_Commit(av_instance, is_primary);
        if (is_primary)
        {
            AvInstance_RegisterMediaControlInterfaceForInstance(av_instance);
        }
    }
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void av_Commit(bool is_primary)
{
    DeviceList_Iterate(commit_device, (void *)is_primary);

    if (!is_primary)
    {
        /* Clear suspend reason, so if secondary becomes primary (without
           handover) no invalid suspend reason is inherited when reconnecting
           to a handset */
        AvGetTaskData()->suspend_state = 0;
    }
}

#endif /* INCLUDE_MIRRORING */
#endif /* INCLUDE_AV */

