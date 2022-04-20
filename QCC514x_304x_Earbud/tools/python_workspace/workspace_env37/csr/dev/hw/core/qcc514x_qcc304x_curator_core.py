############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides classes representing the Curator Core on QCC514X_QCC304X chips.
"""
from csr.wheels.global_streams import iprint
from csr.dev.hw.core.curator_core import CuratorCore
from csr.dev.hw.address_space import AddressSpace
from .meta.i_core_info import XapCoreInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo
from csr.dev.hw.core.curator_core import CuratorCore
from .mixin.supports_custom_digits import SupportsCustomDigits

class QCC514X_QCC304XCuratorD00CoreInfo(XapCoreInfo, SupportsCustomDigits):
    """\
    Curator QCC514X_QCC304X D00 Core meta-data.
    """
    DIGITS_SS_NAME = "curator"

    def __init__(self, custom_digits=None):

        SupportsCustomDigits.__init__(self, custom_digits=custom_digits)

    # ICoreInfo compliance

    @property
    def io_map_info(self):

        try:
            self._io_map_info
        except AttributeError:
            if self.custom_io_struct:
                io_struct = self.custom_io_struct
            else:
                from ..io import qcc514x_qcc304x_curator_d00_io_struct as io_struct

            self._io_map_info = IoStructIOMapInfo(io_struct, None,
                                                  self.layout_info)
        return self._io_map_info


class QCC514X_QCC304XCuratorD00Core(CuratorCore): # pylint: disable=too-many-ancestors
    """\
    Curator QCC514X_QCC304X D00 Core.
    """
    def __init__(self, subsystem):

        CuratorCore.__init__(self, subsystem)

    def populate(self, access_cache_type):

        CuratorCore.populate(self, access_cache_type)
    # BaseCore compliance

    @property
    def _info(self):

        try:
            self._core_info
        except AttributeError:
            self._core_info = QCC514X_QCC304XCuratorD00CoreInfo(custom_digits=
                                                         self.emulator_build)
        return self._core_info

    # CuratorCore compliance

    def show_pmu(self):
        """\
        Used in CuratorCore so must be defined for each chip variant.
        """
        raise NotImplementedError(
            "Need to implement QCC514X_QCC304XCuratorD00Core.show_pmu!")

    @property
    def _is_running_from_rom(self):
        """
        Is the core configured to fetch code from ROM or SQIF?
        """
        return (self.bitfields.NV_MEM_ADDR_MAP_CFG_STATUS_ORDER.read()
                in (self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_SQIF_LOW_ROM,
                    self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_ROM))

    # Private
    def halt_chip(self):
        """
        Halts the chip and also explicitly halt the BT ARM core.
        """
        CuratorCore.halt_chip(self)
        try:
            self.subsystem.chip.bt_subsystem.core.halt()
        except AddressSpace.AccessFailure as exc:
            iprint("AccessFailure: {}".format(exc))
