#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra a2dp streams"""

import logging

from kats.kalsim.hydra_service.constants import DEVICE_TYPE_L2CAP
from .audio_data import StreamHydraAudioData, DEVICE_TYPE


class StreamHydraA2dp(StreamHydraAudioData):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'a2dp'

    def __init__(self, stream_type, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        kwargs.setdefault(DEVICE_TYPE, DEVICE_TYPE_L2CAP)
        self._log.warning('a2dp streams are obsolete, please use l2cap')
        super().__init__(stream_type, **kwargs)
