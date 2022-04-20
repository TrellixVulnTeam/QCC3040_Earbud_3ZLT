#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""RAW stream writer."""
import logging
import numpy

from ksp.lib.data_types import KspDataType
from .streamwriter import StreamWriter

logger = logging.getLogger(__name__)


class RawWriter(StreamWriter):
    """RAW stream writer.

    Args:
        filename (str): The output filename with full path but without an
            extension. The writer will use this string as a pattern to
            generate the output file.
    """
    def __init__(self, filename):
        super(RawWriter, self).__init__(filename)
        self._channels = {}

    @property
    def filenames(self):
        """Returns a list of generated filenames."""
        if self._channels:
            return [writer.filename for writer in self._channels]

        return super(RawWriter, self).filenames

    def close(self):
        """Close all the opened files."""
        if self._channels:
            for channel_writer in self._channels:
                channel_writer.close()
        else:
            super(RawWriter, self).close()

    def _init_handlers(self):
        if self._header.data_type.is_audio_type() or self._header.n_chans == 1:
            try:
                filename = '{}_stream{}_{}channels_{}.raw'.format(
                    self._filename,
                    self._header.stream_id,
                    self._header.n_chans,
                    self._header.data_type
                )
                self._handler = open(filename, 'wb')

            except IOError:
                logger.error("Cannot open % filename", self._filename)
                raise

        else:
            for channel in range(self._header.n_chans):
                filename = '{}_stream{}_channel{}_{}'.format(
                    self._filename,
                    self._header.stream_id,
                    channel,
                    self._header.data_type
                )
                self._channels[channel] = ChannelWriter(
                    filename,
                    self._header.data_type
                )

    def _write(self):
        if self._handler is None and not self._channels:
            self._init_handlers()

        if self._handler:
            super(RawWriter, self)._write()

        else:
            for channel in range(self._header.n_chans):
                channel_payload = self._payload[channel::self._header.n_chans]
                self._channels[channel].write(channel_payload)

    def _write_channels(self):
        for ch_no in self._header.n_chans:
            ch_writer = self._channels.get(ch_no)
            if ch_writer is None:
                filename = '{filename}_stream{stream_id}_{data_type}'.format(
                    filename=self._filename,
                    stream_id=self._header.stream_id,
                    data_type=self._header.data_type
                )
                ch_writer = ChannelWriter(
                    filename,
                    self._header.data_type
                )

                self._channels[ch_no] = ch_writer

            ch_writer.write(self._payload)


class ChannelWriter():
    """ Single channel in a KSP Stream.

    Args:
        filename (str): extracted raw file name.
        data_type (KspDataType): data type of the channel.
    """

    def __init__(self, filename, data_type):
        self._handler = open(filename, "wb")
        self._data_type = data_type

    @property
    def filenames(self):
        """Returns the filename of the file handler.

        Based on the derived classes, this can be multiple filenames.
        """
        return [self._handler.name]

    # Write channel data
    def write(self, payload):
        """Writes a payload to the file.

        Args:
            payload (Payload): The payload to write.
        """
        # For 24-bit data type, input is 32-bits,
        # convert it to 3x8bit.
        if self._data_type == KspDataType.PCM24:
            tmp = numpy.empty(3 * len(payload), dtype="u1")
            tmp[0::3] = (payload >> 8) & 0xFF
            tmp[1::3] = (payload >> 16) & 0xFF
            tmp[2::3] = (payload >> 24) & 0xFF
            payload = tmp

        # Write to channel file.
        payload.tofile(self._handler)

    def close(self):
        """Closes the opened file."""
        self._handler.close()
