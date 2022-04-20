#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Extractor module.

Module for extracting channels from a lrw recorded file, which comes from
KSP operator.
"""
import os
import sys

from ksp.lib.extractor.header import LrwHeadersFileWriter
from ksp.lib.extractor.reader import LrwReader
from ksp.lib.extractor.writer import WaveWriter, RawWriter


class LrwStreamExtractor():
    """Stream extractor for LRW files.

    Args:
        filename (str): The file name of the lrw file to extract.
        byte_swap (bool): Set this flag if the packets are formed of
            little endian 32-bit words.
        sample_rates (dict): Sample rates for each stream. The key is the
            stream ID and the value is the rate. For example, if there are two
            streams in the lrw file and the second stream has the sample rate
            of 44000, then the record in the dictionary is:
                {'0': None, '1': 44000}
        gen_header_file (bool): Flag to generate the header file.
    """
    def __init__(self, filename, byte_swap=False, sample_rates=None,
                 gen_header_file=True):
        self._filename = filename
        self._byte_swap = byte_swap
        self._gen_header_file = gen_header_file

        self._sample_rates = sample_rates
        self._streams = {}
        self._headers_filename = None

    # B-307747
    # pylint: disable=too-many-locals
    def extract(self, output_pattern=None):
        """Extracts all streams.

        The extraction method generates output files based on
        specifications which it finds from the lrw file. It automatically
        add information to the output files. When user provides ``output``
        parameter, the method uses the ``output`` string as the base for
        the outputs.

        Args:
            output_pattern (str): An optional output file pattern. When
                it is not given, the extractor will use the base name of
                the lrw file as output pattern to generate output files.

        Returns:
            dict: The summary of the extraction.
        """
        dir_name = os.path.dirname(self._filename)
        if output_pattern:
            output = os.path.join(dir_name, output_pattern)

        else:
            base_name = os.path.basename(self._filename).split('.')[0]
            output = os.path.join(dir_name, base_name)

        if self._gen_header_file:
            header_filename = output + '_headers.txt'
            header_writer = LrwHeadersFileWriter(header_filename)

        try:
            reader_file_handler = open(self._filename, "rb")
        except IOError as exc:
            sys.stderr.write(str(exc) + ".\n")
            raise exc

        self._streams = {}
        for pkt in LrwReader(reader_file_handler, byte_swap=self._byte_swap):

            header, timestamp, _ = pkt

            stream_writer = self._streams.get(header.stream_id)
            if stream_writer is None:
                sample_rate = self._get_sample_rate(header.stream_id)
                if header.data_type.is_audio_type() and sample_rate != 0:
                    stream_writer = WaveWriter(
                        output,
                        sample_rate
                    )
                    self._streams[header.stream_id] = stream_writer

                else:
                    stream_writer = RawWriter(output)
                    self._streams[header.stream_id] = stream_writer

            if self._gen_header_file:
                header_writer.write(header, timestamp)
            stream_writer.write(pkt)

        # Closing all the opened files.
        for stream_writer in self._streams.values():
            stream_writer.close()
        reader_file_handler.close()
        if self._gen_header_file:
            header_writer.close()

        if self._gen_header_file:
            self._headers_filename = header_writer.name
        return self.result()
    # pylint: enable=too-many-locals

    def _get_sample_rate(self, stream_id):
        if self._sample_rates is None:
            return 0

        try:
            return self._sample_rates[stream_id]

        except IndexError:
            return 0

    def result(self):
        """Returns the summary of the extraction.

        Returns:
            dict: A dictionary of the summary.
        """
        result = {}
        for stream_id, stream_writer in self._streams.items():
            result['stream{}'.format(stream_id)] = {
                'data_type': stream_writer.data_type,
                'channels': stream_writer.n_chans,
                'packets': stream_writer.packets,
                'samples_in_packet': stream_writer.samples,
                'total_samples': stream_writer.total_samples,
                'filenames': stream_writer.filenames
            }

        if self._headers_filename:
            result['headers_filename'] = self._headers_filename

        return result
