############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class HydraPatchedROMFirmwareBuildInfo.
"""
from csr.wheels.global_streams import iprint
from .i_firmware_build_info import IFirmwareBuildInfo, \
    IGenericHydraFirmwareBuildInfo, PathNotFoundError
from ....dwarf.read_dwarf import Dwarf_Reader
from ....dwarf.read_elf import Elf_Reader

try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int

class PatchedROMFirmwareBuildInfo(IFirmwareBuildInfo):
    """
    Combines build information e.g. elf file, from both the build info
    for a ROM and the one for any loaded patch.
    """

    def __init__(self, rom_build_info, patch_build_dir_or_ver, layout_info,
                 interface_dir, chip_name, core=None, cache_dir=None):
        #pylint: disable=too-many-arguments
        self._rom_bi = rom_build_info
        self._layout_info = layout_info

        autolookup_key = None
        if isinstance(patch_build_dir_or_ver, str):
            # We were given the patch build directory, so just construct the
            # patch firmware info directly
            patch_build_dir = patch_build_dir_or_ver
        else:
            # We were given a function to call on a ROM-based firmware
            # environment that would give us the patch build ID (int or string).
            # So we create a throwaway environment based on the ROM, and
            # call the function that gives us the ID.
            from ...env.standalone_env import StandaloneFirmwareEnvironment
            # Wrap the rom_env as a LazyProxy so we don't waste time parsing the
            # ROM DWARF if we don't need to.
            from csr.wheels.bitsandbobs import LazyProxy
            rom_env = LazyProxy("rom_env", StandaloneFirmwareEnvironment,
                                (rom_build_info, core.data, layout_info), {})
            patch_id = patch_build_dir_or_ver(rom_env)
            # If Patch ID is detected as empty string or 0 this means we are
            # running from ROM with no patches
            if patch_id in ['', 0]:
                iprint("Running from ROM with no patches")
                patch_build_dir = None
            elif isinstance(patch_id, int_type):
                patch_build_dir = (
                    self.key_cache_lookup(patch_id) or
                    (core.patch_build_info_type.build_from_patch_id(patch_id)))
                autolookup_key = str(patch_id)
            elif isinstance(patch_id, str):
                patch_build_dir = (
                    self.key_cache_lookup(patch_id) or
                    (core.patch_build_info_type.build_from_patch_id(
                        id=patch_id, id_number=None)))
                autolookup_key = patch_id
            else:
                if patch_id is not None:
                    # Unexpected failure
                    iprint("Patch ID evaluation failed (%s) - "
                           "not loading patch build info" % patch_id)
                patch_build_dir = None

        if patch_build_dir is not None:
            self._patch_bi = core.patch_build_info_type(
                patch_build_dir,
                layout_info,
                interface_dir,
                chip_name,
                cache_dir=cache_dir,
                autolookup_key=autolookup_key)
        elif patch_id is not None:
            if patch_id != 0:
                iprint("Couldn't get usable build dir for patch")
            self._patch_bi = None
        else:
            self._patch_bi = None

    @property
    def toolchain(self):
        '''returns the ROM's build information object's toolchain'''
        return self._rom_bi.toolchain

    # Overrides of IFirmwareBuildInfo
    #------------------------------------------------

    @property
    def _path_anchor(self):
        #friendly access
        #pylint: disable=protected-access
        # There's no guarantee that the patch one is the same
        # but source_file() will take care of considering using both.
        return self._rom_bi._path_anchor

    def source_file(self, srcfile):
        """
        Returns the entire contents of the given source file, mapping paths as
        required.

        If you want just a few lines from the file and/or you want decoration
        with headers and line numbers then use source_code().
        """
        msg = None
        try:
            if self._patch_bi is not None:
                return self._patch_bi.source_file(srcfile)
        except ValueError as exc:
            msg = str(exc) + ' within patch sources\n'
        try:
            return self._rom_bi.source_file(srcfile)
        except ValueError as exc:
            if msg:
                raise ValueError(msg + str(exc) + ' within ROM sources')
            raise

    # Standard extensions to resemble FirmwareBuildInfoElfMixin
    # ---------------------------------------------------------
    # but excluding the ambiguous singular elf, elf_file, elf_code as well as
    # load_program_cache,
    # which in any case are found via __getattr__ to return the ones for rom.

    @property
    def elf_reader(self):
        '''\
        returns the patch's build information object's elf_reader if
        the build information available otherwise that for the ROM.
        '''
        try:
            self._elf_reader
        except AttributeError:
            if self._patch_bi is not None and self._patch_bi.elf_symbols_valid:
                self._elf_reader = Elf_Reader([self._rom_bi.elf_reader,
                                               self._patch_bi.elf_reader],
                                               self._rom_bi.toolchain)
            else:
                self._elf_reader = self._rom.bi.elf_reader
        return self._elf_reader

    @property
    def elf_symbol_info_type(self): # TBD: This looks like a Fossil.
        '''returns the ROM's build information object's elf_symbol_info_type'''
        return self._rom_bi.elf_symbol_info_type

    @property
    def dwarf_sym(self):
        '''\
        returns a merged ROM's and patch's build information object's
        dwarf reader
        '''
        #friendly access
        #pylint: disable=protected-access
        try:
            self._dwarf_sym
        except AttributeError:
            readers = [bi.dwarf_sym for bi in (self._rom_bi, self._patch_bi)
                       if bi is not None]
            self._dwarf_sym = Dwarf_Reader(
                readers,
                # Note: DWARF uses byte to mean 8 bits regardless of the size
                # of addressable units on the target platform
                ptr_size=self._rom_bi._layout_info.data_word_bits//8)
        return self._dwarf_sym

    @property
    def dwarf_full(self):
        '''\
        returns a merged ROM's and patch's build information object's
        dwarf reader
        '''
        return self.dwarf_sym

    def get_asm_sym(self):
        return self.rom.get_asm_sym()

    # PatchedROM specific extensions
    # ---------------------------------------------

    @property
    def rom(self):
        '''returns the ROM's build information object'''
        return self._rom_bi

    @property
    def patch(self):
        '''returns the patch's build information object'''
        return self._patch_bi

    @property
    def slt_type(self):
        """returns the rom's SLT type"""
        return self._rom_bi.slt_type

    # Catch-all to avoid a sudden interface breakage

    def __getattr__(self, attr):
        if attr in ("_elf_reader", "_dwarf_sym"):
            raise AttributeError(
                "%s object has no attribute '%s'" % (type(self), attr))
        return getattr(self._rom_bi, attr)


class HydraPatchedROMFirmwareBuildInfo(
        PatchedROMFirmwareBuildInfo, IGenericHydraFirmwareBuildInfo):
    """
    A build info class for a patched rom augmented with hydra APIs.
    """

    # Overrides of IGenericHydraFirmwareBuildInfo
    # -------------------------------------------

    @property
    def _log_firm_basename(self):
        #friendly access
        #pylint: disable=protected-access
        return self._rom_bi._log_firm_basename

    @property
    def panic_and_fault_info(self):
        """
        Panic and Fault information (PanicAndFaultInfo)
        """
        return self._rom_bi.panic_and_fault_info

    @property
    def interface_info(self):
        """Interface to interface/protocol information"""
        return self._rom_bi.interface_info

    def get_hip(self, hip_name):
        """
        Get HIP interface by name.
        E.g. 'ccp' return HIP wrapper for ccp.xml
        """

        # if patch does not have a _common_dir then use the ROM one.
        try:
            if self._patch_bi is not None:
                return self._patch_bi.get_hip(hip_name)
        except PathNotFoundError:
            pass
        return self._rom_bi.get_hip(hip_name)

    @property
    def debug_strings(self):
        # Do patches actually have any extra debug strings in them?
        dbg_strings = self._rom_bi.debug_strings
        if self._patch_bi is not None:
            dbg_strings.update(self._patch_bi.debug_strings)
        return dbg_strings

    @property
    def debug_string_packing_info(self):
        return self._rom_bi.debug_string_packing_info

    @property
    def config_xml_db(self):
        # if patch does not have a file then use the ROM one.
        try:
            if self._patch_bi is not None:
                result = self._patch_bi.config_xml_db
                if result is not None:
                    return result
        except PathNotFoundError:
            pass
        return self._rom_bi.config_xml_db

    @property
    def config_db(self):
        # if patch does not have a real path for _get_config_db()
        # then use the ROM one.
        try:
            if self._patch_bi is not None:
                return self._patch_bi.config_db
        except PathNotFoundError:
            pass
        return self._rom_bi.config_db

    @config_db.setter
    def config_db(self, value):
        'set config_db in the patch if you really know better than the defaults'
        if self._patch_bi is not None:
            self._patch_bi.config_db = value
        else:
            self._rom_bi.config_db = value

    @property
    def mibdb(self):
        # if patch does not have a _common_dir then use the ROM one.
        # return of None means no database available or path not found.
        if self._patch_bi is not None:
            result = self._patch_bi.mibdb
            if result:
                return result
        return self._rom_bi.mibdb

    @property
    def _mibdb_basename(self):
        #friendly access
        #pylint: disable=protected-access
        return self._rom_bi._mibdb_basename

        
