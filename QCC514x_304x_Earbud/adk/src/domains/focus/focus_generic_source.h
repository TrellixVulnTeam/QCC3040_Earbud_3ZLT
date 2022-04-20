/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Focus interface definition for instantiating a module which shall
            return the focused Generic Source.
*/
#ifndef FOCUS_GENERIC_SOURCE_H
#define FOCUS_GENERIC_SOURCE_H

#include "focus_types.h"

#include <source_param_types.h>

/*! \brief Focus interface callback used by Focus_GetFocusedGenericSourceForAudioRouting API */
typedef generic_source_t (*focus_get_generic_source_for_routing_t)(void);

/*! \brief Structure used to configure the focus interface callbacks to be used
           to access the focused generic source. */
typedef struct
{
    focus_get_generic_source_for_routing_t for_audio_routing;

} focus_get_generic_source_t;

/*! \brief Configure a set of function pointers to use for retrieving the focused generic source

    \param a structure containing the functions implementing the focus interface for retrieving
           the focused generic source.
*/
void Focus_ConfigureGenericSource(focus_get_generic_source_t const * focus_get_generic_source);

/*! \brief Get the focused generic source for audio routing purposes

    \return a generic_source_t which is the focused source that should be routed. If there is
            no valid source to route, the generic source will have the type source_type_invalid.
*/
generic_source_t Focus_GetFocusedGenericSourceForAudioRouting(void);

#endif // FOCUS_GENERIC_SOURCE_H
