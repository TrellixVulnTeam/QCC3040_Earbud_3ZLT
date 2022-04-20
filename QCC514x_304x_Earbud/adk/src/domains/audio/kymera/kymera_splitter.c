/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage creation of splitter chains with multiple streams
*/

#include "kymera_splitter.h"
#include "kymera_config.h"
#include <custom_operator.h>
#include <logging.h>
#include <panic.h>
#include <stream.h>
#include <stdlib.h>

/*!
    Changing the value of this define will break the code
    The implementation is heavily based on this value being true
*/
#define NUM_OF_STREAMS_PER_SPLITTER (2)
#define MAX_NUM_OF_SPCS (MAX_NUM_OF_CONCURRENT_MICS)  // max consumers per splitter output

/*! The following capabilities are used for the mic sharing graph */
#define SWITCHED_PASSTHROUGH_CONSUMER_CAP_ID capability_id_switched_passthrough_consumer
#define PASSTHROUGH_CAP_ID                   capability_id_passthrough
#define SPLITTER_CAP_ID                      capability_id_splitter

typedef struct
{
    Operator op;
    unsigned started:1;
} spc_operator_t;

typedef struct
{
    Operator op;
    splitter_output_stream_set_t active_streams;
    unsigned started:1;
    spc_operator_t spc[NUM_OF_STREAMS_PER_SPLITTER][MAX_NUM_OF_SPCS];
    uint8 num_of_spcs[NUM_OF_STREAMS_PER_SPLITTER];
} splitter_operator_t;

struct splitter_tag
{
    const splitter_config_t *config;
    uint8 num_of_inputs;
    uint8 num_of_splitters;
    splitter_operator_t splitters[];
};

#ifdef HOSTED_TEST_ENVIRONMENT
Sink connected_sinks[MAX_NUM_OF_CONCURRENT_MIC_USERS][MAX_NUM_OF_CONCURRENT_MICS+1]; // mics + aec_ref
#endif      //HOSTED_TEST_ENVIRONMENT

typedef void (*OperatorFunction)(Operator *ops, uint8 num_of_ops);

static void kymera_SetRunningStreams(splitter_operator_t *splitter, splitter_output_stream_set_t running_streams)
{
    if (running_streams != splitter->active_streams)
    {
        OperatorsSplitterSetRunningStreams(splitter->op, running_streams);
    }
    splitter->active_streams = running_streams;
}

static void kymera_ActivateStream(splitter_operator_t *splitter, splitter_output_stream_set_t stream)
{
    kymera_SetRunningStreams(splitter, splitter->active_streams | stream);
    DEBUG_LOG("kymera_ActivateStream: active_streams = 0x%x", splitter->active_streams);
}

static void kymera_DeactivateStream(splitter_operator_t *splitter, splitter_output_stream_set_t stream)
{
    kymera_SetRunningStreams(splitter, splitter->active_streams & ~stream);
    DEBUG_LOG("kymera_DeactivateStream: active_streams = 0x%x", splitter->active_streams);
}

static bool kymera_IsLastClientStream(splitter_handle_t handle, uint8 stream_index)
{
    PanicFalse(stream_index <= handle->num_of_splitters);
    return (stream_index == handle->num_of_splitters);
}

static splitter_output_stream_set_t kymera_GetSplitterClientStream(splitter_handle_t handle, uint8 stream_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ? splitter_output_stream_1 : splitter_output_stream_0;
}

static uint8 kymera_GetSplitterOutputIndex(splitter_handle_t handle, uint8 stream_index)
{
    splitter_output_stream_set_t stream = kymera_GetSplitterClientStream(handle, stream_index);
    return (stream == splitter_output_stream_1) ? 1 : 0;
}

static void kymera_StartSpc(spc_operator_t *spc)
{
    if (spc->started == FALSE)
    {
        DEBUG_LOG("kymera_StartSpc: %p", spc);
        OperatorStart(spc->op);
        spc->started = TRUE;
    }
}

static void kymera_StopSpc(spc_operator_t *spc)
{
    if (spc->started)
    {
        OperatorStop(spc->op);
        spc->started = FALSE;
    }
}

static void kymera_DestroySpc(spc_operator_t *spc)
{
    if (spc->op != INVALID_OPERATOR)
    {
        DEBUG_LOG("kymera_DestroySpc: %p", spc);
        kymera_StopSpc(spc);
        CustomOperatorDestroy(&spc->op, 1);
        memset(spc, 0, sizeof(*spc));
    }
}

static void kymera_CreateSpc(splitter_handle_t handle, uint8 splitter_index, uint8 stream_index, uint8 spc_index)
{
    if (spc_index >= MAX_NUM_OF_SPCS)
    {
        DEBUG_LOG_ERROR("kymera_CreateSpc: Max number of SPCs (%d) already spent for handle %p splitter_index %d stream_index %d",
                        MAX_NUM_OF_SPCS, handle, splitter_index, stream_index);
        Panic();
    }
    uint8 out_idx = kymera_GetSplitterOutputIndex(handle, stream_index);
    handle->splitters[splitter_index].spc[out_idx][spc_index].op = CustomOperatorCreate(SWITCHED_PASSTHROUGH_CONSUMER_CAP_ID,
                                                                               OPERATOR_PROCESSOR_ID_0,
                                                                               operator_priority_lowest,
                                                                               NULL);
    DEBUG_LOG("kymera_CreateSpc: SPC[%d] created for handle %p splitter_index %d stream_index %d out_idx %d",
              spc_index, handle, splitter_index, stream_index, out_idx);
    OperatorsSetSwitchedPassthruEncoding(handle->splitters[splitter_index].spc[out_idx][spc_index].op, spc_op_format_pcm);
}

static void kymera_StartSplitter(splitter_operator_t *splitter)
{
    if (splitter->started == FALSE)
    {
        OperatorStart(splitter->op);
        splitter->started = TRUE;
    }
}

static void kymera_StopSplitter(splitter_operator_t *splitter)
{
    if (splitter->started)
    {
        OperatorStop(splitter->op);
        splitter->started = FALSE;
    }
}

static void kymera_CreateSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    operator_data_format_t format = operator_data_format_pcm;
    PanicFalse(handle->splitters[splitter_index].op == INVALID_OPERATOR);
    handle->splitters[splitter_index].op = CustomOperatorCreate(SPLITTER_CAP_ID, OPERATOR_PROCESSOR_ID_0,
                                                                operator_priority_lowest, NULL);

    if (handle->config)
    {
        format = handle->config->data_format;
        if (handle->config->transform_size_in_words)
        {
            OperatorsStandardSetBufferSize(handle->splitters[splitter_index].op, handle->config->transform_size_in_words);
        }
    }
    OperatorsSplitterSetDataFormat(handle->splitters[splitter_index].op, format);
}

static void kymera_DestroySplitter(splitter_operator_t *splitter)
{
    if (splitter->op != INVALID_OPERATOR)
    {
        kymera_StopSplitter(splitter);
        CustomOperatorDestroy(&splitter->op, 1);
        memset(splitter, 0, sizeof(*splitter));
    }
}

static bool kymera_IsLastSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    PanicFalse(splitter_index < handle->num_of_splitters);
    return (splitter_index == (handle->num_of_splitters - 1));
}

static uint8 kymera_GetSplitterIndex(splitter_handle_t handle, uint8 stream_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ? handle->num_of_splitters - 1 : stream_index;
}

static splitter_output_stream_set_t kymera_GetSplitterInterconnectStream(splitter_handle_t handle, uint8 splitter_index)
{
    return (kymera_IsLastSplitter(handle, splitter_index)) ? splitter_output_stream_none : splitter_output_stream_1;
}

static bool kymera_IsSplitterConnectedToClient(splitter_handle_t handle, uint8 splitter_index)
{
    return (((handle->splitters[splitter_index].active_streams != splitter_output_stream_none) &&
            (kymera_IsLastSplitter(handle, splitter_index)) ? splitter_output_streams_all : splitter_output_stream_0)
            > 0);
}

static bool kymera_IsSplitterOutputConnected(splitter_handle_t handle, uint8 splitter_index)
{
    DEBUG_LOG("kymera_IsSplitterOutputConnected: splitter_index %d active_streams 0x%x",
              splitter_index, handle->splitters[splitter_index].active_streams);
    return handle->splitters[splitter_index].active_streams != splitter_output_stream_none;
}

static uint8 kymera_GetSplitterOutputTerminalForClient(splitter_handle_t handle, uint8 stream_index, uint8 input_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ?
                (NUM_OF_STREAMS_PER_SPLITTER * input_index + 1) :
                (NUM_OF_STREAMS_PER_SPLITTER * input_index);
}

static uint8 kymera_GetSplitterInterconnectOutputTerminal(splitter_handle_t handle, uint8 splitter_index, uint8 input_index)
{
    PanicFalse(kymera_IsLastSplitter(handle, splitter_index) == FALSE);
    return NUM_OF_STREAMS_PER_SPLITTER * input_index + 1;
}

static uint8 kymera_GetNumOfSplittersRequired(uint8 num_of_streams)
{
    PanicZero(num_of_streams);
    return (num_of_streams <= NUM_OF_STREAMS_PER_SPLITTER) ? 1 : (num_of_streams - 1);
}

static void kymera_InterconnectSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    uint8 i, j, terminal;
    Sink sink;
    Source source;

    for(i = 0; i < splitter_index; i++)
    {
        if (handle->splitters[i + 1].op == INVALID_OPERATOR)
        {
            kymera_CreateSplitter(handle, i + 1);

            for(j = 0; j < handle->num_of_inputs; j++)
            {
                terminal = kymera_GetSplitterInterconnectOutputTerminal(handle, i, j);
                source = StreamSourceFromOperatorTerminal(handle->splitters[i].op, terminal);
                sink = StreamSinkFromOperatorTerminal(handle->splitters[i + 1].op, j);
                DEBUG_LOG("kymera_InterconnectSplitter: out terminal %d source 0x%x sink 0x%x", terminal, source, sink);
                PanicNull(StreamConnect(source, sink));
            }
            kymera_ActivateStream(&handle->splitters[i], kymera_GetSplitterInterconnectStream(handle, i));
        }
    }
}

static void kymera_DestroyUnconnectedSplitters(splitter_handle_t handle)
{
    // Never destroy the first splitter
    for(uint8 splitter_index = handle->num_of_splitters - 1; splitter_index >= 1; splitter_index--)
    {
        if (kymera_IsSplitterOutputConnected(handle, splitter_index) == FALSE)
        {
            if (handle->splitters[splitter_index].op != INVALID_OPERATOR)
            {
                uint8 upstream_splitter_index = splitter_index - 1;
                splitter_output_stream_set_t stream = kymera_GetSplitterInterconnectStream(handle, upstream_splitter_index);
                DEBUG_LOG("kymera_DestroyUnconnectedSplitters: DeactivateStream for splitter_index %d: stream %d",
                          upstream_splitter_index, stream);
                kymera_DeactivateStream(&handle->splitters[upstream_splitter_index], stream);
                /* Disconnect interconnected splitters */
                for(uint8 i = 0; i < handle->num_of_inputs; i++)
                {
                    uint8 terminal = kymera_GetSplitterInterconnectOutputTerminal(handle, upstream_splitter_index, i);
                    Source source = StreamSourceFromOperatorTerminal(handle->splitters[upstream_splitter_index].op, terminal);
                    DEBUG_LOG("kymera_DestroyUnconnectedSplitters: Disconnect splitter_index %d out terminal %d source 0x%x",
                              upstream_splitter_index, terminal, source);
                    StreamDisconnect(source, NULL);
                }
                if (kymera_IsSplitterOutputConnected(handle, upstream_splitter_index) == FALSE)
                {
                    kymera_StopSplitter(&handle->splitters[upstream_splitter_index]);
                }
                kymera_DestroySplitter(&handle->splitters[splitter_index]);
            }
        }
    }
}

static void kymera_ConnectClientToStream(splitter_handle_t handle, uint8 stream_index, const Sink *input)
{
    uint8 i, terminal, splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    Source source;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        if(input[i])
        {
            terminal = kymera_GetSplitterOutputTerminalForClient(handle, stream_index, i);
            source = StreamSourceFromOperatorTerminal(handle->splitters[splitter_index].op, terminal);
            DEBUG_LOG("kymera_ConnectClientToStream: out terminal %d source 0x%x sink 0x%x", terminal, source, input[i]);
            PanicNull(StreamConnect(source, input[i]));
        }
    }
}

static void kymera_SplitterSetRunningStreams(splitter_handle_t handle, uint8 stream_index)
{
    uint8 splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    DEBUG_LOG("kymera_SplitterSetRunningStreams for splitter_index %d:", splitter_index);
    kymera_ActivateStream(&handle->splitters[splitter_index], kymera_GetSplitterClientStream(handle, stream_index));
}

static void kymera_StopAndDestroySpc(splitter_handle_t handle, uint8 splitter_index, uint8 stream_index)
{
    uint8 i;
    uint8 out_idx = kymera_GetSplitterOutputIndex(handle, stream_index);

    for(i = 0; i < handle->splitters[splitter_index].num_of_spcs[out_idx]; i++)
    {
        kymera_DestroySpc(&handle->splitters[splitter_index].spc[out_idx][i]);
    }
    handle->splitters[splitter_index].num_of_spcs[out_idx] = 0;
}

static void kymera_DisconnectClientFromStream(splitter_handle_t handle, uint8 stream_index)
{
    uint8 i, terminal, splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    Source source;

    if (kymera_IsSplitterConnectedToClient(handle, splitter_index))
    {
        DEBUG_LOG("kymera_DisconnectClientFromStream: DeactivateStream for splitter_index %d:", splitter_index);
        kymera_DeactivateStream(&handle->splitters[splitter_index], kymera_GetSplitterClientStream(handle, stream_index));
        if (kymera_IsSplitterOutputConnected(handle, splitter_index) == FALSE)
        {
            kymera_StopSplitter(&handle->splitters[splitter_index]);
        }
        for(i = 0; i < handle->num_of_inputs; i++)
        {
            terminal = kymera_GetSplitterOutputTerminalForClient(handle, stream_index, i);
            source = StreamSourceFromOperatorTerminal(handle->splitters[splitter_index].op, terminal);
            DEBUG_LOG("kymera_DisconnectClientFromStream: Disconnect splitter_index %d terminal %d source 0x%x",
                      splitter_index, terminal, source);
            StreamDisconnect(source, NULL);
        }
        kymera_StopAndDestroySpc(handle, splitter_index, stream_index);
    }
}

static void kymera_CreateAndConnectSpcToSplitter(splitter_handle_t handle, uint8 stream_index, const Sink *input)
{
    uint8 i, terminal, splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    uint8 spc_index;
    Source source;
    Sink sink;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        if(input[i] == NULL)
        {
            /* For each mic input without sink add a consumer */
            uint8 out_idx = kymera_GetSplitterOutputIndex(handle, stream_index);
            spc_index = handle->splitters[splitter_index].num_of_spcs[out_idx];
            kymera_CreateSpc(handle, splitter_index, stream_index, spc_index);

            terminal = kymera_GetSplitterOutputTerminalForClient(handle, stream_index, i);
            source = StreamSourceFromOperatorTerminal(handle->splitters[splitter_index].op, terminal);
            sink = StreamSinkFromOperatorTerminal(handle->splitters[splitter_index].spc[out_idx][spc_index].op, 0);
            DEBUG_LOG("kymera_CreateAndConnectSpcToSplitter: handle %p index %d index %d terminal %d source 0x%x sink 0x%x",
                      handle, stream_index, splitter_index, terminal, source, sink);
            PanicNull(StreamConnect(source, sink));
            handle->splitters[splitter_index].num_of_spcs[out_idx]++;
        }
    }
}

static void kymera_DisconnectChainInput(splitter_handle_t handle)
{
    uint8 i;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        StreamDisconnect(StreamSourceFromOperatorTerminal(handle->splitters[0].op, i), NULL);
    }
}

static void kymera_DestroyChain(splitter_handle_t handle)
{
   uint8 i, stream_index;

   kymera_DisconnectChainInput(handle);
   for(i = 1; i <= handle->num_of_splitters; i++)
   {
       for(stream_index = 0; stream_index < NUM_OF_STREAMS_PER_SPLITTER; stream_index++)
       {
           kymera_StopAndDestroySpc(handle, handle->num_of_splitters - i, stream_index);
       }
       kymera_DestroySplitter(&handle->splitters[handle->num_of_splitters - i]);
   }
}

static splitter_handle_t kymera_CreateHandle(uint8 num_of_streams, uint8 num_of_inputs, const splitter_config_t *config)
{
    splitter_handle_t handle;
    uint8 num_of_splitters = kymera_GetNumOfSplittersRequired(num_of_streams);
    size_t handle_size = sizeof(*handle) + num_of_splitters * sizeof(handle->splitters[0]);
    DEBUG_LOG("kymera_CreateHandle for num_of_splitters %d with size %d", num_of_splitters, handle_size);

    handle = PanicUnlessMalloc(handle_size);
    memset(handle, 0, handle_size);
    handle->num_of_splitters = num_of_splitters;
    PanicZero(num_of_inputs);
    handle->num_of_inputs = num_of_inputs;
    handle->config = config;

    return handle;
}

static void kymera_RunOnChainOperators(splitter_handle_t handle, OperatorFunction function)
{
    uint8 num_of_ops = 0;
    uint8 num_of_splitters = handle->num_of_splitters;
    uint8 max_num_of_ops = (num_of_splitters + 1) * NUM_OF_STREAMS_PER_SPLITTER * MAX_NUM_OF_SPCS;
    splitter_operator_t *splitters = handle->splitters;
    Operator *ops = PanicUnlessMalloc(max_num_of_ops * sizeof(*ops));

    for(uint8 i = 0; i < num_of_splitters; i++)
    {
        if (splitters[i].op != INVALID_OPERATOR)
        {
            ops[num_of_ops] = splitters[i].op;
            num_of_ops++;
            for (uint8 n = 0; n < NUM_OF_STREAMS_PER_SPLITTER; n++)
            {
                for (uint8 m = 0; m < MAX_NUM_OF_SPCS; m++)
                {
                    if (splitters[i].spc[n][m].op != INVALID_OPERATOR)
                    {
                        ops[num_of_ops] = splitters[i].spc[n][m].op;
                        num_of_ops++;
                    }
                }
            }
        }
    }

    if (num_of_ops)
    {
        function(ops, num_of_ops);
    }

    free(ops);
}

static void kymera_PreserveOperators(Operator *ops, uint8 num_of_ops)
{
    OperatorFrameworkPreserve(num_of_ops, ops, 0, NULL, 0, NULL);
}

static void kymera_ReleaseOperators(Operator *ops, uint8 num_of_ops)
{
    OperatorFrameworkRelease(num_of_ops, ops, 0, NULL, 0, NULL);
}

splitter_handle_t Kymera_SplitterCreate(uint8 num_of_streams, uint8 num_of_inputs, const splitter_config_t *config)
{
    splitter_handle_t handle = kymera_CreateHandle(num_of_streams, num_of_inputs, config);

    DEBUG_LOG("Kymera_SplitterCreate: %p num_of_inputs %d", handle, num_of_inputs);
    OperatorsFrameworkEnable();
    kymera_CreateSplitter(handle, 0);
#ifdef HOSTED_TEST_ENVIRONMENT
    for(unsigned j = 0; j < MAX_NUM_OF_CONCURRENT_MIC_USERS ; j++)
    for(unsigned i = 0; i < (MAX_NUM_OF_CONCURRENT_MICS+1) ; i++)
    {
        connected_sinks[j][i] = NULL;
    }
#endif
    return handle;
}

void Kymera_SplitterDestroy(splitter_handle_t *handle)
{
    DEBUG_LOG("Kymera_SplitterDestroy: %p", *handle);
    kymera_DestroyChain(*handle);
    OperatorsFrameworkDisable();
    free(*handle);
    *handle = NULL;
}

Sink Kymera_SplitterGetInput(splitter_handle_t *handle, uint8 input_index)
{
    PanicFalse(input_index < (*handle)->num_of_inputs);
    return StreamSinkFromOperatorTerminal((*handle)->splitters[0].op, input_index);
}

void Kymera_SplitterConnectToOutputStream(splitter_handle_t *handle, uint8 stream_index, const Sink *input)
{
    uint8 splitter_index = kymera_GetSplitterIndex(*handle, stream_index);

    DEBUG_LOG("Kymera_SplitterConnectToOutputStream: handle %p, stream_index %d Sink 0x%x", *handle, stream_index, *input);
    kymera_InterconnectSplitter(*handle, splitter_index);
    kymera_ConnectClientToStream(*handle, stream_index, input);
    kymera_CreateAndConnectSpcToSplitter(*handle, stream_index, input);
    kymera_SplitterSetRunningStreams(*handle, stream_index);
#ifdef HOSTED_TEST_ENVIRONMENT
    kymera_SplitterCollectSinks(*handle, stream_index, input);
#endif      //HOSTED_TEST_ENVIRONMENT
}

void Kymera_SplitterDisconnectFromOutputStream(splitter_handle_t *handle, uint8 stream_index)
{
#ifdef HOSTED_TEST_ENVIRONMENT
    Sink unconnect[MAX_NUM_OF_CONCURRENT_MICS+1] = {NULL};  // mics + aec_ref
    kymera_SplitterCollectSinks(*handle, stream_index, unconnect);
#endif      //HOSTED_TEST_ENVIRONMENT
    DEBUG_LOG("Kymera_SplitterDisconnectFromOutputStream: handle %p, stream index %d", *handle, stream_index);
    kymera_DisconnectClientFromStream(*handle, stream_index);
    kymera_DestroyUnconnectedSplitters(*handle);
}

void Kymera_SplitterStartOutputStream(splitter_handle_t *handle, uint8 stream_index)
{
    uint8 i, j, k;
    uint8 splitter_index = kymera_GetSplitterIndex(*handle, stream_index);

    DEBUG_LOG("Kymera_SplitterStartOutputStream: handle %p, stream index %d splitter_index %d",
              *handle, stream_index, splitter_index);

    for(i = 0; i <= splitter_index; i++)
    {
        kymera_StartSplitter(&(*handle)->splitters[splitter_index - i]);
        for(k = 0; k < NUM_OF_STREAMS_PER_SPLITTER; k++)
        {
            for(j = 0; j < (*handle)->splitters[splitter_index - i].num_of_spcs[k]; j++)
            {
                kymera_StartSpc(&(*handle)->splitters[splitter_index - i].spc[k][j]);
            }
        }
    }
}

void Kymera_SplitterSleep(splitter_handle_t *handle)
{
    kymera_RunOnChainOperators(*handle, kymera_PreserveOperators);
    OperatorsFrameworkDisable();
}

void Kymera_SplitterWake(splitter_handle_t *handle)
{
    OperatorsFrameworkEnable();
    kymera_RunOnChainOperators(*handle, kymera_ReleaseOperators);
}

#ifdef HOSTED_TEST_ENVIRONMENT
void kymera_SplitterCollectSinks(splitter_handle_t handle, uint8 stream_index, const Sink *input)
{
    if (handle)
    {
        for(uint8 i = 0; i < handle->num_of_inputs; i++)
        {
            connected_sinks[stream_index][i] = input[i];
        }

        /* Print all connected sinks */
        for(uint8 j = 0; j < MAX_NUM_OF_CONCURRENT_MIC_USERS; j++)
        {
            for(uint8 i = 0; i < handle->num_of_inputs; i++)
            {
                Sink connected_sink = connected_sinks[j][i];
                DEBUG_LOG("kymera_SplitterCollectSinks: stream_index[%d] channel[%d] = 0x%x", j, i, connected_sink);
            }
        }
    }
}

uint8 Kymera_SplitterGetNumOfInputs(splitter_handle_t handle)
{
    if (handle)
    {
        return handle->num_of_inputs;
    }
    return 0;
}

Sink Kymera_SplitterGetSink(splitter_handle_t handle, uint8 stream_index, uint8 channel)
{
    return connected_sinks[stream_index][channel];
}
#endif   //HOSTED_TEST_ENVIRONMENT
