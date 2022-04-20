#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra pcm streams"""

from .hw import StreamHw


class StreamPcm(StreamHw):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'pcm'
