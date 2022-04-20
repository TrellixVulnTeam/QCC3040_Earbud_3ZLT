/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP library link priority to Voice Source mappings. These are required
            to allow the HFP Profile CAA code to be able to associate messages
            received from the HFP library with HFP profile instances stored in the
            Device List (transitively, using a lookup of the Voice Source associated
            with the HFP instance).
*/

#ifndef HFP_PROFILE_VOICE_SOURCE_LINK_PRIO_MAPPING_H_
#define HFP_PROFILE_VOICE_SOURCE_LINK_PRIO_MAPPING_H_

#include <hfp.h>
#include <voice_sources_list.h>

/*! \brief Accessor to get the HFP library link priority associated with a given Voice Source.
    \param source - the Voice Source for which to get the HFP link priority
    \return the HFP link priority associated with the voice source
*/
hfp_link_priority HfpProfile_GetHfpLinkPrioForVoiceSource(voice_source_t source);

/*! \brief Accessor to get the Voice Source associated with a given HFP library link priority.
    \param priority - the HFP link priority for which to get the Voice Source
    \return the Voice Source assigned the specified link priority
*/
voice_source_t HfpProfile_GetVoiceSourceForHfpLinkPrio(hfp_link_priority priority);

#endif /* HFP_PROFILE_VOICE_SOURCE_LINK_PRIO_MAPPING_H_ */
