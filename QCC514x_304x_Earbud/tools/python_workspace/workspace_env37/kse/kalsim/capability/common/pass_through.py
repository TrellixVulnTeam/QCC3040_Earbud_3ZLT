#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Pass through kats operator"""

import logging

from kats.kalsim.capability.capability_base import CapabilityBase, STATE_STARTED


class PassThroughCapability(CapabilityBase):
    """Pass through kats operator

    This capability has one input and one output.
    It waits for data to be present in its input (call to consume) and then forwards it
    immediately to its output. It does not do any rate matching or packing or anything
    """

    platform = 'common'
    interface = 'pass_through'
    input_num = 1
    output_num = 1

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        super().__init__(*args, **kwargs)

    def consume(self, input_num, data):
        if input_num == 0 and self.get_state() == STATE_STARTED:
            self.produce(input_num, data)

    def eof_detected(self, input_num):
        if input_num == 0:
            self.dispatch_eof(input_num)
