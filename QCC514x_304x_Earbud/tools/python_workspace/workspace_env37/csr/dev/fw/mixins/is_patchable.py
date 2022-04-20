############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
from ....wheels.bitsandbobs import NameSpace
from ....wheels.global_streams import iprint
from ...env.standalone_env import StandaloneFirmwareEnvironment
from ..meta.i_firmware_build_info import IGenericHydraFirmwareBuildInfo
from ...model.base_component import BaseComponent

class Patch(BaseComponent):
    """
    Represents a firmware patch.  Intended to provide useful details of the
    patch, like usage of RAM, as well as access to a
    firmware environment built from the patch ELF
    """
    def __init__(self, core, build_info):
        
        self._core = core
        self._build_info = build_info
        self._elf_sections = list(self._build_info.elf_code.sections)
        
        self._on_reset()
        
    def _on_reset(self):
        """
        Recreate the firmware environment for the patch, as the loaded patch
        might have changed
        """
        self.env = StandaloneFirmwareEnvironment(self._build_info, self._core,
                                                 self._core.info.layout_info)
        
    @property
    def total_size_bytes(self):
        return self.data_size_bytes + self.code_size_bytes
    
    @property
    def data_size_bytes(self):
        return sum(sec.byte_size for sec in self._elf_sections if not sec.is_instructions)

    @property
    def code_size_bytes(self):
        return sum(sec.byte_size for sec in self._elf_sections if sec.is_instructions)

    def code_sizes(self, report=False):
        return self.env.functions.info(
            known_size_bytes=self.code_size_bytes,
            remainder_name="[uninstrumented patch code]",
            report=report)
    
    def data_sizes(self, report=False):
        return self.env.vars.info(
            known_size_bytes=self.data_size_bytes,
            remainder_name="[uninstrumented patch variables]",
            report=report)
    
    def info(self, report=False):
        var_info = self.data_sizes(report=report)
        func_info = self.code_sizes(report=report)
        if report:
            from csr.dev.model.interface import Group
            output = Group()
            output.extend(var_info)
            output.extend(func_info)
            return output
    
    def _generate_report_body_elements(self):
        
        return self.info(report=True)


class HydraPatch(Patch):
    """
    Represents a Hydra firmware patch specifically.  Extends the generic
    size + environment interface to include details of the HCF file and use of
    hardware patch points.
    """

    # ?Potential extension: Implement an HCF decoder?
    @property
    def hcf_file(self):
        rom_number = "%08X" % self._core.fw.slt.build_id_number
        ss_number = self._core.subsystem.number
        return os.path.join(self.env.build_info.build_dir,
                            "subsys%d_patch0_fw%s.hcf" % (ss_number, rom_number))


class IsPatchable(object):
    """
    Mixin for Firmware classes that are patchable.  Provides "rom" and "patch"
    attributes that contain separate rom-specific or patch-specific firmware
    environments, based on the build_info
    """
    @property
    def rom(self):
        
        try:
            self._rom
        except AttributeError:
            self._rom = NameSpace()
            try:
                rom_build_info = self.env.build_info.rom
            except AttributeError:
                # The main build_info doesn't have a ROM variant. The chip
                # isn't running from
                iprint("The firmware detected is not a patched ROM")
                return None
            self._rom.env = StandaloneFirmwareEnvironment(rom_build_info,
                                                          self._core,
                                                          self._core.info.layout_info)
        return self._rom
    
    @property
    def patch(self):
        
        try:
            self._patch
        except AttributeError:
            try:
                patch_build_info = self.env.build_info.patch
            except AttributeError:
                # The main build_info doesn't have a ROM variant. The chip
                # isn't running from
                iprint("The firmware detected is not a patched ROM")
                return None
            patch_type = HydraPatch if isinstance(patch_build_info, 
                                                  IGenericHydraFirmwareBuildInfo) else Patch
            self._patch = patch_type(self._core, patch_build_info)
        return self._patch
    
    def _all_subcomponents(self):
        return {"patch" : "_patch"}