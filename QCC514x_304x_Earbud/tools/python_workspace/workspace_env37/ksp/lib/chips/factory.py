#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Chip factory."""
import logging

from ksp.lib.chips import aura, auraplus, crescendo, mora
from ksp.lib.logger import function_logger

logger = logging.getLogger(__name__)

# Keys are Major versions and values are their corresponding objects.
CHIPS = {
    '0x49': aura.Aura,
    '0x4b': auraplus.AuraPlus,
    '0x46': crescendo.Crescendo,
    '0x50': mora.Mora
}


@function_logger(logger)
def chip_factory(device):
    """Creates a relevant chip instance.

    Args:
        device (object): A device instance from pydbg tool.
    """
    chip_version = device.chip.curator_subsystem.core.data[0xFE81]
    major_version = hex(chip_version & 0x00FF).lower()

    chip = CHIPS.get(major_version)
    if chip:
        return chip(device)

    raise NotImplementedError(
        "Unsupported chip: Major Version={}".format(major_version)
    )
