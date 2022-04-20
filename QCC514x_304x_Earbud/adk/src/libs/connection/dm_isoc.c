/****************************************************************************
Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_isoc.c

DESCRIPTION
    This file contains the functions responsible for managing the setting up
    and tearing down of Isochronous connections.

NOTES

*/


/****************************************************************************
    Header files
*/
#include "connection.h"
#include "connection_private.h"
#include "bdaddr.h"

#ifndef CL_EXCLUDE_ISOC


/*****************************************************************************/
void ConnectionIsocRegister(Task theAppTask, uint16 isoc_type)
{
    /* Send an internal register request message */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REGISTER_REQ);
    message->theAppTask = theAppTask;
    message->isoc_type  = isoc_type;
    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_REGISTER_REQ, message);
}

/****************************************************************************
NAME
    ConnectionIsocConnectRequest

DESCRIPTION
    Establish connected isochronous streams(CIS) with remote device.

RETURNS
    void
*/
void ConnectionIsocConnectRequest(Task theAppTask, uint8 cis_count, CL_DM_CIS_CONNECTION_T  *cis_conn[])
{
    /* Send an internal Isochronous connect request */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CIS_CONNECT_REQ);
    message->theAppTask = theAppTask;
    message->cis_count = cis_count;

    memmove(message->cis_conn, &cis_conn, cis_count * sizeof(CL_DM_CIS_CONNECTION_T));

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_CIS_CONNECT_REQ, message);
}


/*****************************************************************************/
void ConnectionIsocConnectResponse(Task theAppTask, uint16 cis_handle, uint8 status)
{
    /* Send an internal Isochronous connect response */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CIS_CONNECT_RES);
    message->theAppTask = theAppTask;
    message->cis_handle = cis_handle;
    message->status     = status;

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_CIS_CONNECT_RES, message);
}


/*****************************************************************************/
void ConnectionIsocDisconnectRequest(Task theAppTask, uint16 cis_handle, hci_status reason)
{
    /* Send an internal Isochronous disconnect request */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ);
    message->theAppTask = theAppTask;
    message->cis_handle = cis_handle;
    message->reason = reason;
    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_CIS_DISCONNECT_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocSetupIsochronousDataPathRequest(Task theAppTask, uint16 cis_handle, uint8 data_path_direction, uint8 data_path_id)
{
    /* Send an internal Isochronous Set Data Path request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ);
    message->theAppTask = theAppTask;
    message->cis_handle = cis_handle;
    message->data_path_direction = data_path_direction;
    message->data_path_id = data_path_id;

    memset(message->codec_id, 0, ISOC_CODEC_ID_SIZE * sizeof(uint8));

    message->controller_delay = 0;
    message->codec_config_length = 0;
    message->codec_config_data = NULL;
    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_SETUP_ISOCHRONOUS_DATA_PATH_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocRemoveIsoDataPathRequest(Task theAppTask, uint16 handle, uint8 data_path_direction)
{
    /* Send an internal Isochronous Remove Data Path request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ);
    message->theAppTask             = theAppTask;
    message->handle                 = handle;
    message->data_path_direction    = data_path_direction;

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_REMOVE_ISO_DATA_PATH_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocConfigureCigRequest( Task theAppTask,
                                        uint32 sdu_interval_m_to_s,
                                        uint32 sdu_interval_s_to_m,
                                        uint16 max_transport_latency_m_to_s,
                                        uint16 max_transport_latency_s_to_m,
                                        uint8  cig_id,
                                        uint8  sca,
                                        uint8  packing,
                                        uint8  framing,
                                        uint8  cis_count,
                                        CL_DM_CIS_CONFIG_T  *cis_config[])
{
    /* Send an internal Isochronous Configure CIG request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ);
    message->theAppTask                     = theAppTask;
    message->sdu_interval_m_to_s            = sdu_interval_m_to_s;
    message->sdu_interval_s_to_m            = sdu_interval_s_to_m;
    message->max_transport_latency_m_to_s   = max_transport_latency_m_to_s;
    message->max_transport_latency_s_to_m   = max_transport_latency_s_to_m;
    message->cig_id                         = cig_id;
    message->sca                            = sca;
    message->packing                        = packing;
    message->framing                        = framing;
    message->cis_count                      = cis_count;

    memmove(message->cis_config, cis_config, cis_count * sizeof(CL_DM_CIS_CONFIG_T));

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_CONFIGURE_CIG_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocRemoveCigRequest(Task theAppTask, uint8 cig_id)
{
    /* Send an internal Isochronous Remove CIG request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_REMOVE_CIG_REQ);
    message->theAppTask = theAppTask;
    message->cig_id     = cig_id;

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_REMOVE_CIG_REQ, message);
}


/*****************************************************************************/
void ConnectionIsocCreateBigRequest(Task theAppTask,
                                    CL_DM_BIG_CONFIG_PARAM_T big_config,
                                    uint8 big_handle,
                                    uint8 adv_handle,
                                    uint8 num_bis,
                                    uint8 encryption,
                                    uint8 broadcast_code[BROADCAST_CODE_SIZE])
{
    /* Send an internal Isochronous Create BIG request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_CREATE_BIG_REQ);

    message->theAppTask = theAppTask;

    message->big_config.sdu_interval            = big_config.sdu_interval;
    message->big_config.max_sdu                 = big_config.max_sdu;
    message->big_config.max_transport_latency   = big_config.max_transport_latency;
    message->big_config.rtn                     = big_config.rtn;
    message->big_config.phy                     = big_config.phy;
    message->big_config.packing                 = big_config.packing;
    message->big_config.framing                 = big_config.framing;

    message->big_handle = big_handle;
    message->adv_handle = adv_handle;
    message->num_bis    = num_bis;
    message->encryption = encryption;

    memmove((message->broadcast_code), broadcast_code, BROADCAST_CODE_SIZE * sizeof(uint8));


    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_CREATE_BIG_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocTerminateBigRequest( Task theAppTask,
                                        uint8 big_handle,
                                        uint8 reason)
{
    /* Send an internal Isochronous Terminate BIG request. */
    MAKE_CL_MESSAGE(CL_INTERNAL_ISOC_TERMINATE_BIG_REQ);

    message->theAppTask = theAppTask;
    message->big_handle = big_handle;
    message->reason     = reason;

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_TERMINATE_BIG_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocBigCreateSyncRequest(Task theAppTask,
                                        uint16 sync_handle,
                                        uint16 big_sync_timeout,
                                        uint8 big_handle,
                                        uint8 mse,
                                        uint8 encryption,
                                        uint8 broadcast_code[BROADCAST_CODE_SIZE],
                                        uint8 num_bis,
                                        uint8 *bis)
{
    /* Send an internal Isochronous BIG Create Sync request. */
    MAKE_CL_MESSAGE_WITH_LEN(CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ, num_bis - 1);

    message->theAppTask = theAppTask;
    message->big_handle = big_handle;
    message->sync_handle = sync_handle;
    message->encryption = encryption;

    memmove((message->broadcast_code), broadcast_code, BROADCAST_CODE_SIZE * sizeof(uint8));

    message->mse = mse;
    message->big_sync_timeout = big_sync_timeout;
    message->num_bis = num_bis;

    memmove(message->bis, bis, num_bis * sizeof(uint8));

    MessageSend(connectionGetCmTask(), CL_INTERNAL_ISOC_BIG_CREATE_SYNC_REQ, message);
}

/*****************************************************************************/
void ConnectionIsocBigTerminateSyncRequest(int8 big_handle)
{
    MAKE_PRIM_T(DM_ISOC_BIG_TERMINATE_SYNC_REQ);

    prim->big_handle = big_handle;

    /* This message will bypass the lock since we want it to be able to interrupt
       a potentially ongoing CreateSync scenario. */
    VmSendDmPrim(prim);
}

#endif
