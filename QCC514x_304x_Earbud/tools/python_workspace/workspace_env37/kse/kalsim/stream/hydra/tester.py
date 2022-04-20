#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra tester streams"""

from kats.kalsim.hydra_service.constants import DEVICE_TYPE_TESTER
from .audio_data import StreamHydraAudioData, DEVICE_TYPE


class StreamHydraTester(StreamHydraAudioData):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'tester'

    def __init__(self, stream_type, **kwargs):
        kwargs.setdefault(DEVICE_TYPE, DEVICE_TYPE_TESTER)
        super().__init__(stream_type, **kwargs)
