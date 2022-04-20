/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief      Implementation of the audio_router to route a single device at a time.
*/

#ifndef SINGLE_ENTITY_H_
#define SINGLE_ENTITY_H_

#include "audio_router.h"

/*! \brief Initialise the single entity version of the audio_router

    \param int_task unused

    \return TRUE;
 */
bool SingleEntity_Init(Task init_task);

/*! \brief Get the handlers to pass to audio_router

    \return pointer to audio_router_t interface to associate
            single_entity with the audio_router API
 */
const audio_router_t* SingleEntity_GetHandlers(void);

#endif /* SINGLE_ENTITY_H_ */
