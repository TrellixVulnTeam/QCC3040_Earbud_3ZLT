/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       state_proxy_connection.c
\brief      Handling of connection manager events in the state proxy.
*/

/* local includes */
#include "state_proxy.h"
#include "state_proxy_private.h"
#include "state_proxy_marshal_defs.h"
#include "state_proxy_connection.h"
#include "state_proxy_link_quality.h"

/* framework includes */
#include <connection_manager.h>

void stateProxy_HandleConManagerConnectInd(const CON_MANAGER_TP_CONNECT_IND_T* ind)
{
    UNUSED(ind);
    stateProxy_LinkQualityKick();
}

void stateProxy_HandleConManagerDisconnectInd(const CON_MANAGER_TP_DISCONNECT_IND_T* ind)
{
    UNUSED(ind);
    stateProxy_LinkQualityKick();
}

void stateProxy_HandleMirrorProfileConnectInd(const MIRROR_PROFILE_CONNECT_IND_T *ind)
{
    UNUSED(ind);
    stateProxy_LinkQualityKick();
}

void stateProxy_HandleMirrorProfileDisconnectInd(const MIRROR_PROFILE_DISCONNECT_IND_T *ind)
{
    UNUSED(ind);
    stateProxy_LinkQualityKick();
}

