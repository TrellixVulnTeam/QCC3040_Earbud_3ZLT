############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
from csr.dev.fw.meta.xuv_stream_decoder import XUVStreamDecoder
from csr.wheels import iprint, wprint

class IsInHydra (object):
    """
    Hydra Core Mixin
    
    Implementations and extensions common to all hydra cores (XAP and KAL).
    """     
    def __init__(self, containing_subsystem):
        
        self._subsystem = containing_subsystem
        
    # Extensions
    
    @property
    def subsystem(self):
        """\
        The subsystem containing this core.
        
        All Hydra subsystems are live within a subsystem.
        This concept/property is not meaningful to pre-hydra cores.
        """
        return self._subsystem
    
    @property
    def firmware_build_info_type(self):
        """
        Most subsystems have a single core and so there is a 1-2-1 mapping from subsystem to core.
        Dual code subsystems overload this method to return a different type for each core.
        """
        return self.subsystem.firmware_build_info_type

    @property
    def patch_build_info_type(self):
        return self.subsystem.patch_build_info_type
    
    @property
    def firmware_type(self):
        return self.subsystem.firmware_type
    
    @property
    def default_firmware_type(self):
        return self.subsystem.default_firmware_type

    @property
    def patch_type(self):
        """
        Type of patch object, that this core firmware needs.
        """
        return self.subsystem.patch_type
        
    @property
    def emulator_build(self):
        return self.subsystem.emulator_build

    def build_standalone_env(self, elf_path_or_dir=None, build_id=None,
                             interface_dir=None):
        '''
        Return a firmware environment for current core, specified by either
        full path to symbol file/dir or by the build_id (e.g. from SLT).
        interface_dir is optional and may be used when using elf_path_or_dir.
        Using build_id will raise an exception if the file path is not uniquely
        deducible, e.g. AttributeError or KeyError.
        Argument errors raise ValueError, e.g. elf_path_or_dir not found.
        Then you can use the resulting fw_env to do symbol accesses, e.g.
        cur_env = cur.build_standalone_env(elf_path_or_dir)
        if (cur_env.cus['globman.c'].localvars['globman_state'].value <=
        cur_env.cus['globman.c'].enums['globman_states'][
        'GLOBMAN_STATE_PATCHED']):
        '''
        import os
        from csr.dev.env.standalone_env import StandaloneFirmwareEnvironment
        from csr.dev.fw.meta.i_firmware_build_info import BuildDirNotFoundError
        if not (elf_path_or_dir is None) ^ (build_id is None):
            if elf_path_or_dir is None:
                raise ValueError(
                    'Cannot convert build_id {} to path'.format(build_id))
            raise ValueError("Just one of elf_path_or_dir or build_id required")
        if build_id is not None:
            elf_path_or_dir = (self.firmware_build_info_type.
                               build_from_id(id_number=build_id))
        if not elf_path_or_dir:
            msg = "Elf dir not found for build id '{}'".format(build_id)
        elif not os.path.exists(elf_path_or_dir):
            msg = "Elf path or dir '{}' not found".format(elf_path_or_dir)
        if not elf_path_or_dir or not os.path.exists(elf_path_or_dir):
            raise BuildDirNotFoundError(msg)

        return StandaloneFirmwareEnvironment(
            self.firmware_build_info_type(
                elf_path_or_dir, self.info.layout_info,
                interface_dir=interface_dir,
                chip_name=self.subsystem.chip.name),
            self.data, self.info.layout_info)

    def load_program_cache_from_xuv(self, xuv_path):
        """
        Read the given XUV file and load its contents into the core's program
        space cache.
        
        The aim here is to work around the fact that there is no SLT available
        in coredumps, so we need to retrieve it from the XUV.
        """
        try:
            self.access_cache_type.cache
        except AttributeError:
            wprint("Not loading {} XUV file as on-chip program space is accessible".format(self.title))
            return
        else:
            
            orig_fw_ver = self.fw.slt.build_id_string
                
            def write_block(start_addr, word_values):
                # Convert from 16-bit XUV to appropriate width for this core
                byte_values = list(self.info.layout_info.adjust_stream(word_values, 16, 
                                           self.info.layout_info.addr_unit_bits))
                start_addr *= 16//self.info.layout_info.addr_unit_bits
                self.program_space[start_addr:start_addr+len(byte_values)] = byte_values 
                
            try:
                with open(xuv_path) as xuv:
                    iprint("Loading {} XUV file {}".format(self.title, xuv_path))
                    values, start_addr = XUVStreamDecoder(xuv).value_block
                    write_block(start_addr, values)
            except XUVStreamDecoder.NotContiguous:
                with open(xuv_path) as xuv:
                    for addr, values in XUVStreamDecoder(xuv).chunks(0):
                        write_block(addr, values)

            # We should still get the same firmware version
            fw_ver = self.fw.slt.build_id_string
            
            if (os.getenv("PYDBG_DISABLE_FW_VER_CHECKS") is None and 
                fw_ver != orig_fw_ver):
                raise ValueError("{} contains firmware version\n\n  '{}'\n\nbut the "
                                 "loaded coredump contains\n\n  '{}'".format(xuv_path, fw_ver, orig_fw_ver))