/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to AV context providers
*/

#ifndef AV_CONTEXT_PROVIDER_IF_H_
#define AV_CONTEXT_PROVIDER_IF_H_

#include "audio_sources.h"

typedef struct
{
    /*! \brief Populates the context for this source
        \param source The source id
        \return TRUE if the context was populated, FALSE if the provider has no such information
     */
    bool (*PopulateSourceContext)(audio_source_t source, audio_source_provider_context_t *context);
} av_context_provider_if_t;

#endif /* AV_CONTEXT_PROVIDER_IF_H_ */
