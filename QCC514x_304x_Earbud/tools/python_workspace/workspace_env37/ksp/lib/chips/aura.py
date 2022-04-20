#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Aura Chip interfaces."""
import logging

from ksp.lib.chips.chipbase import GenericChip
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


class Aura(GenericChip):
    """KSP Emulator for Aura Chip type.

    Major version:      0x49
    """
    @method_logger(logger)
    def __init__(self, device):
        super(Aura, self).__init__(device)
