############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.fw.slt import CuratorBaseSLT, CuratorFakeSLT, \
                            CuratorSLTNotImplemented, \
                            CuratorSLTBlank, RawSLT
from csr.dev.fw.firmware import  DefaultFirmware
from csr.dev.hw.address_space import AddressSpace
from csr.dev.model import interface
from csr.dev.fw.cucmd import CuCmd

class CuratorDefaultFirmware(DefaultFirmware):
    
    """
    Curator default firmware. 
    This is specifically for cases where there's no "firmware environment" due 
    to the absence of ELF/DWARF info to create it from.  We're still able to
    do cucmd in this scenario.
    
    Note that CuratorFirmware inherits from CuratorDefaultFirmware since anything
    you can do without an ELF you can also do with one.
    """

    @property
    def title(self):
        return "Curator default firmware"
    
    @property
    def cucmd(self):
        """\
        CUCMD interface
        """        
        # Construct lazily...
        try:
            self._cucmd
        except AttributeError:
            # Rely on CuCmd module to create correct version by reading 
            # the SLT.
            #
            # This is designed to work even if have no other meta data 
            # available.
            #
            # FUTURE: If we _do_ have other meta-data could check we get the 
            # expected CUCMD version here.
            # 
            self._cucmd = CuCmd.create(self, self._core.data, version=1)
            
        return self._cucmd

    
    def create_slt(self):
        # Current firmware uses baseline SLT.
        # Change this to specific SLT if/when SLT gets extended.
        try:
            return CuratorBaseSLT(self._core)
        except AddressSpace.NoAccess:
            if hasattr(self._core, "dump_build_id") and \
               hasattr(self._core, "dump_build_string"):
                return CuratorFakeSLT(self._core.dump_build_id,
                    self._core.dump_build_string)
            else:
                return CuratorSLTNotImplemented()
        except RawSLT.BadFingerprint as e:
            # If the fingerprint is 0xFFFF it is likely because we have a blank
            # SQIF, treat this differently.
            if e.fingerprint == 0xFFFF:
                return CuratorSLTBlank()
            else:
                raise e

