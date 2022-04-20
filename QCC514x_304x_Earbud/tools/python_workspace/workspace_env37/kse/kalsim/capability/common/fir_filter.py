#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""FIR KATS operator"""

import argparse
import logging

# NOTE numpy/scipy are not kats requirements, but they are in aaplib (which is a kats requirement)
# This code cannot be released as part of KSE as it does not include numpy, scipy neither directly
# or indirectly
import numpy as np
from scipy import signal

from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.capability.capability_base import CapabilityBase, STATE_STARTED
from kats.kymera.kymera.generic.accmd import ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ
from kats.library.registry import get_instance

NUM_COEFFS = 'num_coeffs'
NUM_COEFFS_DEFAULT = [1]
DEN_COEFFS = 'den_coeffs'
DEN_COEFFS_DEFAULT = [1]
START_GAIN = 'start_gain'
START_GAIN_DEFAULT = 1
UNSOLICITED_MSG_ID = 'unsolicited_msg_id'
UNSOLICITED_MSG_DEFAULT = 0
PAYLOAD_GAIN_IDX = 'payload_gain_idx'
PAYLOAD_GAIN_IDX_DEFAULT = 3

# 4 gives the offset to match the unsolicited message payload index
UNSOLICITED_MSG_OFFSET = 4

PARAM_SCHEMA = {
    'type': 'object',
    'properties': {
        NUM_COEFFS: {'type': 'array', 'default': NUM_COEFFS_DEFAULT},
        DEN_COEFFS: {'type': 'array', 'default': DEN_COEFFS_DEFAULT},
        START_GAIN: {'type': 'number', 'default': START_GAIN_DEFAULT},
        UNSOLICITED_MSG_ID: {'type': 'number', 'default': UNSOLICITED_MSG_DEFAULT},
        PAYLOAD_GAIN_IDX: {'type': 'number', 'default': PAYLOAD_GAIN_IDX_DEFAULT}
    }
}


class FirFilterCapability(CapabilityBase):
    """FIR kats operator

    This capability has one input and one output, and it performs FIR filtering on its input data
    The FIR filter filters based the specified numerator and denominator coefficients.
    If an unsolicited message (gain) is received from the capability,
    it is applied to the filtered output data

    Args:
        num_coeffs (list): numerator coefficients for FIR filter
        den_coeffs (list): denominator coefficients for FIR filter
        start_gain (float): initial gain value
        unsolicited_msg_id (int): message ID for the unsolicited message
        payload_gain_idx (int): index of the gain value in the unsolicited message
    """

    platform = 'common'
    interface = 'fir_filter'
    input_num = 1
    output_num = 1

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')
        self.__helper.accmd = get_instance('accmd')
        self.__helper.accmd.register_receive_callback(self._cb_accmd_receive_message)

        self.__config = argparse.Namespace()  # configuration values
        self.__config.num_coeffs = kwargs.pop(NUM_COEFFS)
        self.__config.den_coeffs = kwargs.pop(DEN_COEFFS)
        self.__config.gain = kwargs.pop(START_GAIN)
        self.__config.msg_id = kwargs.pop(UNSOLICITED_MSG_ID)
        self.__config.gain_idx = kwargs.pop(PAYLOAD_GAIN_IDX)

        self.gain = self.__config.gain

        max_coeffs = max(len(self.__config.num_coeffs), len(self.__config.den_coeffs))
        self.state_filt = np.zeros(max_coeffs - 1)

        super().__init__(*args, **kwargs)

    @log_input(logging.INFO)
    def _cb_accmd_receive_message(self, data):
        """Gain is received as an unsolicited messages from AANC, and
        it is converted from dB value to linear gain and stored.
        """

        cmd_id, _, payload = self.__helper.accmd.receive(data)

        if (cmd_id == ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ and
                payload[1:3] == [0, self.__config.msg_id]):
            gain_val = payload[UNSOLICITED_MSG_OFFSET + self.__config.gain_idx]
            gain_conv = float(gain_val) / 128
            self.gain = gain_conv
            self._log.info('Gain event received: gain set to %d -> %f' % (gain_val, gain_conv))

    def consume(self, input_num, data):
        if input_num == 0 and self.get_state() == STATE_STARTED:
            bp_num_coeffs = self.__config.num_coeffs
            bp_den_coeffs = self.__config.den_coeffs
            filt_data, self.state_filt = signal.lfilter(
                bp_num_coeffs, bp_den_coeffs, data, zi=self.state_filt)
            out = np.round(self.gain * filt_data).astype(int).tolist()
            self.produce(input_num, out)

    def eof_detected(self, input_num):
        if input_num == 0:
            self.dispatch_eof(input_num)
