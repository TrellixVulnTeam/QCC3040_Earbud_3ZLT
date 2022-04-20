/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Gaia Handover interfaces

*/
#if defined(INCLUDE_MIRRORING) && defined (INCLUDE_DFU)

#define DEBUG_LOG_MODULE_NAME gaia_handler
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "gaia_transport.h"
#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "gaia_framework_internal.h"
#include "gaia_framework_feature.h"
#include "gaia.h"
#include "tws_topology_role_change_client_if.h"

#include <panic.h>
#include <stdlib.h>

typedef struct
{
    gaia_transport_type type;
    uint8 flags;
    uint32 client_data;
    tp_bdaddr tp_bd_addr;
} gaia_transport_marshalled_t;

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/


/******************************************************************************
 * Global Declarations
 ******************************************************************************/



/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*!
    \brief Handle Veto check during handover

    Veto if there are pending messages for the task

    \return bool
*/
static bool gaiaHandover_Veto(void)
{
    bool veto = FALSE;
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);

    while (t)
    {
        /* Check if any feature plugins want to veto handover */
        veto = GaiaFrameworkFeature_QueryFeaturesHandoverVeto(t);
        if (veto)
        {
            DEBUG_LOG_V_VERBOSE("gaiaHandover_Veto, vetoed by feature on transport type %d", t->type);
            break;
        }

        /* Static handover requires transport to be disconnected */
        if (Gaia_TransportIsConnected(t) && Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_STATIC_HANDOVER))
        {
            DEBUG_LOG_DEBUG("gaiaHandover_Veto, disconnecting transport %p", t);
            gaiaFrameworkInternal_GaiaDisconnect(t);
            veto = TRUE;
        }
        else if (Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_DYNAMIC_HANDOVER))
        {
            if (t->functions->handover_veto)
            {
                veto = t->functions->handover_veto(t);
                if (veto)
                {
                    DEBUG_LOG_V_VERBOSE("gaiaHandover_Veto, vetoed by transport type %d", t->type);
                    break;
                }
            }
        }

        t = Gaia_TransportIterate(&index);
    }

    DEBUG_LOG_DEBUG("gaiaHandover_Veto: %d", veto);
    return veto;
}


static bool gaiaHandover_Marshal(const tp_bdaddr *tp_bd_addr,
                                        uint8 *buf, uint16 buf_length,
                                        uint16 *written)
{
    DEBUG_LOG_DEBUG("gaiaHandover_Marshal, bd_addr %04X-%02X-%06X",
                    tp_bd_addr->taddr.addr.nap, tp_bd_addr->taddr.addr.uap, tp_bd_addr->taddr.addr.lap);

    bool marshalled = TRUE;
    uint16 marshalled_amount = 0;
    uint8 num_transports = 0;
    if (buf_length > 1)
    {
        /* Reserve space for number of transports */
        marshalled_amount = 1;

        /* Iterate through transports finding those to be marshalled */
        gaia_transport_index index = 0;
        GAIA_TRANSPORT *t = Gaia_TransportFindByTpBdAddr(tp_bd_addr, &index);
        while (t && marshalled)
        {
            /* Check if transport supports dynamic handover */
            if (Gaia_TransportIsConnected(t) && Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_DYNAMIC_HANDOVER))
            {
                if ((buf_length - marshalled_amount) >= sizeof(gaia_transport_marshalled_t))
                {
                    DEBUG_LOG_DEBUG("gaiaHandover_Marshal, marshall transport %p, type %u", t, t->type);

                    /* Marshall common state */
                    gaia_transport_marshalled_t *md = PanicUnlessNew(gaia_transport_marshalled_t);
                    md->type = t->type;
                    md->flags = t->flags;
                    md->client_data = t->client_data;
                    md->tp_bd_addr = t->tp_bd_addr;
                    memcpy(buf + marshalled_amount, md, sizeof(gaia_transport_marshalled_t));
                    free(md);
                    marshalled_amount += sizeof(gaia_transport_marshalled_t);

                    /* Call transport function to marshall transport specific state */
                    uint16 transport_amount = 0;
                    PanicFalse(t->functions->handover_marshal != NULL);
                    marshalled = t->functions->handover_marshal(t, buf + marshalled_amount, buf_length - marshalled_amount, &transport_amount);
                    marshalled_amount += transport_amount;

                    /* Move to pre-commit state awaiting for commit */
                    t->state = GAIA_TRANSPORT_PRE_COMMIT_SECONDARY;

                    /* Increment count of marshalled transports */
                    num_transports += 1;
                }
                else
                    marshalled = FALSE;

            }

            t = Gaia_TransportFindByTpBdAddr(tp_bd_addr, &index);
        }

        /* Store number of transports marshalled */
        buf[0] = num_transports;
    }
    else
        marshalled = FALSE;

    if (marshalled)
    {
        DEBUG_LOG_DEBUG("gaiaHandover_Marshal, marshalled %u transports in %u bytes", num_transports, marshalled_amount);
        DEBUG_LOG_DATA_V_VERBOSE(buf, marshalled_amount);
    }
    else
        DEBUG_LOG_WARN("gaiaHandover_Marshal, marshalling failed");

    *written = marshalled_amount;
    return marshalled;
}


static bool gaiaHandover_Unmarshal(const tp_bdaddr *tp_bd_addr,
                                   const uint8 *buf, uint16 buf_length,
                                   uint16 *consumed)
{
    DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, bd_addr %04X-%02X-%06X, buf_length %u",
                    tp_bd_addr->taddr.addr.nap, tp_bd_addr->taddr.addr.uap, tp_bd_addr->taddr.addr.lap,
                    buf_length);

    bool unmarshalled = TRUE;
    uint16 unmarshalled_consumed = 0;
    if (buf_length >= 1)
    {
        /* Read number of transports */
        uint8 num_transports = buf[0];
        unmarshalled_consumed = 1;
        DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, unmarshalling %u transports", num_transports);

        while (unmarshalled && num_transports--)
        {
            if ((buf_length - unmarshalled_consumed) >= sizeof(gaia_transport_marshalled_t))
            {
                /* Read common transport marshalled data */
                gaia_transport_marshalled_t *md = PanicUnlessNew(gaia_transport_marshalled_t);
                memcpy(md, buf + unmarshalled_consumed, sizeof(gaia_transport_marshalled_t));
                unmarshalled_consumed += sizeof(gaia_transport_marshalled_t);

                /* Find instance that is not in use and can unmarshall successfully */
                gaia_transport_index index = 0;
                gaia_transport *t = Gaia_TransportFindService(md->type, &index);
                while (t)
                {
                    unmarshalled = FALSE;

                    /* Check transport is started (but not connected) and has unmarshalling API */
                    if (t->state == GAIA_TRANSPORT_STARTED && t->functions->handover_unmarshal)
                    {
                        /* See if this transport can successfull unmarshal the data */
                        uint16 transport_consumed = 0;
                        if (t->functions->handover_unmarshal(t, buf + unmarshalled_consumed, buf_length - unmarshalled_consumed , &transport_consumed))
                        {
                            /* Unmarshalling is successful, so update common state */
                            t->type = md->type;
                            t->flags = md->flags;
                            t->client_data = md->client_data;
                            t->tp_bd_addr = md->tp_bd_addr;

                            /* Move to pre-commit state awaiting for commit */
                            t->state = GAIA_TRANSPORT_PRE_COMMIT_PRIMARY;
                            DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, transport %p, type %u unmarshal successful", t, md->type);
                            unmarshalled_consumed += transport_consumed;
                            unmarshalled = TRUE;
                            break;
                        }
                        else
                        {
                            /* Unmarshalling wasn't successful for this transport */
                            DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, transport %p, type %u unmarshal failed", t, md->type);
                        }
                    }
                    else
                    {
                        /* Transport isn't in the correct store, or doesn't support unmarshalling */
                        DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, transport %p, type %u not available", t, md->type);
                    }

                    /* Get next transport of the required type */
                    t = Gaia_TransportFindService(md->type, &index);
                }

                free(md);
            }
            else
            {
                DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, not enough data");
            }
        }
    }
    else
        unmarshalled = FALSE;

    if (unmarshalled)
    {
        DEBUG_LOG_DEBUG("gaiaHandover_Unmarshal, unmarshalled %u bytes", unmarshalled_consumed);
        DEBUG_LOG_DATA_V_VERBOSE(buf, unmarshalled_consumed);
    }
    else
        DEBUG_LOG_WARN("gaiaHandover_Unmarshal, unmarshalling failed");

    *consumed = unmarshalled_consumed;
    return unmarshalled;
}

/*!
    \brief Component commits to the specified role

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void gaiaHandover_Commit(const tp_bdaddr *tp_bd_addr, bool is_primary)
{
    DEBUG_LOG_DEBUG("gaiaHandover_Commit, bd_addr %04X-%02X-%06X, is_primary %u",
                    tp_bd_addr->taddr.addr.nap, tp_bd_addr->taddr.addr.uap, tp_bd_addr->taddr.addr.lap,
                    is_primary);

    gaia_transport_index index = 0;
    GAIA_TRANSPORT *t = Gaia_TransportFindByTpBdAddr(tp_bd_addr, &index);
    while (t)
    {
        if ((t->state >= GAIA_TRANSPORT_PRE_COMMIT_PRIMARY) &&
            (t->state <= GAIA_TRANSPORT_PRE_COMMIT_SECONDARY))
        {
            if (is_primary)
                DEBUG_LOG_DEBUG("gaiaHandover_Commit, primary, transport %p", t);
            else
                DEBUG_LOG_DEBUG("gaiaHandover_Commit, secondary, transport %p", t);

            PanicZero(t->functions->handover_commit);
            t->functions->handover_commit(t, is_primary);
            t->state = GAIA_TRANSPORT_POST_COMMIT;
        }

        t = Gaia_TransportFindByTpBdAddr(tp_bd_addr, &index);
    }
}

static void gaiaHandover_Complete(bool is_primary)
{
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        if (t->state == GAIA_TRANSPORT_POST_COMMIT)
        {
            if (is_primary)
                DEBUG_LOG_DEBUG("gaiaHandover_Complete, primary, transport %p", t);
            else
                DEBUG_LOG_DEBUG("gaiaHandover_Complete, secondary, transport %p", t);

            PanicZero(t->functions->handover_complete);
            t->functions->handover_complete(t, is_primary);

            Gaia_TransportHandoverInd(t, TRUE, is_primary);
        }
        t = Gaia_TransportIterate(&index);
    }
}


static void gaiaHandover_Abort(void)
{
    DEBUG_LOG_DEBUG("gaiaHandover_Abort");

    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        if ((t->state >= GAIA_TRANSPORT_PRE_COMMIT_PRIMARY) &&
            (t->state <= GAIA_TRANSPORT_PRE_COMMIT_SECONDARY))
        {
            DEBUG_LOG_DEBUG("gaiaHandover_Abort, bd_addr %04X-%02X-%06X, transport %p",
                            t->tp_bd_addr.taddr.addr.nap, t->tp_bd_addr.taddr.addr.uap, t->tp_bd_addr.taddr.addr.lap,
                            t);

            PanicZero(t->functions->handover_abort);
            t->functions->handover_abort(t);
            Gaia_TransportHandoverInd(t, FALSE, t->state == GAIA_TRANSPORT_PRE_COMMIT_PRIMARY);
            if (t->state == GAIA_TRANSPORT_PRE_COMMIT_PRIMARY)
            {
                /* Aborted just before new primary (i.e. was secondary) so go back to not connected */
                t->state = GAIA_TRANSPORT_STARTED;
            }
            else if (t->state == GAIA_TRANSPORT_PRE_COMMIT_SECONDARY)
            {
                /* Aborted just before new secondary (i.e. was primary) so go back to connected */
                t->state = GAIA_TRANSPORT_CONNECTED;
            }
        }

        t = Gaia_TransportIterate(&index);
    }
}


extern const handover_interface gaia_handover_if =
{
    &gaiaHandover_Veto,
    &gaiaHandover_Marshal,
    &gaiaHandover_Unmarshal,
    &gaiaHandover_Commit,
    &gaiaHandover_Complete,
    &gaiaHandover_Abort
};


static Task gaiaHandoverTws_serverTask;

static void gaiaHandoverTws_Initialise(Task server, int32_t reconnect_delay)
{
    gaiaHandoverTws_serverTask = server;
    UNUSED(reconnect_delay);
}

static void gaiaHandoverTws_RoleChangeIndication(tws_topology_role role)
{
    DEBUG_LOG_DEBUG("gaiaHandoverTws_RoleChangeIndication, role %u", role);
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        GaiaFrameworkFeature_NotifyFeaturesRoleChangeCompleted(t, role);
        t = Gaia_TransportIterate(&index);
    }
}

static void gaiaHandoverTws_ProposeRoleChange(void)
{
    DEBUG_LOG_DEBUG("gaiaHandoverTws_ProposeRoleChange");
    /* Must inform the role change notifier that the force notification has been handled */
    MAKE_TWS_ROLE_CHANGE_ACCEPTANCE_MESSAGE(TWS_ROLE_CHANGE_ACCEPTANCE_CFM);
    message->role_change_accepted = TRUE;
    MessageSend(gaiaHandoverTws_serverTask, TWS_ROLE_CHANGE_ACCEPTANCE_CFM, message);
}

static void gaiaHandoverTws_ForceRoleChange(void)
{
    DEBUG_LOG_DEBUG("gaiaHandoverTws_ForceRoleChange");
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        GaiaFrameworkFeature_NotifyFeaturesRoleAboutToChange(t);
        t = Gaia_TransportIterate(&index);
    }
}

static void gaiaHandoverTws_PrepareRoleChange(void)
{
    DEBUG_LOG_DEBUG("gaiaHandoverTws_PrepareRoleChange");
    MessageSend(gaiaHandoverTws_serverTask, TWS_ROLE_CHANGE_PREPARATION_CFM, NULL);
}

static void gaiaHandoverTws_CancelRoleChange(void)
{
    DEBUG_LOG_DEBUG("gaiaHandoverTws_CancelRoleChange");
    gaia_transport_index index = 0;
    gaia_transport *t = Gaia_TransportIterate(&index);
    while (t)
    {
        GaiaFrameworkFeature_NotifyFeaturesRoleChangeCancelled(t);
        t = Gaia_TransportIterate(&index);
    }
}

TWS_ROLE_CHANGE_CLIENT_REGISTRATION_MAKE(GAIA_HANDOVER_TWS, gaiaHandoverTws_Initialise, gaiaHandoverTws_RoleChangeIndication,
                                         gaiaHandoverTws_ProposeRoleChange, gaiaHandoverTws_ForceRoleChange,
                                         gaiaHandoverTws_PrepareRoleChange, gaiaHandoverTws_CancelRoleChange);


#endif /* defined(INCLUDE_MIRRORING) && defined (INCLUDE_DFU) */
