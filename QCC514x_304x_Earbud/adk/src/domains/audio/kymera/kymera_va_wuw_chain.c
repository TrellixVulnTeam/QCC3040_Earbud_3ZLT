/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to handle VA Wake-Up-Word chain

*/

#include "kymera_va_wuw_chain.h"
#include "kymera_va_mic_chain.h"
#include "kymera_va_common.h"
#include "kymera_chain_roles.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <opmsg_prim.h>
#include <custom_operator.h>
#include <vmal.h>

#define GET_WUW_VERSION_NUMBER_MSG_ID (0x0A)

static void kymera_ConfigureGraphManager(Operator graph_manager, const void *params);
static void kymera_ConfigureWuwEngine(Operator wuw, const void *params);
static const operator_config_map_t graph_manager_operator_config_map[] = {
    { OPR_VA_GRAPH_MANAGER, kymera_ConfigureGraphManager }
};
static const operator_config_map_t wuw_engine_operator_config_map[] = {
    { OPR_WUW, kymera_ConfigureWuwEngine }
};

static const appKymeraVaWuwChainTable *chain_config_map = NULL;

static kymera_chain_handle_t va_wuw_chain = NULL;
static kymera_chain_handle_t graph_manager_chain = NULL;
static DataFileID wuw_model_handle = DATA_FILE_ID_INVALID;
static va_wuw_engine_t largest_wuw_engine = va_wuw_engine_qva;

static void kymera_ConfigureGraphManager(Operator graph_manager, const void *params)
{
    const va_wuw_chain_create_params_t *chain_params = params;
    Task task = chain_params->operators_params.wuw_detection_handler;
    PanicNull(task);
    MessageOperatorTask(graph_manager, task);
    const OPMSG_VA_GM_SET_SPLITTER_OFFSET splitter_offset = {
        OPMSG_VA_GM_SET_SPLITTER_OFFSET_CREATE(OPMSG_VA_GM_ID_SET_SPLITTER_OFFSET,
                                               chain_params->operators_params.engine_init_preroll_ms)
    };
    PanicFalse(OperatorMessage(graph_manager, splitter_offset._data,
                               OPMSG_VA_GM_SET_SPLITTER_OFFSET_WORD_SIZE, NULL, 0));
}

static void kymera_ConfigureWuwEngine(Operator wuw, const void *params)
{
    const va_wuw_chain_create_params_t *chain_params = params;
    const va_wuw_chain_op_params_t *op_params = &chain_params->operators_params;
    if (wuw_model_handle == DATA_FILE_ID_INVALID)
    {
        PanicFalse(op_params->LoadWakeUpWordModel != NULL);
        wuw_model_handle = op_params->LoadWakeUpWordModel(op_params->wuw_model);
        PanicFalse(wuw_model_handle != DATA_FILE_ID_INVALID);
    }
    OperatorsStandardSetSampleRate(wuw, Kymera_GetVaSampleRate());
    OperatorsWuwEngineLoadModel(wuw, wuw_model_handle);
}

static const chain_config_t * kymera_GetWuwEngineChainConfig(va_wuw_engine_t wuw_engine)
{
    PanicNull((void *)chain_config_map);

    for(unsigned i = 0; i < chain_config_map->table_length; i++)
    {
        if (chain_config_map->chain_table[i].chain_params.wuw_engine == wuw_engine)
        {
            return chain_config_map->chain_table[i].chain_config;
        }
    }

    PANIC("kymera_GetWuwEngineChainConfig: Wake-Up-Word engine not supported!");
    return NULL;
}

static const chain_config_t * kymera_GetGraphManagerChainConfig(void)
{
    return Kymera_GetChainConfigs()->chain_va_graph_manager_config;
}

static Operator kymera_GetOperatorFromChain(unsigned operator_role, kymera_chain_handle_t chain)
{
    PanicNull(chain);
    return ChainGetOperatorByRole(chain, operator_role);
}

static Sink kymera_GetChainInput(unsigned input_role)
{
    PanicNull(va_wuw_chain);
    return ChainGetInput(va_wuw_chain, input_role);
}

static void kymera_CreateChains(const kymera_va_wuw_chain_params_t *params)
{
    PanicNotNull(va_wuw_chain);
    PanicNotNull(graph_manager_chain);

    bool creating_largest_wuw_engine = largest_wuw_engine == params->wuw_engine;

    if(creating_largest_wuw_engine)
    {
        va_wuw_chain = PanicNull(ChainCreate(kymera_GetWuwEngineChainConfig(params->wuw_engine)));
        graph_manager_chain = PanicNull(ChainCreate(kymera_GetGraphManagerChainConfig()));
    }
    else
    {
        /* Create largest WuW engine first to prevent fragmentation */
        va_wuw_chain = PanicNull(ChainCreate(kymera_GetWuwEngineChainConfig(largest_wuw_engine)));
        graph_manager_chain = PanicNull(ChainCreate(kymera_GetGraphManagerChainConfig()));

        /* Remove the largest WuW engine and replace with requested engine now that VA graph manager is place-holding */
        ChainDestroy(va_wuw_chain);
        va_wuw_chain = NULL;
        va_wuw_chain = PanicNull(ChainCreate(kymera_GetWuwEngineChainConfig(params->wuw_engine)));
    }
}

static void kymera_ConfigureChains(const va_wuw_chain_create_params_t *params)
{
    Kymera_ConfigureChain(va_wuw_chain, wuw_engine_operator_config_map, ARRAY_DIM(wuw_engine_operator_config_map), params);

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureWuwChain)
    {
        KymeraGetTaskData()->chain_config_callbacks->ConfigureWuwChain(va_wuw_chain);
    }

    Kymera_ConfigureChain(graph_manager_chain, graph_manager_operator_config_map, ARRAY_DIM(graph_manager_operator_config_map), params);

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureGraphManagerChain)
    {
        KymeraGetTaskData()->chain_config_callbacks->ConfigureGraphManagerChain(graph_manager_chain);
    }
}

static void kymera_ConnectChains(void)
{
    ChainConnect(va_wuw_chain);
    ChainConnect(graph_manager_chain);
}

static void kymera_DisconnectChain(void)
{
    StreamDisconnect(NULL, kymera_GetChainInput(EPR_VA_WUW_IN));
}

static void kymera_RunUsingOperatorsNotToPreserve(OperatorFunction function)
{
    Operator ops[] = {kymera_GetOperatorFromChain(OPR_VA_GRAPH_MANAGER, graph_manager_chain), kymera_GetOperatorFromChain(OPR_WUW, va_wuw_chain)};
    function(ops, ARRAY_DIM(ops));
}

static void kymera_ChainSleep(Operator *array, unsigned length_of_array)
{
    operator_list_t operators_to_exclude = {array, length_of_array};
    ChainSleep(va_wuw_chain, &operators_to_exclude);
    ChainSleep(graph_manager_chain, &operators_to_exclude);
}

static void kymera_ChainWake(Operator *array, unsigned length_of_array)
{
    operator_list_t operators_to_exclude = {array, length_of_array};
    ChainWake(va_wuw_chain, &operators_to_exclude);
    ChainWake(graph_manager_chain, &operators_to_exclude);
}

void Kymera_CreateVaWuwChain(const va_wuw_chain_create_params_t *params)
{
    PanicFalse(params != NULL);
    kymera_CreateChains(&params->chain_params);
    kymera_ConfigureChains(params);
    kymera_ConnectChains();
}

void Kymera_DestroyVaWuwChain(void)
{
    kymera_DisconnectChain();
    if (wuw_model_handle != DATA_FILE_ID_INVALID)
    {
        PanicFalse(OperatorDataUnloadEx(wuw_model_handle));
        wuw_model_handle = DATA_FILE_ID_INVALID;
    }
    ChainDestroy(va_wuw_chain);
    ChainDestroy(graph_manager_chain);
    va_wuw_chain = NULL;
    graph_manager_chain = NULL;
}

void Kymera_ConnectVaWuwChainToMicChain(void)
{
    PanicNull(StreamConnect(Kymera_GetVaMicChainWuwOutput(), kymera_GetChainInput(EPR_VA_WUW_IN)));
}

void Kymera_StartVaWuwChain(void)
{
    ChainStart(va_wuw_chain);
    ChainStart(graph_manager_chain);
}

void Kymera_StopVaWuwChain(void)
{
    ChainStop(va_wuw_chain);
    ChainStop(graph_manager_chain);
}

void Kymera_VaWuwChainSleep(void)
{
    PanicFalse(OperatorFrameworkTriggerNotificationStart(TRIGGER_ON_GM, kymera_GetOperatorFromChain(OPR_VA_GRAPH_MANAGER, graph_manager_chain)));
    kymera_RunUsingOperatorsNotToPreserve(kymera_ChainSleep);
}

void Kymera_VaWuwChainWake(void)
{
    kymera_RunUsingOperatorsNotToPreserve(kymera_ChainWake);
    PanicFalse(OperatorFrameworkTriggerNotificationStop());
}

void Kymera_VaWuwChainStartGraphManagerDelegation(void)
{
    Kymera_VaMicChainStartGraphManagerDelegation(kymera_GetOperatorFromChain(OPR_VA_GRAPH_MANAGER, graph_manager_chain), kymera_GetOperatorFromChain(OPR_WUW, va_wuw_chain));
}

void Kymera_VaWuwChainStopGraphManagerDelegation(void)
{
    Kymera_VaMicChainStopGraphManagerDelegation(kymera_GetOperatorFromChain(OPR_VA_GRAPH_MANAGER, graph_manager_chain), kymera_GetOperatorFromChain(OPR_WUW, va_wuw_chain));
}

void Kymera_SetVaWuwChainTable(const appKymeraVaWuwChainTable *chain_table)
{
    chain_config_map = chain_table;
}

void Kymera_GetWakeUpWordEngineVersion(va_wuw_engine_t wuw_engine, va_audio_wuw_engine_version_t *version)
{
    uint16 id = GET_WUW_VERSION_NUMBER_MSG_ID;
    uint16 recv_msg[3];
    bool chain_already_created = (va_wuw_chain != NULL);

    if(!chain_already_created)
    {
        va_wuw_chain = PanicNull(ChainCreate(kymera_GetWuwEngineChainConfig(wuw_engine)));
    }

    Operator op = kymera_GetOperatorFromChain(OPR_WUW, va_wuw_chain);
    PanicFalse(VmalOperatorMessage(op, &id, SIZEOF_OPERATOR_MESSAGE(id), recv_msg, SIZEOF_OPERATOR_MESSAGE(recv_msg)));
    PanicFalse(recv_msg[0] == GET_WUW_VERSION_NUMBER_MSG_ID);
    version->msw = recv_msg[1];
    version->lsw = recv_msg[2];

    DEBUG_LOG("Kymera_GetWuwEngineVersion id 0x%X, msw 0x%X, lsw 0x%X", recv_msg[0], version->msw, version->lsw);

    if(!chain_already_created)
    {
        ChainDestroy(va_wuw_chain);
        va_wuw_chain = NULL;
    }
}

void Kymera_StoreLargestWuwEngine(void)
{
    PanicNull((void *)chain_config_map);

    uint32 largest_size = 0;

    for(unsigned i = 0; i < chain_config_map->table_length; i++)
    {
        uint32 size = 0;

        if(chain_config_map->chain_table[i].chain_config->operator_config->role == OPR_WUW)
        {
            size = CustomOperatorGetProgramSize(chain_config_map->chain_table[i].chain_config->operator_config->capability_id);
        }

        if(size > largest_size)
        {
            largest_size = size;
            largest_wuw_engine = chain_config_map->chain_table[i].chain_params.wuw_engine;
        }
    }

    DEBUG_LOG("Kymera_StoreLargestWuwEngine enum:va_wuw_engine_t:%d", largest_wuw_engine);

    PanicFalse(largest_size != 0);
}
