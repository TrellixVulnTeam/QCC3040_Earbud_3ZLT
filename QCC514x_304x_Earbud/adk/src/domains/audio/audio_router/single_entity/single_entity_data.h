/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief      Storage and retrieval of dynamic data for the single entity module
*/

#ifndef SINGLE_ENTITY_DATA_H_
#define SINGLE_ENTITY_DATA_H_

#include "audio_router.h"
#include "audio_sources.h"
#include "voice_sources.h"

/* Structure used for getting information relevant to source routing
 *
 * To avoid this information getting out-of-sync with other parts of the code,
 * this type should be created, used, and destroyed within the scope of a single
 * routing event. */
typedef struct
{
    unsigned have_source_to_route:1;
    unsigned have_routed_source:1;
    unsigned have_interrupted_source:1;
    generic_source_t highest_priority_source;
    generic_source_t source_to_route;
    generic_source_t routed_source;
    generic_source_t interrupted_source;
    union
    {
        audio_source_provider_context_t audio;
        voice_source_provider_context_t voice;
    } highest_priority_source_context;
    union
    {
        audio_source_provider_context_t audio;
        voice_source_provider_context_t voice;
    } routed_source_context;
    union
    {
        audio_source_provider_context_t audio;
        voice_source_provider_context_t voice;
    } interrupted_source_context;
} source_routing_t;

/*! \brief Add source to the list of current connected sources

    \param source The source to be added to the list

    \return TRUE if successfully added. 
    
    \note This will fail if you try to add the same source twice. 
 */
bool SingleEntityData_AddSource(generic_source_t source);

/*! \brief Remove source from the list of current connected sources

    \param source The source to be removed from the list

    \return TRUE if successfully removed. Note that this will fail
            if the source isn't in the list. 
 */
bool SingleEntityData_RemoveSource(generic_source_t source);

/*! \brief Get the source currently active

    \param source Pointer to generic_source_t that will be populated
                  with active source (if there is one)

    \return TRUE if a source is marked active.
 */
bool SingleEntityData_GetActiveSource(generic_source_t* source);

/*! \brief Get the interrupted source

    \param source Pointer to generic_source_t that will be populated
                  with interrupted source (if there is one)

    \return TRUE if a source has been interrupted.
 */
bool SingleEntityData_GetInterruptedSource(generic_source_t* source);

/*! \brief Get the source to route

    \param source Pointer to a source structure to be populated with the source to route

    \return TRUE if source contains a source to route.
 */
bool SingleEntityData_GetSourceToRoute(generic_source_t* source);

/*! \brief Utlity to compare two source structures

    \param source1 first source for comparison

    \param source2 second source for comparison

    \return TRUE if source1 == source2
 */
bool SingleEntityData_AreSourcesSame(generic_source_t source1, generic_source_t source2);

/*! \brief Test if source is present within single entity data

    \param source source to find

    \return TRUE if source is marked as present
*/
bool SingleEntityData_IsSourcePresent(generic_source_t source);

/*! \brief Set the state of source

    \param source Source to set the state of

    \param state State to set source to

    \return TRUE if the state is successfully set.
*/
bool SingleEntityData_SetSourceState(generic_source_t source, audio_router_state_t state);

/*! \brief Get the state of source

    \param source Source to get the state of

    \return The state of source. audio_router_state_invalid if source is not present
*/
audio_router_state_t SingleEntityData_GetSourceState(generic_source_t source);

/*! \brief Find the first active source. (Source with state not disconnected)

    \param[out] source pointer to source to populate with result.

    \return TRUE if an active source is found
*/
bool SingleEntityData_GetActiveSource(generic_source_t* source);

/*! \brief Find first source that is in the process of connecting or disconnecting.

    \param[out] source pointer to an generic_source_t to populate with result.

    \return TRUE if an active source is found
*/
bool SingleEntityData_FindTransientSource(generic_source_t* source);

/*! \brief Check of source is active

    \param source source to check.

    \return TRUE if an source is active.
*/
bool SingleEntityData_IsSourceActive(generic_source_t source);

/*! \brief Finds a new source that has been added but not used.

    \param[out] source pointer to an generic_source_t to populate with result.

    \return TRUE if a source was found.

    \note This will only return the first source it finds. There may be more than one.
*/
bool SingleEntityData_FindNewSource(generic_source_t* source);

#endif /* SINGLE_ENTITY_DATA_H_ */
