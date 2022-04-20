############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .dynamic_factory.get_plugins import get_chip_version_factory_plugins, get_jtag_version_factory_plugins

class ChipFactory (object):
    """
    Constructs appropriate Chip variant given a unique ChipVersion.
    """

    @staticmethod
    def fry(chip_version, access_cache_type):
 
        # Find modules in this package that export a CHIP_MAJOR_VERSION attribute
        factory_plugins = get_chip_version_factory_plugins()
        try:
            factory = factory_plugins[chip_version.major]
        except KeyError:
            raise NotImplementedError("Unsupported chip version %s" % chip_version)
        else:
            chip = factory(chip_version, access_cache_type)
        
        chip.populate()
        return chip

    @staticmethod
    def bake(jtag_version, access_cache_type, io_struct=None):

        factory_plugins = get_jtag_version_factory_plugins()
        try:
            factory = factory_plugins[jtag_version]
        except KeyError:
            raise NotImplementedError("Unsupported chip version %s" % chip_version)
        else:
            chip = factory(jtag_version, access_cache_type, io_struct=io_struct)

        return chip


