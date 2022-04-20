############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import TypeCheck
from csr.dev.fw.meta.i_firmware_build_info import \
BaseGenericHydraFirmwareBuildInfo, get_network_homes, XAPGCCToolchain
from csr.dev.fw.slt import CuratorBaseSLT
from csr.dev.hw.core.meta.i_layout_info import XapDataInfo
import re
import platform
import glob
from csr.interface.mibdb import MIBDB

try:
    long # not available on python3 because int is size of a long
except NameError:
    long = int #pylint: disable=redefined-builtin

class CuratorFirmwareBuildInfo (BaseGenericHydraFirmwareBuildInfo):
    """
    Interface to debugging information applicable to a specific build of
    curator firmware.
    
    N.B. In future we may distribute debug data in a flat pack - that would
    require an alternative specialisation or an indirection as the paths to
    all the useful things may differ.
    """
      
    def __init__(self, development_build_dir, data_layout_info = XapDataInfo(), 
                 interface_dir=None, chip_name=None, multi_elf_dir=None, cache_dir=None,
                 autolookup_key=None):
        BaseGenericHydraFirmwareBuildInfo.__init__(self,
                                                   development_build_dir,
                                                   data_layout_info,
                                                   interface_dir, chip_name,
                                                   multi_elf_dir, cache_dir=cache_dir,
                                                   autolookup_key=autolookup_key)
      
    @staticmethod
    def create_custom(build_dir):
        """
        Construct firmware build info abstraction from the specified custom
        location
        """
        return CuratorFirmwareBuildInfo(build_dir, XapDataInfo())

    @property
    def toolchain(self):
        return XAPGCCToolchain

    @property
    def slt_type(self):
        return CuratorBaseSLT

    @property
    def _mibdb_basename(self):
        return "curator_mib.xml"
        
    @property
    def _src_root_for_rebuild(self):
        """
        returns tuple (_rootdir, rel_path_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """
        return (self._root_dir, os.path.join('fw', 'src'))

    def _create_mibdb(self):
        """
        Internal method that creates a mibdb object based on curator's knowledge
        of where to find the xml file on which it is based.  It looks first
        in the place used in _get_config_db().
        """
        try:
            return MIBDB(os.path.join(self._dir, 'mib', self._mibdb_basename))
        except IOError:
            # Fall back to common locations
            try:
                return super(CuratorFirmwareBuildInfo, self)._create_mibdb()
            except IOError:
                return None

    def _get_config_db(self):
        """
        Path to database that defines the .sdb file used by configcmd 
        containing keys for a developer system label of dev_system_label.  
        Typically this is mib.sdb in the same folder as the executable.
        """
        return os.path.join(self.build_dir, 'mib', 'mib.sdb')
        
    def _get_build_id(self):
        '''
        Utility function to find the build id from path
        '''

        
        regex = r'.*_(\d+)' 
        pattern = re.compile(regex)
        match = pattern.match(self._root_dir)
        return int(match.group(1)) if match else None

    def _read_debug_strings(self):
        # The XAP compiler writes debug strings unpacked
        return self.elf_reader.get_debug_strings(2,False)

    @staticmethod
    def build_from_id(id=None, id_number=None, cache=None):
        """
        Determine a build path from the firmware build ID in id or id_number
        integer.
        This function only currently handles a build ID string of the form
        taken from the string returned by cur.fw_ver()
        in which the build id number is extracted from the 3rd component.
        """
        if not id and id_number is None:
            raise ValueError("Cannot look up build without an id or id_number")

        if id_number:
            id = id_number
        if isinstance(id, (int, long)):
            id_string = str(id)
        else:
            id_string = id.split()[3]
        iprint(" (Attempting to find Curator firmware build %s)" % id_string)
        build_root = os.path.join(get_network_homes(), "cursw","curator_builds",
                                  "builds")

        if cache:
            # Can we find a matching path amongst the cached ELF files?  If so
            # we avoid having to do a listing of a very large remote directory,
            # which can be slow in some circumstances.
            cached_build_dirs = list(set(os.path.dirname(cached_path) for (cache_file, cached_path, _) 
                                         in cache.list(os.path.join(build_root, "*_{}".format(id_string), 
                                                                    "fw","build","carlos","xap","xapgcc",
                                                                    "hw","debug","curator_*", "*"))))
            if len(cached_build_dirs) == 1:
                dir, _ = cached_build_dirs[0].split(os.path.join("fw","build","carlos","xap"))
                iprint(" (Found build at %s)" % dir.rstrip(os.path.sep))
                return cached_build_dirs[0]
        
        if not os.path.isdir(build_root):
            iprint(" (No directory %s: can't look up build)" % build_root)
            return None
        build_dirs = [d for d in os.listdir(build_root) if
                      d.endswith("_{}".format(id_string))]
        if len(build_dirs) == 0:
            # No matching build
            return None
        elif len(build_dirs) > 1:
            # Multiple matching builds?!?
            raise ValueError("Multiple Curator firmware build directories "
                             "matched fw ID %s: %s" % (id_string, build_dirs))

        dir = os.path.join(build_root, build_dirs[0])
        glob_pat = os.path.join(dir,"fw","build","carlos","xap","xapgcc",
                                "hw","debug","curator_*")
        matches = glob.glob(glob_pat)
        if len(matches) != 1:
            # Glob didn't work properly
            raise ValueError("glob %s didn't match a single directory as "
                             "expected" % glob_pat)
        iprint(" (Found build at %s)" % dir)
        return matches[0]

    @property
    def _bad_var_names(self):
        """
        Hydra firmware typically contains lots of "log_fmt" variables that 
        aren't present in the running firmware. We need to filter them out of
        the DWARF.  Curator firmware also contains log_string_*
        """
        return set(["log_fmt", "log_string_"])


class CuratorPatchFirmwareBuildInfo(CuratorFirmwareBuildInfo):
    """
    Patch builds vary slightly from normal firmware builds
    """
    
    @property
    def _build_file_nameroot(self):
        return "patch"
    
    @property
    def _mibdb_basename(self):
        return "curator_patch_mib.xml"
    
    @staticmethod
    def build_from_patch_id(id):
        """
        Look up the patch build area based on the numeric patch ID
        """
        return CuratorFirmwareBuildInfo.build_from_id(None, id)

    @property
    def elf_symbols(self):
        """
        Override the raw symbol look-up to add an extra filtering step which
        deletes functions that appear in the abs section from both the global
        and CU lists.
        """
        gbls, funcs, cus, abs, minim = super(CuratorPatchFirmwareBuildInfo, 
                                             self).elf_symbols
        
        abs_set = {name for (name, value) in abs.items()}
        
        funcs = {f:addr for (f, addr) in funcs.items() if f not in abs_set}

        # Now look up file-scoped vars in the CUs
        for cu, cu_dict in cus.items():
            cu_dict["funcs"] = {f:addr for (f, addr) in cu_dict["funcs"].items() if f not in abs_set}
        
        return gbls, funcs, cus, abs, minim
    
    @property
    def elf_slt(self):
        # There's no SLT in a Curator patch
        raise AttributeError
