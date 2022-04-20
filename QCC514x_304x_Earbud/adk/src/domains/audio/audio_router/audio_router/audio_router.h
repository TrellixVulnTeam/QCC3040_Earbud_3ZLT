/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router Audio Router
\ingroup    audio_domain
\brief      The audio router provides a standard API to enable or disable audio paths

Specific implementations of the audio router behaviour can be configured by the registration
of handlers with the AudioRouter_ConfigureCallbacks() function.

This implementation can then call into the audio router to connect and disconnect
sources as required.
*/

#ifndef AUDIO_ROUTER_H_
#define AUDIO_ROUTER_H_

#include "source_param_types.h"
#include "audio_sources_list.h"
#include "voice_sources_list.h"
#include "audio_sources_audio_interface.h"
#include "device.h"

typedef enum
{
    audio_router_state_new_source,
    audio_router_state_disconnected,
    audio_router_state_connecting,
    audio_router_state_connected_pending,
    audio_router_state_connected,
    audio_router_state_disconnecting,
    audio_router_state_disconnecting_no_connect,
    audio_router_state_disconnected_pending,
    audio_router_state_to_be_interrupted,
    audio_router_state_interrupting,
    audio_router_state_interrupted_pending,
    audio_router_state_interrupted,
    audio_router_state_to_be_resumed,
    audio_router_state_invalid
}audio_router_state_t;

typedef struct
{
    generic_source_t source;
    audio_router_state_t state;
    unsigned present : 1;
}audio_router_data_t;

typedef struct
{
    audio_router_data_t *data;
    unsigned max_data;
    unsigned next_index;
}audio_router_data_iterator_t;

/* structure containing the implementation specific handlers for the audio_router APIs */
typedef struct
{
    void (*add_source)(generic_source_t source);
    bool (*remove_source)(generic_source_t source);
    bool (*is_device_in_use)(device_t device);
    void (*update)(void);
} audio_router_t;

void AudioRouter_Init(void);

/*! \brief Configures the handlers for AddSource and RemoveSource functions.

    \param callbacks Pointers to the handler functions.
 */
void AudioRouter_ConfigureHandlers(const audio_router_t* handlers);

/*! \brief Calls handler for adding source configured with AudioRouter_ConfigureHandlers

    \param source The source to be added
 */
void AudioRouter_AddSource(generic_source_t source);
 
/*! \brief Calls handler for removing source configured with AudioRouter_ConfigureHandlers

    \param source The source to be removed
 */
void AudioRouter_RemoveSource(generic_source_t source);

/*! \brief Connects the passed source

    \param source The source to be connected

    \return TRUE if successfully connected.
 */
bool AudioRouter_CommonConnectSource(generic_source_t source);

/*! \brief Disconnects the passed source

    \param source The source to be disconnected

    \return TRUE if successfully disconnected.
 */
bool AudioRouter_CommonDisconnectSource(generic_source_t source);

/*! \brief Kick the audio router to attempt to update the routing
 */
void AudioRouter_Update(void);

/*! \brief Check if any source accociated with device
           active within the audio router

    \param device device to check.

    \return TRUE if active.
 */
bool AudioRouter_IsDeviceInUse(device_t device);

/*! \brief Set the state of source

    \param source Source to set the state of

    \param state State to set source to.

    \return Response from the attempt to set the state.
 */
source_status_t AudioRouter_CommonSetSourceState(generic_source_t source, source_state_t state);

/*! \brief Initialise audio router source state data.
 */
void AudioRouter_InitData(void);

/*! \brief Get an iterator to traverse the audio router source state data.

    \return An iterator to pass to AudioRouter_GetNextEntry to get audio router state data.

    \note the iterator is malloced and must be freed after use using AudioRouter_DestroyDataIterator()
 */
audio_router_data_iterator_t* AudioRouter_CreateDataIterator(void);

/*! \brief Free the iterator created using AudioRouter_CreateDataIterator();

    \param iterator iterator to free.
 */
void AudioRouter_DestroyDataIterator(audio_router_data_iterator_t *iterator);

/*! \brief Get the next source state from the audio router data.

    \param iterator The currently in use iterator to traverse the stored data.

    \return pointer to an audio_router_data_t structure containing the source state data.
            Will return NULL when the end of the data is reached.
 */
audio_router_data_t* AudioRouter_GetNextEntry(audio_router_data_iterator_t *iterator);

/*! \brief Get the last routed audio from the audio router;

    \return The last audio source routed by the audio router
            audio_source_none if none currently routed;
 */
audio_source_t AudioRouter_GetLastRoutedAudio(void);

#endif /* #define AUDIO_ROUTER_H_ */
