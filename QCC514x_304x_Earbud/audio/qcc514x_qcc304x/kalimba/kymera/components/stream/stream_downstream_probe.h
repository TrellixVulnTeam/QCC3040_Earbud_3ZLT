/****************************************************************************
 * Copyright (c) 2020 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file stream_downstream_probe.h
 * \ingroup stream
 *
 * This file contains public stream functions that are used by operators
 * and capabilities.
 */

#ifndef STREAM_DOWNSTREAM_PROBE_H
#define STREAM_DOWNSTREAM_PROBE_H

#include "types.h"
#include "stream/stream_common.h"
#include "buffer/cbuffer_c.h"

/*****************************************
 * Type definitions
 */

/** Structure for results of stream_short_downstream_probe */
typedef struct stream_short_downstream_probe_result
{
    /** True if the buffer ends at a constant rate real endpoint */
    bool    constant_rate_buffer_consumer   :  8;

    /** True if the connection is via an in-place chain */
    bool    is_in_place                     :  8;

    /** Pointer to cbuffer connected to the source terminal.
     */
    tCbuffer* first_cbuffer;

    /* Additional information to add could include sample rate
     * and block size of the consumer, if it turns out to be
     * useful, and easily obtained.
     */
    /** Pointer to sink endpoint at the end of the chain */
    ENDPOINT* sink_ep;

} STREAM_SHORT_DOWNSTREAM_PROBE_RESULT;


/*****************************************
 * Public function declarations
 */

/**
 * \brief Query whether a source endpoint is connected to
 *        a constant rate sink (audio sink or AEC Ref real sink terminal)
 * \param source_ep_id External endpoint ID of the source endpoint,
 *                     usually an operator terminal
 * \param result Pointer to result structure. On return, the result
 *               is valid if the return value is TRUE.
 *
 * \return TRUE if query could be processed, FALSE if there
 *         was an internal error, invalid parameter, or the query
 *         is not supported for the downstream graph.
 *
 * \note A simple latency estimate is possible if this function
 *       returns TRUE and the constant_rate_buffer_consumer field
 *       of the result is TRUE.
 *       If an answer cannot be obtained synchronously, this
 *       function will return FALSE.
 *       This function needs to perform some linear searches so it should
 *       not be called in performance critical places, such as from
 *       every processing run of an operator.
 */
extern bool stream_short_downstream_probe(
        ENDPOINT_ID source_ep_id,
        STREAM_SHORT_DOWNSTREAM_PROBE_RESULT* result);

/**
 * \brief Get the amount buffered downstream. Optionally use
 * additional information contained in the probe result.
 *
 * \param first_cbuffer The connection cbuffer of the source endpoint
 *        for which a stream_short_downstream_probe has been successful.
 *
 * \param probe_info Pointer to the result from
 *        stream_short_downstream_probe.
 *
 * \return Number of samples buffered. The types of endpoints
 *         for which constant_rate_buffer_consumer is TRUE
 *         only support PCM data, so compressed data amounts
 *         in octets don't need to be considered.
 *
 * \note This function is fast, so it can be called from processing runs.
 */
extern unsigned stream_calc_downstream_amount_words_ex(
        tCbuffer* first_cbuffer,
        STREAM_SHORT_DOWNSTREAM_PROBE_RESULT* probe_info);

#endif /* STREAM_DOWNSTREAM_PROBE_H */
