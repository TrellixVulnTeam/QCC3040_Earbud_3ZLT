#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Module for defining KSP data types."""
from enum import Enum


# pylint: disable=too-few-public-methods
class KspDataType(Enum):
    """Definitions of KSP data types."""
    DATA16 = 0   # 16-bit DATA, will be stored big endian.
    PCM16 = 1    # 16-bit PCM, will be stored little endian.
    PCM24 = 2    # 24-bit PCM, will be stored little endian.
    PCM32 = 3    # 32-bit PCM, will be stored little endian.
    DATA32 = 4   # 32-bit DATA, will be stored big endian.
    TTR = 5      # Timing Trace Records

    def __str__(self):
        return self.name

    @property
    def n_octets(self):
        """Returns octets per sample for the data type."""
        if self == KspDataType.DATA16:
            return 2
        if self == KspDataType.PCM16:
            return 2
        if self == KspDataType.PCM24:
            return 3
        if self == KspDataType.PCM32:
            return 4
        if self == KspDataType.DATA32:
            return 4
        if self == KspDataType.TTR:
            return 4
        raise ValueError

    def is_audio_type(self):
        """Determines whether the Data Type is an audio type.

        Returns:
            bool: True if data type is an audio type.
        """
        return self.value not in (
            KspDataType.DATA16,
            KspDataType.DATA32,
            KspDataType.TTR
        )
