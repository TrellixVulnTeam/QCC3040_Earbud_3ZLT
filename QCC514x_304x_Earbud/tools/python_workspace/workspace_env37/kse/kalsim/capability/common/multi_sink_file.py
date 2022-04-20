#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Multiple sink file kats operator"""

import argparse
import logging
import os

from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.capability.capability_base import CapabilityBase
from kats.library.audio_file.audio import audio_get_instance
from kats.library.registry import get_instance

FILENAME = 'filename'
SAMPLE_RATE = 'sample_rate'
SAMPLE_WIDTH = 'sample_width'
SOURCE = 'source'

PARAM_SCHEMA = {
    'type': 'object',
    'required': [FILENAME, SAMPLE_RATE, SAMPLE_WIDTH],
    'properties': {
        FILENAME: {'type': 'string'},
        SAMPLE_RATE: {'type': 'number', 'minimum': 0, 'exclusiveMinimum': 'true'},
        SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
        SOURCE: {'type': 'integer', 'minimum': 0},
    },
}


class MultiSinkFileCapability(CapabilityBase):
    """Multiple file source capability

    This capability will generate multiple output files ou of the same stream.
    The decision to close a file and open another one is based on the companion multi_source_file
    capability.

    - *filename* is the file to back the stream (mandatory).
        - raw files only store audio data but no information about number of channels,
          sampling frequency or data format.
          This information (*channels*, *sample_rate*, *sample_width*) has to be supplied
        - wav files store number of channels, sampling frequency and sample data format.
          Note that if *sample_rate* is provided then information in the file is overriden
        - qwav files store number of channels, sampling frequency, sample data format, optional
          metadata and optional packet based information
    - *sample_rate* is the sampling frequency in hertzs,
      for raw source files (mandatory), wav source files (optional) and all sink files (mandatory).
    - *sample_width* is the number of bits per sample,
      for raw source files (mandatory) and all sink files (mandatory).
    - *source* is the index to the multi source file kats operator (mandatory).

    Args:
        filename (str): Filename to back the stream
        sample_rate (int): Sample rate
        sample_width (int): Number of bit per sample
        source (int): Index to multi_source_file operator
    """

    platform = 'common'
    interface = 'multi_sink_file'
    input_num = 1
    output_num = 0

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')

        self._config = argparse.Namespace()  # configuration values
        self._config.filename = kwargs.pop(FILENAME)
        self._config.sample_rate = kwargs.pop(SAMPLE_RATE)
        self._config.sample_width = kwargs.pop(SAMPLE_WIDTH)
        self._config.source = kwargs.pop(SOURCE)
        self._config.installed = False

        self.__data = argparse.Namespace()  # data values
        self.__data.pos = 0  # current file
        self.__data.sink_audio_buffer = []

        super().__init__(*args, **kwargs)

    def _eof(self, index):
        _ = index
        self.__data.pos += 1

    @log_input(logging.INFO)
    def start(self):
        """Start stream"""
        super().start()
        if not self._config.installed:
            source = get_instance('koperator_multi_source_file', self._config.source)
            source.install_callback(self._eof)
            self._config.installed = True
        self.__data.sink_audio_buffer = []

    @log_input(logging.INFO)
    def destroy(self):
        """Destroy operator"""
        super().destroy()

        # save the files
        for ind, data in enumerate(self.__data.sink_audio_buffer):
            base, ext = os.path.splitext(self._config.filename)
            filename = base + '_%s' % (ind) + ext
            audio = audio_get_instance(filename, 'w', allow_encoded=False)
            audio.add_audio_stream(self._config.sample_rate, self._config.sample_width, data)
            audio.write()
            del audio

    def consume(self, input_num, data):
        if input_num == 0:
            if len(self.__data.sink_audio_buffer) == self.__data.pos:
                self.__data.sink_audio_buffer.append([])
            self.__data.sink_audio_buffer[self.__data.pos].extend(data)

    def eof_detected(self, input_num):
        pass  # we have no outputs so there is nothing we can do with this
