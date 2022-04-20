############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.hw.address_space import AddressMap, AddressConnection,\
    AddressSlavePort, AddressMasterPort, NullAccessCache, AccessView
from csr.wheels.bitsandbobs import NameSpace, PureVirtualError
from csr.dev.hw.core.mixin.is_in_hydra import IsInHydra
from csr.dev.fw.audio_core import AudioVMCore
from .meta.i_core_info import Kalimba32CoreInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo
from .mixin.supports_custom_digits import SupportsCustomDigits


class QCC514X_QCC304XAudioCore(IsInHydra, AudioVMCore):
    """
    Generic Audio core for QCC512X_QCC302X: relies on parametrisation of the following
    quantities:

        NUM_120MHZ_RAM_BANKS
        NUM_240MHZ_RAM_BANKS
        NUM_REMOTE_BAC_WINDOWS
        NUM_DM_NVMEM_WINDOWS
        NUM_PM_NVMEM_WINDOWS
        NUM_PM_RAM_BANKS
        MAPPED_PM_RAM_SIZE
    """
    
    def __init__(self, subsystem, access_cache_type, hw_version):
        '''
        Create the fundamental memory blocks
        '''
        AudioVMCore.__init__(self)
        IsInHydra.__init__(self, subsystem)
        self._hw_version = hw_version

    @property
    def num_brk_regs(self):
        return 8

    @property
    def program_memory(self):
        return self._proc_pm.port
    # BaseCore Compliance

    @property
    def data(self):
        return self._components.proc_data_map.port
                        
    @property
    def program_space(self):
        return self._components.proc_pm_map.port

    @property
    def register_space(self):
        '''
        Registers appear in data space at their "well known" addresses, i.e.
        starting from 0xfff00000
        '''
        return self.data
    
    @property
    def _info(self):
        global _core_info
        try:
            _core_info
        except NameError:
            _core_info = QCC514X_QCC304XAudioCoreInfo(self._hw_version,
                                       custom_digits=self.emulator_build)
        return _core_info 


class QCC514X_QCC304XAudioP0Core(QCC514X_QCC304XAudioCore):
    
    data_view = AccessView.PROC_0_DATA
    prog_view = AccessView.PROC_0_PROG
    processor = 0

    @property
    def nicknames(self):
        return ("audio0", "audio")

    @property
    def firmware_build_info_type(self):
        from csr.dev.fw.meta.i_firmware_build_info import HydraAudioP0FirmwareBuildInfo
        return HydraAudioP0FirmwareBuildInfo

    @property
    def firmware_type(self):
        from csr.dev.fw.audio_firmware import AudioP0Firmware
        return AudioP0Firmware

class QCC514X_QCC304XAudioP1Core(QCC514X_QCC304XAudioCore):
    
    data_view = AccessView.PROC_1_DATA
    prog_view = AccessView.PROC_1_PROG
    processor = 1

    @property
    def nicknames(self):
        return ("audio1",)

    @property
    def firmware_build_info_type(self):
        from csr.dev.fw.meta.i_firmware_build_info import HydraAudioP1FirmwareBuildInfo
        return HydraAudioP1FirmwareBuildInfo

    @property
    def firmware_type(self):
        from csr.dev.fw.audio_firmware import AudioP1Firmware
        return AudioP1Firmware


class QCC514X_QCC304XAudioCoreInfo (Kalimba32CoreInfo, SupportsCustomDigits):
    """
    QCC514X_QCC304X Audio Core meta-data.
    """
    DIGITS_SS_NAME = "audio_sys"

    def __init__(self, hw_version,
                 custom_digits=None):
        self._hw_version = hw_version
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
                if self._hw_version == "d00":
                    from ..io import qcc514x_qcc304x_audio_d00_io_struct as io_struct
                elif self._hw_version == "d01":
                    from ..io import qcc514x_qcc304x_audio_d01_io_struct as io_struct
                else:
                    from ..io import qcc514x_qcc304x_audio_fpga_io_struct as io_struct
            self._io_map_info = IoStructIOMapInfo(io_struct, None, 
                                               self.layout_info) 
        return self._io_map_info
