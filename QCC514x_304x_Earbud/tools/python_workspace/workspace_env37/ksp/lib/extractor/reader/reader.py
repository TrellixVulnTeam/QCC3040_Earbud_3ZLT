#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""LRW file packet reader."""
import collections
import numpy

from ksp.lib.data_types import KspDataType
from ksp.lib.extractor.header import Header
from ksp.lib.extractor.exceptions import PacketReadError


Packet = collections.namedtuple('Packet', ['header', 'timestamp', 'payload'])


class LrwReader():
    """Iterates over a raw file handler and reads packets.

    Args:
        handler (file): A file handler.
        byte_swap (bool): Set this flag if the packets are formed of
            little endian 32-bit words.
    """
    SYNC_WORD = 0xA6

    def __init__(self, handler, byte_swap=False):
        self._handler = handler
        self._byte_swap = byte_swap

        # Payload converter classes.
        self._data_convertors = {
            KspDataType.DATA16: self._convert_to_data16,
            KspDataType.PCM16: self._convert_to_pcm16,
            KspDataType.PCM24: self._convert_to_pcm24,
            KspDataType.PCM32: self._convert_to_pcm32,
            KspDataType.TTR: self._convert_to_pcm32,
            KspDataType.DATA32: lambda *args: None
        }

        # Current values.
        self._header = None
        self._timestamp = None
        self._payload = None

    def __iter__(self):
        return self

    def __next__(self):
        """Iterate over the buffer content and report the content one by one.

        Raises:
            StopIteration: When there is no next item available.
            DebugLogFormatterStringError: When the content of the buffer is
                erroneous.
        """
        self._read_check_header_timestamp()
        self._read_payload()

        return Packet(self._header, self._timestamp, self._payload)

    def _read_check_header_timestamp(self):
        dtype = '<u4' if self._byte_swap else '>u4'
        try:
            header, timestamp = numpy.fromfile(
                self._handler,
                count=2,
                dtype=dtype
            )

        except ValueError:
            raise StopIteration('Invalid header')

        self._header = Header(header)
        self._timestamp = timestamp

        # Check sync.
        if self._header.sync != self.SYNC_WORD:
            raise StopIteration(
                'Invalid Sync Word: {}'.format(self._header.sync)
            )

        # Check channel info type, at the moment doesn't support
        # channel-speceif info.
        if self._header.channel_info != 0:
            raise StopIteration(
                "Found an invalid channel info: {}".format(
                    self._header.channel_info
                )
            )

    def _read_payload(self):
        if self._header.data_type not in self._data_convertors:
            raise PacketReadError(
                "Unsupported %s data type." % self._header.data_type
            )

        total_samples = self._header.samples * self._header.n_chans

        packet_n_words32 = self._header.data_type.n_octets * total_samples // 4
        self._payload = numpy.fromfile(
            self._handler,
            count=packet_n_words32,
            dtype=">u4"
        )

        if self._byte_swap:
            self._payload = self._payload.byteswap()

        self._data_convertors[self._header.data_type]()

    def _convert_to_data16(self):
        """Converts the payload to 16-bit data."""
        self._payload = self._payload.view(dtype='>u2')

    def _convert_to_pcm16(self):
        """Converts the payload to 16-bit PCM, change indianness."""
        self._payload = self._payload.view(dtype='>u2')
        self._payload = self._payload.astype('<u2')

    def _convert_to_pcm24(self):
        temp = self._payload.view(dtype='u1')
        self._payload = numpy.empty_like(temp)
        self._payload[0::3] = temp[2::3]
        self._payload[1::3] = temp[1::3]
        self._payload[2::3] = temp[0::3]

    def _convert_to_pcm32(self):
        self._payload = self._payload.astype('<u4')
