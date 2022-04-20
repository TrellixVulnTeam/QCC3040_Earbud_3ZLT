#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""AuraPlus Chip interface."""
import logging

from ksp.lib.chips.chipbase import GenericChip
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


class AuraPlus(GenericChip):
    """KSP Emulator for Aura Plus Chip type.

    Major version:      0x4B
    """
    @method_logger(logger)
    def __init__(self, device):
        super(AuraPlus, self).__init__(device)
