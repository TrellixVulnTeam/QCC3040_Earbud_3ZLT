#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra timestamped streams"""

from kats.kalsim.hydra_service.constants import DEVICE_TYPE_TIMESTAMPED
from .audio_data import StreamHydraAudioData, DEVICE_TYPE, METADATA_FORMAT, METADATA_ENABLE, \
    METADATA_FORMAT_TIMESTAMPED


class StreamHydraTimestamped(StreamHydraAudioData):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'timestamped'

    def __init__(self, stream_type, **kwargs):
        kwargs.setdefault(DEVICE_TYPE, DEVICE_TYPE_TIMESTAMPED)
        kwargs.setdefault(METADATA_ENABLE, True)
        kwargs.setdefault(METADATA_FORMAT, METADATA_FORMAT_TIMESTAMPED)
        super().__init__(stream_type, **kwargs)
