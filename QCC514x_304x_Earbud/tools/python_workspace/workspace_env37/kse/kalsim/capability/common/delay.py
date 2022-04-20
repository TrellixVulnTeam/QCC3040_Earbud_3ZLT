#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Delay kats operator"""

import argparse
import logging

from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.capability.capability_base import CapabilityBase, STATE_STARTED
from kats.library.registry import get_instance

DELAY = 'delay'
DELAY_DEFAULT = 0.050

PARAM_SCHEMA = {
    'type': 'object',
    'properties': {
        DELAY: {'type': 'number', 'minimum': 0, 'exclusiveMinimum': True,
                'default': DELAY_DEFAULT},
    }
}


class DelayCapability(CapabilityBase):
    """Delay capability

    This capability has one input and one output
    It waits for data to be present in its input (call to consume) and then presents it in its
    output with the selected delay

    - *delay* indicates the delay in seconds between consume and produce,
      (optional default=0.050)

    Args:
        delay (float): Delay in seconds from consume to produce
    """

    platform = 'common'
    interface = 'delay'
    input_num = 1
    output_num = 1

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')

        self.__config = argparse.Namespace()  # configuration values
        self.__config.delay = kwargs.pop(DELAY)

        self.__data = argparse.Namespace()  # data values
        self.__data.pending = []

        super().__init__(*args, **kwargs)

    def _delay_consume_callback(self, timer_id):
        _ = timer_id
        data = self.__data.pending.pop(0)
        self.produce(0, data)

    def _delay_eof_callback(self, timer_id):
        _ = timer_id
        self.dispatch_eof(0)

    def consume(self, input_num, data):
        if input_num == 0 and self.get_state() == STATE_STARTED:
            self.__data.pending.append(data)
            self.__helper.uut.timer_add_relative(self.__config.delay, self._delay_consume_callback)

    def eof_detected(self, input_num):
        if input_num == 0:
            self.__helper.uut.timer_add_relative(self.__config.delay, self._delay_eof_callback)
