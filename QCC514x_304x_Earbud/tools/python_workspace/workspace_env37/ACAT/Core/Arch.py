############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Architecture Information.

Module holding the chip architecture related data.
"""
import logging

from ACAT.Core.exceptions import ChipNotSet, NotPmRegion, NotDmRegion

# pylint: disable=invalid-name
# All the name are kept lower case because this is a special module which only
# holds some data.

# There are places where we magically need to know, say, whether an address
# is in the DM region, BAC, MMR, or whatever. This module is used to
# perform the lookup.
# Like CoreUtils and CoreTypes, all the variables and functions are
# globally-scoped.  It's easier than putting them all into a singleton
# class or passing an object around between all the analyses.

# Information about the chip we're analysing.
# This is provided so that any component can query whether the chip
# is (e.g.) KAL_ARCH_3 or KAL_ARCH_5.
# (Both Chipdata and Debuginfo probably know this already, but
# this provides a common format).
# Set by a call to chip_select.
# * kal_arch is an integer, e.g. 3, 4, 5
# * chip_arch is a string containing e.g. "Bluecore" and "Hydra"
# * chip_id is either an integer representing the chip id, e.g. 28
#   (see http://wiki/ChipVersionIDs), or a string representing the
#   internal name of the chip (e.g. "crescendo")
kal_arch = 0
chip_arch = None
chip_id = 0
chip_cpu_speed_mhz = 0
addr_per_word = 0
chip_revision = 0

logger = logging.getLogger(__name__)

# There's *probably* enough information in the elf 'section hdrs' to
# distinguish the different PM and DM regions, so we should really use
# that. There are two problems with that though:
#  - The section information would only be available
#    to components with access to debuginfo.
#    This rules out Chipdata (specifically, LiveSpi),
#    which sometimes wants to know whether
#    (e.g.) a PM address is in RAM.
#  - The names of sections/regions are not guaranteed
#    to remain constant, and certainly will
#    vary between Bluecore/HydraCore chips. So
#    we'd need some kind of chip-specific code here anyway.
# These need to be set via a call to chip_select()
dRegions = None
pRegions = None

# Due to private information not accessible some ROM builds need to 
# access certain variables via hardcoded address, here we record the 
# build ids that need this information, as well addresses and structrures.
ROM_BUILD_INFO = {
    'qcc515x': {
        'build_id': 11639,
        'L_pm_reserved_size': 0x4b60,
        'pm_block_type': {'PM_BLOCK_P0': 0,
                          'PM_BLOCK_P1': 1,
                          'NUMBER_PM_BLOCKS': 2},
        'pm_heap_block': [
            {
                "start_addr": 0x00004b64,
                "end_addr": 0x00004b64 + 4,
                "offset": 0x00004b64 + 8},
            {
                "start_addr": 0x00004b64 + 12,
                "end_addr": 0x00004b64 + 16,
                "offset": 0x00004b64 + 20}],
        'freelist_pm': [0x00004b7c, 0x00004b7c + 4],
        'magic_offset': 1
    },
    'qcc516x': {
        'build_id': 12510,
        'L_pm_reserved_size': 0x4c68,
        'pm_block_type': {'PM_BLOCK_P0': 0,
                          'PM_BLOCK_P1': 1,
                          'NUMBER_PM_BLOCKS': 2},
        'pm_heap_block': [
            {
                "start_addr": 0x00004c6c,
                "end_addr": 0x00004c6c + 4,
                "offset": 0x00004c6c + 8},
            {
                "start_addr": 0x000004c6c + 12,
                "end_addr": 0x000004c6c + 16,
                "offset": 0x000004c6c + 20}],
        'freelist_pm': [0x00004c84, 0x00004c84 + 4],
        'magic_offset': 1
    }
}


def chip_clear():
    """Resets the internal global architecture related variables."""
    global dRegions, pRegions, kal_arch, \
        chip_arch, chip_id, chip_cpu_speed_mhz, \
        addr_per_word, chip_revision
    kal_arch = 0
    chip_arch = None
    chip_id = 0
    chip_cpu_speed_mhz = 0
    addr_per_word = 0
    chip_revision = 0

    dRegions = None
    pRegions = None


dRegions_Crescendo_d01 = {
    'DM1RAM': (0x00000000, 0x00080000),
    # The view from DM; used for constants right now.
    'PMRAM': (0x00100000, 0x00130000),
    'BAC': (0x00800000, 0x00C00000),
    # 'DMFLASHWIN1_LARGE_REGION' (note name confusion)
    'NVMEM0': (0xf8000000, 0xf8800000),
    # 'DMFLASHWIN2_LARGE_REGION' (note name confusion)
    'NVMEM1': (0xf8800000, 0xf9000000),
    'NVMEM2': (0xf9000000, 0xf9800000),
    'NVMEM3': (0xf9800000, 0xfa000000),
    'DMSRAM': (0xfa000000, 0xfa800000),  # SRAM
    # Lots more NVMEM windows here...
    'DEBUG': (0x13500000, 0x13600000),
    'DBG_PTCH': (0x14500000, 0x14600000),
    'DBG_DWL': (0x15500000, 0x15600000),
    'DM2RAM': (0xfff00000, 0xfff80000),  # Aliased to DM1.
    'MMR': (0xffff8000, 0x100000000)  # Memory-mapped registers
}

dRegions_Aura = {
    'DM1RAM': (0x00000000, 0x00040000),
    # The view from DM; used for constants right now.
    'PMRAM': (0x00100000, 0x00114000),
    'BAC': (0x00800000, 0x00C00000),
    # 'DMFLASHWIN1_LARGE_REGION' (note name confusion)
    'NVMEM0': (0xf8000000, 0xf8800000),
    # 'DMFLASHWIN2_LARGE_REGION' (note name confusion)
    'NVMEM1': (0xf8800000, 0xf9000000),
    'NVMEM2': (0xf9000000, 0xf9800000),
    'NVMEM3': (0xf9800000, 0xfa000000),
    'DMSRAM': (0xfa000000, 0xfa800000),  # SRAM
    # Lots more NVMEM windows here...
    'DEBUG': (0x13500000, 0x13600000),
    'DBG_PTCH': (0x14500000, 0x14600000),
    'DBG_DWL': (0x15500000, 0x15600000),
    'DM2RAM': (0xfff00000, 0xfff40000),  # Aliased to DM1.
    'MMR': (0xffff8000, 0x100000000)  # Memory-mapped registers
}

dRegions_AuraPlus = {
    'DM1RAM': (0x00000000, 0x00070000),
    # The view from DM; used for constants right now.
    'PMRAM': (0x00100000, 0x0011C000),
    'BAC': (0x00800000, 0x00C00000),
    # 'DMFLASHWIN1_LARGE_REGION' (note name confusion)
    'NVMEM0': (0xf8000000, 0xf8800000),
    # 'DMFLASHWIN2_LARGE_REGION' (note name confusion)
    'NVMEM1': (0xf8800000, 0xf9000000),
    'NVMEM2': (0xf9000000, 0xf9800000),
    'NVMEM3': (0xf9800000, 0xfa000000),
    'NVMEM4': (0xfa000000, 0xfa800000),
    'NVMEM5': (0xfa800000, 0xfb000000),
    'NVMEM6': (0xfb000000, 0xfb800000),
    'NVMEM7': (0xfb800000, 0xfc000000),
    # Lots more NVMEM windows here...
    'DEBUG': (0x13500000, 0x13600000),
    'DBG_PTCH': (0x14500000, 0x14600000),
    'DBG_DWL': (0x15500000, 0x15600000),
    'DM2RAM': (0xfff00000, 0xfff70000),  # Aliased to DM1.
    'MMR': (0xffff8000, 0x100000000)  # Memory-mapped registers
}

dRegions_Mora = {
    'DM1RAM': (0x00000000, 0x00160000),
    'DM_AS_PM': (0x0150000, 0x0160000),
    # The view from DM; used for constants right now.
    'PMRAM': (0x00400000, 0x00460000),
    'BAC': (0x00800000, 0x00C00000),
    # 'DMFLASHWIN1_LARGE_REGION' (note name confusion)
    'NVMEM0': (0xf8000000, 0xf8800000),
    # 'DMFLASHWIN2_LARGE_REGION' (note name confusion)
    'NVMEM1': (0xf8800000, 0xf9000000),
    'NVMEM2': (0xf9000000, 0xf9800000),
    'NVMEM3': (0xf9800000, 0xfa000000),
    'NVMEM4': (0xfa000000, 0xfa800000),
    'NVMEM5': (0xfa800000, 0xfb000000),
    'NVMEM6': (0xfb000000, 0xfb800000),
    'NVMEM7': (0xfb800000, 0xfc000000),
    # Lots more NVMEM windows here...
    'DEBUG': (0x13500000, 0x13600000),
    'DBG_PTCH': (0x14500000, 0x14600000),
    'DBG_DWL': (0x15500000, 0x15600000),
    'DM2RAM': (0xff000000, 0xff160000),  # Aliased to DM1.
    'MMR': (0xffff8000, 0x100000000)  # Memory-mapped registers
}

# PM regions
pRegions_Crescendo = {
    'PMCACHE': (0, 0),  # Doesn't seem to be one at the moment
    'PMROM': (0x00000000, 0x02000000),
    # No defined region for SLT at the moment
    'SLT': (0x00000020, 0x00000028),
    'PMRAM': (0x04000000, 0x04030000)
}

pRegions_Aura = {
    'PMCACHE': (0, 0),  # Doesn't seem to be one at the moment
    'PMROM': (0x00000000, 0x02000000),
    'SLT': (0x00000020, 0x00000028),
    'PMRAM': (0x04000000, 0x04014000)
}

pRegions_AuraPlus = {
    'PMCACHE': (0, 0),  # Assume that the PM cache is disabled
    'PMROM': (0x00000000, 0x02000000),
    'SLT': (0x00000020, 0x00000028),
    'PMRAM': (0x04000000, 0x0401C000)
}

pRegions_Mora = {
    'PMCACHE': (0, 0),  # Assume that the PM cache is disabled
    'PMROM': (0x00000000, 0x02000000),
    'SLT': (0x00000020, 0x00000028),
    'PMRAM': (0x04000000, 0x04060000),
    # This 
    'DM_AS_PM': (0x01150000, 0x01160000)
}


#####################
# Utility functions #
#####################


def chip_select(l_kal_arch, l_chip_arch, l_chip_id, l_chip_revision=None):
    """Sets dRegions and pRegions, based on chip architecture/ID.

     * ``l_kal_arch`` is an integer, e.g. 3, 4, 5
     * ``l_chip_arch`` is a string containing e.g. ``Bluecore`` and ``Hydra``.
     * ``l_chip_id`` is an integer representing the chip id, e.g. 28
       (see http://wiki/ChipVersionIDs)

    This function must be called prior to any calls to get_dm_region or
    get_pm_region.

    Args:
        l_kal_arch (int)
        l_chip_arch (str)
        l_chip_id (int)
        l_chip_revision
    """
    # pylint: disable=global-statement
    # pylint: disable=too-many-statements
    # At this stage, the chip_id isn't necessary to distinguish between
    # the chips we support, but eventually it probably will be.
    global dRegions, pRegions, kal_arch, \
        chip_arch, chip_id, chip_cpu_speed_mhz, \
        addr_per_word, chip_revision

    kal_arch = l_kal_arch
    chip_arch = l_chip_arch
    chip_id = l_chip_id
    chip_revision = l_chip_revision

    old_dRegions = dRegions
    old_pRegions = pRegions

    if (kal_arch == 4 and chip_arch == "Hydra" and
            chip_id == 0x46 and chip_revision > 0x1F):
        logger.warning(
            "Crescendo is a deprecated chip. Not all the analyses may work on "
            "this version of ACAT. Please use ACAT versions prior to 1.11.0!"
        )
        # Crescendo d01
        dRegions = dRegions_Crescendo_d01
        pRegions = pRegions_Crescendo
        chip_cpu_speed_mhz = 240
        addr_per_word = 4
    elif kal_arch == 4 and chip_arch == "Hydra" and chip_id == 0x49:
        # Aura
        dRegions = dRegions_Aura
        pRegions = pRegions_Aura
        chip_cpu_speed_mhz = 120
        addr_per_word = 4
    elif kal_arch == 4 and chip_arch == "Hydra" and chip_id == 0x4A:
        # Stretto uses the same memory map as Aura.
        dRegions = dRegions_Aura
        pRegions = pRegions_Aura
        chip_cpu_speed_mhz = 120
        addr_per_word = 4
    elif kal_arch == 4 and chip_arch == "Hydra" and chip_id == 0x4b:
        # AuraPlus
        dRegions = dRegions_AuraPlus
        pRegions = pRegions_AuraPlus
        chip_cpu_speed_mhz = 120
        addr_per_word = 4
    elif kal_arch == 4 and chip_arch == "Hydra" and chip_id == 0x4c:
        # StrettoPlus uses the same memory map as AuraPlus.
        dRegions = dRegions_AuraPlus
        pRegions = pRegions_AuraPlus
        chip_cpu_speed_mhz = 120
        addr_per_word = 4
    elif (kal_arch == 4 and chip_arch == "Hydra" and
          chip_id == 0x50 and chip_revision < 0x20):
        # Mora
        dRegions = dRegions_Mora
        pRegions = pRegions_Mora
        chip_cpu_speed_mhz = 240
        addr_per_word = 4
    elif (kal_arch == 4 and chip_arch == "Hydra" and
          chip_id == 0x50 and chip_revision >= 0x20):
        # Mora 2.0
        dRegions = dRegions_Mora
        pRegions = pRegions_Mora
        chip_cpu_speed_mhz = 240
        addr_per_word = 4
    else:
        raise Exception("Unknown/unsupported chip architecture")

    if old_dRegions is not None and (
            dRegions != old_dRegions or pRegions != old_pRegions
    ):
        # if the secondary processor is asleep, the chip_revision will be None.
        # must also check for kal_arch to make sure it's a platform that
        # supports multi-core (only Crescendo for now).
        if chip_revision is None and kal_arch == 4:
            raise Exception(
                "Secondary processor is asleep - cannot connect to it!")
        else:
            raise Exception("Conflicting chip details provided!")


def get_dm_region(address, panic_on_error=True):
    """Gets symbols in DM region.

    Args:
        address
        panic_on_error (bool, optional)
    """
    if dRegions is None:
        raise ChipNotSet("Chip architecture not set")

    try:
        # Try to convert from a hex string to an integer
        address = int(address, 16)
    except TypeError:
        # Probably means address was already an integer
        pass

    region = None
    for k in dRegions.keys():
        if (address >= dRegions[k][0]) and (address < dRegions[k][1]):
            region = k

    if region is None and panic_on_error:
        raise NotDmRegion("Address 0x%x is not in any regions." % (address))
    return region


def get_pm_region(address, panic_on_error=True):
    """Get symbols in PM region.

    Args:
        address
        panic_on_error (bool, optional)
    """
    if pRegions is None:
        raise ChipNotSet("Chip architecture not set")

    try:
        # Try to convert from a hex string to an integer
        address = int(address, 16)
    except TypeError:
        # Probably means address was already an integer
        pass

    region = None
    for k in pRegions.keys():
        if (address >= pRegions[k][0]) and (address < pRegions[k][1]):
            region = k

    if region is None and panic_on_error:
        raise NotPmRegion("Address 0x%x is not in any regions." % (address))
    return region

def get_pm_end_limit():
    """Get the upper limit of the platform's PM.
    
    Returns:
        The upper limit of PM.
    """
    return pRegions['PMRAM'][1]