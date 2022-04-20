from csr.dev.hw.core.simple_space import SimpleRegisterSpace
from csr.dev.hw.core.mixin.supports_custom_digits import SupportsCustomDigits,\
SupportsCustomDigits
from csr.dev.hw.core.meta.i_core_info import ArmCortexMCoreInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo


class TMEDMISpaceInfo(ArmCortexMCoreInfo, SupportsCustomDigits):

    IO_STRUCT_NAME_PATTERNS = ("dmi",)

    def __init__(self, custom_digits=None, custom_digits_module=None):
        ArmCortexMCoreInfo.__init__(self)
        SupportsCustomDigits.__init__(
            self, custom_digits=custom_digits,
            custom_digits_module=custom_digits_module)

    @property
    def io_map_info(self):

        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.custom_io_struct, None,
                                                  self.layout_info)
        return self._io_map_info


class TMEDMISpace(SimpleRegisterSpace):

    def __init__(self, custom_digits, access_cache_type):
        self.info = TMEDMISpaceInfo(custom_digits=custom_digits)
        SimpleRegisterSpace.__init__(self, "tme_dmi", length=0x24, info=self.info, cache_type=access_cache_type)    
