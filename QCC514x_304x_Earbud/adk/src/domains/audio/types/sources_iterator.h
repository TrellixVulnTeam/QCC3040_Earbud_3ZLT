/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      API for iterating through the registered audio sources in the audio source registry.
*/

#ifndef AUDIO_SOURCES_ITERATOR_H__
#define AUDIO_SOURCES_ITERATOR_H__

#include <source_param_types.h>

#include "audio_sources_interface_registry.h"
#include "audio_sources_list.h"

/* Opaque handle for an audio source iterator. */
typedef struct generic_source_iterator_tag * sources_iterator_t;

/*! \brief Create an iterator handle to use for iterating through either
           audio or voice sources which have a registered control interface.

    \param type - the type of source that the caller wants to iterate through
    \return the iterator handle

    \note This iterator is typically used to iterate through Source UI context
          providers
*/
sources_iterator_t SourcesIterator_Create(source_type_t type);

/*! \brief Add sources to an iterator only if they are in one of the requested contexts

    \param iterator - the iterator handle
    \param type - the type of source that the caller wants to iterate through
    \param contexts - the requested contexts
    \param num_contexts - the number of requested contexts
*/
void SourcesIterator_AddSourcesInContextArray(sources_iterator_t iterator, source_type_t type, const unsigned* contexts, const unsigned num_contexts);

#define SourcesIterator_AddSourcesInContexts(iterator, type, contexts) SourcesIterator_AddSourcesInContextArray(iterator, type, contexts, ARRAY_DIM(contexts))

/*! \brief Remove a source from an iterator

    \param iterator - the iterator handle
    \param source - the source to remove
*/
void SourcesIterator_RemoveSource(sources_iterator_t iterator, generic_source_t source);

/*! \brief Remove a voice source from an iterator

    \param iterator - the iterator handle
    \param voice_source - the source to remove
*/
void SourcesIterator_RemoveVoiceSource(sources_iterator_t iterator, voice_source_t voice_source);

/*! \brief Remove an audio source from an iterator

    \param iterator - the iterator handle
    \param source - the source to remove
*/
void SourcesIterator_RemoveAudioSource(sources_iterator_t iterator, audio_source_t audio_source);

/*! \brief Get the next registered audio source

    \param iterator - an iterator handle
    \return The next audio source in the iterator object, or audio_source_none
            if there are no more registered audio sources (the iterator is empty)
*/
audio_source_t SourcesIterator_NextAudioSource(sources_iterator_t iterator);

/*! \brief Get the next registered voice source

    \param iterator - an iterator handle
    \return The next voice source in the iterator object, or voice_source_none
            if there are no more registered voice sources (the iterator is empty)
*/
voice_source_t SourcesIterator_NextVoiceSource(sources_iterator_t iterator);

/*! \brief Get the next registered generic source

    \param iterator - an iterator handle
    \return The next generic source in the iterator object, or a generic source with
            source_type_invalid if there are no more registered sources (i.e. the iterator
            is empty)
*/
generic_source_t SourcesIterator_NextGenericSource(sources_iterator_t iterator);

/*! \brief Destroy the iterator handle

    \param iterator - the iterator handle to destroy
*/
void SourcesIterator_Destroy(sources_iterator_t iterator);

#endif /* AUDIO_SOURCES_ITERATOR_H__ */
