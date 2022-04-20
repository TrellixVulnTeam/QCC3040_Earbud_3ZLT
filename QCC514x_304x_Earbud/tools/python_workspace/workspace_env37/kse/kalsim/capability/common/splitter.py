#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Splitter kats operator"""

import logging

from kats.kalsim.capability.capability_base import CapabilityBase, STATE_STARTED


class SplitterCapability(CapabilityBase):
    """Splitter kats operator

    This capability has one input and four outputs
    It waits for data to be present in its input (call to consume) and then forwards it
    immediately to all of its outputs
    """

    platform = 'common'
    interface = 'splitter'
    input_num = 1
    output_num = 4

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        super().__init__(*args, **kwargs)

    def consume(self, input_num, data):
        if input_num == 0 and self.get_state() == STATE_STARTED:
            for output in range(self.output_num):
                self.produce(output, data)

    def eof_detected(self, input_num):
        if input_num == 0:
            for output in range(self.output_num):
                self.dispatch_eof(output)
