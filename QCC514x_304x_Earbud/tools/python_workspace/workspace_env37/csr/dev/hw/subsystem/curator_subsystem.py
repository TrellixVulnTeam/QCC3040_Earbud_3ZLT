############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import PureVirtualError
from .hydra_subsystem import HydraSubsystem
from .mixins.has_xap import HasXAP
from csr.dev.fw.curator_firmware import  CuratorDefaultFirmware
from csr.dev.fw.meta.curator_firmware_build_info import CuratorFirmwareBuildInfo, \
CuratorPatchFirmwareBuildInfo

class CuratorSubsystem (HasXAP, HydraSubsystem):
    """\
    Curator Subsystem Hardware Component (Abstract Base)
    """
    # HydraSubsystem Compliance
    @property
    def name(self):
        return "Curator"

    @property
    def subcomponents(self):
        cmps = HydraSubsystem.subcomponents.fget(self)
        cmps.update(HasXAP.subcomponents.fget(self))
        cmps.update({"core" : "_core"})
        return cmps

    @property
    def number(self):
        """
        The subsystem number (not to be confused with the SSID)
        as defined by csr.dev.hw.chip.hydra_chip.FixedSubsystemNumber.
        """
        from csr.dev.hw.chip.hydra_chip import FixedSubsystemNumber
        return FixedSubsystemNumber.CURATOR

    @property
    def firmware_type(self):
        return CuratorDefaultFirmware

    @property
    def default_firmware_type(self):
        return CuratorDefaultFirmware

    @property
    def patch_type(self):
        from csr.dev.fw.patch import HydraPatch
        return HydraPatch

    @property
    def firmware_build_info_type(self):
        return CuratorFirmwareBuildInfo
    @property
    def patch_build_info_type(self):
        return CuratorPatchFirmwareBuildInfo

    # HydraSubsystem Overrides

    @property
    def cores(self):
        return [self.core]

    @property
    def is_up(self):
        
        # Curator Subsystem Always powered
        #
        return True

    def set_power(self, on_not_off = True):
        
        # Curator Subsystem power can't be controlled.
        #
        if not on_not_off:
            raise ValueError("Curator power can't be disabled")

    # Extensions

    @property
    def core(self):
        """
        Curator Subsystem's one and only cpu Core.
        """
        try:
            self._core
        except AttributeError:
            self._core = self._create_curator_core(self._access_cache_type)
        return self._core          

    # Protected / HasXAP Compliance

    def _create_baseline_slt(self):
        return CuratorDefaultFirmware.create_baseline_slt(self.core)

    # Protected / Required

    def _create_curator_core(self, access_cache_type):
        """\
        Create CuratorCore Proxy.
        
        Derived classes must override to create appropriate variant.
        
        Called on first request for the Proxy.
        """
        raise PureVirtualError(self)

    def _create_spi_data_map(self):
        
        #The Curator core inherits a SPI map creation thing from IsXAP so
        #we'll just call that
        return self.core._create_spi_data_map()

    def _create_trb_map(self):
        
        return self.core._create_trb_map()

    def _create_mmu(self):
        from ..mmu import CuratorMMU
        return CuratorMMU(self)

    @property
    def sqif_trb_address_block_id(self):
        '''
        Returns a list (indexed by sqif device number) of tuples of TRB address 
        and block IDs at which the SQIF contents can be read (assuming the
        SQIF is mapped in place of the ROM).
        '''
        return [ (0x20000,0) ]
