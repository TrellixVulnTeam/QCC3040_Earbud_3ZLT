/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to handle VA Wake-Up-Word chain

*/

#ifndef KYMERA_VA_WUW_CHAIN_H_
#define KYMERA_VA_WUW_CHAIN_H_

#include "kymera.h"
#include "va_audio_types.h"

/*! \brief Parameters used to configure the VA Wake-Up-Word chain operators */
typedef struct
{
    Task       wuw_detection_handler;
    FILE_INDEX wuw_model;
    DataFileID (*LoadWakeUpWordModel)(wuw_model_id_t model);
    /*! Sets the time offset to send to the splitter at VAD trigger */
    uint16     engine_init_preroll_ms;
} va_wuw_chain_op_params_t;

/*! \brief Parameters used to create the VA Wake-Up-Word chain */
typedef struct
{
    kymera_va_wuw_chain_params_t chain_params;
    va_wuw_chain_op_params_t operators_params;
} va_wuw_chain_create_params_t;

/*! \brief Create VA Wake-Up-Word chain.
           Must be called after VA mic chain is instantiated, since it will connect to it.
    \param params Parameters used to create/configure the chain.
*/
void Kymera_CreateVaWuwChain(const va_wuw_chain_create_params_t *params);
void Kymera_DestroyVaWuwChain(void);

void Kymera_StartVaWuwChain(void);
void Kymera_StopVaWuwChain(void);

void Kymera_ConnectVaWuwChainToMicChain(void);

void Kymera_VaWuwChainSleep(void);
void Kymera_VaWuwChainWake(void);

void Kymera_VaWuwChainStartGraphManagerDelegation(void);
void Kymera_VaWuwChainStopGraphManagerDelegation(void);

#endif /* KYMERA_VA_WUW_CHAIN_H_ */
