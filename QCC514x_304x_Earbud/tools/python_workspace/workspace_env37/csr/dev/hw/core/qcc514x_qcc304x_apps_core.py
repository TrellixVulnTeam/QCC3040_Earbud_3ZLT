############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .apps_p0_core import AppsP0Core
from .apps_p1_core import AppsP1Core
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressMap, AccessView, NullAccessCache
from csr.wheels.bitsandbobs import NameSpace
from csr.dev.hw.mmu import AppsVMWindow
from csr.dev.hw.subsystem.host_subsystem import AppsHifTransform

from .meta.i_core_info import ICoreInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo 
from .meta.i_core_info import Kalimba32CoreInfo
from .mixin.supports_custom_digits import SupportsCustomDigits

class QCC514X_QCC304XAppsD00CoreInfo (Kalimba32CoreInfo, SupportsCustomDigits):
    """\
    QCC514X_QCC304X Apps D00 P0 Core meta-data.
    """
    DIGITS_SS_NAME = "apps_sys"
    # ICoreInfo compliance

    def __init__(self, custom_digits=None):
        SupportsCustomDigits.__init__(self, custom_digits=custom_digits)

    @property
    def io_map_info(self):
        
        try:
            self._io_map_info
        except AttributeError:
            if self.custom_io_struct:
                io_struct = self.custom_io_struct
            else:
                from ..io import qcc514x_qcc304x_apps_d00_io_struct as io_struct
            self._io_map_info = IoStructIOMapInfo(io_struct, None,
                                                  self.layout_info)
        return self._io_map_info




class QCC514X_QCC304XAppsD00P0Core(AppsP0Core):
    """
    QCC514X_QCC304X Specific P0 Definition
    """
    
    
    @property
    def _info(self):
        
        try:
            self._core_info
        except AttributeError:
            self._core_info = QCC514X_QCC304XAppsD00CoreInfo(custom_digits=
                                                      self.emulator_build) 
        return self._core_info



class QCC514X_QCC304XAppsD00P1Core(AppsP1Core):
    """
    Updates for QCC514X_QCC304X Map
    """

    
    @property
    def _info(self):
        
        try:
            self._core_info
        except AttributeError:
            self._core_info = QCC514X_QCC304XAppsD00CoreInfo(custom_digits=
                                          self.subsystem.chip.emulator_build) 
        return self._core_info

