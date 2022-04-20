#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Generic SPDIF endpoint class"""

import logging

from kats.core.endpoint_base import EndpointBase
from .mapping import map_endpoint


class EndpointSpdifHydra(EndpointBase):
    """Kymera SPDIF Endpoint

    Args:
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
        endpoint_type (str): Type of endpoint "source" or "sink"
        instance (int): SPDIF instance
        channel (int): Time slot 0 to 1

        spdif_output_rate (int): Transmit sample clock rate in hertzs (only for sink endpoints)

            - 32000
            - 44100
            - 48000
            - 88200
            - 96000
            - 176400
            - 192000

        spdif_chnl_sts_report_mode (int): Specifies the channels for which notification of status
            changes will be sent.
            On BlueCore devices up to CSR8675, this notification is sent to the Kalimba
            DSP as KAL_MSG_SPDIF_CHNL_STS_EVENT. On devices running Kymera, this is sent from the
            SPDIF_DECODE operator to the application as a MessageFromOperator (NEW_CHANNEL_STATUS).

                - 0: No Channel status
                - 1: Channel status A
                - 2: Channel status B
                - 3: Both channels. (Not supported)

        spdif_output_chnl_sts_dup_en (int): Output channel status duplicate enable

            - 0: Channel B carries its own channel status
            - 1: Channel A channel status is duplicated on channel B

        spdif_output_chnl_sts_word (int): 192-bit output channel status word, divided into 12
            words of 16 bits each. Each word can be individually set.

            - Bits [31:16]: channel status word index:

                - 0: Min value
                - 11: Max value
                - Any other value: Entire channel status is made 0.

            - Bits [15:0]: value

        spdif_auto_rate_detect (int): Input auto rate detect enable

            - 0: SPDIF RX rate is not automatically detected
            - 1: SPDIF RX rate is automatically detected and changed (required in most use cases)

        spdif_output_clk_source (int): Output click source selection

            - 0 (SYSTEM_ROOT_CLK): Interface clock derived from fixed internal root
              clock (as on pre-CSRA681xx devices).
            - 1 (MCLK): Interface clock derived from MCLK source configured with
              AudioMasterClockConfigure(). Before the interface can be used,
              Source/SinkMasterClockEnable() must be called per interface.
    """

    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'spdif'

    def __init__(self, kymera, endpoint_type, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        kwargs = map_endpoint(self.interface, endpoint_type, kwargs)
        self._instance = kwargs.pop('instance', 0)
        self._channel = kwargs.pop('channel', 0)

        # initialise values
        self.__args = []
        for entry in args:
            if not isinstance(entry, list):
                raise RuntimeError('arg %s invalid should be a list' % (entry))
            if len(entry) != 2:
                raise RuntimeError('arg %s invalid should be list of 2 elements' % (entry))
            self.__args.append(entry)

        self.__args += list(kwargs.items())

        super().__init__(kymera, endpoint_type)

    def create(self, *_, **__):
        self._create('spdif', [self._instance, self._channel])

    def config(self):

        for entry in self.__args:
            self.config_param(entry[0], entry[1])

        super().config()
