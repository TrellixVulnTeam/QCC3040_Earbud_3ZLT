#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Crescendo Chip interface."""
import logging

from ksp.lib.chips.chipbase import GenericChip
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


class Crescendo(GenericChip):
    """KSP Emulator for Crescendo Chip type.

    Major version:      0x46
    """
    IS_EDKCS = False

    @method_logger(logger)
    def __init__(self, device):
        super(Crescendo, self).__init__(device)
