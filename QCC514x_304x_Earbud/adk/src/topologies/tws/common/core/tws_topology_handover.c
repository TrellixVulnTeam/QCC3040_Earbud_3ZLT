/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       tws_topology_handover.c
\brief      TWS Topology Handover interfaces

*/
#ifdef INCLUDE_MIRRORING
#include "app_handover_if.h"
#include "tws_topology_goals.h"
#include "tws_topology_rule_events.h"
#include "tws_topology_private.h"
#include "tws_topology_marshal_types.h"
#include "hdma.h"

#include <panic.h>
#include <logging.h>

typedef struct
{
    uint8 reconnect_post_handover;
}tws_topology_marshal_data_t;

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool twsTopology_Veto(void);

static bool twsTopology_Marshal(const bdaddr *bd_addr,
                                marshal_type_t type,
                                void **marshal_obj);

static app_unmarshal_status_t twsTopology_Unmarshal(const bdaddr *bd_addr,
                                                    marshal_type_t type,
                                                    void *unmarshal_obj);

static void twsTopology_Commit(bool is_primary);
/******************************************************************************
 * Global Declarations
 ******************************************************************************/
static tws_topology_marshal_data_t tws_topology_marshal_data;

const marshal_type_descriptor_t marshal_type_descriptor_tws_topology_marshal_data_t = MAKE_MARSHAL_TYPE_DEFINITION_BASIC(tws_topology_marshal_data_t);

const marshal_type_info_t tws_topology_marshal_types[] = {
    MARSHAL_TYPE_INFO(tws_topology_marshal_data_t, MARSHAL_TYPE_CATEGORY_GENERIC)
};

const marshal_type_list_t tws_topology_marshal_types_list = {tws_topology_marshal_types, ARRAY_DIM(tws_topology_marshal_types)};

REGISTER_HANDOVER_INTERFACE(TWS_TOPOLOGY, &tws_topology_marshal_types_list, twsTopology_Veto, twsTopology_Marshal, twsTopology_Unmarshal, twsTopology_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! 
    \brief Handle Veto check during handover
    \return bool
*/
static bool twsTopology_Veto(void)
{
    bool veto = FALSE;

    /* Veto if there are pending goals */
    if (TwsTopology_IsAnyGoalPending())
    {
        veto = TRUE;
        DEBUG_LOG("twsTopology_Veto, Pending goals");
    }
    
    return veto;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.

    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool twsTopology_Marshal(const bdaddr *bd_addr,
                                marshal_type_t type,
                                void **marshal_obj)
{
    DEBUG_LOG_FN_ENTRY("AP: twsTopology_Marshal");
    UNUSED(bd_addr);
    *marshal_obj = NULL;

    switch (type)
    {
        case MARSHAL_TYPE(tws_topology_marshal_data_t):
        {
            tws_topology_marshal_data.reconnect_post_handover = TwsTopologyGetTaskData()->reconnect_post_handover;
            DEBUG_LOG("twsTopology_Marshal reconnect_post_handover %d", tws_topology_marshal_data.reconnect_post_handover);
            *marshal_obj = &tws_topology_marshal_data;
            return TRUE;
        }

        default:
        break;
    }

    return FALSE;
}

/*!
    \brief The function shall copy the unmarshal_obj associated to specific
            marshal type \ref marshal_type_t

    \param[in] bd_addr       Bluetooth address of the link to be unmarshalled
                             \ref bdaddr
    \param[in] type          Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.

    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t twsTopology_Unmarshal(const bdaddr *bd_addr,
                                                    marshal_type_t type,
                                                    void *unmarshal_obj)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_Unmarshal");
    UNUSED(bd_addr);

    switch(type)
    {
        case MARSHAL_TYPE(tws_topology_marshal_data_t):
        {
            /* Unmarshal the topology data */
            tws_topology_marshal_data_t *data = (tws_topology_marshal_data_t*)unmarshal_obj;
            DEBUG_LOG("twsTopology_Unmarshal reconnect_post_handover %d",data->reconnect_post_handover);
            /* Set the topology data */
            twsTopology_SetReconnectPostHandover(data->reconnect_post_handover);
            return UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        default:
        break;
    }

    return UNMARSHAL_FAILURE;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void twsTopology_Commit(bool is_primary)
{
    if (is_primary)
    {
        DEBUG_LOG("twsTopology_Commit, Create HDMA, Set Role Primary");
        twsTopology_CreateHdma();
        twsTopology_SetRole(tws_topology_role_primary);
        twsTopology_RulesSetEvent(TWSTOP_RULE_EVENT_ROLE_SWITCH);
    }
    else
    {
        DEBUG_LOG("twsTopology_Commit, Destroy HDMA, Set Role Secondary");
        twsTopology_DestroyHdma();
        /* RESET the reconnect_post_handover flag in secondary. */
        twsTopology_SetReconnectPostHandover(FALSE);
        /* Don't set role here, the procedure sets the role later when handover
           script is complete */
    }
}

#endif /* INCLUDE_MIRRORING */
