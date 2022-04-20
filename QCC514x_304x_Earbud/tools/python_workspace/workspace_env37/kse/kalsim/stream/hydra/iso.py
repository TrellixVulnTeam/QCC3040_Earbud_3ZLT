#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra iso streams"""

import argparse
import copy
import logging
import struct
from functools import partial

from kats.core.stream_base import STREAM_TYPE_SOURCE, STREAM_TYPE_SINK, \
    CALLBACK_EOF, STREAM_NAME, STREAM_RATE, STREAM_DATA_WIDTH, STATE_STARTED
from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.hydra_service.iso_processing_service import HydraIsoProcessingService, \
    ISOAL_FRAMING_UNFRAMED
from kats.kalsim.hydra_service.util import get_buffer_stats
from kats.kalsim.library.period import compute_period
from kats.kalsim.stream.kalsim_helper import get_user_stream_config
from kats.kalsim.stream.kalsim_stream import KalsimStream
from kats.kalsim.stream.packet.packetiser import Packetiser
from kats.library.audio_file.audio import audio_get_instance
from kats.library.registry import register_instance, get_instance, get_instance_num
from .hydra import HYDRA_TYPE, HYDRA_TYPE_SUBSYSTEM, HYDRA_BAC_HANDLE

BACKING = 'backing'
BACKING_FILE = 'file'
BACKING_DATA = 'data'
FILENAME = 'filename'
CHANNELS = 'channels'
CHANNELS_DEFAULT = 1
CHANNEL = 'channel'
CHANNEL_DEFAULT = 0
BITRATE = 'bitrate'
SAMPLE_RATE = 'sample_rate'
DELAY = 'delay'
DELAY_DEFAULT = 0
LOOP = 'loop'
LOOP_DEFAULT = 1
CALLBACK_CONSUME = 'callback_consume'
METADATA_ENABLE = 'metadata_enable'
METADATA_ENABLE_DEFAULT = False
METADATA_FORMAT = 'metadata_format'
METADATA_FORMAT_STANDARD = 'standard'
METADATA_FORMAT_FRAMED_ISOAL = 'framed_isoal'
METADATA_FORMAT_DEFAULT = METADATA_FORMAT_STANDARD
STREAM = 'stream'
STREAM_DEFAULT = None
DRIFT_FROM_AIR = 'drift_from_air'

SERVICE_TAG = 'service_tag'
SERVICE_TAG_DEFAULT = None

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
                BITRATE: {'type': 'number', 'minimum': 0},
                SAMPLE_RATE: {'type': 'number', 'minimum': 0},
                DELAY: {'type': 'number', 'minimum': 0},
                LOOP: {'type': 'integer', 'minimum': 1},

                METADATA_ENABLE: {'type': 'boolean', 'default': METADATA_ENABLE_DEFAULT},
                METADATA_FORMAT: {'type': 'string', 'default': METADATA_FORMAT_DEFAULT,
                                  'enum': [METADATA_FORMAT_STANDARD, METADATA_FORMAT_FRAMED_ISOAL]},
                STREAM: {'default': STREAM_DEFAULT},
                DRIFT_FROM_AIR: {'type': 'number', 'default': 0},

                SERVICE_TAG: {'default': SERVICE_TAG_DEFAULT},
            }
        },
        {
            'type': 'object',
            'required': [BACKING],
            'properties': {
                BACKING: {'type': 'string', 'enum': [BACKING_DATA]},
                BITRATE: {'type': 'number', 'minimum': 0},
                SAMPLE_RATE: {'type': 'number', 'minimum': 0},  # only for sink

                METADATA_ENABLE: {'type': 'boolean', 'default': METADATA_ENABLE_DEFAULT},
                METADATA_FORMAT: {'type': 'string', 'default': METADATA_FORMAT_DEFAULT,
                                  'enum': [METADATA_FORMAT_STANDARD, METADATA_FORMAT_FRAMED_ISOAL]},
                STREAM: {'default': STREAM_DEFAULT},
                DRIFT_FROM_AIR: {'type': 'number', 'minimum': 0, 'default': 0},

                SERVICE_TAG: {'default': SERVICE_TAG_DEFAULT},
            }
        }
    ]
}

METADATA_SYNC_WORD = 0x5c5c
METADATA_STATUS_OK = 0
METADATA_STATUS_CRC_ERROR = 1
METADATA_STATUS_NOTHING_RECEIVED = 2
METADATA_STATUS_NEVER_SCHEDULED = 3
METADATA_STATUS_ZEAGLE = 4

METADATA_STATUS_ZEAGLE_OK = 0
METADATA_STATUS_ZEAGLE_ONE_PACKET_WITH_BAD_CRC = 1
METADATA_STATUS_ZEAGLE_MULTIPLE_PACKETS_WITH_BAD_CRC = 2
METADATA_STATUS_ZEAGLE_PACKET_LOST = 3


class StreamHydraIso(KalsimStream):
    """Hydra iso streams

    Args:
        stream_type (str): Type of stream

            - "source". Data is pushed into kymera
            - "sink". Data is extracted from kymera

        backing (str): Origin (for sources) or destination (for sinks) of data.
            This parameter defines what other parameters are needed.

            - "file". Data is backed by a file, if it is "source" is the contents to be
              streamed, if it is a sink is the file where data will be written to.
            - "data". Data is coming from (source) or going to (sink) an external software
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

        bitrate (float): Bits per second, if supplied instead of sample_rate, it will be used, for
            source and sink files (optional)

        sample_rate (int): Sample rate in hertzs.

            - backing=="file". For raw source files (mandatory), wav source files (optional)
              and all sink files (mandatory). If it is 0 it means as fast as possible.
            - backing=="data". Valid for all sink files (mandatory).

        sample_width (int): Number of bits per sample

            - backing=="file". For raw source files (mandatory) and all sink files (mandatory).
            - backing=="data". Valid for all file types and stream types (mandatory).

        frame_size (int): Number of frames per transfer. **Currently unused**

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

        callback_consume (function(int)): Callback function when data is received

            - backing=="file". Unavailable
            - backing=="data". Only used in sink streams (mandatory) but can be set in the
              config method.

        metadata_enable (bool): Indicates if metadata should be sent alongside the audio data
            (source streams), this metadata will be auto-generated if the format is not qwav or
            extracted from the qwav file. Indicates if metadata is expected in the sink stream.
            (optional default=False)
        metadata_format (str): Metadata format to be used, this only applies if metadata is
            enabled, valid values are "standard" and "framed_isoal". Only for source streams
            (optional default "standard")
        stream (int): Parent stream, the sco services supports one tx and one rx channel,
            for the second stream in the service this provides the parent sco stream index
        service_tag (int): ISO processing service tag, if not provided one will be
            computed (optional)
        from_air_buffer_size (int): From-air buffer size in octets, 0 for from air direction
            unused
        wallclock_accuracy (float): Wallclock simulation accuracy in parts per million.
            This allows the wallclock simulation to not run at the same rate as TIMER_TIME kymera
            clock, sco insertion/extraction is synchronised with wallclock accuracy
        wallclock_handle (int) Wallclock MMU handle, dummy handle, any non zero value is valid
            (optional default 0x254)
        wallclock_offset (int): Wallclock offset in bytes (optional default 0)
        bt_clock_value (int): Initial bt clock value (2*slots)
        endpoint_type (int): Type of endpoint, 0 for sco, 1 for iso
        to_air_buffer_size (int): to air buffer size in octets, 0 for to air direction unused
        iso_stream_type (str/int): Type of iso stream, 0/"cis" for connected, 1/"bis" for broadcast
        isoal_framing (str/int): Type of framing, 0/"unframed" for unframed, 1/"framed" for framed
        iso_interval (int): ISO interval in 1.25 msec units, valid values 4 to 3200
        sdu_interval_to_air (int): sdu interval for data being transmitted in microseconds
        sdu_interval_from_air (int): sdu interval for data being received in microseconds
        max_sdu_to_air (int): to-air maximum sdu length in octets
        max_sdu_from_air (int): from-air maximum sdu length in octets
        next_slot_time (int): Next reserved slot time in bt ticks (2*slots)
        cis_sync_delay/stream_sync_delay (int): CIS/BIS synchronization delay in microseconds
        cig_sync_delay/group_sync_delay (int): CIG/BIG synchronization delay in microseconds
        flush_timeout_to_air (int): Flush timeout for data being transmitted in ISO intervals
        flush_timeout_from_air (int): Flush timeout for data being received in ISO intervals
        to_air_latency (int): Latency in the to-air direction in microseconds
        from_air_latency (int): Latency in the from-air direction in microseconds
        sdu_number (int): Initial sequence number (both to air and from air)
        sdu_number_to_air (int): Initial to air sequence number, if not supplied use sdu_number
        sdu_number_from_air (int): Initial from air sequence number, if not supplied use sdu_number
        drift_from_air (float): Sample rate drift in the from_air direction in ppm
    """

    platform = ['streplus', 'maorgen1', 'maor']
    interface = 'iso'

    def __init__(self, stream_type, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')
        self.__helper.kalcmd = get_instance('kalcmd')

        self.__config = argparse.Namespace()  # configuration values
        self.__config.backing = kwargs.pop(BACKING)
        self.__config.callback_data_received = None  # backing=data sink stream callback
        self.__config.delay = None
        self.__config.metadata_enable = kwargs.pop(METADATA_ENABLE)
        self.__config.metadata_format = kwargs.pop(METADATA_FORMAT)
        if self.__config.metadata_format == METADATA_FORMAT_STANDARD:
            self.__config.metadata_size = 10
        else:
            self.__config.metadata_size = 14
        self.__config.stream = kwargs.pop(STREAM)
        self.__config.drift_from_air = kwargs.pop(DRIFT_FROM_AIR)
        self.__config.wallclock_accuracy = 0  # we read it after iso service created
        self.__config.isoal_framing = 0  # we read it after iso service created

        self.__config.service_tag = kwargs.pop(SERVICE_TAG)

        self.__data = argparse.Namespace()
        self.__data.loop_timer_id = None  # timer when looping
        self.__data.loop = 1
        self.__data.source_packetiser = None  # source with packet based streaming
        self.__data.source_metadata = None
        self.__data.source_timer_id = None
        self.__data.source_timer_remain = 0.0  # source manual timer remainder
        self.__data.sink_timer_id = None  # sink manual streaming timer id
        self.__data.sink_timer_remain = 0.0  # sink manual timer remainder
        self.__data.sink_audio_buffer = None  # sink manual streaming audio data buffer
        self.__data.stream2 = None
        self.__data.sdu = 0  # will be initialised when iso service is started
        self.__data.sdu_tx_remainder = 0
        self.__data.timestamp = 0  # metadata timestamp
        self.__data.audio_data = None
        self.__data.source_packetiser = None  # source with packet based streaming
        self.__data.total_samples = 0
        self.__data.sent_samples = 0
        self.__data.buffer_size = None
        self.__data.buffer_width = None
        self.__data.sent_packets = 0

        self.__data.iso_service = None

        if self.__config.backing == BACKING_FILE:
            self.__config.filename = kwargs.pop(FILENAME)
            if stream_type == STREAM_TYPE_SOURCE:
                self.__config.delay = kwargs.pop(DELAY, DELAY_DEFAULT)
            else:
                self.__config.delay = None
            self.__config.loop = kwargs.pop(LOOP, LOOP_DEFAULT)
            self.__config.user_callback_eof = kwargs.get(CALLBACK_EOF, lambda *args, **kwargs: None)

            params = getattr(self, '_init_%s_file' % (stream_type))(kwargs)
            params[CALLBACK_EOF] = self.__eof  # required for loop
        else:  # BACKING_DATA
            if stream_type == STREAM_TYPE_SINK:
                bitrate = kwargs.pop(BITRATE, None)
                self.__config.sample_rate = kwargs.pop(SAMPLE_RATE, bitrate / 8.0)  # needed in iso
            self.__config.sample_width = 8
            if stream_type == STREAM_TYPE_SINK:
                self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME, None)
            self.__config.loop = 1

            params = get_user_stream_config(self.__config.sample_width)

        self.__parameters = {}
        self.__parameters[HYDRA_TYPE] = HYDRA_TYPE_SUBSYSTEM
        self.__parameters[HYDRA_BAC_HANDLE] = 0  # we will modify this when we know the handle

        self._iso_kwargs = copy.copy(kwargs)

        super().__init__(stream_type, **params)

    def _init_source_file(self, kwargs):
        channels = kwargs.pop(CHANNELS, CHANNELS_DEFAULT)
        channel = kwargs.pop(CHANNEL, CHANNEL_DEFAULT)
        bitrate = kwargs.pop(BITRATE, None)
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE, bitrate / 8.0)
        self.__config.sample_width = 8

        audio_kwargs = {
            'channels': channels,
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
        if self.__config.sample_width != 8:
            raise RuntimeError('stream filename:%s sample_width:%s not suported' %
                               (self.__config.filename, self.__config.sample_width))

        # FIXME here we are only supporting streaming manually with kats
        # might be interesting to add kalsim support in the future whenever is possible
        # check if the format supports packet_info and that packet_info actually is there
        audio_data = audio_instance.get_audio_stream_data(channel)
        if (hasattr(audio_instance, 'get_packet_data_size') and
                audio_instance.get_packet_data_size('audio', channel)):
            packet_info = audio_instance.get_packet_data('audio', channel)
            self.__data.source_packetiser = Packetiser(self, audio_data, packet_info)
        else:
            self.__data.audio_data = audio_data
            self.__data.total_samples = len(audio_instance.get_audio_stream_data(channel))
        params = get_user_stream_config(self.__config.sample_width)
        params[STREAM_NAME] = self.__config.filename
        params[STREAM_RATE] = self.__config.sample_rate  # pointless in qwav with packet_info
        params[STREAM_DATA_WIDTH] = self.__config.sample_width

        del audio_instance
        return params

    def _init_sink_file(self, kwargs):
        # we will use sample_rate if defined, if not bitrate
        # sample_width is not used (internally used as 8)
        bitrate = kwargs.pop(BITRATE, None)
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE, bitrate / 8.0)
        self.__config.sample_width = 8

        # As we have to check received packet timing is correct and discard or insert packets
        # we need to be able to extract data at all times so user streams are required
        self.__data.sink_audio_buffer = []
        params = get_user_stream_config(self.__config.sample_width)
        params[STREAM_NAME] = self.__config.filename
        params[STREAM_RATE] = self.__config.sample_rate
        params[STREAM_DATA_WIDTH] = self.__config.sample_width

        return params

    def _start(self, timer_id):
        _ = timer_id
        self.__data.loop_timer_id = None
        super().start()

        if self.get_type() == STREAM_TYPE_SOURCE:
            self.__data.sent_packets = 0

            if self.__config.backing == BACKING_FILE:
                self.__data.sent_samples = 0
                self.__data.iso_service.start_channel(1, partial(self._data_transmit, 0))
                # sdu number could have been updated during channel start
                if self.get_type() == STREAM_TYPE_SOURCE:
                    self.__data.sdu = self.__data.iso_service.sdu_number_from_air
                else:
                    self.__data.sdu = self.__data.iso_service.sdu_number_to_air
            else:
                # we receive data with calls to consume
                pass
        else:
            aux_handle = self.__data.iso_service.to_air_handle
            self.__data.buffer_size = self.__helper.kalcmd.get_buffer_size(aux_handle)
            self.__data.buffer_width = self.__helper.kalcmd.get_handle_sample_size(aux_handle)
            if self.__config.backing == BACKING_FILE:
                self.__data.iso_service.start_channel(0, partial(self._data_received, 0))
            else:  # backing=data
                # we receive data from iso endpoint, but we need to poll it
                self.__data.iso_service.start_channel(0, partial(self._data_received, 0))

    def _compute_byte_metadata(self, data_length, timestamp=0, status=METADATA_STATUS_OK):
        # compute metadata in 8 bit format
        metadata = [METADATA_SYNC_WORD, self.__config.metadata_size // 2, data_length,
                    status, self.__data.sdu & 0xFFFF]
        if self.__config.metadata_size == 14:
            metadata += [(int(timestamp) >> 16) & 0xFFFF, int(timestamp) & 0xFFFF]
        return list(bytearray(struct.pack('<%sH' % len(metadata), *metadata)))

    def _generate_packet(self, data):
        """Build the actual packet to be sent

        Args:
            data (list[int]): Packet data (no metadata)

        Returns:
            list[int]: Packet data (with optional metadata including error insertion)
        """

        data_out = data

        # generate metadata
        if self.__config.metadata_enable:
            if not self.__data.timestamp:
                self.__data.timestamp = int(self.__helper.kalcmd.get_current_time() * 1e6)

            status = METADATA_STATUS_OK
            metadata = self._compute_byte_metadata(
                len(data_out), timestamp=self.__data.timestamp, status=status)
            data_out = metadata + data_out
            self.__data.timestamp += self.__data.iso_service.sdu_interval_from_air \
                * (1e6 + self.__config.wallclock_accuracy + self.__config.drift_from_air) / 1e6

        # perform padding for odd number of bytes
        if len(data_out) % 2:
            data_out += bytearray([0])

        self.__data.sdu += 1

        return data_out

    def _data_transmit(self, timer_id):
        _ = timer_id
        self.__data.source_timer_id = None

        if self.get_state() == STATE_STARTED:
            if self.__data.source_packetiser:  # with packetiser support
                self.__data.source_packetiser.start()
            else:
                # btss will provide only complete sdu at every period,
                # For unframed every period (sdu_interval) will deliver one sdu
                # For framed every period (iso_interval) could deliver none, one or more
                # sdus depending on the relationship between iso_interval and sdu_interval

                iso_interval = self.__data.iso_service.iso_interval_sec
                max_sdu = self.__data.iso_service.max_sdu_from_air  # max bytes per packet
                sdu_interval = self.__data.iso_service.sdu_interval_from_air_sec

                if self.__config.isoal_framing == ISOAL_FRAMING_UNFRAMED:
                    period = sdu_interval
                    sdu_tx = 1
                else:
                    # sdu interval with drift
                    self.__data.sdu_tx_remainder += iso_interval / (
                            sdu_interval * (1e6 + self.__config.drift_from_air) / 1e6)

                    period = iso_interval
                    sdu_tx = int(self.__data.sdu_tx_remainder)

                data_length = round(
                    sdu_tx * sdu_interval * self.__config.sample_rate)  # bytes we want to send

                if self.__data.sent_samples < self.__data.total_samples:
                    if (self.__data.total_samples - self.__data.sent_samples) < data_length:
                        data_length = self.__data.total_samples - self.__data.sent_samples

                    while data_length and sdu_tx:
                        bytes_to_send = data_length if data_length < max_sdu else max_sdu
                        sent_samples = self.__data.sent_samples
                        data = list(
                            self.__data.audio_data[sent_samples:sent_samples + bytes_to_send])
                        data_to_send = self._generate_packet(data)
                        if data_to_send:
                            self.insert(data_to_send)
                            self.__data.sent_packets += 1

                        self.__data.sent_samples += bytes_to_send
                        data_length -= bytes_to_send
                        sdu_tx -= 1
                        self.__data.sdu_tx_remainder -= 1

                    # apply wallclock accuracy for next timer
                    period *= (1e6 + self.__config.wallclock_accuracy) / 1e6
                    period, self.__data.source_timer_remain = compute_period(
                        period, self.__data.source_timer_remain)
                    self.__data.source_timer_id = self.__helper.uut.timer_add_relative(
                        period, self._data_transmit)
                else:
                    self.eof()

    def _get_stats(self):
        wr_handle = self.__data.iso_service.to_air_write_handle
        aux_handle = self.__data.iso_service.to_air_handle
        used, free = get_buffer_stats(aux_handle, wr_handle,
                                      buffer_size=self.__data.buffer_size,
                                      buffer_width=self.__data.buffer_width)
        used = int((used * 8) / self.get_data_width())
        free = int((free * 8) / self.get_data_width())
        return used, free

    def _check_packet(self, data, expected_data_length):
        """Check if a received packet (with optional metadata as configured in the stream)
        is valid. Only metadata in standard format is supported in kymera

        Args:
            data (list[int]): Data received
            expected_data_length (int): Expected data length in samples

        Returns:
            list[int]: User data in packet, not including metadata
        """
        if self.__config.metadata_enable:
            if len(data) < self.__config.metadata_size:
                self._log.warning(
                    'received packet length:%s less than metadata packet size', len(data))
                return []
            if self.__config.metadata_size == 10:
                metadata = list(struct.unpack('<5H', bytearray(data[:self.__config.metadata_size])))
            else:
                metadata = list(struct.unpack('<7H', bytearray(data[:self.__config.metadata_size])))
            data = data[self.__config.metadata_size:]

            if metadata[0] != METADATA_SYNC_WORD:
                self._log.warning('received invalid metadata sync word:0x%04x', metadata[0])
                return []

            expected_bytes = int(expected_data_length * (self.get_data_width() / 8))
            if metadata[2] != expected_bytes:
                self._log.warning(
                    'received invalid metadata packet_length:%s expected:%s', metadata[2],
                    expected_bytes)
                return []

        if expected_data_length % 2:  # unpad if needed
            data = data[:-1]

        if len(data) != expected_data_length:
            self._log.warning(
                'received invalid data_length:%s expected:%s', len(data), expected_data_length)
            return []

        return data

    def _data_received(self, timer_id):
        _ = timer_id
        self.__data.sink_timer_id = None

        # we will trigger timers every period sdu_interval and try to receive one sdu
        max_sdu = self.__data.iso_service.max_sdu_to_air  # max bytes per packet
        read_sdu = max_sdu + 1 if max_sdu % 2 else max_sdu
        sdu_interval = self.__data.iso_service.sdu_interval_to_air_sec

        period = sdu_interval  # timer period
        data_length = int(period * self.__config.sample_rate)  # bytes we want to receive
        max_packets = 1

        metadata_length = 0
        if self.__config.metadata_enable:
            metadata_length += int(self.__config.metadata_size / (self.get_data_width() / 8))
        total_length = max_packets * (read_sdu + metadata_length)

        if self.get_state() == STATE_STARTED:
            # read the buffer statistics to see how many samples are available
            # if there is a full packet available that we should receive then read it.
            # if not just do nothing
            used, free = self._get_stats()
            self._log.info('iso data expected:%s available:%s space:%s', total_length, used, free)

            while data_length and max_packets:
                used, free = self._get_stats()
                if used >= read_sdu + metadata_length:  # enough bytes for a whole packet, then read
                    data = self.extract(read_sdu + metadata_length)
                    data = self._check_packet(data, max_sdu)
                    if self.__config.backing == BACKING_FILE:
                        if data:
                            self.__data.sink_audio_buffer += data
                    else:
                        if data and self.__config.callback_consume:
                            self.__config.callback_consume[0](data=data)
                    max_packets -= 1
                    data_length -= max_sdu
                else:
                    self._log.warning('sco data avail:%s is not a multiple of expected:%s',
                                      used, total_length)
                    break

            period *= (1e6 + self.__config.wallclock_accuracy) / 1e6
            period, self.__data.sink_timer_remain = compute_period(
                period, self.__data.sink_timer_remain)
            self.__data.sink_timer_id = self.__helper.uut.timer_add_relative(
                period, self._data_received)

    def __eof(self):
        """This is our own callback for an End of File.

        In the case of source file backed streams we install this callback handler when there is a
        stream eof. This will cause to check if there are any additional loops to be done and
        in case there are rewind the stream and replay
        """
        self.__data.loop = 0 if self.__data.loop <= 1 else self.__data.loop - 1
        if self.__data.loop > 0:
            self.stop()
            self._start(0)
        else:
            self.__config.user_callback_eof()

    @log_input(logging.INFO)
    def create(self):
        """Start service and create stream

        TODO: If stream_type of a SCO Processing Service instance is easily available,
        raise a RuntimeError if we are trying to start two instances with the same
        stream_type and hci_handle.
        """
        if self.__config.stream is not None:
            stream = get_instance('stream_iso')
            if stream.get_type() == self.get_type():
                raise RuntimeError('trying to start two iso streams of same type')
            self.__data.iso_service = stream.get_iso_service()
        else:
            if self.__config.service_tag is not None:
                service_tag = self.__config.service_tag
            else:
                service_tag = get_instance_num('iso_processing_service') + 200

            self.__data.iso_service = HydraIsoProcessingService(service_tag=service_tag,
                                                                **self._iso_kwargs)
            register_instance('iso_processing_service', self.__data.iso_service)
            self.__data.iso_service.start()
            self.__data.iso_service.config()
            if self.get_type() == STREAM_TYPE_SOURCE:
                self.__data.sdu = self.__data.iso_service.sdu_number_from_air
            else:
                self.__data.sdu = self.__data.iso_service.sdu_number_to_air
            self.__data.timestamp = 0  # metadata timestamp
            self.__config.wallclock_accuracy = self.__data.iso_service.wallclock_accuracy
            self.__config.isoal_framing = self.__data.iso_service.isoal_framing

        return super().create()

    @log_input(logging.INFO)
    def config(self, **kwargs):
        if CALLBACK_CONSUME in kwargs:
            self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME)
            if not isinstance(self.__config.callback_consume, list):
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))
            if len(self.__config.callback_consume) != 1:
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))

        if self.get_type() == STREAM_TYPE_SOURCE:
            bac_handle = self.__data.iso_service.from_air_handle
        else:
            bac_handle = self.__data.iso_service.to_air_handle

        self.__parameters[HYDRA_BAC_HANDLE] = bac_handle

        for key in self.__parameters:
            self._config_param(key, self.__parameters[key])

        super().config(**kwargs)

        for key in self.__parameters:
            _ = self.query(key)

    @log_input(logging.INFO)
    def start(self):
        """Start streaming

        Notes:
        Before we start streaming:

        - we check if Audio FW is ready to supply or consume data - check_for_channels_ready()
        - start BT clock if Audio ready
        - Audio FW ready is indicated by a 'run state' message which should have
          already been handled by SCO Processing Service

        Raises:
            RuntimeError: - If Audio FW not ready to process data
        """

        self.__data.loop = self.__config.loop
        if self.__config.delay:
            self._log.info('delaying start for %s seconds', self.__config.delay)
            self.__data.loop_timer_id = self.__helper.uut.timer_add_relative(
                self.__config.delay, callback=self._start)
        else:
            self._start(0)

    @log_input(logging.INFO)
    def stop(self):
        """Stop stream"""

        if self.get_type() == STREAM_TYPE_SOURCE:
            self._log.info('packets sent:%s', self.__data.sent_packets)

        if self.__data.loop_timer_id is not None:
            self.__helper.uut.timer_cancel(self.__data.loop_timer_id)
            self.__data.loop_timer_id = None
        if self.__data.source_packetiser:
            self.__data.source_packetiser.stop()
        if self.__data.source_timer_id:
            self.__helper.uut.timer_cancel(self.__data.source_timer_id)
            self.__data.source_timer_id = None
        if self.__data.sink_timer_id:
            self.__helper.uut.timer_cancel(self.__data.sink_timer_id)
            self.__data.sink_timer_id = None

        super().stop()

    @log_input(logging.INFO)
    def destroy(self):

        if self.__config.backing == BACKING_FILE and self.__data.sink_audio_buffer is not None:
            self._log.info('creating file %s', self.__config.filename)
            audio_instance = audio_get_instance(self.__config.filename, 'w', allow_encoded=False)
            audio_instance.add_audio_stream(self.__config.sample_rate,
                                            self.__config.sample_width,
                                            self.__data.sink_audio_buffer)
            audio_instance.write()
            del audio_instance
            self.__data.sink_audio_buffer = []

        # Note that stopping the service will destroy the endpoint associated with it
        # hence we delay stopping the service until we are destroying the stream
        if self.__data.iso_service.check_started():
            self.__data.iso_service.stop()
        super().destroy()

    def check_active(self):
        # FIXME this code assumes that we are connected after a kymera graph
        # if we are not it should be removed
        # a similar thing happens with a2dp and pcm streams
        if self.get_type() == STREAM_TYPE_SOURCE and self.__config.backing == BACKING_DATA:
            return False
        return super().check_active()

    def consume(self, input_num, data):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            data_to_send = self._generate_packet(data)
            if data_to_send:
                self.insert(data_to_send)

    def eof_detected(self, input_num):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            self.eof()

    def get_iso_service(self):
        """Return iso service instance

        Returns:
            HydraIsoProcessingService: iso service
        """
        return self.__data.iso_service

    def get_hci_handle(self):
        """Get hci handle

        Returns:
            int: hci handle
        """
        return self.__data.iso_service.hci_handle
