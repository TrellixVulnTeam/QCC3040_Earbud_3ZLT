#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra file streams"""

from kats.kalsim.hydra_service.constants import DEVICE_TYPE_FILE
from .audio_data import StreamHydraAudioData, DEVICE_TYPE


class StreamHydraFile(StreamHydraAudioData):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'file'

    def __init__(self, stream_type, **kwargs):
        kwargs.setdefault(DEVICE_TYPE, DEVICE_TYPE_FILE)
        super().__init__(stream_type, **kwargs)
