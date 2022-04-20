#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Wave Stream Writer."""
import logging
import wave

from .streamwriter import StreamWriter

logger = logging.getLogger(__name__)


class WaveWriter(StreamWriter):
    """Wave Writer.

    Args:
        filename (str): The output filename with full path but without an
            extension. The writer will use this string as a pattern to
            generate the output file.
    """
    # Wave file header size in octet.
    HEADER_SIZE = 44
    # Offset of each data chunk size.
    HEADER_DATA_CHUNK_SIZE_OFFSET = 40

    def __init__(self, filename, sample_rate):
        super(WaveWriter, self).__init__(filename)

        self._sample_rate = sample_rate
        self._header_is_written = False

    @property
    def filenames(self):
        """Returns the generated filenames."""
        return [self._filename]

    def _init_handler_header(self):
        # Re-set the specific output filename.
        self._filename = '{}_stream{}_{}channels_{}.wav'.format(
            self._filename,
            self._header.stream_id,
            self._header.n_chans,
            self._header.data_type
        )

        self._handler = wave.open(self._filename, 'wb')
        # Writes the wave header.
        self._handler.setnchannels(self._header.n_chans)
        self._handler.setsampwidth(self._header.data_type.n_octets)
        self._handler.setframerate(self._sample_rate)

        self._header_is_written = True

    def _write(self):
        if self._header_is_written is False:
            self._init_handler_header()

        self._handler.writeframes(self._payload.tobytes())
