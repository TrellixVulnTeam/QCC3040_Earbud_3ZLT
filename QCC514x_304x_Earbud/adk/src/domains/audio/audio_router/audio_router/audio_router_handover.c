/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Audio Router Handover related interfaces

*/

/* Only compile if mirroring defined */
#ifdef INCLUDE_MIRRORING
#include "audio_router_typedef.h"
#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "adk_log.h"
#include <panic.h>
#include <stdlib.h>

#include <device_properties.h>
#include <device_list.h>
#include <mirror_profile_protected.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool ar_Veto(void);

static bool ar_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type,
                       void **marshal_obj);
                       
static audio_router_data_iterator_t* ar_CreateHandoverDataIterator(audio_router_data_container_t *handover_data_container);

static bool ar_IsLeAudioAvailable(void);

static bool ar_IsLeAudioSource(const generic_source_t* source);

static void ar_UnmarshalWithLeAudio(audio_router_data_container_t* handover_data_container);

static void ar_UnmarshalNoLeAudio(audio_router_data_container_t* handover_data_container);

static app_unmarshal_status_t ar_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type,
                         void *unmarshal_obj);

static void ar_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t ar_marshal_types[] = {
    MARSHAL_TYPE_INFO(audio_router_data_container_t, MARSHAL_TYPE_CATEGORY_GENERIC)
};

const marshal_type_list_t ar_marshal_types_list = {ar_marshal_types, ARRAY_DIM(ar_marshal_types)};

REGISTER_HANDOVER_INTERFACE(AUDIO_ROUTER, &ar_marshal_types_list, ar_Veto, ar_Marshal, ar_Unmarshal, ar_Commit);

extern audio_router_data_container_t audio_router_data_container;

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! 
    \brief Handle Veto check during handover
    \return TRUE to veto handover.
*/
static bool ar_Veto(void)
{
    return FALSE;
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
static bool ar_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type, 
                       void **marshal_obj)
{
    UNUSED(bd_addr);
    *marshal_obj = NULL;

    switch (type)
    {
        case MARSHAL_TYPE(audio_router_data_container_t):
        {
            *marshal_obj = &audio_router_data_container;
            return TRUE;
        }
        
        default:
        break;
    }

    return FALSE;
}

/*!
    \brief Get an iterator to traverse the supplied handover audio router source state data.

    \param[in] handover_data_container      The handover audio router source state data
                                            \ref audio_router_data_container_t

    \return An iterator to pass to AudioRouter_GetNextEntry to get audio router state data.
    
    \note The iterator is malloced and must be freed after use using AudioRouter_DestroyDataIterator()

*/
static audio_router_data_iterator_t* ar_CreateHandoverDataIterator(audio_router_data_container_t *handover_data_container)
{
    audio_router_data_iterator_t *iterator 
        = (audio_router_data_iterator_t*)PanicUnlessMalloc(sizeof(audio_router_data_iterator_t));

    iterator->data = handover_data_container->data;
    iterator->max_data = MAX_NUM_SOURCES;
    iterator->next_index = 0;

    return iterator;
}

/*!
    \brief Gets if an LE Audio source is routable in the build.

    \return TRUE if an LE Audio source is routable in the build. FALSE otherwise.

*/
static bool ar_IsLeAudioAvailable(void)
{
    bool le_audio_available = FALSE;
    return le_audio_available;
}

/*!
    \brief Gets if the provided source is an LE Audio source.

    \param[in] source       The source to check
                            \ref generic_source_t
                                            
    \return TRUE if the provided source is an LE Audio source. FALSE otherwise.

*/
static bool ar_IsLeAudioSource(const generic_source_t* source)
{
    bool le_audio_source = FALSE;
    
    if (source->type == source_type_audio)
    {
        if ((source->u.audio == audio_source_le_audio_broadcast) ||
            (source->u.audio == audio_source_le_audio_unicast))
        {
            le_audio_source = TRUE;
        }
    }
    else if (source->type == source_type_voice)
    {
        if (source->u.voice == voice_source_le_audio_unicast)
        {
            le_audio_source = TRUE;
        }
    }
    
    return le_audio_source;
}

/*!
    \brief Unmarshals the audio routing data when LE Audio is a routable source.

    \param[in] handover_data_container      The handover routing data
                                            \ref audio_router_data_container_t

*/
static void ar_UnmarshalWithLeAudio(audio_router_data_container_t* handover_data_container)
{
    audio_router_data_t* handover_data = NULL;
    audio_router_data_t* ar_data = NULL;
    audio_router_data_iterator_t* handover_iterator = NULL;
    audio_router_data_iterator_t* ar_iterator = NULL;
    bool copy_data = FALSE;

    /* LE Audio state is not handed over, need to retain the original state for these sources. */
    
    handover_iterator = ar_CreateHandoverDataIterator(handover_data_container);
    ar_iterator = AudioRouter_CreateDataIterator();
    while(NULL != (handover_data = AudioRouter_GetNextEntry(handover_iterator)))
    {
        if (ar_IsLeAudioSource(&handover_data->source))
        {
            continue;
        }
        copy_data = FALSE;
        while(!copy_data && (NULL != (ar_data = AudioRouter_GetNextEntry(ar_iterator))))
        {
            if (!ar_IsLeAudioSource(&ar_data->source))
            {
                copy_data = TRUE;
            }
        }
        if (copy_data)
        {
            *ar_data = *handover_data;
        }
    }
    while(NULL != (ar_data = AudioRouter_GetNextEntry(ar_iterator)))
    {
        if (!ar_IsLeAudioSource(&ar_data->source))
        {
            memset(ar_data, 0, sizeof(*ar_data));
        }
    }
    AudioRouter_DestroyDataIterator(handover_iterator);
    AudioRouter_DestroyDataIterator(ar_iterator);
    audio_router_data_container.last_routed_audio_source = handover_data_container->last_routed_audio_source;
}

/*!
    \brief Unmarshals the audio routing data when there is no LE Audio routable source.

    \param[in] handover_data_container      The handover routing data
                                            \ref audio_router_data_container_t

*/
static void ar_UnmarshalNoLeAudio(audio_router_data_container_t* handover_data_container)
{
    audio_router_data_container = *handover_data_container;
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
static app_unmarshal_status_t ar_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type, 
                         void *unmarshal_obj)
{    
    UNUSED(bd_addr);
    
    switch(type)
    {
        case MARSHAL_TYPE(audio_router_data_container_t):
        {
            audio_router_data_container_t* handover_data_container = (audio_router_data_container_t*)unmarshal_obj;
            if (ar_IsLeAudioAvailable())
            {
                ar_UnmarshalWithLeAudio(handover_data_container);
            }
            else
            {
                ar_UnmarshalNoLeAudio(handover_data_container);
            }
            return UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        default:
        break;
    }
    
    return UNMARSHAL_FAILURE;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary
*/
static void ar_Commit(bool is_primary)
{
    DEBUG_LOG("AudioRouter Handover Commit, is_primary:%d", is_primary);
    /*! On the secondary any audio/voice sources that are not mirrored are cleared */
    if(!is_primary)
    {
        audio_router_data_t* data = NULL;
        audio_router_data_iterator_t *iterator;
        audio_source_t mirrored_audio_source = MirrorProfile_GetAudioSource();
        voice_source_t mirrored_voice_source = MirrorProfile_GetVoiceSource();

        for (iterator = AudioRouter_CreateDataIterator(), data = AudioRouter_GetNextEntry(iterator);
             data != NULL;
             data = AudioRouter_GetNextEntry(iterator))
        {
            if (data->source.type == source_type_audio)
            {
                audio_source_t uaudio = data->source.u.audio;
                if (uaudio == mirrored_audio_source ||
                    uaudio == audio_source_le_audio_unicast ||
                    uaudio == audio_source_le_audio_broadcast)
                {
                    continue;
                }
            }
            else if (data->source.type == source_type_voice)
            {
                voice_source_t uvoice = data->source.u.voice;
                if (uvoice == mirrored_voice_source ||
                    uvoice == voice_source_le_audio_unicast)
                {
                    continue;
                }
            }
            memset(data, 0, sizeof(*data));
        }
        AudioRouter_DestroyDataIterator(iterator);

        if (audio_router_data_container.last_routed_audio_source != mirrored_audio_source &&
            audio_router_data_container.last_routed_audio_source != audio_source_le_audio_unicast &&
            audio_router_data_container.last_routed_audio_source != audio_source_le_audio_broadcast)
        {
            /* Only cleared for non-mirrored sources */
            audio_router_data_container.last_routed_audio_source = audio_source_none;
        }
    }
}

#endif /* INCLUDE_MIRRORING */

