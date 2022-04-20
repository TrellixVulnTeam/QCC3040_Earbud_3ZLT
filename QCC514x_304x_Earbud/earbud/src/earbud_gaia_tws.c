/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support for the earbud gaia plugin
*/

#ifdef INCLUDE_GAIA
#include "earbud_gaia_tws.h"

#include <logging.h>
#include <message.h>
#include <panic.h>

#include <earbud_gaia_plugin.h>
#include <gaia_framework_feature.h>
#include "tws_topology_role_change_client_if.h"

/*! \brief GAIA TWS data structure */
typedef struct
{
    /*! TWS server task */
    Task serverTask;
    /*! TWS server reconncection delay */
    int32_t reconnection_delay;
    /*! TWS current role */
    tws_topology_role current_role;
} earbud_gaia_tws_data_t;

static void earbudGaiaTws_Initialise(Task server, int32_t reconnect_delay);
static void earbudGaiaTws_RoleChangeIndication(tws_topology_role role);
static void earbudGaiaTws_ProposeRoleChange(void);
static void earbudGaiaTws_ForceRoleChange(void);
static void earbudGaiaTws_PrepareRoleChange(void);
static void earbudGaiaTws_CancelRoleChange(void);

static earbud_gaia_tws_data_t earbud_gaia_tws_data =
{
    .serverTask = NULL,
    .reconnection_delay = D_SEC(6),
    .current_role = tws_topology_role_none
};

bool EarbudGaiaTws_Init(Task task)
{
    UNUSED(task);
    DEBUG_LOG_INFO("EarbudGaiaTws_Init");
    return TRUE;
}

static void earbudGaiaTws_Initialise(Task server, int32_t reconnect_delay)
{
    DEBUG_LOG_INFO("earbudGaiaTws_Initialise server = 0x%p, reconnect delay = %d",server,reconnect_delay);
    earbud_gaia_tws_data.reconnection_delay = reconnect_delay;
    earbud_gaia_tws_data.serverTask = server;
}

static void earbudGaiaTws_RoleChangeIndication(tws_topology_role role)
{
    DEBUG_LOG_INFO("earbudGaiaTws_RoleChangeIndication, role %u", role);
    earbud_gaia_tws_data.current_role = role;
    if (role == tws_topology_role_primary)
        EarbudGaiaPlugin_RoleChanged(role);
}

static void earbudGaiaTws_ProposeRoleChange(void)
{
    bool accept = TRUE;

    /* Inform the mobile app that the device is going to Primary/Secondary swap */
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        /* Check if GAIA is connected, if it is check if Earbud notifications have not been registered */
        if (Gaia_TransportIsConnected(t) && !GaiaFrameworkFeature_IsNotificationsActive(t, GAIA_EARBUD_FEATURE_ID))
        {
            DEBUG_LOG_WARN("earbudGaiaTws_ProposeRoleChange, notications not enabled on transport %p", t);
            accept = FALSE;
        }
        t = Gaia_TransportIterate(&index);
    }

    DEBUG_LOG_INFO("earbudGaiaTws_ProposeRoleChange, accept %u", accept);

    /* Must inform the role change notifier that the force notification has been handled */
    MAKE_TWS_ROLE_CHANGE_ACCEPTANCE_MESSAGE(TWS_ROLE_CHANGE_ACCEPTANCE_CFM);
    message->role_change_accepted = accept;
    MessageSend(earbud_gaia_tws_data.serverTask, TWS_ROLE_CHANGE_ACCEPTANCE_CFM, message);
}

static void earbudGaiaTws_ForceRoleChange(void)
{
    DEBUG_LOG_INFO("earbudGaiaTws_ForceRoleChange");
    EarbudGaiaPlugin_PrimaryAboutToChange(EarbudGaiaTws_MobileAppReconnectionDelay());
}

static void earbudGaiaTws_PrepareRoleChange(void)
{
    DEBUG_LOG_INFO("earbudGaiaTws_PrepareRoleChange");

    /* We must inform the role change notifier that the force notification has been handled */
    MessageSend(earbud_gaia_tws_data.serverTask, TWS_ROLE_CHANGE_PREPARATION_CFM, NULL);
}

static void earbudGaiaTws_CancelRoleChange(void)
{
    DEBUG_LOG_INFO("earbudGaiaTws_CancelRoleChange");
}

TWS_ROLE_CHANGE_CLIENT_REGISTRATION_MAKE(EARBUD_GAIA_TWS, earbudGaiaTws_Initialise, earbudGaiaTws_RoleChangeIndication,
                                         earbudGaiaTws_ProposeRoleChange, earbudGaiaTws_ForceRoleChange,
                                         earbudGaiaTws_PrepareRoleChange, earbudGaiaTws_CancelRoleChange);

#endif /* INCLUDE_GAIA */
