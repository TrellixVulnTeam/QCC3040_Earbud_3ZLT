#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra ISO Processing Services"""

import argparse
import logging
from functools import partial

from kalcmd2 import kalmem_spaces

from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.hydra_service.constants import SERVICE_TYPE_SCO_PROCESSING, \
    TIMER_TIME, SCO_PROC_SERV_RUN_STATE_MSG, SCO_DIR_TO_AIR, SCO_DIR_FROM_AIR, ENDPOINT_TYPE_ISO
from kats.kalsim.library.period import compute_period
from kats.library.registry import get_instance

TIMESLOT_DURATION = 0.000625
TIMESLOT_DURATION_2 = 0.0003125
TIMESLOT_DURATION_US = 625
TIMESLOT_FRACTION = 400
WALLCLOCK_UPDATE_PERIOD = 0.250
DUMMY_ANTI_TEAR_COUNTER = 0x4B0
BT_CLOCK_MAX = 2 ** 28 - 1
ISO_INTERVAL_DURATION = 0.00125

SERVICE_TAG = 'service_tag'
HCI_HANDLE = 'hci_handle'
FROM_AIR_BUFFER_SIZE = 'from_air_buffer_size'
WALLCLOCK_ACCURACY = 'wallclock_accuracy'
WALLCLOCK_HANDLE = 'wallclock_handle'
WALLCLOCK_OFFSET = 'wallclock_offset'
BT_CLOCK_VALUE = 'bt_clock_value'
ENDPOINT_TYPE = 'endpoint_type'
TO_AIR_BUFFER_SIZE = 'to_air_buffer_size'
ISO_STREAM_TYPE = 'iso_stream_type'
ISO_STREAM_TYPE_CIS = 'cis'
ISO_STREAM_TYPE_BIS = 'bis'
ISO_STREAM_TYPE_VALUES = {ISO_STREAM_TYPE_CIS: 0, ISO_STREAM_TYPE_BIS: 1}
ISOAL_FRAMING = 'isoal_framing'
ISOAL_FRAMING_UNFRAMED = 'unframed'
ISOAL_FRAMING_FRAMED = 'framed'
ISOAL_FRAMING_VALUES = {ISOAL_FRAMING_UNFRAMED: 0, ISOAL_FRAMING_FRAMED: 1}
ISO_INTERVAL = 'iso_interval'
SDU_INTERVAL_TO_AIR = 'sdu_interval_to_air'
SDU_INTERVAL_FROM_AIR = 'sdu_interval_from_air'
MAX_SDU_TO_AIR = 'max_sdu_to_air'
MAX_SDU_FROM_AIR = 'max_sdu_from_air'
NEXT_SLOT_TIME = 'next_slot_time'
CIS_SYNC_DELAY = 'cis_sync_delay'
STREAM_SYNC_DELAY = 'stream_sync_delay'
CIG_SYNC_DELAY = 'cig_sync_delay'
GROUP_SYNC_DELAY = 'group_sync_delay'
FLUSH_TIMEOUT_TO_AIR = 'flush_timeout_to_air'
FLUSH_TIMEOUT_FROM_AIR = 'flush_timeout_from_air'
TO_AIR_LATENCY = 'to_air_latency'
FROM_AIR_LATENCY = 'from_air_latency'
SDU_NUMBER = 'sdu_number'
SDU_NUMBER_TO_AIR = 'sdu_number_to_air'
SDU_NUMBER_FROM_AIR = 'sdu_number_from_air'

PARAM_SCHEMA = {
    'type': 'object',
    'required': [SERVICE_TAG, ISO_INTERVAL, CIS_SYNC_DELAY, CIG_SYNC_DELAY],
    'properties': {
        # create service
        SERVICE_TAG: {'type': 'integer', 'minimum': 1},
        HCI_HANDLE: {'type': 'integer', 'minimum': 0},
        FROM_AIR_BUFFER_SIZE: {'type': 'integer', 'minimum': 0, 'default': 0},
        WALLCLOCK_ACCURACY: {'type': 'number', 'default': 0},
        WALLCLOCK_HANDLE: {'type': 'integer', 'minimum': 1, 'default': 0x254},
        WALLCLOCK_OFFSET: {'type': 'integer', 'minimum': 0, 'default': 0},
        BT_CLOCK_VALUE: {'type': 'integer', 'minimum': 0, 'maximum': BT_CLOCK_MAX, 'default': 0},
        ENDPOINT_TYPE: {'type': 'integer', 'minimum': 0, 'default': ENDPOINT_TYPE_ISO},
        TO_AIR_BUFFER_SIZE: {'type': 'integer', 'minimum': 0, 'default': 0},
        ISO_STREAM_TYPE: {'type': ['integer', 'string'], 'default': ISO_STREAM_TYPE_CIS,
                          'enum': [ISO_STREAM_TYPE_CIS, ISO_STREAM_TYPE_BIS, 0, 1]},
        ISOAL_FRAMING: {'type': ['integer', 'string'], 'default': ISOAL_FRAMING_UNFRAMED,
                        'enum': [ISOAL_FRAMING_UNFRAMED, ISOAL_FRAMING_FRAMED, 0, 1]},
        # set_iso_params
        ISO_INTERVAL: {'type': 'integer', 'minimum': 4, 'maximum': 3200},
        SDU_INTERVAL_TO_AIR: {'type': 'integer', 'minimum': 0, 'default': 0},
        SDU_INTERVAL_FROM_AIR: {'type': 'integer', 'minimum': 0, 'default': 0},
        MAX_SDU_TO_AIR: {'type': 'integer', 'minimum': 0, 'default': 0},
        MAX_SDU_FROM_AIR: {'type': 'integer', 'minimum': 0, 'default': 0},
        NEXT_SLOT_TIME: {'type': 'integer', 'minimum': 0, 'default': 0},
        CIS_SYNC_DELAY: {'type': 'integer', 'exclusiveMinimum': 0, },  # kept for backwards compat
        STREAM_SYNC_DELAY: {'type': 'integer', 'exclusiveMinimum': 0, },
        CIG_SYNC_DELAY: {'type': 'integer', 'exclusiveMinimum': 0, },  # kept for backwards compat
        GROUP_SYNC_DELAY: {'type': 'integer', 'exclusiveMinimum': 0, },
        FLUSH_TIMEOUT_TO_AIR: {'type': 'integer', 'minimum': 1, 'default': 1},
        FLUSH_TIMEOUT_FROM_AIR: {'type': 'integer', 'minimum': 1, 'default': 1},
        # TODO give possibility of optional and precomputed
        TO_AIR_LATENCY: {'type': 'integer', 'minimum': 0, 'default': 0},
        FROM_AIR_LATENCY: {'type': 'integer', 'minimum': 0, 'default': 0},
        SDU_NUMBER: {'type': 'integer', 'minimum': 0, 'default': 0},  # kept for backwards compat
        SDU_NUMBER_TO_AIR: {'type': 'integer', 'minimum': 0},
        SDU_NUMBER_FROM_AIR: {'type': 'integer', 'minimum': 0},
    }
}


# pylint: disable=too-many-public-methods
class HydraIsoProcessingService:
    """Hydra ISO Processing Service

    This is a hydra service
    It supports one bidirectional connection, with from air and to air streams.

    Args:
        service_tag (int): Hydra service tag. Every service including non iso processing services
            should have a unique tag
        hci_handle (int): Host Controller Interface handle. This should be a unique number among all
            sco/iso processing services. This number is used to create sco endpoints, if not
            provided then service_tag value will be used
        from_air_buffer_size (int): from-air buffer size in octets, 0 for from air direction unused
        wallclock_accuracy (float): Wallclock simulation accuracy in parts per million.
        wallclock_handle (int): Wallclock MMU handle, dummy handle, any non zero value is valid
        wallclock_offset (int): Wallclock offset in bytes
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
    """

    def __init__(self, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')
        self.__helper.hydra_prot = get_instance('hydra_protocol')

        self.__config = argparse.Namespace()  # configuration values
        self.__config.service_tag = kwargs.pop(SERVICE_TAG)
        self.__config.hci_handle = kwargs.pop(HCI_HANDLE, self.__config.service_tag)
        self.__config.from_air_buffer_size = kwargs.pop(FROM_AIR_BUFFER_SIZE)
        self.__config.wallclock_accuracy = kwargs.pop(WALLCLOCK_ACCURACY)
        self.__config.wallclock_handle = kwargs.pop(WALLCLOCK_HANDLE)
        self.__config.wallclock_offset = kwargs.pop(WALLCLOCK_OFFSET)
        self.__config.bt_clock_value = kwargs.pop(BT_CLOCK_VALUE)
        self.__config.endpoint_type = kwargs.pop(ENDPOINT_TYPE)
        self.__config.to_air_buffer_size = kwargs.pop(TO_AIR_BUFFER_SIZE)
        iso_stream_type = kwargs.pop(ISO_STREAM_TYPE)
        self.__config.iso_stream_type = ISO_STREAM_TYPE_VALUES.get(iso_stream_type, iso_stream_type)
        isoal_framing = kwargs.pop(ISOAL_FRAMING)
        self.__config.isoal_framing = ISOAL_FRAMING_VALUES.get(isoal_framing, isoal_framing)

        self.__config.iso_interval = kwargs.pop(ISO_INTERVAL)
        self.__config.sdu_interval_to_air = kwargs.pop(SDU_INTERVAL_TO_AIR)
        self.__config.sdu_interval_from_air = kwargs.pop(SDU_INTERVAL_FROM_AIR)
        self.__config.max_sdu_to_air = kwargs.pop(MAX_SDU_TO_AIR)
        self.__config.max_sdu_from_air = kwargs.pop(MAX_SDU_FROM_AIR)
        self.__config.next_slot_time = kwargs.pop(NEXT_SLOT_TIME)
        if STREAM_SYNC_DELAY in kwargs:
            self.__config.stream_sync_delay = kwargs.pop(STREAM_SYNC_DELAY)
        else:
            self.__config.stream_sync_delay = kwargs.pop(CIS_SYNC_DELAY)
        if GROUP_SYNC_DELAY in kwargs:
            self.__config.group_sync_delay = kwargs.pop(GROUP_SYNC_DELAY)
        else:
            self.__config.group_sync_delay = kwargs.pop(CIG_SYNC_DELAY)
        self.__config.flush_timeout_to_air = kwargs.pop(FLUSH_TIMEOUT_TO_AIR)
        self.__config.flush_timeout_from_air = kwargs.pop(FLUSH_TIMEOUT_FROM_AIR)
        self.__config.to_air_latency = kwargs.pop(TO_AIR_LATENCY)
        self.__config.from_air_latency = kwargs.pop(FROM_AIR_LATENCY)
        sdu_number = kwargs.pop(SDU_NUMBER)
        self.__config.sdu_number_to_air = kwargs.pop(SDU_NUMBER_TO_AIR, sdu_number)
        self.__config.sdu_number_from_air = kwargs.pop(SDU_NUMBER_FROM_AIR, sdu_number)

        # calculate/update other parameters
        self.__config.wallclock_period = \
            WALLCLOCK_UPDATE_PERIOD * (1 + self.__config.wallclock_accuracy / 1e6)

        self.__data = argparse.Namespace()  # data values
        self.__data.started = False  # service has not been started
        self.__data.channel_ready = [False, False]  # none of the possible channels are ready yet
        self.__data.bt_clock = 0  # bluetooth wallclock (2*slot)
        self.__data.bt_clock_timestamp = 0  # timer time when bt_clock has last been updated
        self.__data.timestamp_start = 0  # timer time when we expect to start
        self.__data.wallclock_remain = 0  # remainder for bluetooth clock (usec units)
        self.__data.wallclock_timer_id = None
        self.__data.to_air_read_handle = None
        self.__data.to_air_write_handle = None
        self.__data.to_air_aux_handle = None
        self.__data.from_air_read_handle = None
        self.__data.from_air_write_handle = None
        self.__data.from_air_aux_handle = None
        self.__data.msg_handler = self.__helper.hydra_prot.install_message_handler(
            self._message_received)

        if kwargs:
            self._log.warning('unknown kwargs:%s', str(kwargs))

    @log_input(logging.INFO, formatters={'msg': '0x%04x'})
    def _message_received(self, msg):
        """Hydra message sniffer. Waits for SCO_PROC_SERV_RUN_STATE_MSG messages to detect when
        streams are ready

        Args:
            msg (list[int]): Message received
        """
        if len(msg) == 4 and \
                msg[0] == SCO_PROC_SERV_RUN_STATE_MSG and \
                msg[1] == self.__config.service_tag:

            direction = msg[2]
            if direction == SCO_DIR_TO_AIR:
                self.__data.channel_ready[0] = True
                frame_length = msg[3]
                self._log.info('iso service to air ready frame_length:%s', frame_length)
            elif direction == SCO_DIR_FROM_AIR:
                self.__data.channel_ready[1] = True
                frame_length = msg[3]
                self._log.info('iso service from air ready frame_length:%s', frame_length)

    def _wallclock_update_callback(self, timer_id):
        _ = timer_id
        self.__data.wallclock_timer_id = None

        self.__data.bt_clock += 2 * TIMESLOT_FRACTION
        self.__data.bt_clock %= BT_CLOCK_MAX + 1
        self._update_wallclock(self.__data.bt_clock)

        period, self.__data.wallclock_remain = compute_period(
            self.__config.wallclock_period, self.__data.wallclock_remain)
        self.__data.wallclock_timer_id = self.__helper.uut.timer_add_relative(
            period, self._wallclock_update_callback)

    def _update_wallclock(self, bt_clock):
        """Update wall clock

        Args:
            bt_clock (int): Bluetooth clock value, this clock is half a slot 312.5 usec
        """
        self.__data.bt_clock_timestamp = self.__helper.uut.timer_get_time()
        self._log.info('_update_wallclock bt_clock:%s time:%s',
                       bt_clock, self.__data.bt_clock_timestamp)
        time_stamp = self.__helper.uut.mem_peek(1, TIMER_TIME, 4)
        mem_space = kalmem_spaces.BAC_WINDOW_2
        offset = self.__config.wallclock_offset

        self.__helper.uut.mem_poke(mem_space, offset + 0x0000, 2, DUMMY_ANTI_TEAR_COUNTER)
        self.__helper.uut.mem_poke(mem_space, offset + 0x0002, 2, (bt_clock >> 16) & 0xFFFF)
        self.__helper.uut.mem_poke(mem_space, offset + 0x0004, 2, (bt_clock & 0xFFFF))
        self.__helper.uut.mem_poke(mem_space, offset + 0x0006, 2, (time_stamp >> 16) & 0xFFFF)
        self.__helper.uut.mem_poke(mem_space, offset + 0x0008, 2, time_stamp & 0xFFFF)

    @staticmethod
    def _start_channel_callback(callback, timer_id):
        _ = timer_id
        callback()

    @property
    def service_tag(self):
        """int: ISO service tag"""
        return self.__config.service_tag

    @property
    def hci_handle(self):
        """int: HCI handle"""
        return self.__config.hci_handle

    @property
    def from_air_buffer_size(self):
        """int: From air buffer size in octets"""
        return self.__config.from_air_buffer_size

    @property
    def wallclock_accuracy(self):
        """float: Wallclock accuracy in ppm"""
        return self.__config.wallclock_accuracy

    @property
    def wallclock_handle(self):
        """int: Wallclock handle"""
        return self.__config.wallclock_handle

    @property
    def wallclock_offset(self):
        """int: Wallclock offset"""
        return self.__config.wallclock_offset

    @property
    def bt_clock_value(self):
        """int: BT clock initial value"""
        return self.__config.bt_clock_value

    @property
    def endpoint_type(self):
        """int: Endpoint type"""
        return self.__config.endpoint_type

    @property
    def to_air_buffer_size(self):
        """int: To air buffer size in octets"""
        return self.__config.to_air_buffer_size

    @property
    def iso_stream_type(self):
        """int: ISO stream type"""
        return self.__config.iso_stream_type

    @property
    def isoal_framing(self):
        """int: isoal framing stream type"""
        return self.__config.isoal_framing

    @property
    def iso_interval(self):
        """int: ISO interval in 1.25 msec units"""
        return self.__config.iso_interval

    @property
    def iso_interval_sec(self):
        """float: iso interval in seconds"""
        return (self.__config.iso_interval * 1.25) / 1000.0

    @property
    def sdu_interval_to_air(self):
        """int: SDU interval to air in usecs"""
        return self.__config.sdu_interval_to_air

    @property
    def sdu_interval_to_air_sec(self):
        """float: Get sdu interval to air parameter in seconds"""
        return self.__config.sdu_interval_to_air / 1e6

    @property
    def sdu_interval_from_air(self):
        """int: SDU interval from air in usecs"""
        return self.__config.sdu_interval_from_air

    @property
    def sdu_interval_from_air_sec(self):
        """float: Get sdu interval from air parameter in seconds"""
        return self.__config.sdu_interval_from_air / 1e6

    @property
    def max_sdu_to_air(self):
        """int: Max SDU to air in octets"""
        return self.__config.max_sdu_to_air

    @property
    def max_sdu_from_air(self):
        """int: Max SDU from air in octets"""
        return self.__config.max_sdu_from_air

    @property
    def next_slot_time(self):
        """int: Next slot time in bt ticks"""
        return self.__config.next_slot_time

    @property
    def stream_sync_delay(self):
        """int: CIS/BIS synchronisation delay in usec"""
        return self.__config.stream_sync_delay

    @property
    def group_sync_delay(self):
        """int: CIG/BIG synchronisation delay in usec"""
        return self.__config.group_sync_delay

    @property
    def flush_timeout_to_air(self):
        """int: Flush timeout to air in iso interval units"""
        return self.__config.flush_timeout_to_air

    @property
    def flush_timeout_from_air(self):
        """int: Flush timeout from air in iso interval units"""
        return self.__config.flush_timeout_from_air

    @property
    def to_air_latency(self):
        """int: To air latency in usecs"""
        return self.__config.to_air_latency

    @property
    def from_air_latency(self):
        """int: From air latency in usecs"""
        return self.__config.from_air_latency

    @property
    def sdu_number_to_air(self):
        """int: Initial to air sdu number"""
        return self.__config.sdu_number_to_air

    @property
    def sdu_number_from_air(self):
        """int: Initial from air sdu number"""
        return self.__config.sdu_number_from_air

    @property
    def from_air_handle(self):
        """int: Get ISO processing service from air bac handle"""
        return self.__data.from_air_aux_handle

    @property
    def from_air_read_handle(self):
        """int: Get ISO processing service from air read bac handle"""
        return self.__data.from_air_read_handle

    @property
    def from_air_write_handle(self):
        """int: Get ISO processing service from air write bac handle"""
        return self.__data.from_air_write_handle

    @property
    def to_air_handle(self):
        """int: Get ISO processing service to air bac handle"""
        return self.__data.to_air_aux_handle

    @property
    def to_air_read_handle(self):
        """int: Get ISO processing service to air read bac handle"""
        return self.__data.to_air_read_handle

    @property
    def to_air_write_handle(self):
        """int: Get ISO processing service to air read bac handle"""
        return self.__data.to_air_write_handle

    def start(self):
        """Start ISO processing service

        Raises:
            RuntimeError: - If the service has already been started
                          - The response payload length is incorrect
                          - The response service tag doesn't match expected value
        """
        if self.__data.started:
            raise RuntimeError('service already started')

        msg = [
            self.__config.service_tag,
            self.__config.hci_handle,
            self.__config.from_air_buffer_size,
            0,  # air_compression_factor is not used in iso
            self.__config.wallclock_handle,
            self.__config.wallclock_offset,
            self.__config.endpoint_type,
            self.__config.to_air_buffer_size,
            self.__config.iso_stream_type,
            self.__config.isoal_framing,
        ]

        service_tag, payload = self.__helper.hydra_prot.start_service(
            SERVICE_TYPE_SCO_PROCESSING, msg)
        if len(payload) < 6:
            raise RuntimeError('start response length:%s invalid' % (len(payload)))
        if service_tag != self.__config.service_tag:
            raise RuntimeError('start response service tag:%s invalid' % (service_tag))

        self.__data.started = True
        self.__data.to_air_read_handle = payload[0]
        self.__data.to_air_write_handle = payload[1]
        self.__data.to_air_aux_handle = payload[2]
        self.__data.from_air_read_handle = payload[3]
        self.__data.from_air_write_handle = payload[4]
        self.__data.from_air_aux_handle = payload[5]

        # start wall clock simulation
        self.__data.bt_clock = self.__config.bt_clock_value
        self._update_wallclock(self.__data.bt_clock)

        self.__data.wallclock_remain = 0
        period, self.__data.wallclock_remain = compute_period(
            self.__config.wallclock_period, self.__data.wallclock_remain)
        self.__data.wallclock_timer_id = self.__helper.uut.timer_add_relative(
            period, self._wallclock_update_callback)

    @log_input(logging.INFO)
    def config(self):
        """Configure ISO processing service"""
        if not self.__data.started:
            raise RuntimeError('set_iso_parameters iso service not started')

        timestamp = self.__helper.uut.timer_get_time()
        timestamp_clock = self.__data.bt_clock_timestamp
        bt_clock = self.__data.bt_clock + (timestamp - timestamp_clock) / TIMESLOT_DURATION_2

        # timestamp when we expect to start streaming (if possible)
        self.__data.timestamp_start = timestamp
        self.__data.timestamp_start += self.__config.from_air_latency * 1e-6
        self.__data.timestamp_start += self.__config.next_slot_time / TIMESLOT_DURATION_2
        self.__data.timestamp_start *= (1e6 + self.__config.wallclock_accuracy) / 1e6

        # next_slot_time is the bt clock when we expect to start streaming (if possible)
        self.__config.next_slot_time = int(round(
            bt_clock +
            self.__config.from_air_latency * 1e-6 / TIMESLOT_DURATION_2 +
            self.__config.next_slot_time))

        self._log.info(
            'iso service set iso params tstamp:%f bt_clock:%d desired next_slot_time:%s tstamp:%f',
            timestamp, bt_clock, self.__config.next_slot_time, self.__data.timestamp_start)

        kwargs = {
            'iso_interval': self.__config.iso_interval,
            'sdu_interval_to_air': self.__config.sdu_interval_to_air,
            'sdu_interval_from_air': self.__config.sdu_interval_from_air,
            'max_sdu_to_air': self.__config.max_sdu_to_air,
            'max_sdu_from_air': self.__config.max_sdu_from_air,
            'next_slot_time': self.__config.next_slot_time,
            'stream_sync_delay': self.__config.stream_sync_delay,
            'group_sync_delay': self.__config.group_sync_delay,
            'flush_timeout_to_air': self.__config.flush_timeout_to_air,
            'flush_timeout_from_air': self.__config.flush_timeout_from_air,
            'to_air_latency': self.__config.to_air_latency,
            'from_air_latency': self.__config.from_air_latency,
            'sdu_number_to_air': self.__config.sdu_number_to_air,
            'sdu_number_from_air': self.__config.sdu_number_from_air,
        }
        self.__helper.hydra_prot.set_iso_params(self.__config.service_tag, **kwargs)

    def stop(self):
        """Stop ISO processing service

        Raises:
            RuntimeError: If the service as not been previously started
        """
        if not self.__data.started:
            raise RuntimeError('stop iso service not started')

        if self.__data.wallclock_timer_id:
            self.__helper.uut.timer_cancel(self.__data.wallclock_timer_id)
            self.__data.wallclock_timer_id = None

        self.__helper.hydra_prot.stop_service(SERVICE_TYPE_SCO_PROCESSING,
                                              self.__config.service_tag)
        self.__data.started = False

    def check_started(self):
        """Check if the ISO processing service is started

        Returns:
            bool: Service already started
        """
        return self.__data.started

    def get_buffer_stats(self):
        """Get buffer statistics

        Returns:
            tuple:
                int: Used bytes
                int: Free bytes
        """
        return self.__helper.hydra_prot.get_buffer_stats()

    @log_input(logging.INFO)
    def start_channel(self, channel, callback):
        """Request callback to be invoked when a given channel is ready to start streaming

        Args:
            channel (int): 0 for to air, 1 for from air
            callback (func): Callback to be called when the service is ready to start streaming
        """
        # verify channel is ready to start
        if not self.__data.channel_ready[channel]:
            raise RuntimeError('start channel:%s channel not ready' % (channel))

        # get timestamp
        timestamp = self.__helper.uut.timer_get_time()
        timestamp_start = self.__data.timestamp_start
        self._log.info('iso service start_channel timestamp:%f timestamp_start:%f',
                       timestamp, timestamp_start)

        timestamp_delta = timestamp - timestamp_start
        if timestamp_delta > 0:
            interval = 1 + int(timestamp_delta /
                               (self.__config.iso_interval * ISO_INTERVAL_DURATION))
            self._log.info(
                'iso service start_channel timestamp:%f already passed, adding iso intervals:%d',
                timestamp_start, interval)
            delta = interval * (self.__config.iso_interval * ISO_INTERVAL_DURATION)
            delta *= (1e6 + self.__config.wallclock_accuracy) / 1e6
            timestamp_start += delta

            # update sdu to air initial value accounting for the delay
            if self.__config.sdu_interval_to_air:
                self.__config.sdu_number_to_air += int(
                    interval * self.__config.iso_interval * ISO_INTERVAL_DURATION / (
                            self.__config.sdu_interval_to_air * 1e-6))

            # update sdu from air initial value accounting for the delay
            if self.__config.sdu_interval_from_air:
                self.__config.sdu_number_from_air += int(
                    interval * self.__config.iso_interval * ISO_INTERVAL_DURATION / (
                            self.__config.sdu_interval_from_air * 1e-6))

        self._log.info('iso service start_channel start timestamp:%f', timestamp_start)
        _ = self.__helper.uut.timer_add_absolute(
            timestamp_start, partial(self._start_channel_callback, callback))
