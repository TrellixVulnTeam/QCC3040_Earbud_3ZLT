#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Sum KATS operator"""

import logging

from kats.kalsim.capability.capability_base import CapabilityBase, STATE_STARTED


class SumCapability(CapabilityBase):
    """Sum KATS operator

    This capability has two inputs and one output.
    It expects data to be received first in the first channel and then in the second channel.
    As soon as data is received on second channel, it sums the input data and produces output data.
    The size of input data for both channels has to be the same across the whole session.
    """

    platform = 'common'
    interface = 'sum'
    input_num = 2
    output_num = 1

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self.input0 = []
        self.input1 = []
        super().__init__(*args, **kwargs)

    def consume(self, input_num, data):
        if input_num == 0 and self.get_state() == STATE_STARTED:
            self.input0 = data
        if input_num == 1 and self.get_state() == STATE_STARTED:
            self.input1 = data
            sum_in = [p + q for p, q in zip(self.input0, self.input1)]
            self.produce(0, sum_in)

    def eof_detected(self, input_num):
        if input_num in [0, 1]:
            self.dispatch_eof(0)
