/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_connection.h
\brief      Handling of connection manager events in the state proxy.
*/

#ifndef STATE_PROXY_CONNECTION_H
#define STATE_PROXY_CONNECTION_H

/* framework includes */
#include <connection_manager.h>
#include <mirror_profile.h>

/*! \brief Handle local connection events.
    \param[in] ind Connection indication event message.
*/
void stateProxy_HandleConManagerConnectInd(const CON_MANAGER_TP_CONNECT_IND_T* ind);

/*! \brief Handle local disconnection events.
    \param[in] ind Disconnection indication event message.
*/
void stateProxy_HandleConManagerDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T* ind);

/*! \brief Handle mirror profile connection indication.
    \param[in] ind The connection indication.
*/
void stateProxy_HandleMirrorProfileConnectInd(const MIRROR_PROFILE_CONNECT_IND_T *ind);

/*! \brief Handle mirror profile disconnection indication.
    \param[in] ind The disconnection indication.
*/
void stateProxy_HandleMirrorProfileDisconnectInd(const MIRROR_PROFILE_DISCONNECT_IND_T *ind);

#endif /* STATE_PROXY_CONNECTION_H */
