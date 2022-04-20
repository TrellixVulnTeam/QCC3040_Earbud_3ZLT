/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       hfp_profile_handover.c
\brief      HFP Profile Handover related interfaces

*/
#ifdef INCLUDE_MIRRORING

#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "hfp_profile.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "mirror_profile.h"
#include <logging.h>
#include <stdlib.h>
#include <panic.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool hfpProfile_Veto(void);

static bool hfpProfile_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type,
                               void **marshal_obj);

static app_unmarshal_status_t hfpProfile_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type,
                                 void *unmarshal_obj);

static void hfpProfile_GetSinks(hfpInstanceTaskData *instance);

static void hfpProfile_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t hfp_profile_marshal_types[] = {
    MARSHAL_TYPE_INFO(hfpInstanceTaskData, MARSHAL_TYPE_CATEGORY_PER_INSTANCE)
};

const marshal_type_list_t hfp_profile_marshal_types_list = {hfp_profile_marshal_types, ARRAY_DIM(hfp_profile_marshal_types)};
REGISTER_HANDOVER_INTERFACE(HFP_PROFILE, &hfp_profile_marshal_types_list, hfpProfile_Veto, hfpProfile_Marshal, hfpProfile_Unmarshal, hfpProfile_Commit);


/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! 
    \brief Handle Veto check during handover
    \return TRUE to veto handover.
*/
static bool hfpProfile_Veto(void)
{
    bool veto = FALSE;

    /* Veto if any one of the following conditions is TRUE
       a) In transient state (lock is held). 
       b) Detach is pending.
       c) Pending messages for the task.
     */
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        /* Cancel aptX voice counter monitoring msg. */
        MessageCancelAll(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ);

        /* Handle any pending config write immediately */
        if (MessageCancelAll(&hfp_profile_task_data.task, HFP_INTERNAL_CONFIG_WRITE_REQ))
        {
            device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
            HfpProfile_HandleConfigWriteRequest(device);
        }

        if (MessagesPendingForTask(HfpProfile_GetInstanceTask(instance), NULL))
        {
            DEBUG_LOG_INFO("hfpProfile_Veto(%p), Messages pending for HFP task", instance);
            veto = TRUE;
        }
        else
        {
            if (*HfpProfileInstance_GetLock(instance))
            {
                DEBUG_LOG_INFO("hfpProfile_Veto(%p), hfp_lock", instance);
                veto = TRUE;
            }
            else if (!HfpProfile_IsDisconnected(instance) && instance->bitfields.detach_pending)
            {
                /* We are not yet disconnected, but we have a "detach pending", i.e. ACL has been disconnected
                 * and now we wait for profile disconnection event from Stack. Veto untill the profile is A2DP_STATE_DISCONNECTED.
                 */
                DEBUG_LOG_INFO("hfpProfile_Veto(%p), detach_pending", instance);
                veto = TRUE;
            }
        }
        if (veto)
        {
            return veto;
        }
    }
    return veto;
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
static bool hfpProfile_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type, 
                               void **marshal_obj)
{
    bool status = FALSE;
    DEBUG_LOG("hfpProfile_Marshal");
    *marshal_obj = NULL;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
    if(NULL != instance)
    {
        switch (type)
        {
            case MARSHAL_TYPE(hfpInstanceTaskData):
                *marshal_obj = instance;
                status = TRUE;
                break;

            default:
                break;
        }
    }
    else
    {
        DEBUG_LOG("hfpProfile_Marshal:Bluetooth Address Mismatch");
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
static app_unmarshal_status_t hfpProfile_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type, 
                                 void *unmarshal_obj)
{
    DEBUG_LOG("hfpProfile_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;

    switch (type)
    {
        case MARSHAL_TYPE(hfpInstanceTaskData):
            {
                hfpInstanceTaskData *hfpInst = (hfpInstanceTaskData*)unmarshal_obj;
                hfpInstanceTaskData *instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
                if (instance == NULL)
                {
                    instance = HfpProfileInstance_Create(bd_addr, FALSE);
                }

                instance->state = hfpInst->state;
                instance->profile = hfpInst->profile;
                instance->ag_bd_addr = *bd_addr;
                instance->bitfields = hfpInst->bitfields;
                instance->sco_supported_packets = hfpInst->sco_supported_packets;
                instance->codec = hfpInst->codec;
                instance->wesco = hfpInst->wesco;
                instance->tesco = hfpInst->tesco;
                instance->qce_codec_mode_id = hfpInst->qce_codec_mode_id;
                instance->source_state = hfpInst->source_state;
                HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive();
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
    \brief Gets the SCO and SLC Sinks

    This function retrieves the SLC and SCO Sinks and sets the correspoding instance fields.

    \param[in] instance   HFP instance task data

*/
static void hfpProfile_GetSinks(hfpInstanceTaskData *instance)
{
    Sink sink;
    hfp_link_priority priority;

    DEBUG_LOG("hfpProfile_GetSinks");

    if (HfpIsAudioConnected(&instance->ag_bd_addr))
    {
        instance->sco_sink = MirrorProfile_GetScoSink();

        if (instance->sco_sink)
        {
            DEBUG_LOG("hfpProfile_GetSinks:: Override SCO sink");
            /* Set the HFP Sink in the HFP profile library for the handset connection */
            PanicFalse(HfpOverideSinkBdaddr(&instance->ag_bd_addr, instance->sco_sink));
        }
    }

    /* Derive slc_sink using link priority over which handset is connected*/
    priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
    PanicFalse(priority != hfp_invalid_link);

    if (HfpLinkGetSlcSink(priority, &sink))
    {
        instance->slc_sink = sink;
    }
    else
    {
        DEBUG_LOG("hfpProfile_GetSinks:: Deriving slc_link failed for device[0x%06x], enum:hfp_link_priority:%d", instance->ag_bd_addr.lap, priority);
    }
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if new role is primary, else secondary

*/
static void hfpProfile_Commit(bool is_primary)
{
    DEBUG_LOG("hfpProfile_Commit");
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_hfp_1);
    HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_hfp_2);

    for_all_hfp_instances(instance, &iterator)
    {
        if (is_primary)
        {
            voice_source_t voice_source;

            DEBUG_LOG("hfpProfile_Commit:: New Role Primary");
            hfpProfile_GetSinks(instance);
            voice_source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
            if(voice_source != voice_source_none)
            {
                HfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source);
            }
        }
        else
        {
            DEBUG_LOG("hfpProfile_Commit:: New Role Secondary");
            /* Silently move to disconnected state post commit in the new-secondary device */
            instance->state = HFP_STATE_DISCONNECTED;
            instance->slc_sink = 0;
            instance->sco_sink = 0;

            appBatteryUnregister(HfpProfile_GetInstanceTask(instance));
            instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;
        }
    }
}

#endif /* INCLUDE_MIRRORING */
