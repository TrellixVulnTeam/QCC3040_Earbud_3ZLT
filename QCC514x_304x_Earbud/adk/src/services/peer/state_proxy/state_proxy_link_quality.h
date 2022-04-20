/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link quality measurement
*/

#ifndef STATE_PROXY_LINK_QUALITY_H
#define STATE_PROXY_LINK_QUALITY_H

#include "connection_abstraction.h"

/*  \brief Kick link quality code to start/stop measurements based on changes
    to external state. Link quality measurements will when mirroring is active
    and there are clients registered for link quality events.
*/
void stateProxy_LinkQualityKick(void);

/*! \brief Handle interval timer for measuring link quality.
*/
void stateProxy_HandleIntervalTimerLinkQuality(void);

/*! \brief Handle remote link quality message */
void stateProxy_HandleRemoteLinkQuality(const STATE_PROXY_LINK_QUALITY_T *msg);

#endif /* STATE_PROXY_LINK_QUALITY_H */
