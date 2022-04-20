#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra audio data service streams"""

import argparse
import logging
import os
import struct

from kats.core.stream_base import STREAM_TYPE_SOURCE, STREAM_TYPE_SINK, \
    CALLBACK_EOF, STATE_STARTED, STREAM_NAME, STREAM_RATE, STREAM_DATA_WIDTH
from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.hydra_service.audio_sink_service import HydraAudioSink
from kats.kalsim.hydra_service.audio_source_service import HydraAudioSource
from kats.kalsim.hydra_service.constants import DEVICE_TYPE_L2CAP
from kats.kalsim.hydra_service.util import get_buffer_stats
from kats.kalsim.library.period import compute_period
from kats.kalsim.stream.hydra.hydra import StreamHydra
from kats.kalsim.stream.kalsim_helper import get_file_user_stream_config, get_user_stream_config
from kats.kalsim.stream.kalsim_stream import KalsimStream
from kats.kalsim.stream.packet.packetiser import Packetiser
from kats.library.audio_file.audio import audio_get_instance
from kats.library.registry import get_instance, get_instance_num, register_instance
from kats.library.test_base import add_output_file

BACKING = 'backing'
BACKING_FILE = 'file'
BACKING_DATA = 'data'
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
LOOP = 'loop'
LOOP_DEFAULT = 1
CALLBACK_CONSUME = 'callback_consume'
KICK_ENABLE = 'kick_enable'
KICK_ENABLE_DEFAULT = True
DEVICE_TYPE = 'device_type'
DEVICE_TYPE_DEFAULT = DEVICE_TYPE_L2CAP
SERVICE_TAG = 'service_tag'
SERVICE_TAG_DEFAULT = None
DATA_BUFFER_SIZE = 'data_buffer_size'
DATA_BUFFER_SIZE_DEFAULT = 1024
METADATA_ENABLE = 'metadata_enable'
METADATA_ENABLE_DEFAULT = False
METADATA_FORMAT = 'metadata_format'
METADATA_FORMAT_BASIC = 'basic'
METADATA_FORMAT_TIMESTAMPED = 'timestamped'
METADATA_FORMAT_DEFAULT = METADATA_FORMAT_BASIC
# METADATA_CHANNEL = 'metadata_channel'
# METADATA_CHANNEL_DEFAULT = 0
METADATA_BUFFER_SIZE = 'metadata_buffer_size'
METADATA_BUFFER_SIZE_DEFAULT = 256
TTP_DELAY = 'ttp_delay'
TTP_DELAY_DEFAULT = 0.05

METADATA_BASE_LENGTH = 2

METADATA_HEADER_LENGTH = {
    METADATA_FORMAT_BASIC: 2,
    METADATA_FORMAT_TIMESTAMPED: 16
}

PARAM_SCHEMA = {
    'oneOf': [
        {
            'type': 'object',
            'required': [BACKING, FILENAME],
            'properties': {
                BACKING: {'type': 'string', 'enum': [BACKING_FILE]},
                FILENAME: {'type': 'string'},
                CHANNELS: {'type': 'integer', 'minimum': 1},
                CHANNEL: {'type': 'integer', 'minimum': 0},
                SAMPLE_RATE: {'type': 'number', 'minimum': 0},
                SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
                FRAME_SIZE: {'type': 'integer', 'minimum': 1},
                DELAY: {'type': 'number', 'minimum': 0},
                LOOP: {'type': 'integer', 'minimum': 1},
                KICK_ENABLE: {'type': 'boolean', 'default': KICK_ENABLE_DEFAULT},
                DEVICE_TYPE: {'type': 'integer', 'minimum': 0},
                SERVICE_TAG: {'type': 'integer', 'minimum': 1},
                DATA_BUFFER_SIZE: {'type': 'integer', 'minimum': 1,
                                   'default': DATA_BUFFER_SIZE_DEFAULT},
                METADATA_ENABLE: {'type': 'boolean', 'default': METADATA_ENABLE_DEFAULT},
                METADATA_FORMAT: {'type': 'string',
                                  'enum': [METADATA_FORMAT_BASIC, METADATA_FORMAT_TIMESTAMPED],
                                  'default': METADATA_FORMAT_DEFAULT},
                # METADATA_CHANNEL: {'type': 'boolean', 'default': METADATA_CHANNEL_DEFAULT},
                METADATA_BUFFER_SIZE: {'type': 'integer', 'minimum': 1,
                                       'default': METADATA_BUFFER_SIZE_DEFAULT},
                TTP_DELAY: {'type': 'number', 'minimum': 0, 'default': TTP_DELAY_DEFAULT},
            }
        },
        {
            'type': 'object',
            'required': [BACKING],
            'properties': {
                BACKING: {'type': 'string', 'enum': [BACKING_DATA]},
                SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
                FRAME_SIZE: {'type': 'integer', 'minimum': 1},
            }
        }
    ]
}

HYDRA_TYPE = 'hydra_type'
HYDRA_TYPE_SUBSYSTEM = 'subsystem'
HYDRA_BAC_HANDLE = 'hydra_bac_handle'


class StreamHydraAudioData(KalsimStream):
    """Hydra audio data streams

    Args:
        stream_type (str): Type of stream

            - "source". Data is pushed into kymera
            - "sink". Data is extracted from kymera

        backing (str): Origin (for sources) or destination (for sinks) of data.
            This parameter defines what other parameters are needed.

            - "file". Data is backed by a file, if it is "source" is the contents to be
              streamed, if it is a "sink" is the file where data will be written to.
            - "data". Data is coming from ("source") or going to ("sink) an external software
              component. This allow to have loops where a sink stream loops back to a
              source stream.

        filename (str): Filename to back the stream

            - backing=="file". Mandatory

                - raw files only store audio data but no information about number of channels,
                  sampling frequency or data format.
                  This information (*channels*, *sample_rate*, *sample_width*) has to be supplied
                - wav files store number of channels, sampling frequency and sample data format.
                  Note that if *sample_rate* is provided then information in the file is overridden
                - qwav files store number of channels, sampling frequency, sample data format,
                  optional metadata and optional packet based information

            - backing =="data". Unavailable

        channels (int): Number of channels/streams in the audio file

            - backing=="file". Only for source streams, sink streams are always created with 1
              channel (optional default=1).
            - backing =="data". Unavailable

        channel (int): Channel index in the audio file

            - backing=="file". Only for source streams, sink streams are always created with 1
              channel (optional default=0).
            - backing =="data". Unavailable

        sample_rate (int): Sample rate in hertzs.

            - backing=="file". For raw source files (mandatory), wav source files (optional)
              and all sink files (mandatory). For sources if it is 0 it means as fast as possible.
            - backing=="data". Unavailable.

        sample_width (int): Number of bits per sample. Only 8 bits is recommended

            - backing=="file". For raw source files (mandatory) and all sink files (mandatory).
            - backing=="data". Valid for all file types and stream types (mandatory).

        frame_size (int): Number of frames per transfer

            - backing=="file". Valid for all file types and stream types (optional, default=1).
            - backing=="data". Only used in sink streams (optional default=1).

        delay (float): Delay in seconds from start to real start.
            Indicates the delay between the stream start command and the actual start in seconds

            - backing=="file". Only for source streams (optional default=0.0)
            - backing =="data". Unavailable

        loop (int): Number of times the source is played, when the source gets to end of file
            it is rewinded and replayed, only for source streams (optional default=1)

            - backing=="file". Available
            - backing =="data". Unavailable

        kick_enable (bool): Enables kicking the hydra audio data service every time audio is sent,
            for source streams (optional default=True)
        device_type (int): Hydra audio data service device type, for all streams
            (optional default=12 meaning l2cap/a2dp)
        service_tag (int): Hydra audio data service tag, for all stream
            (optional default=None meaning autogenerated)
        data_buffer_size (int): Size of the buffer in the hydra audio data service,
            (optional default=1024)
        metadata_enable (bool): Metadata should be sent alongside the audio data, this metadata
            will be auto-generated if the format is not qwav or extracted from the qwav file,
            for source streams (optional default=False)
        metadata_format (str): Metadata format, "basic" or "timestamped"
            (optional, default="basic")
        metadata_channel (int): Metadata channel in the qwav file,
            for source qwav streams (optional default=0)
        metadata_buffer_size (int): Size of the buffer in the hydra audio data service
            (optional default=256)
        ttp_delay (int): Time to play delay in seconds for source timestamped streams,
            (optional default=0.050)
        callback_consume (function): Function to be invoked when data is available

            - backing=="file". Unavailable
            - backing =="data". Only used in sink streams (mandatory) but can be set in the
              config method.
    """

    def __init__(self, stream_type, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')
        self.__helper.hydra_prot = get_instance('hydra_protocol')

        self.__config = argparse.Namespace()  # configuration values
        self.__config.backing = kwargs.pop(BACKING)
        self.__config.callback_data_received = None  # backing=data sink stream callback
        self.__config.delay = None
        self.__config.kick_enable = kwargs.pop(KICK_ENABLE, KICK_ENABLE_DEFAULT)
        self.__config.device_type = kwargs.pop(DEVICE_TYPE, DEVICE_TYPE_DEFAULT)
        self.__config.service_tag = kwargs.pop(SERVICE_TAG, SERVICE_TAG_DEFAULT)
        self.__config.data_buffer_size = kwargs.pop(DATA_BUFFER_SIZE, DATA_BUFFER_SIZE_DEFAULT)
        self.__config.metadata_enable = kwargs.pop(METADATA_ENABLE, METADATA_ENABLE_DEFAULT)
        self.__config.metadata_format = kwargs.pop(METADATA_FORMAT, METADATA_FORMAT_DEFAULT)
        # self.__config.metadata_channel = kwargs.pop(METADATA_CHANNEL, METADATA_CHANNEL_DEFAULT)
        self.__config.metadata_buffer_size = kwargs.pop(METADATA_BUFFER_SIZE,
                                                        METADATA_BUFFER_SIZE_DEFAULT)
        self.__config.ttp_delay = kwargs.pop(TTP_DELAY, TTP_DELAY_DEFAULT)

        self.__data = argparse.Namespace()
        self.__data.loop_timer_id = None  # timer when looping
        self.__data.loop = 1
        self.__data.source_packetiser = None  # source with packet based streaming
        self.__data.source_metadata = None
        self.__data.source_timer_id = None
        self.__data.sink_timer_id = None  # sink manual streaming timer id
        self.__data.sink_timer_remain = 0.0  # sink manual timer remainder
        self.__data.sink_audio_buffer = None  # sink manual streaming audio data buffer
        self.__data.stream2 = None
        self.__data.total_samples = 0
        self.__data.sent_samples = 0

        if self.__config.backing == BACKING_FILE:
            self.__config.filename = kwargs.pop(FILENAME)
            add_output_file(self.__config.filename)
            if stream_type == STREAM_TYPE_SOURCE:
                self.__config.delay = kwargs.pop(DELAY, DELAY_DEFAULT)
            else:
                self.__config.delay = None
            self.__config.loop = kwargs.pop(LOOP, LOOP_DEFAULT)
            self.__config.user_callback_eof = kwargs.get(CALLBACK_EOF, lambda *args, **kwargs: None)

            params = getattr(self, '_init_%s_file' % (stream_type))(kwargs)
            params[CALLBACK_EOF] = self.__eof  # required for loop
        else:  # BACKING_DATA
            self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH)
            if stream_type == STREAM_TYPE_SINK:
                self.__config.frame_size = kwargs.pop(FRAME_SIZE)
                self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME, None)
            else:
                self.__config.frame_size = None
            self.__config.loop = 1

            params = get_user_stream_config(self.__config.sample_width)

        self.__parameters = {}
        self.__parameters[HYDRA_TYPE] = HYDRA_TYPE_SUBSYSTEM
        self.__parameters[HYDRA_BAC_HANDLE] = 0  # we will modify this when we know the handle

        if self.__config.metadata_enable:
            kwargs2 = get_user_stream_config(8)
            kwargs2[HYDRA_TYPE] = HYDRA_TYPE_SUBSYSTEM
            kwargs2[HYDRA_BAC_HANDLE] = 0  # we will modify this when we know the handle
            self.__data.stream2 = StreamHydra(stream_type, **kwargs2)

        if kwargs:
            self._log.warning('unknown kwargs:%s', str(kwargs))

        super().__init__(stream_type, **params)

    def _build_default_packet_info(self, audio_data, frame_size, sample_rate):
        # FIXME be careful with loop as all loops but first should not zero frst packet
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
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE, None)
        self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH, None)
        self.__config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)

        audio_kwargs = {
            'channels': channels,  # some formats do not require
            'sample_rate': self.__config.sample_rate,  # some formats do not require
            'sample_width': self.__config.sample_width,  # some formats do not require
            'allow_encoded': False,
        }
        audio_instance = audio_get_instance(self.__config.filename, **audio_kwargs)
        channels = audio_instance.get_audio_stream_num()
        # we allow overriding sample_rate from what is in the file
        self.__config.sample_width = audio_instance.get_audio_stream_sample_width(channel)

        if channel >= channels:
            raise RuntimeError('channels:%s channel:%s inconsistency' % (channels, channel))
        if audio_instance.get_audio_stream_sample_rate(channel) is None:
            raise RuntimeError('stream filename:%s sample_rate not set' % (self.__config.filename))
        if self.__config.sample_width is None:
            raise RuntimeError('stream filename:%s sample_width not set' % (self.__config.filename))

        # kalsim supports raw and wav files with only 1 stream in the file at the file designated
        # sample_rate
        file_sample_rate = audio_instance.get_audio_stream_sample_rate(channel)
        if (channels == 1 and not self.__config.filename.endswith('.qwav') and
                (self.__config.sample_rate == file_sample_rate or
                 self.__config.sample_rate is None)):
            if self.__config.sample_rate is None:
                self.__config.sample_rate = file_sample_rate
            params = get_file_user_stream_config(self.__config.filename,
                                                 self.__config.sample_rate,
                                                 self.__config.sample_width)
            self.__data.total_samples = len(audio_instance.get_audio_stream_data(channel))
        else:
            audio_data = audio_instance.get_audio_stream_data(channel)
            if self.__config.sample_rate is None:
                self.__config.sample_rate = file_sample_rate

            # check if the format supports packet_info and that packet_info actually is there
            if (hasattr(audio_instance, 'get_packet_data_size') and
                    audio_instance.get_packet_data_size('audio', channel)):
                packet_info = audio_instance.get_packet_data('audio', channel)
            else:
                packet_info = self._build_default_packet_info(audio_data, self.__config.frame_size,
                                                              self.__config.sample_rate)
            self.__data.source_packetiser = Packetiser(self, audio_data, packet_info)
            params = get_user_stream_config(self.__config.sample_width)
            params[STREAM_NAME] = self.__config.filename
            params[STREAM_RATE] = self.__config.sample_rate  # pointless in qwav with packet_info
            params[STREAM_DATA_WIDTH] = self.__config.sample_width

        del audio_instance
        return params

    def _init_sink_file(self, kwargs):
        self.__config.channels = 1
        self.__config.channel = 0
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE)
        self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH)
        self.__config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)

        # kalsim supports raw and wav files with only 1 stream
        if not self.__config.filename.endswith('.qwav'):
            params = get_file_user_stream_config(self.__config.filename,
                                                 self.__config.sample_rate,
                                                 self.__config.sample_width)
        else:
            self.__data.sink_audio_buffer = []
            params = get_user_stream_config(self.__config.sample_width)
            params[STREAM_NAME] = self.__config.filename
            params[STREAM_RATE] = self.__config.sample_rate
            params[STREAM_DATA_WIDTH] = self.__config.sample_width
        return params

    def _induce_block(self, frame_size, check_space=True):
        if check_space:
            _, space = self.__helper.audio_hydra.get_buffer_stats()
            space = int((space * 8) / self.get_data_width())
            if space < frame_size:
                raise RuntimeError('induce:%s not enough space:%s' % (frame_size, space))
        self.induce(frame_size)
        if self.__config.metadata_enable:
            metadata_value = bytearray(struct.pack('<H', 0x8000 | frame_size))
            if self.__config.metadata_format == METADATA_FORMAT_TIMESTAMPED:
                delay = int(self.__config.ttp_delay * 1e6)
                ttp = int(self.__helper.uut.timer_get_time() * 1e6) + delay
                metadata_value += bytearray(struct.pack('<L', ttp))
                metadata_value += bytearray(struct.pack('<L', 0))
                metadata_value += bytearray(struct.pack('<B', 0))
                metadata_value += bytearray(struct.pack('<5B', 0, 0, 0, 0, 0))  # padding
            if check_space:
                _, space = self.__helper.audio_hydra.get_metadata_buffer_stats()
                # metadata buffer is always 8 bits
                if space < len(metadata_value):
                    raise RuntimeError('metadata induce:%s not enough space:%s' %
                                       (len(metadata_value), space))
            self.__data.stream2.insert(list(metadata_value))
        if self.__config.kick_enable:
            self.__helper.audio_hydra.kick()
        self.__data.sent_samples += frame_size

    def _send_data(self, timer_id):
        _ = timer_id
        self.__data.source_timer_id = None
        if self.get_state() == STATE_STARTED:
            if self.__data.sent_samples >= self.__data.total_samples:
                try:
                    self.induce(1)  # this will force EOF to be signalled
                except Exception:  # pylint: disable=broad-except
                    pass
            else:
                send = self.__data.total_samples - self.__data.sent_samples
                send = (send if send < self.__config.frame_size else self.__config.frame_size)
                self._induce_block(send)

                period = (1.0 * self.__config.frame_size) / self.__config.sample_rate
                period, self.__data.source_timer_remain = compute_period(
                    period, self.__data.source_timer_remain)
                self.__data.source_timer_id = self.__helper.uut.timer_add_relative(
                    period, self._send_data)

    @log_input(logging.DEBUG)
    def _space_available(self, avail, space):
        if self.get_type() == STREAM_TYPE_SOURCE:
            # avail = int((avail * 8) / self.get_data_width())
            space = int((space * 8) / self.get_data_width())
            if self.__data.sent_samples >= self.__data.total_samples:
                try:
                    self.induce(1)  # this will force EOF to be signalled
                except Exception:  # pylint: disable=broad-except
                    pass
            elif space >= self.__config.frame_size:
                send = self.__data.total_samples - self.__data.sent_samples
                send = (send if send < self.__config.frame_size else self.__config.frame_size)
                self._induce_block(send)
        else:
            avail = int((avail * 8) / self.get_data_width())
            # space = int((space * 8) / self.get_data_width())
            length = self.__config.frame_size * int(avail / self.__config.frame_size)

            if self.__config.metadata_enable:
                # Here we have to get as many metadata packets as available
                # to understand how much audio data is available
                length = 0
                rd_handle = self.__helper.audio_hydra.get_meta_data_read_handle()
                wr_handle = self.__helper.audio_hydra.get_meta_data_write_handle()
                meta_length = METADATA_HEADER_LENGTH[self.__config.metadata_format]

                meta_avail, _ = get_buffer_stats(rd_handle, wr_handle)
                while meta_avail > meta_length:
                    data = self.__data.stream2.extract(
                        METADATA_HEADER_LENGTH[self.__config.metadata_format])
                    # we trust the audio data length that comes in metadata
                    avail = struct.unpack('<H', bytearray(data[:2]))[0] & ~0x8000
                    if self.__config.metadata_format == METADATA_FORMAT_TIMESTAMPED:
                        _ttp = struct.unpack('<L', bytearray(data[2:6]))[0]
                    # if there is metadata we ignore frame_size
                    length += avail
                    meta_avail -= meta_length
                length = int((length * 8) / self.get_data_width())

            if self.__data.sink_audio_buffer is None:
                if length:
                    self.induce(length)
            else:
                if length:
                    # extract data
                    data = self.extract(length)
                    if self.__config.backing == BACKING_FILE:
                        self.__data.sink_audio_buffer += data
                    else:
                        if self.__config.callback_consume:
                            self.__config.callback_consume[0](data=data)

    @log_input(logging.INFO)
    def _start(self, timer_id):
        """Callback to be invoked when the source stream start delay has elapsed to actually start
        the stream.

        Args:
            timer_id (int): Timer id
        """
        _ = timer_id
        self.__data.loop_timer_id = None
        super().start()

        if self.__config.metadata_enable:
            self.__data.stream2.start()

        if self.get_type() == STREAM_TYPE_SOURCE:
            if self.__config.backing == BACKING_FILE:
                if self.__data.source_packetiser:  # with packetiser support
                    self.__data.source_packetiser.start()
                else:
                    self.__data.sent_samples = 0
                    send = self.__data.total_samples - self.__data.sent_samples
                    send = (send if send < self.__config.frame_size else self.__config.frame_size)
                    self._induce_block(send)
                    if self.__config.sample_rate > 0:
                        period = (1.0 * self.__config.frame_size) / self.__config.sample_rate
                        period, self.__data.source_timer_remain = compute_period(period, 0)
                        self.__data.source_timer_id = self.__helper.uut.timer_add_relative(
                            period, self._send_data)
            else:  # backing=data
                # we receive data with calls to consume
                pass
        else:  # sink stream
            if self.__config.backing == BACKING_FILE:
                # if we have kalsim support then the start call above will handle streaming
                if self.__data.sink_audio_buffer is not None:  # without kalsim support
                    if os.path.isfile(self.__config.filename):
                        os.remove(self.__config.filename)
                    self.__data.sink_audio_buffer = []
            else:  # backing=data
                # we receive data in the hydra audio data service space callback
                pass

    def __eof(self):
        """Internal End of file callback.

        This is used to accommodate the loop paramater which allows a source stream to
        automatically restart without delay multiple times
        """
        self.__data.loop = 0 if self.__data.loop <= 1 else self.__data.loop - 1
        if self.__data.loop > 0:
            self.stop()
            self._start(0)
        else:
            self.__config.user_callback_eof()

    @log_input(logging.INFO)
    def create(self):
        if self.__config.service_tag is not None:
            service_tag = self.__config.service_tag
        else:
            service_tag = get_instance_num('hydra_service') + 1

        md_buffer_size = self.__config.metadata_buffer_size * self.__config.metadata_enable
        md_length = METADATA_HEADER_LENGTH[self.__config.metadata_format] - METADATA_BASE_LENGTH

        if self._stream_type == STREAM_TYPE_SOURCE:
            callback = None
            if (self.__config.backing == BACKING_FILE and
                    self.__config.sample_rate == 0 and not self.__data.source_packetiser):
                callback = self._space_available
            self.__helper.audio_hydra = HydraAudioSink(
                self.__helper.hydra_prot,
                device_type=self.__config.device_type,
                service_tag=service_tag,
                data_buffer_size=self.__config.data_buffer_size,
                metadata_buffer_size=md_buffer_size,
                metadata_header_length=md_length,
                space_handler=callback)
            self.__helper.audio_hydra.start()
        else:
            self.__helper.audio_hydra = HydraAudioSource(
                self.__helper.hydra_prot,
                device_type=self.__config.device_type,
                service_tag=service_tag,
                data_buffer_size=self.__config.data_buffer_size,
                metadata_buffer_size=md_buffer_size,
                metadata_header_length=md_length,
                space_handler=self._space_available)
            self.__helper.audio_hydra.start()
        register_instance('hydra_service', self.__helper.audio_hydra)

        if self.__config.metadata_enable:
            self.__data.stream2.create()

        return super().create()

    @log_input(logging.INFO)
    def config(self, **kwargs):
        """Configure stream"""
        if CALLBACK_CONSUME in kwargs:
            self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME)
            if not isinstance(self.__config.callback_consume, list):
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))
            if len(self.__config.callback_consume) != 1:
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))

        if self._stream_type == STREAM_TYPE_SOURCE:
            bac_handle = self.__helper.audio_hydra.get_data_write_handle()
        else:
            bac_handle = self.__helper.audio_hydra.get_data_read_handle()
        self.__parameters[HYDRA_BAC_HANDLE] = bac_handle
        for key in self.__parameters:
            self._config_param(key, self.__parameters[key])

        super().config(**kwargs)

        for key in self.__parameters:
            _ = self.query(key)

        if self.__config.metadata_enable:
            if self._stream_type == STREAM_TYPE_SOURCE:
                bac_handle = self.__helper.audio_hydra.get_meta_data_write_handle()
            else:
                bac_handle = self.__helper.audio_hydra.get_meta_data_read_handle()
            kwargs_metadata = {HYDRA_BAC_HANDLE: bac_handle}
            self.__data.stream2.config(**kwargs_metadata)

    @log_input(logging.INFO)
    def start(self):
        """Start stream"""
        self.__data.loop = self.__config.loop
        if self.__config.delay:
            self._log.info('delaying start for %s seconds', self.__config.delay)
            self.__data.loop_timer_id = self.__helper.uut.timer_add_relative(self.__config.delay,
                                                                             callback=self._start)
        else:
            self._start(0)

    @log_input(logging.INFO)
    def stop(self):
        """Stop stream"""

        if self.__data.loop_timer_id is not None:
            self.__helper.uut.timer_cancel(self.__data.loop_timer_id)
            self.__data.loop_timer_id = None
        if self.__data.source_packetiser:
            self.__data.source_packetiser.stop()
        if self.__data.sink_timer_id is not None:
            self.__helper.uut.timer_cancel(self.__data.sink_timer_id)
            self.__data.sink_timer_id = None

        if self.__config.metadata_enable:
            self.__data.stream2.stop()

        super().stop()

    @log_input(logging.INFO)
    def destroy(self):
        """Destroy stream"""
        # Note that stopping the service will destroy the endpoint associated with it
        # hence we delay stopping the service until we are destroying the stream
        if self.__helper.audio_hydra and self.__helper.audio_hydra.check_started():
            self.__helper.audio_hydra.stop()

        if self.__config.backing == BACKING_FILE and self.__data.sink_audio_buffer is not None:
            self._log.info('creating file %s', self.__config.filename)
            audio_instance = audio_get_instance(self.__config.filename, 'w', allow_encoded=False)
            audio_instance.add_audio_stream(self.__config.sample_rate,
                                            self.__config.sample_width,
                                            self.__data.sink_audio_buffer)
            audio_instance.write()
            del audio_instance
            self.__data.sink_audio_buffer = []

        if self.__config.metadata_enable:
            self.__data.stream2.destroy()

        super().destroy()

    @log_input(logging.INFO, formatters={'data': '0x%04x'})
    def insert(self, data, check_space=True):
        """Inserts data with the size specified in stream_format property

        Args:
            data(list[int]): Data to insert
            check_space (bool): Check if there is space before inserting, otherwise raise
                exception.

        Raises:
            RuntimeError: If there is no space in MMU buffer
        """
        if check_space:
            _, space = self.__helper.audio_hydra.get_buffer_stats()
            space = int((space * 8) / self.get_data_width())
            if space < len(data):
                raise RuntimeError('insert:%s not enough space:%s' % (len(data), space))

        super().insert(data)

        if self.__config.metadata_enable:
            metadata_value = bytearray(struct.pack('<H', 0x8000 | len(data)))
            if self.__config.metadata_format == METADATA_FORMAT_TIMESTAMPED:
                delay = int(self.__config.ttp_delay * 1e6)
                ttp = int(self.__helper.uut.timer_get_time() * 1e6) + delay
                ttp = ttp & 0xFFFFFFFF  # limit value to 32 bits
                metadata_value += bytearray(struct.pack('<L', ttp))
                metadata_value += bytearray(struct.pack('<L', 0))
                metadata_value += bytearray(struct.pack('<B', 0))
                metadata_value += bytearray(struct.pack('<5B', 0, 0, 0, 0, 0))  # padding
            if check_space:
                _, space = self.__helper.audio_hydra.get_metadata_buffer_stats()
                # metadata buffer is always 8 bits
                if space < len(metadata_value):
                    raise RuntimeError('metadata insert:%s not enough space:%s' %
                                       (len(metadata_value), space))
            self.__data.stream2.insert(list(metadata_value))
        if self.__config.kick_enable:
            self.__helper.audio_hydra.kick()

    def get_endpoint_id(self):
        """Get endpoint id

        Returns:
            int: Endpoint id
        """
        return self.__helper.audio_hydra.get_endpoint_id()

    def consume(self, input_num, data):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            self.insert(data)

    def eof_detected(self, input_num):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            self.eof()
