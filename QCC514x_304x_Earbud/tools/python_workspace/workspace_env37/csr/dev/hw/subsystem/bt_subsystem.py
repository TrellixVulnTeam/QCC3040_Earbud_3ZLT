############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class BTSubsystem to represent the BT subsystem hardware.
"""
from csr.wheels import autolazy, NameSpace
from .hydra_subsystem import SimpleHydraSubsystem
from csr.dev.hw.core.base_core import SimpleHydraCore
from csr.dev.fw.bt_firmware import BTDefaultZeagleFirmware
from csr.dev.fw.meta.i_firmware_build_info import IFirmwareBuildInfo
from csr.dev.hw.core.meta.i_layout_info import ArmCortexMDataInfo
from csr.dev.fw.debug_log import ClassicHydraLog, HCILogDecoder
from csr.dev.fw.firmware import Firmware
from csr.dev.fw.meta.i_firmware_build_info import BaseGenericHydraFirmwareBuildInfo,\
    UnknownToolchain
from csr.dev.hw.address_space import AddressSlavePort, AddressMap, NullAccessCache

class SimpleBTFirmwareBuildInfo(BaseGenericHydraFirmwareBuildInfo):

    toolchain = UnknownToolchain
    _log_firm_basename = ""

class SimpleBTFirmware(Firmware, BTDefaultZeagleFirmware):

    @property
    @autolazy
    def debug_log(self):
        return ClassicHydraLog(self.env, self._core, parent=self)


    @property
    @autolazy
    def debug_log_decoder(self):
        return HCILogDecoder(self)



class SimpleBTCore(SimpleHydraCore):

    def __init__(self, name, data_space_size, layout_info, subsystem, access_cache_type):
        self.access_cache_type = access_cache_type
        SimpleHydraCore.__init__(self, name, data_space_size, layout_info, subsystem)

    @property
    def core_commands(self):
        cmds, excs = super(self.__class__, self).core_commands
        cmds.update({"log" : "self.fw.debug_log.generate_decoded_event_report",
                     "reread_log" : "self.fw.debug_log.reread_log",
                     "live_log" : "self.fw.debug_log.live_log",
                     "log_level" : "self.fw.debug_log.log_level"})
        return cmds, excs

    def _create_memory_map(self, data_space_size):

        access_cache_type = self.access_cache_type

        def address_map(name, size, access_cache_type):
            """
            Helper function to create address_map based
            on memory component name and size.
            """
            return AddressMap(name, length=size, cache_type=access_cache_type,
                              layout_info=self.info.layout_info)

        def address_space(name, size, access_cache_type):
            """
            Helper function to create address_space based
            on memory component name and size.
            """
            return AddressSlavePort(name, length=size,
                                    cache_type=access_cache_type,
                                    layout_info=self.info.layout_info)

        self._mem_map_cmpts = NameSpace()
        self._mem_map_cmpts.sram = address_map("ARM_M0_SRAM",
                                               0x100000,
                                               access_cache_type)
        self._mem_map_cmpts.registers1 = address_space("ARM_M0_REGISTERS1",
                                                       0x100000,
                                                       access_cache_type)
        self._mem_map_cmpts.registers2 = address_space("ARM_M0_REGISTERS2",
                                                       0x50000,
                                                       access_cache_type)
        self._data = address_map("ARM_M0_MEMORY",
                                 1 << 32, NullAccessCache)
        self._data.add_mappings(
            (0x20000000, 0x20100000, self._mem_map_cmpts.sram.port),
            (0x30000000, 0x30100000, self._mem_map_cmpts.registers1),
            (0x40000000, 0x40050000, self._mem_map_cmpts.registers2))
        self._data.add_mappings((0x00000000,
                                 0x00100000,
                                 self._mem_map_cmpts.sram.port))


class SimpleBTSubsystem(SimpleHydraSubsystem):
    
    @property
    def name(self):
        return "Bluetooth"
    
    @property
    def cores(self):
        try:
            self._cores
        except AttributeError:
            self._cores = [SimpleBTCore("bt",
                                 data_space_size=1<<32,
                                 layout_info=ArmCortexMDataInfo(),
                                 subsystem=self,
                                 access_cache_type=self._access_cache_type)]
        return self._cores
    
    @property
    def core(self):
        return self.cores[0]
    
    default_firmware_type = BTDefaultZeagleFirmware
    firmware_type = SimpleBTFirmware
    firmware_build_info_type = SimpleBTFirmwareBuildInfo
    patch_build_info_type = SimpleBTFirmwareBuildInfo