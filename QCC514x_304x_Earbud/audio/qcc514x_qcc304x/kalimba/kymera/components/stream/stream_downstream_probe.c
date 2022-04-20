/****************************************************************************
 * Copyright (c) 2020 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file stream_downstream_probe.c
 * \ingroup stream
 *
 * This file contains public stream functions that are used by operators.
 */

#include "stream_downstream_probe.h"
#include "stream_private.h"
#include "opmgr/opmgr_private.h"

bool stream_short_downstream_probe(
        ENDPOINT_ID source_ep_id,
        STREAM_SHORT_DOWNSTREAM_PROBE_RESULT* result)
{
    /** The endpoint identified by source_ep_id */
    ENDPOINT* source_ep;
    ENDPOINT* sink_ep;
    /** Refers to the furthest downstream transform using the
     * same buffer memory as the connection buffer of source_ep.
     */
    TRANSFORM* last_tr;
    tCbuffer* buffer;

    patch_fn_shared(stream);

    if (NULL == result)
    {
        /* Cannot report results */
        return FALSE;
    }
    if (! get_is_source_from_opidep(source_ep_id))
    {
        /* Only handling operator source terminals */
        return FALSE;
    }

    /* Initialize results as false, most returns
     * from this function will want to report this
     */
    result->constant_rate_buffer_consumer = FALSE;
    result->is_in_place = FALSE;
    result->first_cbuffer = NULL;
    result->sink_ep = NULL;

    source_ep = stream_endpoint_from_extern_id(source_ep_id);
    if (NULL == source_ep)
    {
        /* A query about an operator source terminal which has never been
         * connected (so that the framework has not yet created an endpoint
         * structure for it), or an invalid source terminal ID, ends here.
         */
        return FALSE;
    }
    if (NULL == source_ep->connected_to)
    {
        /* Query ends, terminal endpoint exists but is not connected */
        return TRUE;
    }

    last_tr = stream_transform_from_endpoint(source_ep);
    if (NULL == last_tr)
    {
        /* If connected, there should be a transform */
        return FALSE;
    }

    buffer = last_tr->buffer;
    if (NULL == buffer)
    {
        /* Something very wrong */
        return FALSE;
    }
    result->first_cbuffer = buffer;

    if (! BUF_DESC_BUFFER_TYPE_MMU(buffer->descriptor) &&
        BUF_DESC_IN_PLACE(buffer->descriptor))
    {
        last_tr = stream_transform_from_buffer((tCbuffer*)(buffer->aux_ptr));
        if (NULL == last_tr)
        {
            /*
             * Source terminal of an operator in the middle of an in-place
             * chain.
             * TODO Finding the downstream end of an in-place chain from the
             * middle requires more support from stream_inplace_mgr.
             * Without such support, respond that the query has failed.
             * Save the buffer at the downstream end in result->tail_cbuffer.
             */

            /* Opportunity to fixup results */
            patch_fn_shared(stream);

            return FALSE;
        }

        result->is_in_place = TRUE;
    }

    sink_ep = last_tr->sink;
    /*
     * This condition tries to approximate what the function's name
     * says.
     *
     * - All the audio device sinks have constant consumption rate.
     *   (S/PDIF sink is also an audio endpoint sink.)
     *
     * - Real operator sink terminals should emulate devices.
     *   Currently the only ones are AEC Reference speaker inputs,
     *   which are constant rate, and Source Sync sink groups with
     *   rate adjustment enabled, which are /not/ constant rate,
     *   but rare, and there shouldn't be a volume control upstream
     *   of Source Sync, or cascaded Source Syncs.
     *
     * - USB TX operator inputs also emulate a device, but
     *   the only way to detect them would be to look up the
     *   capability ID. I'm refraining from doing that here.
     */
    uint32 dummy;
    if (stream_get_endpoint_config(sink_ep, EP_LATENCY_AMOUNT, &dummy))
    {
        result->sink_ep = sink_ep;
        result->constant_rate_buffer_consumer = TRUE;
    }
    else if ((endpoint_operator == sink_ep->stream_endpoint_type)
            && sink_ep->is_real)
    {
        result->constant_rate_buffer_consumer = TRUE;
    }

    /* Opportunity to fixup results */
    patch_fn_shared(stream);

    return TRUE;
}

unsigned stream_calc_downstream_amount_words_ex(
        tCbuffer* first_cbuffer,
        STREAM_SHORT_DOWNSTREAM_PROBE_RESULT* probe_info)
{
    patch_fn_shared(stream);

    if (first_cbuffer == NULL)
    {
        /* Sane handling of invalid input */
        return 0;
    }
    /* This gives the correct result for both
     * non-in-place buffers and head of in-place buffers.
     */
    unsigned amount;
    amount = cbuffer_get_size_in_words(first_cbuffer) -
             cbuffer_calc_amount_space_in_words(first_cbuffer) - 1;

    if (probe_info != NULL)
    {
        /* Include part of the amount in the MMU buffer of audio sink EPs */
        ENDPOINT* sink_ep = probe_info->sink_ep;
        uint32 latency_amount_words;
        if ((sink_ep != NULL)
            && stream_get_endpoint_config(sink_ep, EP_LATENCY_AMOUNT,
                                          &latency_amount_words))
        {
            amount += latency_amount_words;
        }
    }

    return amount;
}
