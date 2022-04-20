/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  cbops_howling_limiter_op.c
 * \ingroup cbops
 *
 * This file contains functions for the howling limiter cbops operator
 */

/****************************************************************************
Include Files
 */
#include "pmalloc/pl_malloc.h"
#include "cbops_c.h"

/****************************************************************************
Public Function Definitions
*/

/*
 * create_howling_limiter_op
 */
cbops_op *create_howling_limiter_op(
    unsigned input_idx,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui)
{
    cbops_op *op;

    /* Allocate memory for the CBOPS op, param header and parameters */
    op = (cbops_op *)xzpmalloc(sizeof_cbops_op(cbops_howling_limiter, 1, 0));

    if(op != NULL)
    {
        cbops_howling_limiter *hl_data;

        op->function_vector = cbops_howling_limiter_table;

        /* Setup cbops param struct header info */
        hl_data = (cbops_howling_limiter *)cbops_populate_param_hdr(op, 1, 0, &input_idx, NULL);

        /* Configure the howling limiter */
        hl_limiter_param_config(
            hl_data,
            mic_sampling_rate,
            hl_ui);
    }

    return(op);
}

void initialize_howling_limiter_op(
    cbops_op *op,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui)
{
    cbops_howling_limiter *hl_data = CBOPS_PARAM_PTR(op, cbops_howling_limiter);

    /* initialize the howling limiter */
    hl_limiter_init(
        hl_data,
        mic_sampling_rate,
        hl_ui);
}

void configure_howling_limiter_op(
    cbops_op *op,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui)
{
    cbops_howling_limiter *hl_data = CBOPS_PARAM_PTR(op, cbops_howling_limiter);

    /* Configure the howling limiter */
    hl_limiter_param_config(
        hl_data,
        mic_sampling_rate,
        hl_ui);
}
