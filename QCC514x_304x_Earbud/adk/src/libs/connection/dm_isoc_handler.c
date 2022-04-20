/****************************************************************************
Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_isoc_handler.c

DESCRIPTION
    This file contains the functions responsible for handling ISOC-related
    Bluestack prims (DM-ISOC).

NOTES

*/


/****************************************************************************
    Header files
*/
#include "connection.h"
#include "connection_private.h"
#include "common.h"
#include "dm_link_policy_handler.h"
#include "dm_isoc_handler.h"

#include <bdaddr.h>
#include <vm.h>
#include <sink.h>
#include <stream.h>

#ifndef CL_EXCLUDE_ISOC

/*!
 * \brief Converts CIS params between the CL and Bluestack structs.
 */
static void convertCisParams(DM_CIS_PARAM_T * dm_cis_params, CL_CIS_PARAM_T * cl_cis_params)
{
    cl_cis_params->cig_sync_delay = dm_cis_params->cig_sync_delay;
    cl_cis_params->cis_sync_delay = dm_cis_params->cis_sync_delay;
    cl_cis_params->transport_latency_m_to_s = dm_cis_params->transport_latency_m_to_s;
    cl_cis_params->transport_latency_s_to_m = dm_cis_params->transport_latency_s_to_m;
    cl_cis_params->phy_m_to_s = dm_cis_params->phy_m_to_s;
    cl_cis_params->phy_s_to_m = dm_cis_params->phy_s_to_m;
    cl_cis_params->nse = dm_cis_params->nse;
    cl_cis_params->bn_m_to_s = dm_cis_params->bn_m_to_s;
    cl_cis_params->bn_s_to_m = dm_cis_params->bn_s_to_m;
    cl_cis_params->ft_m_to_s = dm_cis_params->ft_m_to_s;
    cl_cis_params->ft_s_to_m = dm_cis_params->ft_s_to_m;
    cl_cis_params->max_pdu_m_to_s = dm_cis_params->max_pdu_m_to_s;
    cl_cis_params->max_pdu_s_to_m = dm_cis_params->max_pdu_s_to_m;
    cl_cis_params->iso_interval = dm_cis_params->iso_interval;
}

/****************************************************************************
NAME
    connectionHandleIsocRegisterReq

DESCRIPTION
    Register the task as utilising Isochronous connections. This registers it
    with BlueStack. On an incoming Isochronous connection the task will be
    asked whether its willing to accept it. All tasks wishing to use
    Isochronous connections must call this register function.

RETURNS
    void
*/
void connectionHandleIsocRegisterReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REGISTER_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /*
        Send a register request to BlueStack it will keep track of the task id.
        This is sent by each task wishing to use Isochronous connections.
        */
        MAKE_PRIM_T(DM_ISOC_REGISTER_REQ);
        prim->isoc_type     = req->isoc_type;
        prim->reg_context   = (context_t)req->theAppTask;

        VmSendDmPrim(prim);

    /* Commented out for now. This might be a nice later addition if needed. */
     /*{
         MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REGISTER_TIMEOUT_IND);
         message->theAppTask = req->theAppTask;
         MessageSendLater(
                 connectionGetCmTask(),
                 CL_INTERNAL_ISOC_REGISTER_TIMEOUT_IND,
                 message,
                 (uint32)ISOC_REGISTER_TIMEOUT
                 );
     }*/
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REGISTER_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_REGISTER_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleIsocRegisterCfm

DESCRIPTION
    Task has been sucessfully registered for receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
void connectionHandleIsocRegisterCfm(connectionDmIsocState *state, const DM_ISOC_REGISTER_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {
        /* Commented out for now. This might be a nice later addition if needed. */
         /* Cancel the message checking we got a register cfm from BlueStack */
         /*(void) MessageCancelFirst(
                 connectionGetCmTask(),
                 CL_INTERNAL_ISOC_REGISTER_TIMEOUT_IND
                 );*/

        if (cfm->reg_context)
        {
            MAKE_CL_MESSAGE(CL_DM_ISOC_REGISTER_CFM);
            message->status = connectionConvertHciStatus(cfm->status);
            message->isoc_type = cfm->isoc_type;
            MessageSend(
                    state->dmIsocOpLock,
                    CL_DM_ISOC_REGISTER_CFM,
                    message
                    );
        }

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_REGISTER_CFM received without a request being sent.\n"));
    }
}


/****************************************************************************
NAME
    connectionHandleIsocRegisterTimeoutInd

DESCRIPTION
    Task has not been registered for receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/*void connectionHandleIsocRegisterTimeoutInd(
         const CL_INTERNAL_ISOC_REGISTER_TIMEOUT_IND_T *ind
         )
 {
     MAKE_CL_MESSAGE(CL_DM_ISOC_REGISTER_CFM);
     message->status = 1; */ /* Generic failure status, TODO: replace with actual timeout status code. */
     /*MessageSend(
             ind->theAppTask,
             CL_DM_ISOC_REGISTER_CFM,
             message
             );
 }*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterReq

DESCRIPTION
    Unregister task with BlueStack indicating it is no longer interested in
    being notified about incoming Isochronous connections.

RETURNS
    void
*/
/*void connectionHandleIsocUnregisterReq(
         const CL_INTERNAL_ISOC_UNREGISTER_REQ_T *req
         )
 {*/
     /* Send an unregister request to BlueStack */
     /*MAKE_PRIM_T(DM_ISOC_UNREGISTER_REQ);
     prim->phandle = 0;
     prim->pv_cbarg = (context_t)req->theAppTask;
     VmSendDmPrim(prim);

     {
         MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_UNREGISTER_TIMEOUT_IND);
         message->theAppTask = req->theAppTask;
         MessageSendLater(
                 connectionGetCmTask(),
                 CL_INTERNAL_ISOC_UNREGISTER_TIMEOUT_IND,
                 message,
                 (uint32)ISOC_UNREGISTER_TIMEOUT
                 );
     }
 }*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterCfm

DESCRIPTION
    Task has been sucessfully unregistered from receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/*void connectionHandleIsocUnregisterCfm(const DM_ISOC_UNREGISTER_CFM_T *cfm)
 {*/
     /* Cancel the message checking we got a register cfm from BlueStack */
     /*(void) MessageCancelFirst(
             connectionGetCmTask(),
             CL_INTERNAL_ISOC_UNREGISTER_TIMEOUT_IND
             );

     if (cfm->pv_cbarg)
     {
         MAKE_CL_MESSAGE(CL_DM_ISOC_UNREGISTER_CFM);
         message->status = success;
         MessageSend(
                 (Task) (cfm->pv_cbarg),
                 CL_DM_ISOC_UNREGISTER_CFM,
                 message
                 );
     }
 }*/


/****************************************************************************
NAME
    connectionHandleIsocUnregisterTimeoutInd

DESCRIPTION
    Task has not been unregistered from receiving Isochronous connection
    notifications - inform the client.

RETURNS
    void
*/
/* void connectionHandleIsocUnregisterTimeoutInd(
         const CL_INTERNAL_ISOC_UNREGISTER_TIMEOUT_IND_T *ind
         )
 {
     MAKE_CL_MESSAGE(CL_DM_ISOC_UNREGISTER_CFM);
     message->status = fail;
     MessageSend(
             ind->theAppTask,
             CL_DM_ISOC_UNREGISTER_CFM,
             message
             );
 }*/


/****************************************************************************
NAME
    connectionHandleIsocConnectReq

DESCRIPTION
    This function will initiate an Isochronous Connection request.

RETURNS
    void
*/
void connectionHandleIsocConnectReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CIS_CONNECT_REQ_T *req)
{
     /* Check the state of the task lock before doing anything. */
    if (!state->dmIsocOpLock)
    {
        uint8 i;
        MAKE_PRIM_T(DM_ISOC_CIS_CONNECT_REQ);

        /* One request at a time, set the ad lock. */
        state->dmIsocOpLock     = req->theAppTask;

        prim->con_context       = (context_t) req->theAppTask;
        prim->cis_count         = req->cis_count;

        for (i = 0; i < req->cis_count; i++)
        {
            prim->cis_conn[i]->cis_handle = req->cis_conn[i]->cis_handle;
            BdaddrConvertTpVmToBluestack(&prim->cis_conn[i]->tp_addrt, &req->cis_conn[i]->tpaddr);
        }

        VmSendDmPrim(prim);
    }
    else
    {
        /* There is already an Isoc Request being processed, queue this one. */
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CIS_CONNECT_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_CIS_CONNECT_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleIsocConnectCfm

DESCRIPTION

    Response to the Isochronous connect request indicating either that an
    Isochronous has been opened or that the attempt has failed.

RETURNS
    void
*/
void connectionHandleIsocConnectCfm(connectionDmIsocState *state, const DM_ISOC_CIS_CONNECT_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {
        MAKE_CL_MESSAGE(CL_DM_ISOC_CIS_CONNECT_CFM);

        message->status = connectionConvertHciStatus(cfm->status);
        BdaddrConvertTpBluestackToVm(&message->tpaddr, &cfm->tp_addr);
        message->cis_handle = cfm->cis_handle;
        convertCisParams(&cfm->cis_params, &message->cis_params);


        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_CIS_CONNECT_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_CIS_CONNECT_CFM received without a request or response being sent.\n"));
    }
}


/****************************************************************************
NAME
    connectionHandleIsocConnectInd

DESCRIPTION
    Indication that the remote device wishes to open an Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocConnectInd(connectionDmIsocState *state, const DM_ISOC_CIS_CONNECT_IND_T *ind)
{
    /* Make a connection indication to be sent to the application. */
    MAKE_CL_MESSAGE(CL_DM_ISOC_CIS_CONNECT_IND);
    BdaddrConvertTpBluestackToVm(&message->tpaddr, &ind->tp_addrt);
    message-> cis_handle = ind->cis_handle;
    message-> cig_id = ind->cig_id;
    message-> cis_id = ind->cis_id;

    /* Check if there are any Isoc-related messages currently being processed. */
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = (Task) ind->reg_context;

        MessageSend(
                (Task) ind->reg_context,
                CL_DM_ISOC_CIS_CONNECT_IND,
                message
                );
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MessageSendConditionallyOnTask(
                    (Task) ind->reg_context,
                    CL_DM_ISOC_CIS_CONNECT_IND,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleIsocConnectRes

DESCRIPTION
    Response accepting (or not) an incoming Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocConnectRes(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CIS_CONNECT_RES_T *res)
{
    if (state->dmIsocOpLock)
    {
        MAKE_PRIM_T(DM_ISOC_CIS_CONNECT_RSP);

        prim->status        = res->status;
        prim->cis_handle    = res->cis_handle;
        prim->con_context   = (context_t) res->theAppTask;

        VmSendDmPrim(prim);

        /* We are not releasing the lock yet, as the CFM message from Bluestack is the last in the chain. */
    }
    else
    {
        CL_DEBUG(("CL_INTERNAL_ISOC_CIS_CONNECT_RES received without a connection indication being received.\n"));
    }
}


/****************************************************************************
NAME
    connectionHandleIsocDisconnectReq

DESCRIPTION
    Request to disconnect an existing Isochronous connection.

RETURNS
    void
*/
void connectionHandleIsocDisconnectReq(
        connectionDmIsocState *state,
        const CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ_T *req
        )
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /* Send a ISOC disconnect request to BlueStack */
        MAKE_PRIM_T(DM_ISOC_CIS_DISCONNECT_REQ);
        prim->cis_handle    = req->cis_handle;
        prim->reason        = req->reason;
        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleIsocDisconnectInd

DESCRIPTION

    Indication that the Isochronous connection has been disconnected. The
    disconnect will have been initiated by the remote device.

RETURNS
    void
*/
void connectionHandleIsocDisconnectInd(connectionDmIsocState *state, const DM_ISOC_CIS_DISCONNECT_IND_T *ind)
{
    /* Indication that the Isochronous connection has been disconnected -
     * tell the relevant task
     */
    MAKE_CL_MESSAGE(CL_DM_ISOC_CIS_DISCONNECT_IND);

    message->cis_handle = ind->cis_handle;
    message->reason = connectionConvertHciStatus(ind->reason);

    /* Check if there are any Isoc-related messages currently being processed. */
    if (!state->dmIsocOpLock)
    {
        /* No lock used since this is the only message in this sequence. */

        MessageSend(
                (Task) ind->con_context,
                CL_DM_ISOC_CIS_DISCONNECT_IND,
                message
                );
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MessageSendConditionallyOnTask(
                    (Task) ind->con_context,
                    CL_DM_ISOC_CIS_DISCONNECT_IND,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}


/****************************************************************************
NAME
    connectionHandleIsocDisconnectCfm

DESCRIPTION
    Confirmation that the Isochronous connection has been disconnected. The
    discconect will have been initiated by the local device.

RETURNS
    void
*/
void connectionHandleIsocDisconnectCfm(connectionDmIsocState *state, const DM_ISOC_CIS_DISCONNECT_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {

        /* Indication that the Isochronous connection has been disconnected -
         * tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_CIS_DISCONNECT_CFM);

        message->cis_handle = cfm->cis_handle;
        message->status = connectionConvertHciStatus(cfm->status);

        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_CIS_DISCONNECT_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_CIS_DISCONNECT_CFM received without a request being sent.\n"));
    }
}


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateReq

DESCRIPTION
    Request to change the connection parameters of an existing Isochronous
    connection.

RETURNS
    void
*/
 /*void connectionHandleIsocRenegotiateReq(
         const CL_INTERNAL_ISOC_RENEGOTIATE_REQ_T *req
         )
 {
     if ( !req->audio_sink )
     {*/
         /* Sink not valid send an error to the client */
         /*sendIsocConnectCfmToClient(
                 0,
                 req->theAppTask,
                 NULL,
                 hci_error_no_connection,
                 0
                 );
     }
     else
     {
         DM_ISOC_CONFIG_T* config
             = (DM_ISOC_CONFIG_T*) PanicUnlessMalloc(sizeof(DM_ISOC_CONFIG_T));
         MAKE_PRIM_T(DM_ISOC_RENEGOTIATE_REQ);

         prim->length = 0;
         prim->handle = PanicZero(SinkGetScoHandle(req->audio_sink));

         config->max_latency = req->config_params.max_latency;
         config->retx_effort = req->config_params.retx_effort;
         config->voice_settings = req->config_params.voice_settings;
         config->packet_type = (hci_pkt_type_t)req->config_params.packet_type;
         config->rx_bdw = req->config_params.rx_bandwidth;
         config->tx_bdw = req->config_params.tx_bandwidth;
         prim->u.config = VmGetHandleFromPointer(config);*/

         /* EDR bits use inverted logic at HCI interface */
         /* prim->packet_type ^= sync_all_edr_esco;*/
        /* VmSendDmPrim(prim);
     }
 }*/


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateInd

DESCRIPTION
    Indication that remote device has changed the connection parameters of an
    existing Isochronous connection.

RETURNS
    void
*/
/* void connectionHandleIsocRenegotiateInd(const DM_ISOC_RENEGOTIATE_IND_T *ind)
 {
     if (ind->pv_cbarg)
     {
         MAKE_CL_MESSAGE(CL_DM_ISOC_RENEGOTIATE_IND);

         message->audio_sink = StreamScoSink(ind->handle);
         message->status = connectionConvertHciStatus(ind->status);
         MessageSend(
                 (Task) (ind->pv_cbarg),
                 CL_DM_ISOC_RENEGOTIATE_IND,
                 message
                 );
     }
 }*/


/****************************************************************************
NAME
    connectionHandleIsocRenegotiateCfm

DESCRIPTION

    Confirmation of local device's attempt to change the connection parameters
    of an existing Isochronous connection.


RETURNS
    void
*/
 /*void connectionHandleIsocRenegotiateCfm(const DM_ISOC_RENEGOTIATE_CFM_T *cfm)
 {
     if (cfm->pv_cbarg)
     {
         MAKE_CL_MESSAGE(CL_DM_ISOC_RENEGOTIATE_IND);

         message->audio_sink = StreamScoSink(cfm->handle);
         message->status = connectionConvertHciStatus(cfm->status);
         MessageSend(
                 (Task) (cfm->pv_cbarg),
                 CL_DM_ISOC_RENEGOTIATE_IND,
                 message
                 );
     }
 }*/

/****************************************************************************
NAME
    connectionHandleIsocConfigureCigReq

DESCRIPTION
    Request to configure a CIG.


RETURNS
    void
*/
void connectionHandleIsocConfigureCigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /* Send a CIG configure request to BlueStack. */
        MAKE_PRIM_T(DM_ISOC_CONFIGURE_CIG_REQ);

        prim->context                       = (context_t) req->theAppTask;
        prim->sdu_interval_m_to_s           = req->sdu_interval_m_to_s;
        prim->sdu_interval_s_to_m           = req->sdu_interval_s_to_m;
        prim->max_transport_latency_m_to_s  = req->max_transport_latency_m_to_s;
        prim->max_transport_latency_s_to_m  = req->max_transport_latency_s_to_m;
        prim->cig_id                        = req->cig_id;
        prim->sca                           = req->sca;
        prim->packing                       = req->packing;
        prim->framing                       = req->framing;
        prim->cis_count                     = req->cis_count;

        memmove(prim->cis_config, req->cis_config, req->cis_count * sizeof(CL_DM_CIS_CONFIG_T));

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocConfigureCigCfm

DESCRIPTION
    Confirmation of a local device's attempt to configure a CIG.


RETURNS
    void
*/
void connectionHandleIsocConfigureCigCfm(connectionDmIsocState *state, const DM_ISOC_CONFIGURE_CIG_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {
        if (state->dmIsocOpLock == (Task) cfm->context)
        {
            /* Confirmation that the CIG has been configured -
             * tell the relevant task. */
            MAKE_CL_MESSAGE(CL_DM_ISOC_CONFIGURE_CIG_CFM);

            message->status     = connectionConvertHciStatus(cfm->status);
            message->cig_id     = cfm->cig_id;
            message->cis_count  = cfm->cis_count;

            memmove(message->cis_handles, cfm->cis_handles, cfm->cis_count * sizeof(uint16));

            MessageSend(
                    (Task) cfm->context,
                    CL_DM_ISOC_CONFIGURE_CIG_CFM,
                    message
                    );

            /* Release Isoc operation lock. */
            state->dmIsocOpLock = NULL;
        }
        else
        {
            CL_DEBUG(("The locking task does not match the prim's target task.\n"));
        }
    }
    else
    {
        CL_DEBUG(("DM_ISOC_CONFIGURE_CIG_CFM received without a request being sent.\n"));
    }
}


/****************************************************************************
NAME
    connectionHandleIsocRemoveCigReq

DESCRIPTION
    Request to remove a CIG.


RETURNS
    void
*/
void connectionHandleIsocRemoveCigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REMOVE_CIG_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /* Send a Remove CIG request to BlueStack. */
        MAKE_PRIM_T(DM_ISOC_REMOVE_CIG_REQ);

        prim->cig_id = req->cig_id;

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REMOVE_CIG_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_REMOVE_CIG_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocRemoveCigCfm

DESCRIPTION
    Confirmation of a local device's attempt to remove a CIG.


RETURNS
    void
*/
void connectionHandleIsocRemoveCigCfm(connectionDmIsocState *state, const DM_ISOC_REMOVE_CIG_CFM_T *cfm)
{
    if (state->dmIsocOpLock == (Task) cfm->context)
    {
        /* Confirmation that the CIG has been removed; tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_REMOVE_CIG_CFM);

        message->cig_id = cfm->cig_id;
        message->status = connectionConvertHciStatus(cfm->status);

        MessageSend(
                (Task) cfm->context,
                CL_DM_ISOC_REMOVE_CIG_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_REMOVE_CIG_CFM received without a request being sent.\n"));
    }
}

/****************************************************************************
NAME
    connectionHandleIsocSetupDataPathReq

DESCRIPTION
    Request to set an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocSetupDataPathReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        uint8 i;

        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /* Send a Isoc Data path setup request to BlueStack. */
        MAKE_PRIM_T(DM_ISOC_SETUP_ISO_DATA_PATH_REQ);
        prim->handle    = req->cis_handle;
        prim->data_path_direction = req->data_path_direction;
        prim->data_path_id = req->data_path_id;

        for (i = 0; i < ISOC_CODEC_ID_SIZE; i++)
        {
            prim->codec_id[i] = req->codec_id[i];
        }

        prim->controller_delay = req->controller_delay;
        prim->codec_config_length = req->codec_config_length;

        for (i = 0; i < HCI_CODEC_CONFIG_DATA_PTRS; i++)
        {
            prim->codec_config_data[i] = NULL;
        }

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocSetupDataPathCfm

DESCRIPTION
    Confirmation of a local device's attempt to set an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocSetupDataPathCfm(connectionDmIsocState *state, const DM_ISOC_SETUP_ISO_DATA_PATH_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {

        /* Confirmation that the Isochronous data path has been set up -
         * tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_CFM);

        message->handle = cfm->handle;
        message->status = connectionConvertHciStatus(cfm->status);

        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_SETUP_ISO_DATA_PATH_CFM received without a request being sent.\n"));
    }
}

/****************************************************************************
NAME
    connectionHandleIsocRemoveDataPathReq

DESCRIPTION
    Request to remove an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocRemoveDataPathReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ_T * req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        /* Send a Remove Isoc Data path request to BlueStack. */
        MAKE_PRIM_T(DM_ISOC_REMOVE_ISO_DATA_PATH_REQ);
        prim->handle    = req->handle;
        prim->data_path_direction = req->data_path_direction;

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocRemoveDataPathCfm

DESCRIPTION
    Confirmation of a local device's attempt to remove an Isochronous connection's data path.


RETURNS
    void
*/
void connectionHandleIsocRemoveDataPathCfm(connectionDmIsocState *state, const DM_ISOC_REMOVE_ISO_DATA_PATH_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {
        if (state->dmIsocOpLock == (Task) cfm->con_context)
        {
            /* Confirmation that the Isochronous data path has been removed -
             * tell the relevant task. */
            MAKE_CL_MESSAGE(CL_DM_ISOC_REMOVE_ISO_DATA_PATH_CFM);

            message->handle = cfm->handle;
            message->status = connectionConvertHciStatus(cfm->status);

            MessageSend(
                    (Task) cfm->con_context,
                    CL_DM_ISOC_REMOVE_ISO_DATA_PATH_CFM,
                    message
                    );

            /* Release Isoc operation lock. */
            state->dmIsocOpLock = NULL;
        }
        else
        {
            CL_DEBUG(("The locking task does not match the prim's target task.\n"));
        }
    }
    else
    {
        CL_DEBUG(("DM_ISOC_REMOVE_ISO_DATA_PATH_CFM received without a request being sent.\n"));
    }
}

/****************************************************************************
NAME
    connectionHandleIsocCreateBigReq

DESCRIPTION
    Request to create a BIG


RETURNS
    void
*/
void connectionHandleIsocCreateBigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_CREATE_BIG_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        MAKE_PRIM_T(DM_ISOC_CREATE_BIG_REQ);

        prim->con_context   = (context_t) req->theAppTask;

        prim->big_config.sdu_interval            = req->big_config.sdu_interval;
        prim->big_config.max_sdu                 = req->big_config.max_sdu;
        prim->big_config.max_transport_latency   = req->big_config.max_transport_latency;
        prim->big_config.rtn                     = req->big_config.rtn;
        prim->big_config.phy                     = req->big_config.phy;
        prim->big_config.packing                 = req->big_config.packing;
        prim->big_config.framing                 = req->big_config.framing;

        prim->big_handle    = req->big_handle;
        prim->adv_handle    = req->adv_handle;
        prim->num_bis       = req->num_bis;
        prim->encryption    = req->encryption;

        memmove(prim->broadcast_code, req->broadcast_code, BROADCAST_CODE_SIZE);

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CREATE_BIG_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_CREATE_BIG_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocCreateBigCfm

DESCRIPTION
    Confirmation of a local device's attempt to create a BIG


RETURNS
    void
*/
void connectionHandleIsocCreateBigCfm(connectionDmIsocState *state, const DM_ISOC_CREATE_BIG_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {

        /* Confirmation that the a BIG has been created - tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_CREATE_BIG_CFM);

        message->big_sync_delay = cfm->big_sync_delay;

        message->big_params.transport_latency_big   = cfm->big_params.transport_latency_big;
        message->big_params.max_pdu                 = cfm->big_params.max_pdu;
        message->big_params.iso_interval            = cfm->big_params.iso_interval;
        message->big_params.phy                     = cfm->big_params.phy;
        message->big_params.nse                     = cfm->big_params.nse;
        message->big_params.bn                      = cfm->big_params.bn;
        message->big_params.pto                     = cfm->big_params.pto;
        message->big_params.irc                     = cfm->big_params.irc;

        message->big_handle     = cfm->big_handle;
        message->status         = connectionConvertHciStatus(cfm->status);
        message->num_bis        = cfm->num_bis;
        message->bis_handles    = VmGetPointerFromHandle(cfm->bis_handles);

        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_CREATE_BIG_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_CREATE_BIG_CFM received without a request being sent.\n"));
    }
}

/****************************************************************************
NAME
    connectionHandleIsocTerminateBigReq

DESCRIPTION
    Request to terminate a BIG


RETURNS
    void
*/
void connectionHandleIsocTerminateBigReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_TERMINATE_BIG_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        MAKE_PRIM_T(DM_ISOC_TERMINATE_BIG_REQ);

        prim->big_handle    = req->big_handle;
        prim->reason        = req->reason;

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_TERMINATE_BIG_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_TERMINATE_BIG_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}
/****************************************************************************
NAME
    connectionHandleIsocTerminateBigCfm

DESCRIPTION
    Confirmation of a local device's attempt to terminate a BIG


RETURNS
    void
*/
void connectionHandleIsocTerminateBigCfm(connectionDmIsocState *state, const DM_ISOC_TERMINATE_BIG_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {

        /* Confirmation that the a BIG has been created - tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_TERMINATE_BIG_CFM);

        message->big_handle     = cfm->big_handle;
        message->status         = connectionConvertHciStatus(cfm->status_or_reason);

        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_TERMINATE_BIG_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_TERMINATE_BIG_CFM received without a request being sent.\n"));
    }
}
/****************************************************************************
NAME
    connectionHandleIsocBigCreateSyncReq

DESCRIPTION
    Request to synchronize to a BIG


RETURNS
    void
*/
void connectionHandleIsocBigCreateSyncReq(connectionDmIsocState *state, const CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ_T *req)
{
    if (!state->dmIsocOpLock)
    {
        uint8 *ptr;
        uint8 data_size = req->num_bis * sizeof(uint8);

        /* Store the requesting task in the lock. */
        state->dmIsocOpLock = req->theAppTask;

        MAKE_PRIM_T(DM_ISOC_BIG_CREATE_SYNC_REQ);

        prim->con_context   = (context_t) req->theAppTask;
        prim->big_handle    = req->big_handle;
        prim->sync_handle   = req->sync_handle;
        prim->encryption    = req->encryption;

        memmove(prim->broadcast_code, req->broadcast_code, BROADCAST_CODE_SIZE);

        prim->mse               = req->mse;
        prim->big_sync_timeout  = req->big_sync_timeout;
        prim->num_bis           = req->num_bis;

        ptr = PanicUnlessMalloc(data_size);
        memmove(ptr, req->bis, data_size);
        prim->bis = VmGetHandleFromPointer(ptr);

        VmSendDmPrim(prim);
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleIsocBigCreateSyncCfm

DESCRIPTION
    Confirmation of a local device's attempt to to synchronize to a BIG


RETURNS
    void
*/
void connectionHandleIsocBigCreateSyncCfm(connectionDmIsocState *state, const DM_ISOC_BIG_CREATE_SYNC_CFM_T *cfm)
{
    if (state->dmIsocOpLock)
    {

        /* Confirmation that the device has been sycned to a BIG -
         * tell the relevant task. */
        MAKE_CL_MESSAGE(CL_DM_ISOC_BIG_CREATE_SYNC_CFM);

        message->status                 = connectionConvertHciStatus(cfm->status);
        message->big_handle             = cfm->big_handle;
        message->transport_latency_big  = cfm->big_params.transport_latency_big;
        message->nse                    = cfm->big_params.nse;
        message->bn                     = cfm->big_params.bn;
        message->pto                    = cfm->big_params.pto;
        message->irc                    = cfm->big_params.irc;
        message->max_pdu                = cfm->big_params.max_pdu;
        message->iso_interval           = cfm->big_params.iso_interval;
        message->num_bis                = cfm->num_bis;
        message->bis_handle             = VmGetPointerFromHandle(cfm->bis_handles);

        MessageSend(
                state->dmIsocOpLock,
                CL_DM_ISOC_BIG_CREATE_SYNC_CFM,
                message
                );

        /* Release Isoc operation lock. */
        state->dmIsocOpLock = NULL;
    }
    else
    {
        CL_DEBUG(("DM_ISOC_BIG_CREATE_SYNC_CFM received without a request being sent.\n"));
    }
}

/****************************************************************************
NAME
    connectionHandleIsocBigTerminateSyncInd

DESCRIPTION
    Indication of either status of DM_ISOC_BIG_TERMINATE_SYNC_REQ or that the
    BIG has been terminated by the remote device or sync lost with remote device.


RETURNS
    void
*/
void connectionHandleIsocBigTerminateSyncInd(const DM_ISOC_BIG_TERMINATE_SYNC_IND_T *ind)
{
    MAKE_CL_MESSAGE(CL_DM_ISOC_BIG_TERMINATE_SYNC_IND);

    message->big_handle         = ind->big_handle;
    message->status_or_reason   = connectionConvertHciStatus(ind->status_or_reason);

    /* This message will bypass the lock since we want it to be able to interrupt
       a potentially ongoing CreateSync scenario. */
    MessageSend(
                (Task) ind->con_context,
                CL_DM_ISOC_BIG_TERMINATE_SYNC_IND,
                message
                );
}

/****************************************************************************
NAME
    connectionHandleIsocBigInfoAdvReportInd

DESCRIPTION
    Indication that a BIG Advertising report has been received.


RETURNS
    void
*/
void connectionHandleIsocBigInfoAdvReportInd(connectionDmIsocState *state, const DM_HCI_ULP_BIGINFO_ADV_REPORT_IND_T *ind)
{
    MAKE_CL_MESSAGE(CL_DM_BLE_BIGINFO_ADV_REPORT_IND);

    message->sync_handle    = ind->sync_handle;
    message->num_bis        = ind->num_bis;
    message->nse            = ind->big_params.nse;
    message->iso_interval   = ind->big_params.iso_interval;
    message->bn             = ind->big_params.bn;
    message->pto            = ind->big_params.pto;
    message->irc            = ind->big_params.irc;
    message->max_pdu        = ind->big_params.max_pdu;
    message->sdu_interval   = ind->sdu_interval;
    message->max_sdu        = ind->max_sdu;
    message->phy            = ind->big_params.phy;
    message->framing        = ind->framing;
    message->encryption     = ind->encryption;

    /* Check if there are any Isoc-related messages currently being processed. */
    if (!state->dmIsocOpLock)
    {
        /* No lock used since this is the only message in this sequence. */

        MessageSend(
                (Task) ind->reg_context,
                CL_DM_BLE_BIGINFO_ADV_REPORT_IND,
                message
                );
    }
    else /* There is already an Isoc Request being processed, queue this one. */
    {
        MessageSendConditionallyOnTask(
                    (Task) ind->reg_context,
                    CL_DM_BLE_BIGINFO_ADV_REPORT_IND,
                    message,
                    &state->dmIsocOpLock
                    );
    }
}

#endif
