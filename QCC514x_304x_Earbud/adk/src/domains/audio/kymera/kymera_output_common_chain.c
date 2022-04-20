/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Configuration used for the fixed output chain
*/

#include "kymera_output_common_chain.h"
#include "kymera_output_common_chain_config.h"
#include "kymera_output_common_chain_private.h"
#include "kymera_output_if.h"
#include <logging.h>
#include <panic.h>
#include <operators.h>

#define OUTPUT_PREPARE_MAX_COUNT (255)

static struct
{
    uint8_t output_prepare_count;
    unsigned user_registered:1;
} state =
{
    .output_prepare_count = 0,
    .user_registered = FALSE,
};

static bool kymera_HandleOutputDisconnectRequest(void)
{
    return TRUE;
}

static const output_callbacks_t output_callbacks =
{
   .OutputDisconnectRequest = kymera_HandleOutputDisconnectRequest,
};

static const output_registry_entry_t output_info =
{
    .user = output_user_common_chain,
    .connection = output_connection_none,
    .callbacks = &output_callbacks,
};

static void kymera_HandleOutputIdleIndication(void)
{
    const kymera_output_chain_config *common_chain = Kymera_OutputCommonChainGetConfig();
    if(common_chain && (state.output_prepare_count > 0))
        Kymera_OutputPrepare(output_user_common_chain, common_chain);
}

static const output_indications_registry_entry_t output_indications =
{
    .OutputIdleIndication = kymera_HandleOutputIdleIndication
};

static void kymera_OutputCommonChainRegister(void)
{
    Kymera_OutputRegister(&output_info);
    Kymera_OutputRegisterForIndications(&output_indications);
    state.user_registered = TRUE;
}

void Kymera_OutputCommonChainPrepare(void)
{
    OperatorsFrameworkEnable();

    const kymera_output_chain_config * common_config = Kymera_OutputCommonChainGetConfig();
    if (common_config)
    {
        if(state.output_prepare_count == 0)
            Kymera_OutputPrepare(output_user_common_chain, common_config);

        PanicFalse(state.output_prepare_count < OUTPUT_PREPARE_MAX_COUNT);
        state.output_prepare_count++;
    }
}

void Kymera_OutputCommonChainUndoPrepare(void)
{
    if (Kymera_OutputCommonChainGetConfig())
    {
        PanicFalse(state.output_prepare_count > 0);
        state.output_prepare_count--;

        if(state.output_prepare_count == 0)
            Kymera_OutputDisconnect(output_user_common_chain);
    }

    OperatorsFrameworkDisable();
}

void Kymera_OutputCommonChainEnable(void)
{
    if(!state.user_registered)
        kymera_OutputCommonChainRegister();

    Kymera_OutputCommonChainConfigEnable();
}

void Kymera_OutputCommonChainDisable(void)
{
    if (state.output_prepare_count > 0)
    {
        DEBUG_LOG_ERROR("Kymera_OutputCommonChainDisable: Can't disable chain in use, output_prepare_count:%d", state.output_prepare_count);
        Panic();
    }
    Kymera_OutputCommonChainConfigDisable();
}
