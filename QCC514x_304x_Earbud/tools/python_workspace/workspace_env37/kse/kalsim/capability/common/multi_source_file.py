#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Multiple source file kats operator"""

import argparse
import logging

from kats.core.stream_base import STREAM_NAME, STREAM_RATE, STREAM_DATA_WIDTH
from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.capability.capability_base import CapabilityBase
from kats.kalsim.stream.kalsim_helper import get_user_stream_config
from kats.kalsim.stream.packet.packetiser import Packetiser
from kats.library.audio_file.audio import audio_get_instance
from kats.library.registry import get_instance

FILE = 'file'
FILENAME = 'filename'
CHANNELS = 'channels'
CHANNELS_DEFAULT = 1
CHANNEL = 'channel'
CHANNEL_DEFAULT = 0
SAMPLE_RATE = 'sample_rate'
SAMPLE_WIDTH = 'sample_width'
FRAME_SIZE = 'frame_size'
FRAME_SIZE_DEFAULT = 1
DELAY = 'delay'
DELAY_DEFAULT = 0
CALLBACK_DATA_RECEIVED = 'callback_data_received'

PARAM_SCHEMA = {
    'type': 'object',
    'required': [FILE],
    'properties': {
        FILE: {
            'type': 'array',
            'minItems': 1,
            'items': {
                'type': 'object',
                'required': [FILENAME],
                'properties': {
                    FILENAME: {'type': 'string'},
                    CHANNELS: {'type': 'integer', 'minimum': 1},
                    CHANNEL: {'type': 'integer', 'minimum': 0},
                    SAMPLE_RATE: {'type': 'number', 'minimum': 0, 'exclusiveMinimum': 'true'},
                    SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
                    FRAME_SIZE: {'type': 'integer', 'minimum': 1},
                }
            },
        },
        DELAY: {'type': 'number', 'minimum': 0, 'default': DELAY_DEFAULT},
    }
}


class StreamFake:
    """Pseudo stream that always returns id 0"""

    def __init__(self, parent):
        self._parent = parent

    @staticmethod
    def get_id():
        """int: Stream id (always o)"""
        return 0

    def insert(self, data):
        """Insert data

        Args:
            data (list[int]): Data to insert
        """
        self._parent.produce(0, data)

    def eof(self):
        """Signal End of File"""
        self._parent.stream_eof()


class MultiSourceFileCapability(CapabilityBase):
    """Multiple file source capability

    - *file* is a list of files (mandatory)

        - *filename* is the file to back the stream (mandatory).
            - raw files only store audio data but no information about number of channels,
              sampling frequency or data format.
              This information (*channels*, *sample_rate*, *sample_width*) has to be supplied
            - wav files store number of channels, sampling frequency and sample data format.
              Note that if *sample_rate* is provided then information in the file is overriden
            - qwav files store number of channels, sampling frequency, sample data format, optional
              metadata and optional packet based information
        - *channels* is the number of channels/streams in the audio file,
          (optional default=1).
        - *channel* is the channel index in the audio file,
          (optional default=0).
        - *sample_rate* is the sampling frequency in hertzs,
          for raw source files (mandatory), wav source files (optional).
        - *sample_width* is the number of bits per sample
          for raw source files (mandatory).
        - *frame_size* is the number of samples per transaction,
          (optional, default=1).

    Args:
        file (list[dict]): List of files ot stream
    """

    platform = 'common'
    interface = 'multi_source_file'
    input_num = 0
    output_num = 1

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')

        self._config = argparse.Namespace()  # configuration values
        self._config.file = kwargs.pop(FILE)
        self._config.delay = kwargs.pop(DELAY, DELAY_DEFAULT)

        self.__data = argparse.Namespace()  # data values
        self.__data.stream_fake = StreamFake(self)
        self.__data.pos = 0  # current file
        self.__data.callable = []  # external entities interested in our events
        self.__data.source_packetiser = None  # source with packet based streaming

        super().__init__(*args, **kwargs)

    def _build_default_packet_info(self, audio_data, frame_size, sample_rate):
        # FIXME be careful with loop as all loops but first should not zero first packet
        period_usec = (1000000.0 * frame_size) / sample_rate
        packet_info = [
            [0 if ind == 0 else int(period_usec * ind - int(period_usec * (ind - 1))),  # timestamp
             ind * frame_size,  # offset
             frame_size]  # length
            for ind in range(int(len(audio_data) / frame_size))]
        if len(audio_data) % frame_size:
            self._log.warning('excluding %s bytes as it is not a multiple of frame_size',
                              len(audio_data) % frame_size)
        return packet_info

    def _init_source_file(self, kwargs):
        channels = kwargs.pop(CHANNELS, CHANNELS_DEFAULT)
        channel = kwargs.pop(CHANNEL, CHANNEL_DEFAULT)
        self._config.sample_rate = kwargs.pop(SAMPLE_RATE, None)
        self._config.sample_width = kwargs.pop(SAMPLE_WIDTH, None)
        self._config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)

        audio_kwargs = {
            'channels': channels,  # some formats do not require
            'sample_rate': self._config.sample_rate,  # some formats do not require
            'sample_width': self._config.sample_width,  # some formats do not require
            'allow_encoded': False,
        }
        audio_instance = audio_get_instance(self._config.filename, **audio_kwargs)
        channels = audio_instance.get_audio_stream_num()
        # we allow overriding sample_rate from what is in the file
        self._config.sample_width = audio_instance.get_audio_stream_sample_width(channel)

        if channel >= channels:
            raise RuntimeError('channels:%s channel:%s inconsistency' % (channels, channel))
        if audio_instance.get_audio_stream_sample_rate(channel) is None:
            raise RuntimeError('stream filename:%s sample_rate not set' % (self._config.filename))
        if self._config.sample_width is None:
            raise RuntimeError('stream filename:%s sample_width not set' % (self._config.filename))

        # kalsim supports raw and wav files with only 1 stream in the file at the file designated
        # sample_rate
        file_sample_rate = audio_instance.get_audio_stream_sample_rate(channel)

        audio_data = audio_instance.get_audio_stream_data(channel)
        if self._config.sample_rate is None:
            self._config.sample_rate = file_sample_rate

        # check if the format supports packet_info and that packet_info actually is there
        if (hasattr(audio_instance, 'get_packet_data_size') and
                audio_instance.get_packet_data_size('audio', channel)):
            packet_info = audio_instance.get_packet_data('audio', channel)
        else:
            # if packet_info is not available or empty then build default
            # packet based information
            packet_info = self._build_default_packet_info(audio_data, self._config.frame_size,
                                                          self._config.sample_rate)
        self.__data.source_packetiser = Packetiser(self.__data.stream_fake, audio_data, packet_info)
        params = get_user_stream_config(self._config.sample_width)
        params[STREAM_NAME] = self._config.filename
        params[STREAM_RATE] = self._config.sample_rate  # pointless in qwav with packet_info
        params[STREAM_DATA_WIDTH] = self._config.sample_width

        del audio_instance
        return params

    @log_input(logging.INFO)
    def _start(self, timer_id):
        """Callback to be invoked when the source stream start delay has elapsed to actually start
        the stream.

        Args:
            timer_id (int): Timer id
        """
        _ = timer_id
        super().start()
        self.stream_eof()

    @log_input(logging.INFO)
    def start(self):
        """Start stream"""
        if self._config.delay:
            self._log.info('delaying start for %s seconds', self._config.delay)
            self.__data.loop_timer_id = self.__helper.uut.timer_add_relative(self._config.delay,
                                                                             callback=self._start)
        else:
            self._start(0)

    @log_input(logging.INFO)
    def stop(self):
        """Stop stream"""

        if self.__data.source_packetiser:
            self.__data.source_packetiser.stop()

        super().stop()

    def consume(self, input_num, data):
        pass  # we never receive data from other entities as we have no inputs

    def eof_detected(self, input_num):
        pass  # we never receive eof from other entities as we have no inputs

    def stream_eof(self):
        if self.__data.pos < len(self._config.file):
            if self.__data.pos != 0:
                for callback in self.__data.callable:
                    callback(self.__data.pos - 1)
            kwargs = self._config.file[self.__data.pos]
            self._config.filename = kwargs.pop(FILENAME)
            self._init_source_file(kwargs)
            self.__data.pos += 1
            self.__data.source_packetiser.start()
        else:
            self.dispatch_eof(0)

    def install_callback(self, callback):
        self.__data.callable.append(callback)
