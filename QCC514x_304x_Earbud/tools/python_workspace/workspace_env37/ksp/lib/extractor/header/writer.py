#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Header file writer for the lrw extractor."""


class LrwHeadersFileWriter():
    """A header file writer LRW files.

    The header file is a line separated text file.

    Args:
        filename (str): The name of the output file.
    """
    HEADER_SEPARATOR = '|  '

    def __init__(self, filename=None):
        self._handler = open(filename, 'w')

        self._streams_pkt_counter = {}
        self._streams = {}

        self._counter = 0

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    @property
    def name(self):
        """Returns the name of the header text file."""
        return self._handler.name

    def write(self, header, timestamp):
        """Writes the header into the file.

        Args:
            header (Header): The header of a packet.
            timestamp (int): The timestamp of the packet.
        """
        stream_id = header.stream_id
        n_chans = header.n_chans
        data_type = header.data_type
        seq = header.seq
        samples = header.samples

        if stream_id not in self._streams_pkt_counter:
            self._streams['stream{}'.format(stream_id)] = {
                'channels': n_chans,
                'data_type': data_type,
            }
            self._streams_pkt_counter[stream_id] = 0

        record = []
        record.append("packet_counter={0:<6}".format(self._counter))
        record.append("stream_id={0:<4}".format(stream_id))
        record.append("stream_packet_counter={0:<6}".format(
            self._streams_pkt_counter[stream_id]
        ))
        record.append("nr_channels={0:<4}".format(n_chans))
        record.append("data_type={0:<8}".format(data_type))
        record.append("seq={0:<4}".format(seq))
        record.append("timestamp={0:<10}".format(timestamp))
        record.append("samples={0:<4}".format(samples))

        # Write the header into the already opened text file.
        self._handler.write(self.HEADER_SEPARATOR.join(record) + '\n')
        self._handler.flush()

        self._streams_pkt_counter[stream_id] += 1
        self._counter += 1

    def close(self):
        """Closes the file handler for the writer."""
        self._handler.close()
