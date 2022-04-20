# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
from .meta.i_core_info import XapCoreInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo
try:
    from ..io.csra68100_curator_d01_io import values as io_values
except ImportError:
    io_values = None
from ..io import csra68100_curator_d01_io_struct as io_struct
from csr.dev.hw.core.curator_core import CuratorCore
from csr.dev.model import interface

class CuratorCSRA68100D01CoreInfo (XapCoreInfo):
    """\
    Curator CSRA68100 D01 Core meta-data.
    """
            
    # ICoreInfo compliance

    @property
    def io_map_info(self):
        
        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(io_struct, io_values, 
                                                  self.layout_info) 
        return self._io_map_info


class CuratorCSRA68100D01Core (CuratorCore):
    """\
    Curator CSRA68100 D01 Core.
    """
    def __init__(self, subsystem):
        
        CuratorCore.__init__(self, subsystem)
        
    def populate(self, access_cache_type):
        
        CuratorCore.populate(self, access_cache_type)
    # BaseCore compliance

    @property
    def _info(self):
        
        return CuratorCSRA68100D01Core.__core_info
            
    # CuratorCore compliance

    @property
    def num_efuse_banks(self):
        """
        Number of efuse banks defined in curator core.
        """
        return 2

    def show_pmu_report(self):
        """\
        Used in CuratorCore so must be defined for each chip variant.
        """
        return interface.Code("Need to implement "
                              "CuratorCrecendoD01Core.show_pmu_report!")

    @property
    def _is_running_from_rom(self):
        """
        Is the core configured to fetch code from ROM or SQIF?
        """
        return (self.bitfields.NV_MEM_ADDR_MAP_CFG_STATUS_ORDER.read() 
                in (self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_SQIF_LOW_ROM,
                    self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_ROM))
    # Private
    
    __core_info = CuratorCSRA68100D01CoreInfo()

