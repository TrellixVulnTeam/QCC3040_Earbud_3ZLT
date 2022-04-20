#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Generic class of a stream writer.

Specific stream writer types should derive from this class.
"""
import logging

from ksp.lib.exceptions import StreamDataError


logger = logging.getLogger(__name__)


class StreamWriter(object):
    """Stream Writer.

    Args:
        filename (str): The filename to create without an extension.
    """
    def __init__(self, filename):
        self._filename = filename

        # Child classes should set this instance variable.
        self._handler = None

        self._total_samples = 0
        self._packets = 0

        # Current payload to write.
        self._header = None
        self._timestamp = None
        self._payload = None

    @property
    def filenames(self):
        """Returns the generated filenames."""
        return [self._handler.name]

    @property
    def packets(self):
        """Returns the number for the written packets."""
        return self._packets

    @property
    def data_type(self):
        """Returns the data type for the written stream."""
        return self._header.data_type

    @property
    def n_chans(self):
        """Returns the number of channels for the written stream."""
        return self._header.n_chans

    @property
    def samples(self):
        """Returns the number of samples for the written stream."""
        return self._header.samples

    @property
    def total_samples(self):
        """Returns the total number of samples for the written stream."""
        return self._total_samples

    def write(self, packet):
        """Checks and writes the packet into the output file.

        Args:
            packet (Packet): An instance of a Packet object.
        """
        self._check(packet)

        self._packets += 1
        self._total_samples += self._header.samples
        self._write()

    def _write(self):
        self._payload.tofile(self._handler)

    def close(self):
        """Closes the opened file."""
        self._handler.close()

    def _check(self, packet):
        if self._header is None:
            # This is the first write. There is nothing to check against.
            self._header = packet.header
            self._timestamp = packet.timestamp
            self._payload = packet.payload
            return

        # Check data type.
        if packet.header.data_type != self._header.data_type:
            raise StreamDataError(
                "UNEXPECTED DATA TYPE: type=%s exp=%s pkt_no=%s." % (
                    packet.header.data_type,
                    self._header.data_type,
                    self._packets
                )
            )

        # Check number of channels.
        if packet.header.n_chans != self._header.n_chans:
            raise StreamDataError(
                "UNEXPECTED CHANNELS: n_chans=%s exp=%s pkt_no%s." % (
                    packet.header.n_chans,
                    self._header.n_chans,
                    self._packets
                )
            )

        # Check sequence number.
        if packet.header.seq != ((self._header.seq + 1) & 127):
            logger.warning(
                "Sequence number jump: stream_id=%s, from %s to %s",
                self._header.stream_id,
                self._header.seq,
                packet.header.seq
            )

        # Calculate timestamp change.
        if packet.timestamp >= self._timestamp:
            dts = packet.timestamp - self._timestamp
        else:
            dts = packet.timestamp + (1 << 32) - self._timestamp

        # Warn if time stamp has a big jump.
        if dts > 100000:
            logger.warning(
                "Long jump in time stamp, stream_id=%s, jump=%s packet=%s.",
                self._header.stream_id,
                dts,
                self._packets
            )

        # Check went OK. Set the current header, timestamp and payload.
        self._header = packet.header
        self._timestamp = packet.timestamp
        self._payload = packet.payload
