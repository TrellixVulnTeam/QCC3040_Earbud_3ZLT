#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Mora Chip interface."""
import logging

from ksp.lib.chips.chipbase import GenericChip
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


class Mora(GenericChip):
    """KSP Emulator for Mora Chip type.

    Major version:      0x50
    """
    @method_logger(logger)
    def __init__(self, device):
        super(Mora, self).__init__(device)
