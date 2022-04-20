#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra i2s streams"""

from .hw import StreamHw


class StreamI2s(StreamHw):
    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'i2s'
