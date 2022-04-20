#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Converts streams to firmware words."""
from ksp.lib.data_types import KspDataType
from ksp.lib.types import TransformID, TransformIDList


# BUG:B-299459 Unify all the stream concepts.
# pylint: disable=too-many-arguments
def stream_to_words(stream_number, nr_samples, channel_info, data_type,
                    tr_channels, metadata, timed_data):
    """Converts the stream instance to the firmware configuration words.

    The words is to configure a stream in the KSP operator.

    Args:
        stream_number
        nr_samples
        channel_info
        data_type
        tr_channels
        metadata
        timed_data

    returns:
        list: List of integers.
    """
    if tr_channels is None:
        nr_channels = 0
    else:
        nr_channels = TransformIDList.len(tr_channels)

    converters = {
        KspDataType.TTR: _ttr_stream_to_words
    }

    converter = converters.get(data_type, _default_stream_to_words)
    return converter(
        stream_number,
        nr_samples,
        channel_info,
        data_type,
        tr_channels,
        metadata,
        timed_data,
        nr_channels
    )


# BUG:B-299459 Unify all the stream concepts. Once the new stream object
# is in place we no longer passed all these arguments around.
# pylint: disable=unused-argument
def _ttr_stream_to_words(stream_number, nr_samples, channel_info,
                         data_type, tr_channels, metadata, timed_data,
                         nr_channels):
    enables = 0
    transform_mask = [0] * 16
    if TransformIDList.is_all(tr_channels):
        # No transform filter list means enable all transforms.
        enables |= 3

    elif not TransformIDList.is_none(tr_channels) and nr_channels > 0:
        enables |= 1

        for tr_ext_id_str in tr_channels:
            tr_int_id = TransformID.get_int_id(int(tr_ext_id_str, 16))

            if TransformID.is_valid_int_id(tr_int_id):
                index = int(tr_int_id / 16)
                transform_mask[index] |= (1 << (tr_int_id & 15))

    else:
        # Empty transform filter list means disable buffer level recording.
        pass

    if metadata:
        enables |= 4
    if timed_data:
        enables |= 8
    words_to_send = [
        0x3,                                    # msg id
        stream_number,                          # stream number
        nr_samples,                             # nr_samples
        128,                                    # setup buffer size
        2048,                                   # event buffer size
        enables
    ]
    words_to_send.extend(transform_mask)

    if timed_data:
        timed_data_word = int(timed_data, 16)
    else:
        timed_data_word = 0
    words_to_send.append(timed_data_word)

    return words_to_send


def _default_stream_to_words(stream_number, nr_samples, channel_info,
                             data_type, tr_channels, metadata, timed_data,
                             nr_channels):

    words_to_send = [
        0x1,                               # msg id
        stream_number,                     # stream number
        nr_channels,                       # number of channels
        data_type.value,                   # data type
        nr_samples,                        # nr_samples
        channel_info                       # channel info
    ]

    # Add Transform IDs.
    words_to_send.extend(TransformIDList.as_int_list(tr_channels))
    return words_to_send
