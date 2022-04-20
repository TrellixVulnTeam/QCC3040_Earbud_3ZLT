#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra l2cap streams"""

from kats.kalsim.hydra_service.constants import DEVICE_TYPE_L2CAP
from .audio_data import StreamHydraAudioData, DEVICE_TYPE


class StreamHydraL2cap(StreamHydraAudioData):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'l2cap'

    def __init__(self, stream_type, **kwargs):
        kwargs.setdefault(DEVICE_TYPE, DEVICE_TYPE_L2CAP)
        super().__init__(stream_type, **kwargs)
