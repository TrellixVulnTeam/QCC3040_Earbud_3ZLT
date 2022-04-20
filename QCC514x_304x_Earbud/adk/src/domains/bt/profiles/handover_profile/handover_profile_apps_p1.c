/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Handover Profile appsP1 interface
*/

#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"

/*! The size of the source buffer to create to contain the appsP1 marshal data */
#define HANDOVER_PROFILE_MARSHAL_PIPE_BUFFER_SIZE 895

#define FOR_EACH_HANDOVER_CLIENT(ho_client) \
        for (const handover_interface **ho_clientpp = handover_clients, *ho_client = *ho_clientpp; \
             ho_client != NULL; \
             ho_clientpp++, ho_client = *ho_clientpp)

handover_profile_status_t handoverProfile_VetoP1Clients(void)
{
    unsigned counter = 0;
    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if(ho_client->pFnVeto && ho_client->pFnVeto())
        {
            DEBUG_LOG("handoverProfile_VetoP1Clients vetoed by client %d", counter);
            return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
        }
        counter++;
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

handover_profile_status_t handoverProfile_AbortP1Clients(void)
{
    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if (ho_client->pFnAbort)
        {
            ho_client->pFnAbort();
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

typedef struct
{
    Sink sink;
    Source source;
} stream_pipe_t;

/* Create a uni-directional stream pipe for writing from source to sink. */
static inline stream_pipe_t makePipe(uint16 size)
{
    stream_pipe_t pipe_a, pipe_b;

    PanicFalse(StreamPipePair(&pipe_a.sink, &pipe_b.sink, size, size));
    pipe_a.source = StreamSourceFromSink(pipe_b.sink);
    /* Close the stream that is not used */
    SinkClose(pipe_b.sink);
    return pipe_a;
}

static inline uint8* claimAllSpaceInSinkAndMap(Sink s, uint8 **end)
{
    uint8 *start;
    uint16 slack;

    slack = SinkSlack(s);
    PanicNotZero(SinkClaim(s, slack));
    start = PanicNull(SinkMap(s));
    *end = start + slack;
    return start;
}

Source handoverProfile_MarshalP1Clients(const tp_bdaddr *bd_addr)
{
    uint8 client_id = 0;
    stream_pipe_t pipe;
    uint8 *start, *end, *write_ptr;

    pipe = makePipe(HANDOVER_PROFILE_MARSHAL_PIPE_BUFFER_SIZE);
    start = write_ptr = claimAllSpaceInSinkAndMap(pipe.sink, &end);

    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if (ho_client->pFnMarshal)
        {
            uint8 *client_start;
            uint16 client_len = 0;
            *write_ptr++ = client_id;
            client_start = write_ptr + sizeof(client_len);
            PanicFalse(end > client_start);
            PanicFalse(ho_client->pFnMarshal(bd_addr, client_start, end - client_start, &client_len));
            CONVERT_FROM_UINT16(write_ptr, client_len);
            write_ptr += sizeof(client_len);
            write_ptr += client_len;
            DEBUG_LOG("handoverProfile_MarshalP1Clients client=%d, len=%d", client_id, client_len);
        }
        client_id++;
    }
    PanicFalse(SinkFlush(pipe.sink, write_ptr - start));
    return pipe.source;
}

bool handoverProfile_UnmarshalP1Client(tp_bdaddr *addr, const uint8 *src_addr, uint16 src_len, uint16 *consumed)
{
    const uint8 *read_ptr = src_addr;
    uint8 client_id;
    uint16 client_datalen;
    unsigned client_counter = 0;
    const uint16 header_len = sizeof(uint8) + sizeof(uint16);

    PanicFalse(src_len >= header_len);

    client_id = *read_ptr++;
    client_datalen = CONVERT_TO_UINT16(read_ptr);
    read_ptr += sizeof(uint16);
    *consumed = header_len;
    PanicFalse(src_len >= (client_datalen + header_len));

    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if (client_counter == client_id)
        {
            handover_unmarshal unmarshal = handover_clients[client_id]->pFnUnmarshal;
            if (unmarshal)
            {
                if (client_datalen)
                {
                    /* Each client is expected to complete unmarshalling and consume all its data */
                    uint16 client_consumed = 0;
                    PanicFalse(unmarshal(addr, read_ptr, client_datalen, &client_consumed));
                    PanicFalse(client_consumed == client_datalen);
                }
                DEBUG_LOG("handoverProfile_UnmarshalP1Client client=%d, len=%d", client_id, client_datalen);
            }
            *consumed += client_datalen;
            /* More client data to come */
            return TRUE;
        }
        UNUSED(ho_client);
        client_counter++;
    }
    /* Did not find client */
    return FALSE;
}

void handoverProfile_CommitP1Clients(const tp_bdaddr *addr, bool is_primary)
{
    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if(ho_client->pFnCommit)
        {
            HandoverPioSet();
            ho_client->pFnCommit(addr, is_primary);
            HandoverPioClr();
        }
    }
}

void handoverProfile_CompleteP1Clients(bool is_primary)
{
    FOR_EACH_HANDOVER_CLIENT(ho_client)
    {
        if(ho_client->pFnComplete)
        {
            ho_client->pFnComplete(is_primary);
        }
    }
}

#endif
