/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_aanc.h
\brief      State proxy aanc event handling.
*/

#ifndef STATE_PROXY_AANC_H
#define STATE_PROXY_AANC_H
#include <aanc_quiet_mode.h>
#include <anc_state_manager.h>
#include <state_proxy_private.h>
#include <state_proxy.h>

/*! \brief Get AANC data for initial state message. */
void stateProxy_GetInitialAancData(void);

/*! \brief Handle remote events for AANC data update during reconnect cases. */
void stateProxy_HandleInitialPeerAancData(state_proxy_data_t * new_state);

/*! \brief Handle local events for ANC data update. */
void stateProxy_HandleAancUpdateInd(MessageId id, Message aanc_data);

/*! \brief Handle remote events for AANC data update. */
void stateProxy_HandleRemoteAancUpdate(const STATE_PROXY_AANC_DATA_T* aanc_data);

/*! \brief Handle local events for AANC logging update. */
void stateProxy_HandleAancLoggingUpdate(MessageId id, Message aanc_data);

/*! \brief Handle remote events for AANC logging update.
        Updates ANC SM with remote device logging data if local device is primary*/
void stateProxy_HandleRemoteAancLoggingUpdate(const STATE_PROXY_AANC_LOGGING_T* remote_aanc_data);


#endif /* STATE_PROXY_AANC_H */
