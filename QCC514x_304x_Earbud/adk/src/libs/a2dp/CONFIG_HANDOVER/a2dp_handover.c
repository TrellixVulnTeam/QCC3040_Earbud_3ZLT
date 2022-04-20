/****************************************************************************
Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    a2dp_handover.c

DESCRIPTION
    TWS Handover and Marshaling interface for A2DP

NOTES
    See handover_if.h for further interface description

    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER
*/


/****************************************************************************
    Header files
*/
#include "a2dp_marshal_desc.h"
#include "a2dp_private.h"
#include "a2dp_init.h"
#include "a2dp_handover_policy.h"

#include "marshal.h"

#include <panic.h>
#include <sink.h>
#include <stream.h>
#include <source.h>
#include <stdlib.h>
#include <bdaddr.h>

/* This byte is set when unmarshalling is complete and cleared when committing.
   It is used in the abort to determine if the secondary need to clean up
   its state if the abort occurs between unmarshal and commit */
static uint8 unmarshalled = 0;

static bool a2dpVeto(void);

static bool a2dpMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool a2dpUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void a2dpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole);

static void a2dpHandoverAbort( void );

static void a2dpHandoverComplete( const bool newRole );

extern const handover_interface a2dp_handover_if =  {
        &a2dpVeto,
        &a2dpMarshal,
        &a2dpUnmarshal,
        &a2dpHandoverCommit,
        &a2dpHandoverComplete,
        &a2dpHandoverAbort};

/****************************************************************************
NAME
    a2dpVeto

DESCRIPTION
    Veto check for A2DP library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the A2DP Library wishes to veto the handover attempt.
*/
bool a2dpVeto( void )
{
    unsigned  device_id = A2DP_MAX_REMOTE_DEVICES;


    /* Check the a2dp library is initialised */
    if ( !a2dp )
    {
        return TRUE;
    }

    /* Check message queue status */
    if(MessagesPendingForTask(&a2dp->task, NULL) != 0)
    {
        return TRUE;
    }

    while (device_id-- > A2DP_MAX_REMOTE_DEVICES)
    {
        avdtp_connection_state  signal_state, media_state;
        avdtp_stream_state strm_state;
        uint8 stream_id = 0;

        if (BdaddrIsZero(&a2dp->remote_conn[device_id].bd_addr))
        {
            continue;
        }

        /* Check signalling channel status */
        signal_state = a2dp->remote_conn[device_id].signal_conn.status.connection_state;
        if (signal_state != avdtp_connection_idle && signal_state != avdtp_connection_connected)
        {
            return TRUE;
        }

        /* check media channel status */
        for (stream_id=0; stream_id<A2DP_MAX_MEDIA_CHANNELS; stream_id++)
        {
            media_state = a2dp->remote_conn[device_id].media_conn[stream_id].status.conn_info.connection_state;
            if (media_state != avdtp_connection_idle && media_state != avdtp_connection_connected)
            {
                return TRUE;
            }
        }

        /* check stream status */
        strm_state = a2dp->remote_conn[device_id].signal_conn.status.stream_state;
        if (strm_state != avdtp_stream_idle &&
            strm_state != avdtp_stream_configured &&
            strm_state != avdtp_stream_open &&
            strm_state != avdtp_stream_streaming )
        {
            return TRUE;
        }

        /* check for pending transactions */
        if( a2dp->remote_conn[device_id].signal_conn.status.pending_issued_transaction ||
            a2dp->remote_conn[device_id].signal_conn.status.pending_received_transaction )
        {
            return TRUE;
        }
    }

    /* No Veto, good to go */
    return FALSE;
}

/****************************************************************************
NAME
    stitchRemoteDevice

DESCRIPTION
    Stitch connection information to A2DP instance

RETURNS
    void
*/
static void stitchRemoteDevice(remote_device *remote_conn)
{
    uint8 mediaChannelIndex;
    Sink *sink_ptr;

    sink_ptr = &remote_conn->signal_conn.connection.active.sink;

    /* Initialize the connection context for the relevant connection id on signalling channel */
    configureL2capSinkFromMarshalledSinkCid(sink_ptr, &a2dp->task);

    /* Initialize the connection context for the relevant connection id on the media channels */
    for (mediaChannelIndex=0; mediaChannelIndex < A2DP_MAX_MEDIA_CHANNELS; mediaChannelIndex++)
    {
        media_channel *media = &remote_conn->media_conn[mediaChannelIndex];

        if ( ((media->status.conn_info.connection_state == avdtp_connection_connected) ||
              (media->status.conn_info.connection_state == avdtp_connection_disconnecting) ||
              (media->status.conn_info.connection_state == avdtp_connection_disconnect_pending)) &&
              (media->connection.active.sink) )
        {
            sink_ptr = &media->connection.active.sink;
            configureL2capSinkFromMarshalledSinkCid(sink_ptr, &a2dp->task);
        }
    }
}

/****************************************************************************
NAME
    stitchDataBlockHeader

DESCRIPTION
    Stitch data block headers

RETURNS
    void
*/
static void stitchDataBlockHeader(data_block_header *data_blocks, unsigned device_id)
{
    data_block_header *dbh = a2dp->data_blocks[device_id];
    if (dbh)
    {
        free(dbh);
    }
    a2dp->data_blocks[device_id] = data_blocks;
}

/****************************************************************************
NAME
    a2dpMarshal

DESCRIPTION
    Marshal the data associated with A2DP connections

RETURNS
    bool TRUE if A2DP module marshalling complete, otherwise FALSE
*/
static bool a2dpMarshal(const tp_bdaddr *tp_bd_addr,
                        uint8 *buf,
                        uint16 length,
                        uint16 *written)
{
    uint8 device_id;
    if (A2dpDeviceFromBdaddr(&tp_bd_addr->taddr.addr, &device_id))
    {
        bool marshalled;
        marshaller_t marshaller = MarshalInit(mtd_a2dp, A2DP_MARSHAL_OBJ_TYPE_COUNT);
        a2dp_marshal_data data = {
            .data_blocks = a2dp->data_blocks[device_id],
            .remote_conn = &a2dp->remote_conn[device_id],
        };

        MarshalSetBuffer(marshaller, (void *) buf, length);

        marshalled = Marshal(marshaller, &data, MARSHAL_TYPE(a2dp_marshal_data));

        *written = marshalled ? MarshalProduced(marshaller) : 0;

        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }
    else
    {
        /* Device not found, nothing to marshal */
        *written = 0;
        return TRUE;
    }
}

/****************************************************************************
NAME
    a2dpUnmarshal

DESCRIPTION
    Unmarshal the data associated with the A2DP connections

RETURNS
    bool TRUE if A2DP unmarshalling complete, otherwise FALSE
*/
bool a2dpUnmarshal(const tp_bdaddr *tp_bd_addr,
                          const uint8 *buf,
                          uint16 length,
                          uint16 *consumed)
{
    marshal_type_t unmarshalled_type;
    a2dp_marshal_data *data = NULL;
    unmarshaller_t unmarshaller = UnmarshalInit(mtd_a2dp, A2DP_MARSHAL_OBJ_TYPE_COUNT);
    UnmarshalSetBuffer(unmarshaller, (void *) buf, length);

    if (Unmarshal(unmarshaller, (void**)&data, &unmarshalled_type))
    {
        remote_device *new_conn;

        PanicFalse(unmarshalled_type == MARSHAL_TYPE(a2dp_marshal_data));
        *consumed = UnmarshalConsumed(unmarshaller);

        PanicNull(data);
        new_conn = a2dpAddDevice(&tp_bd_addr->taddr.addr);
        PanicNull(new_conn);
        data->remote_conn->bitfields.device_id = new_conn->bitfields.device_id;
        *new_conn = *data->remote_conn;
        new_conn->bd_addr = tp_bd_addr->taddr.addr;

        stitchDataBlockHeader(data->data_blocks, new_conn->bitfields.device_id);
        UnmarshalDestroy(unmarshaller, FALSE);

        free(data->remote_conn);
        data->remote_conn = NULL;
        free(data);
        data = NULL;
        unmarshalled = 1;
        return TRUE;
    }
    else
    {
        *consumed = 0;
        UnmarshalDestroy(unmarshaller, TRUE);
        return FALSE;
    }
}

/****************************************************************************
NAME
    a2dpHandoverCommit

DESCRIPTION
    The A2DP library performs time-critical actions to commit to the specified
    new role (primary or secondary)

RETURNS
    void
*/
static void a2dpHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole)
{
    /* Stitch the unmarshalled state on committing to primary role */
    if (newRole)
    {
        unsigned device_id;

        for (device_id = 0; device_id < A2DP_MAX_REMOTE_DEVICES; device_id++)
        {
            Source src;
            uint8 mediaChannelIndex = 0;
            remote_device *remote_conn = &a2dp->remote_conn[device_id];
            if (BdaddrIsSame(&remote_conn->bd_addr, &tp_bd_addr->taddr.addr))
            {
                /* Stitch connection information to A2DP instance */
                stitchRemoteDevice(remote_conn);

                if (!remote_conn->signal_conn.connection.active.sink)
                {
                    continue;
                }
                src = StreamSourceFromSink(remote_conn->signal_conn.connection.active.sink);
                if (!src)
                {
                    continue;
                }
                /* Set the handover policy on the signalling channel */
                a2dpSourceConfigureHandoverPolicy(src, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);

                /* Set the handover policy on the media channel */
                for (mediaChannelIndex=0; mediaChannelIndex < A2DP_MAX_MEDIA_CHANNELS; mediaChannelIndex++)
                {
                    media_channel *media = &remote_conn->media_conn[mediaChannelIndex];

                    if ( ((media->status.conn_info.connection_state == avdtp_connection_connected) ||
                        (media->status.conn_info.connection_state == avdtp_connection_disconnecting) ||
                        (media->status.conn_info.connection_state == avdtp_connection_disconnect_pending)) &&
                        (media->connection.active.sink) )
                    {
                        src = StreamSourceFromSink(media->connection.active.sink);
                        a2dpSourceConfigureHandoverPolicy(src, SOURCE_HANDOVER_ALLOW);

                        /* If handover is performed when media is streaming, the
                        source will already exist and will be connected via a
                        transform (typically to audio subsystem). If handover is
                        performed when media is not streaming the source will be
                        created during the handover. In this latter case, the
                        source should be connected to a dispose transform in the
                        same way as the AVDTP L2CAP source is disposed when
                        initially connected. */
                        if (!TransformFromSource(src))
                        {
                            StreamConnectDispose(src);
                        }
                    }
                }
            }
        }
    }
    unmarshalled = 0;
}

/****************************************************************************
NAME
    a2dpHandoverComplete

RETURNS
    void
*/
static void a2dpHandoverComplete( const bool newRole)
{
    UNUSED(newRole);
}

/****************************************************************************
NAME
    a2dpHandoverAbort

DESCRIPTION
    Abort the A2DP Handover process.

RETURNS
    void
*/
static void a2dpHandoverAbort(void)
{
    if (unmarshalled)
    {
        /* Unmarshalled but not committed, clear the remote conn instances */
        unsigned device_id;

        for (device_id = 0; device_id < A2DP_MAX_REMOTE_DEVICES; device_id++)
        {
            remote_device *remote_conn = &a2dp->remote_conn[device_id];
            a2dpInitialiseRemoteDevice(remote_conn, (uint8)remote_conn->bitfields.device_id);
        }
        unmarshalled = 0;
    }
}
