// *****************************************************************************
// Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
// *****************************************************************************

#ifndef CVC_STREAM_C_HEADER_INCLUDED
#define CVC_STREAM_C_HEADER_INCLUDED

typedef struct frmbuffer
{
    unsigned *frame_ptr;            /**< pointer, to beginning of frame */
    unsigned  frame_size;           /**< size of frame being processed */
    unsigned  buffer_size;          /**< size of cBuffer */
    unsigned *buffer_addr;          /**< base address of cBuffer */
    unsigned  peak_value;           /**< peak value of the stream calculated */
} cVcStream;

extern void cvc_stream_set_circ(unsigned *frame, unsigned buffer_size, unsigned *buffer_sart_address, cVcStream *stream);

#endif