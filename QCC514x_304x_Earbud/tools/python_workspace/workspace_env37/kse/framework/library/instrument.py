#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KATS framework Test Base Instrument class"""

from abc import ABC, abstractproperty


class Instrument(ABC):
    """Base Class that every instrument must subclass"""

    @abstractproperty
    def interface(self):
        """str: Instrument name/interface"""

    @abstractproperty
    def schema(self):
        """dict: Instrument schema"""
