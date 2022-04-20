# Copyright (c) 2016 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
"""
Provides classes to represent the BT firmware. 
"""

from csr.dev.fw.firmware import DefaultFirmware
from csr.dev.hw.address_space import AddressSpace
from csr.dev.fw.slt import BTBaseSLT, RawSLT, BTSLTNotImplemented,  BTFakeSLT, BTZeagleFakeSLT, BTZeagleSLT
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor

class BTDefaultFirmware(DefaultFirmware):
    """
    BT default firmware to accommodate the absence of a proper ELF based
    environment.
    This will hopefully be fixed in future by the BT team moving to binutils
    for their FW builds
    """

    @property
    def title(self):
        return "BT default firmware"

    def create_slt(self):
        "Creates the symbol lookup table"
        # Current firmware uses baseline SLT.
        # Change this to specific SLT if/when SLT gets extended.
        try:
            return BTBaseSLT.generate(self._core)
        except AddressSpace.NoAccess:
            if hasattr(self._core, "dump_build_id") and  hasattr(self._core, "dump_build_string"):
                return BTFakeSLT(
                    self._core.dump_build_id,
                    self._core.dump_build_string)
            return BTSLTNotImplemented()


    def fw_ver(self):
        return self.slt.fw_ver()


class BTDefaultZeagleFirmware(DefaultFirmware):
    """
    """

    @property
    def title(self):
        return "BT default firmware"

    def create_slt(self):
        "Creates the symbol lookup table"
        # Current firmware uses baseline SLT.
        # Change this to specific SLT if/when SLT gets extended.
        try:
            return BTZeagleSLT.generate(self._core)
        except AddressSpace.NoAccess:
            if hasattr(self._core, "dump_build_id") and  hasattr(self._core, "dump_build_string"):
                return BTZeagleFakeSLT(
                    self._core.dump_build_id,
                    self._core.dump_build_string,
                    self._core)
            return BTSLTNotImplemented()
        except RawSLT.BadFingerprint:
            return None
        
    def fw_ver(self):
        return self.slt.fw_ver()
