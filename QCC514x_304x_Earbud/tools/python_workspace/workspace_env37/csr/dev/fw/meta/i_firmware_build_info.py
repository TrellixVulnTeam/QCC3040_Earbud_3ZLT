############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import os
import glob
import re
import platform
import shutil
import functools
from fnmatch import fnmatch
import sys
import time
if sys.version_info > (3,):
    long = int
from fnmatch import fnmatch
import codecs

try:
    codecs.lookup_error("surrogateescape")
except LookupError:
    def open_file_unknown_encoding(file, mode='r'):
        # Legacy Python 2 implementation
        return open(file, mode)
else:
    def open_file_unknown_encoding(file, mode='r'):
        # Python 3 handling of Unicode files
        # Source code files can contain non-UTF8 characters.
        # Just allow these to be processed unchanged without raising an error.
        return open(file, mode, encoding="utf-8", errors="surrogateescape")

from csr.wheels.global_streams import iprint, wprint
from csr.wheels.bitsandbobs import PureVirtualError, path_is_windows_remote_mount
from csr.dwarf.read_dwarf import Dwarf_Reader, DwarfNoSymbol
from csr.dwarf.read_elf import Elf_Reader
from .elf_code_reader import ElfCodeReader
from csr.dev.fw.meta.asm_listing import XapAsmListing, KalAsmListing, \
                                        AsmOutOfSyncError
from csr.dev.hw.core.meta.i_layout_info import XapDataInfo
from csr.interface.mibdb import MIBDB
from csr.dev.fw.slt import HydraStubBaseSLT, AppsBaseSLT, AudioBaseSLT, RawSLT

class PathNotFoundError(AttributeError):
    """
    Lookup of an attribute value, determined that the path value
    that it holds or relies on is for a path that does not exist.
    (This could be either a directory or a filename depending on the property).
    """

def get_network_homes(check=False):
    """
    Helper function to return a path to the Unix home directories, which
    happens to be where all the standard firmware builds live.  If check is set
    to True, will see if this is a real directory or not, and raise 
    FirmwareSetupException if not.
    """
    if os.getenv("PYDBG_FIRMWARE_LOOKUP_ENABLED") is None:
        raise IFirmwareBuildInfo.LookupDisabledException(
            "PYDBG_FIRMWARE_LOOKUP_DISABLED is set")

    if platform.system() == "Windows":
        build_root = r"\\root.pri\FileRoot\UnixHomes\home"
    else:
        build_root = "/home"
        build_dir_grid = ("/home/cursw/curator_builds",
                      "/home/appsw/app_ss_builds",
                      "/home/bcsw/bc_builds",
                      "/home/bcsw/qc_builds",
                      "/home/svc-audio-dspsw/kymera_builds")
        for build_dir in build_dir_grid:
            if not os.path.isdir(build_dir):
                IFirmwareBuildInfo.LookupDisabledException("Can't see '%s' on this "
                                                           "machine" % build_dir)
    if check and not os.path.isdir(build_root):
        raise IFirmwareBuildInfo.LookupDisabledException("Can't see '%s' on this "
                                                         "machine" % build_root)
    return build_root


class ToolchainQuirks:
    DWARF_BAD_INLINE_ATTRIBUTES = 0

class BaseToolchain(object):
    NAME="Unspecified"
    FUNC_ADDR_FLAG = 0
    FUNC_MODE_FLAG = 0
    GARBAGE_ADDRESS = None
    GLOBVAR_PREFIXES = tuple()
    LOCALVAR_PREFIXES = tuple()
    GLOBFUNC_PREFIXES = tuple()
    LOCALFUNC_PREFIXES = tuple()
    FUNC_IGNORE_PREFIXES = tuple()
    CU_PREFIXES = tuple()
    QUIRKS=tuple()

class UnknownToolchain(BaseToolchain):
    pass

class KCC32Toolchain(BaseToolchain):
    NAME ="KCC32"
    LARGE_SYM_SENTINEL = 0x0f000000
    FUNC_ADDR_FLAG = 0x80000000
    GARBAGE_ADDRESS = 0xff800000
    GLOBVAR_PREFIXES = ("$_", "__", "_")
    LOCALVAR_PREFIXES = ("L_",)
    GLOBFUNC_PREFIXES =  ("$_", "$M.","$")
    LOCALFUNC_PREFIXES = ("L_", "_", "$M.","$")
    CU_PREFIXES = ("/tmp")

class XAPGCCToolchain(BaseToolchain):
    
    NAME="XAPGCC"
    GLOBVAR_PREFIXES = ("$",)
    FUNC_IGNORE_PREFIXES = ("START_OF_", "END_OF_")

class ARMGCCToolchain(BaseToolchain):

    NAME="ARMGCC"
    FUNC_MODE_FLAG = 0x80000000
    GLOBVAR_PREFIXES = ("$",)
    LOCALFUNC_PREFIXES = ("$",) # Seen in a ARM GCC (or possibly realview) ELF

class GenericARMToolchain(BaseToolchain):
    NAME="GenericARM"
    FUNC_MODE_FLAG = 1 # Thumb mode

class SnapdragonLLVMArmToolchain(BaseToolchain):
    NAME="SNAPDRAGONLLVMARM"
    QUIRKS=(ToolchainQuirks.DWARF_BAD_INLINE_ATTRIBUTES,)

class NoElfError(RuntimeError):
    """
    Thrown when an IFirmwareBuildInfo instance is asked for the ELF file path
    when no ELF file is available, either because there is none present or 
    Pydbg was unable to determine which one to select. 
    """


class FWIDReadError(AttributeError):
    """
    Thrown when unable to read the FW ID from the device.
    """


class BuildDirNotFoundError(AttributeError):
    """
    Thrown when a build directory cannot be found from the build ID of the
    subsystem.
    """


class HydraPanicAndFaultInfo(object):

    def __init__(self, fw_info):
        
        self.panic_names = fw_info.enums["panicid"]
        self.fault_names = fw_info.enums["faultid"]


class IFirmwareBuildInfo (object):
    """\
    Interface to interesting information for a specific firmware build.
    """

    class FirmwareSetupException(RuntimeError):
        pass
    class LookupDisabledException(FirmwareSetupException):
        pass
    class NonBinutilsException(FirmwareSetupException):
        def __init__(self):
            IFirmwareBuildInfo.FirmwareSetupException.__init__(self,
                                            "This appears to be a non-binutils "
                                            "build: no firmware environment available")

    @property
    def _path_anchor(self):
        raise PureVirtualError(self)

    def source_file(self, srcfile):
        """
        Returns the entire contents of the given source file, mapping paths as
        required.

        If you want just a few lines from the file and/or you want decoration
        with headers and line numbers then use source_code().
        """
        raise PureVirtualError(self)

    def get_asm_sym(self):
        # No need to get symbols from assembly listing in general
        pass

def get_elf_cache(cache_dir):
    """
    Create an ELF cache if necessary and possible
    """
    if ("Windows" not in platform.system() or 
        os.getenv("PYDBG_DISABLE_ELF_CACHE") is not None):
        return None

    if not cache_dir:
        if os.getenv("USERPROFILE") is None or not os.path.isdir(os.getenv("USERPROFILE")):
            iprint("WARNING: can't set up ELF cache in default location: check USERPROFILE setting")
            return None
        cache_dir = os.path.join(os.getenv("USERPROFILE"), "appdata",
                                                   "local", "pydbg")
        
    return ElfCache(cache_dir)

class ElfCache(object):
    """
    Class representing a cache of ELF files from remote locations.  An ElfCache
    instance is associated with a single flat cache directory containing copies
    of ELF files (identified solely by basename) and accompanying metadata files
    indicating the original path and the modification time of the original path
    at the time of caching.  This is enough information to ensure that a cached
    file matches a requested file and is up-to-date.

    The class provides two operations:
     - get() returns the path to the cached version of a file.  This implicitly
     pulls the file into the cache if it isn't already present, or is out of date.
     It can return None if it wasn't possible to copy the file for some reason
     (e.g. the provided remote path didn't exist, or writing to the cache
     directory failed), but otherwise it returns the full path to the cached
     copy of the file
     
     - list() provides a directory listing-like facility, including support for
     glob pattern matching.  It returns a list of triples consisting of 
     cache path, remote path and remote path modification time (at the time of
     caching).
    
    """
    def __init__(self, cache_dir, num_slots=None):
        self._cache_dir = cache_dir
        if num_slots is not None:
            self._num_slots = num_slots 
        else:
            slots_env = os.getenv("PYDBG_NUM_ELF_CACHE_SLOTS")
            if slots_env is not None:
                self._num_slots = int(slots_env)
            else:
                self._num_slots = 10 # pick a number...
    
        self._slot_dirs = [os.path.join(self._cache_dir, "slot%d" % slot) for slot in range(self._num_slots)]
        
        # Clean up in case of lingering corrupt caches
        self._delete_stale_key_files()
    
    def _delete_stale_key_files(self):
        """
        For a while there was a bug in the caching code in which it was possible
        for the old .key file to be left hanging around when a new ELF entry 
        with no key replaced an old one with a key.  Because caches are
        persistent by nature, we now need to run this checking code every
        time the ElfCache is constructed, just in case we are seeing a cache
        that still exhibits the corruption.
        """
        key_files = sum((glob.glob(os.path.join(slot_dir, "*.key")) 
                                    for slot_dir in self._slot_dirs), [])
        key_cached = [(k, k.replace(".key",".cached")) for k in key_files]
        for dot_key_file, dot_cached_file in key_cached:
            
            if os.path.getmtime(dot_key_file) < os.path.getmtime(dot_cached_file):
                wprint("{} is stale. Deleting from cache.".format(dot_key_file))
                os.remove(dot_key_file)
                
        
        
        
    
    @property
    def is_populated(self):
        return os.path.exists(self._slot_dirs[0])
    
    def _get_cached_file_metadata(self, cache_meta_file):
        if os.path.isfile(cache_meta_file):
            try:
                with open(cache_meta_file) as meta:
                    cached_path = meta.readline().rstrip()
                    cached_mtime = meta.readline().rstrip()
                    return cached_path, cached_mtime
            except IOError:
                # If there is any failure to read the required 
                # information act as if the file wasn't there at all
                pass
        return None, None


    def _mark_as_used(self, cached_elf_name):
        """
        Write the .used file so we know how recently a file was used
        """
        with open(cached_elf_name + ".used", "w"):
            pass

    def _get_lru_slot(self, basename, free=True):
        """
        Return the slot directory that contains the least recently used ELF file.
        If not all slots have been created, created a new one and return that.
        """
        lru_time = time.time() # now ought to be later than any existing file's
        # modification time
        lru_slot_dir = None
        for slot_dir in self._slot_dirs:
            if not os.path.exists(slot_dir):
                os.makedirs(slot_dir)
                return slot_dir
            elif not os.path.exists(os.path.join(slot_dir, basename+".used")):
                # slot dir exists but doesn't have this ELF file in it
                return slot_dir
            else:
                # Keep track of whether this is the least recently used 
                used_time = os.path.getmtime(os.path.join(slot_dir, basename+".used"))
                if used_time <= lru_time:
                    lru_time = used_time
                    lru_slot_dir = slot_dir
        
        if free:
            # Delete all the files with the given basename in this slot, so that
            # we don't risk leaving any stale files around for the repopulation
            # of the slot with this basename.
            basename_files = glob.glob(os.path.join(lru_slot_dir, basename+".*"))
            for f in basename_files:
                os.chmod(f, 0o700)
                os.remove(f)
        
        # All the slots have an ELF with the given name in them so return the
        # least-recently-used one.
        return lru_slot_dir
    
    def get(self, remote_file_name, check_if_stale=True, key=None):
        """
        Populate the cache with a copy of the given remote file and return the
        full path to it.  Obviously, nothing is copied into the cache if it
        is detected that the given file is already cached and is still up-to-date.
        
        If the remote file doesn't exist, isn't a file, or can't be copied into
        the cached area for some reason (or if anything else triggers an OSError
        during the attempt to copy it) the method returns None.
        """
        cache_stale = True
        if not os.path.isdir(self._cache_dir):
            try:
                os.makedirs(self._cache_dir)
            except OSError as e:
                iprint ("Couldn't create cache dir: '%s': "
                                    "Loading remote file directly" % e)
                return
            
        basename = os.path.basename(remote_file_name)
        for slot_dir in self._slot_dirs:
            
            cached_elf_name = os.path.join(slot_dir, basename)
            last_cached = cached_elf_name + ".cached"
            if os.path.isfile(cached_elf_name):
                # We have an entry with the right basename.  Does it come from
                # the right path?
                last_cached_path, last_cached_mtime = self._get_cached_file_metadata(last_cached)
                if (last_cached_path == remote_file_name and 
                    (not check_if_stale or int(last_cached_mtime) == int(os.path.getmtime(remote_file_name)))):
                    # Both path and (if required) modification time match, so this is
                    # the right file
                    cache_stale = False
                    iprint ("Using local cache %s" % cached_elf_name)
                    self._mark_as_used(cached_elf_name)
                    break
                    
        if cache_stale:
            
            iprint("No matching copy of %s found in cache" % basename)
            
            lru_slot = self._get_lru_slot(basename, free=True)
            
            cached_elf_name = os.path.join(lru_slot, basename)

            # See if there is a compressed copy of the ELF available in the
            # source build directory
            # First find files with names of the form "NAME.elf.XYZ"
            target_file_variants = [f for f in os.listdir(os.path.dirname(remote_file_name)) 
                                    if fnmatch(f, basename+".*")]

            file_to_fetch = remote_file_name
            cache_file_target = cached_elf_name
            # Do any of the extensions suggest a compression scheme that we can
            # handle?  Search in order of best compression first.
            try:
                import lzma
            except ImportError:
                known_compressions = ("bz2", "gz")
            else:
                known_compressions = ("xz", "bz2", "gz")
            for ext in known_compressions:
                compressed_elf = remote_file_name + "." + ext
                compressed_elf_basename = os.path.basename(compressed_elf)
                if (compressed_elf_basename in target_file_variants and
                        (int(os.path.getmtime(compressed_elf)) >=
                         int(os.path.getmtime(remote_file_name)))):
                    file_to_fetch = compressed_elf
                    cache_file_target = os.path.join(lru_slot, compressed_elf_basename)
                    iprint("(Found a {}-compressed version)".format(ext))
                    break

            iprint ("Copying to cache")
            
            import time
            start_time = time.time()
            iprint (" (writing to %s)" % lru_slot)
            try:
                if os.path.isfile(cache_file_target):
                    os.chmod(cache_file_target, 0o640)
                    os.remove(cache_file_target)
                shutil.copy(file_to_fetch, cache_file_target)
            except OSError as e:
                iprint ("Copy to cache failed: '%s': "
                                    "loading remote file directly" % e)
            else:
                iprint (" (copied in %0.2fs)" % (time.time() - start_time))
                
                # Uncompress into the target name if necessary
                if file_to_fetch != remote_file_name:
                    if file_to_fetch.endswith(".xz"):
                        import lzma
                        decompress = lzma.decompress
                    elif file_to_fetch.endswith(".bz2"):
                        import bz2
                        decompress = bz2.decompress
                    elif file_to_fetch.endswith(".gz"):
                        import gzip
                        try:
                            decompress = gzip.decompress
                        except AttributeError:
                            # nasty fallback because Py2 gzip doesn't support
                            # a one-shot decompress
                            def decompress(bytes):
                                from StringIO import StringIO
                                bytes_file = StringIO(bytes)
                                gzip_file = gzip.GzipFile(fileobj=bytes_file)
                                return gzip_file.read()
                        
                    iprint(" (Uncompressing)")
                    with open(cache_file_target, "rb") as compressed:
                        decompressed_data = decompress(compressed.read())
                    if os.path.isfile(cached_elf_name):
                        os.chmod(cached_elf_name, 0o640)
                        os.remove(cached_elf_name)
                    iprint(" (Writing to {})".format(cached_elf_name))
                    with open(cached_elf_name, "wb") as decompressed:
                        decompressed.write(decompressed_data)

                with open(cached_elf_name+".cached", "w") as meta:
                    meta.write(remote_file_name+"\n")
                    # note: we just take the integer part of the modification time
                    # to avoid any complications with floating point representations
                    # leading to spurious misses. 
                    meta.write("%d\n" % int(os.path.getmtime(remote_file_name)))
                if key is not None:
                    # If a key was provided, cache that too
                    with open(cached_elf_name+".key", "w") as keyf:
                        keyf.write(str(key)+"\n")
                self._mark_as_used(cached_elf_name)
                cache_stale = False
        elif key is not None and not os.path.isfile(cached_elf_name+".key"):
            # The file isn't stale but the entry predates the keying mechanism
            # so we need to add that file
            with open(cached_elf_name+".key", "w") as keyf:
                keyf.write(key+"\n")
            
        if not cache_stale:
            return cached_elf_name
        
    def list(self, glob_pattern=None, brief=False):
        """
        List the cached files: the return value is a list of 
        (cache file, cached path, cached path modification time) triples.
        If glob_pattern is supplied, only cached_paths matching the pattern
        are listed.
        """
        cache_meta_list = sum((glob.glob(os.path.join(slot_dir, "*.cached")) for slot_dir in self._slot_dirs), [])
        file_list = []
        for cache_meta_file in cache_meta_list:
            cached_path, cached_mtime = self._get_cached_file_metadata(cache_meta_file)
            if glob_pattern is None or fnmatch(cached_path, glob_pattern):
                cache_file = cache_meta_file[:-7]
                if brief:
                    file_list.append(cached_path)
                else:
                    file_list.append((cache_file, cached_path, cached_mtime))
    
        return file_list

    def key_mapping(self, brief=False):
        """
        Return a mapping of keyed ELFs to the full ELF path
        """

        mapping = {}
        for keyf in sum((glob.glob(os.path.join(slot_dir, "*.key"))
                         for slot_dir in self._slot_dirs), []):
            with open(keyf) as keyf_file:
                key = keyf_file.read().strip()
            cache_meta_file = keyf.replace(".key", ".cached")
            cached_path, cached_mtime = self._get_cached_file_metadata(
                cache_meta_file)
            cache_file = cache_meta_file[:-7]
            if brief:
                mapping[key] = cached_path
            else:
                mapping[key] = (cache_file, cached_path, cached_mtime)
        return mapping

class BadElfSectionName(ValueError):
    """
    Indicates that a name given as a unique ELF section name wasn't (either
    not present or ambiguous)
    """

class FirmwareBuildInfoElfMixin (object):
    """
    Mixin for standard ELF-based functionality.
    
    Provides the following attributes/methods:
     - dwarf
     - elf_reader
     - load_program_cache
     - elf_code
     - elf_file (including caching of remotely mounted files)
     - elf (deprecated)
      
    Relies on the following attributes/methods:
     - _elf_file (The raw original ELF file path)
    """

    # Normally the symbols in an ELF file are valid for construction a 
    # firmware environment, but not always - sometimes the running firmware is
    # a heavily post-processed version of what the ELF says 
    elf_symbols_valid = True
    
    @property
    def elf(self):
        """\
        ELF (not DWARF) interface.
        
        Prefer more abstract debug_info.
        """
        try:
            self._elf
        except AttributeError:
            if not os.path.exists(self.elf_file):
                raise IFirmwareBuildInfo.FirmwareSetupException("Can't find elf %s" % self.elf_file)
            from ext.bsd.bintools.elf import ELF
            self._elf = ELF(self.elf_file)
        return self._elf

    @property
    def dwarf_sym(self):
        """
        DWARF info from the same file that the ELF symbol table is taken from
        """
        try:
            self._dwarf_sym
        except AttributeError:
            def filter_out(tp):
                bad_set = getattr(self, "_bad_%s_names"%tp)
                def _filter(n):
                    return not any(n.startswith(r) for r in bad_set)
                return _filter
            include_abstract_symbols = (ToolchainQuirks.DWARF_BAD_INLINE_ATTRIBUTES 
                                             in self.toolchain.QUIRKS)
            self._dwarf_sym = Dwarf_Reader(self.elf_reader,
                                    name_filters={"vars" : filter_out("var"),
                                                  "funcs" : filter_out("func")},
                                    # This is correct for all platforms we care
                                    # about now and are likely to care about in future
                                    # Note: DWARF uses byte to mean 8 bits regardless of the size
                                    # of addressable units on the target platform
                                    ptr_size=self._layout_info.data_word_bits//8,
                                    include_abstract_symbols=include_abstract_symbols)

        return self._dwarf_sym

    @property
    def dwarf_full(self):
        """
        Full set of DWARF info.  Usually this is the same as the DWARF info in
        the file the symbol table comes from, but it might not be
        """
        return self.dwarf_sym
    
    def _cache_elf_if_necessary(self):
        """
        If the file doesn't appear to be hosted locally, copy it to a local
        directory and reset the elf_filename
        """
        self._cached_elf_name = None
        # Don't try to cache non-existent file.
        # User can turn this off in their environment in any case.
        if (path_is_windows_remote_mount(self.elf_file) and self._cache is not None):
            # If we've autolocated the build, we just trust that it is a real file.
            # This avoids an almost certainly unnecessary network access.
            if self._autolookup_key is not None or os.path.isfile(self.elf_file):
                iprint ("Detected remotely mounted file '%s'" % self.elf_file)
                # If the build was autolocated, tell the cache that we know the
                # file won't be stale, because a file's being auto-locatable
                # implies that it isn't one that might be overwritten.  This
                # saves another unnecessary network access.
                key = str(self._autolookup_key) if self._autolookup_key is not None else None
                self._cached_elf_name = self._cache.get(self.elf_file,
                                                        check_if_stale=self._autolookup_key is None,
                                                        key=key)
    
    @property
    def elf_file(self):
        
        try:
            self._cached_elf_name
        except AttributeError:
            self._cached_elf_name = None
            self._cache_elf_if_necessary()
        if self._cached_elf_name is not None:
            return self._cached_elf_name
        return self._elf_file
        
    def load_program_cache(self, prog_cache):
        """
        Load program space from the ELF
        Note: this is *not* suitable for loading code into a DUT!  It is purely
        for populating a local cache of program memory, such as is maintained
        by the address space mechanism when running on a coredump
        """
        for loadable_section in self.elf_code.sections:
            addr = loadable_section.paddr
            section_data = loadable_section.data
            prog_cache[addr: addr + len(section_data)] = section_data

    @property
    def elf_code(self):
        return ElfCodeReader(self.elf_reader, self._layout_info)

    @property
    def elf_reader(self):
        try:
            self._elf_reader
        except AttributeError:
            self._elf_reader = Elf_Reader(self.elf_file, self.toolchain)
        return self._elf_reader

    def get_elf_section_bounds(self, target_sec_name=None, index=None):
        """
        Get start and end addresses of the named section.  If the name is 
        not the name of a section or is the name of more than one section, 
        BadElfSectionName is raised.
        
        If name is not supplied (e.g. because it is ambiguous) the corresponding 
        index must be supplied instead.
        """
        try:
            self._elf_section_table
        except AttributeError:
            self._elf_section_table = self.elf_reader.get_section_table()

        if target_sec_name is not None:
            matching_sections = [(sec_addr, sec_size) 
                                 for (sec_name, sec_addr, sec_size, _) in 
                                      self._elf_section_table if sec_name == target_sec_name]
            if len(matching_sections) == 0:
                raise BadElfSectionName("No section named '{}'".format(target_sec_name))
            
            if len(matching_sections) > 1:
                raise BadElfSectionName("Ambiguous section name '{}'".format(target_sec_name))
            
            sec_addr, sec_size = matching_sections[0]
        else:
            _, sec_addr, sec_size, _ = self._elf_section_table[index]
            
        return sec_addr, sec_addr + sec_size

    @property
    def _elf_file(self):
        """
        The name of the uncached ELF file.
        Derived classes may raise NoElfError or AttributeError.
        """
        raise PureVirtualError


class IGenericHydraFirmwareBuildInfo (IFirmwareBuildInfo):
    """
    Interface to interesting information for any instance of Hydra firmware,
    whether it's from a single build or multiple builds (e.g. a patched ROM)
    """
    
    # Extensions

    @property
    def panic_and_fault_info(self):
        """\
        Panic and Fault information (PanicAndFaultInfo)
        """
        raise PureVirtualError(self)
        
    @property
    def debug_strings(self):
        raise PureVirtualError(self)
     
    @property
    def debug_string_packing_info(self):
        raise PureVirtualError(self)
        
    @property    
    def config_xml_db(self):
        raise PureVirtualError(self)
    
    @property    
    def config_db(self):
        raise PureVirtualError(self)

    @property
    def mibdb(self):
        raise PureVirtualError(self)

    @property
    def _mibdb_basename(self):
        raise PureVirtualError(self)
    
    @property
    def _log_firm_basename(self):
        raise PureVirtualError(self)
    
    @property
    def slt_type(self):
        raise PureVirtualError(self)

class BaseGenericHydraFirmwareBuildInfo (IGenericHydraFirmwareBuildInfo, 
                                         FirmwareBuildInfoElfMixin):
    """\
    Interface to interesting information applicable to a specific _development_
    build of curator + hydra stub firmware.
    
    Told the development build directory this class knows how to find useful
    bits of related information and parse into shape.
    
    If development tree organisation changes then this would need to be
    updated.
    
    Most information is loaded on demand.
    """
    # Developer system label used by configcmd
    dev_system_label = "DevIDString"
    require_elf = True

    class Executable(object):
        "Provide accessors for subsystem executable name and full path"
        # This is a convenience class for users of class pydbg_config.Config.

        #pylint: disable=too-few-public-methods
        def __init__(self, bi):
            "Initialise subsystem executable name and full path"
            self._bi = bi

        @property
        def fullpath(self):
            "Full path to executable"
            return self._bi.xuv_file

        @property
        def name(self):
            "Name of executable file"
            return os.path.basename(self._bi.xuv_file)

    def __init__(self, build_dir_or_id_func=None, data_layout_info=None,
        interface_dir=None, chip_name=None, multi_elf_dir=None, cache_dir=None,
        autolookup_key=None):
        self._chip_name = chip_name

        self._hips = {}
        self._hip_enums = {}
        self._layout_info = data_layout_info
        self._interface_dir = interface_dir
        self.executable = self.Executable(self)

        self.multi_elf_dir = multi_elf_dir
        if self.multi_elf_dir:
            default_elf = os.path.join(multi_elf_dir, self._elf_file_default_basename)
            if os.path.exists(default_elf):
                build_dir_or_id_func = default_elf
            
        self._build_id_func = None
        self._cache = get_elf_cache(cache_dir)

        self._autolookup_key = autolookup_key

        if callable(build_dir_or_id_func):
            self._build_id_func = build_dir_or_id_func
            if multi_elf_dir:
                # Try and find matching file based on the given build_id_func
                self._build_dir = functools.partial(self._match_elf_version,
                                                    self._build_id_func,
                                                    multi_elf_dir,
                                                    self.require_elf)
            else:
                # Check that the network is visible unless the user has enabled
                # network-free cache use and has a non-empty cache to resort to.
                # This makes for less helpful error messages when the network
                # turns out to actually be needed for retrieving a file, but 
                # avoids artificially preventing access to a cached ELF just
                # because the network has temporarily gone away.
                # Note: we use a simple memoisation strategy on the assumption
                # that there's no point checking whether the network is visible
                # for every subsystem we construct.
                try:
                    prev_result = get_network_homes._result
                except AttributeError:
                    try:
                        get_network_homes(check=os.getenv("PYDBG_ENABLE_NETWORK_FREE_CACHE_USE") is None or 
                                          not self._cache.is_populated)
                        get_network_homes._result = True
                    except IFirmwareBuildInfo.LookupDisabledException as exc:
                        if (os.getenv("PYDBG_ENABLE_NETWORK_FREE_CACHE_USE") is None and 
                                          self._cache.is_populated):
                            iprint("\nNetwork not visible but not-empty ELF cache found. "
                                   "\nSet PYDBG_ENABLE_NETWORK_FREE_CACHE_USE to any value "
                                   "in your environment to enable ELF auto-lookup from cache\n")
                        get_network_homes._result = exc
                        raise
                else:
                    if prev_result is not True:
                        raise prev_result
                    
                self._failed_build_ids = set()
        elif autolookup_key is not None:
            if build_dir_or_id_func.endswith(".elf"):
                self._build_dir, self._supplied_elf_basename = os.path.split(build_dir_or_id_func)
                self._supplied_build_file_nameroot = os.path.splitext(self._supplied_elf_basename)[0]
            else:
                self._build_dir = build_dir_or_id_func
        else:
            build = os.path.normpath(build_dir_or_id_func)
            if os.path.isdir(build):
                self._build_dir = build # _elf_file_default_basename needs this
                if not os.path.isfile(os.path.join(build, 
                                                self._elf_file_default_basename)):
                    elf_files = glob.glob(os.path.join(build, "*.elf"))
                    if len(elf_files) != 1:
                        if self.require_elf:
                            raise ValueError("Couldn't find exactly 1 elf file in given "
                                             "directory {}".format(self._build_dir))
                    else:
                        self._build_dir, self._supplied_elf_basename = os.path.split(elf_files[0])
                        self._supplied_build_file_nameroot = os.path.splitext(self._supplied_elf_basename)[0]
                else:                
                    self._build_dir = build
                
            elif os.path.isfile(build):
                self._build_dir, self._supplied_elf_basename = os.path.split(build)
                self._supplied_build_file_nameroot = os.path.splitext(self._supplied_elf_basename)[0]
            else:
                raise IOError("'%s' is not a file or directory" % build)

    @property
    def _dir(self):
        """
        Return the path to the build directory, possibly deriving it from the
        build ID string using subsystem-specific rules.
        """

        try:
            if callable(self._build_dir):
                self._build_dir, self._supplied_elf_basename = self._build_dir()
                self._supplied_build_file_nameroot = os.path.splitext(self._supplied_elf_basename)[0]
            return self._build_dir
        except AttributeError:
            try:
                build_string, build_id = self._build_id_func()
            except Exception as e:
                raise FWIDReadError("Error reading fw id: '%s'" % e)
            build_dir = None
            if build_string is not None and build_string not in self._failed_build_ids:
                build_dir = self.key_cache_lookup(build_string) or self.build_from_id(id=build_string, cache=self._cache)
                if build_dir is not None:
                    self._autolookup_key = build_string
            elif build_id is not None and build_id not in self._failed_build_ids:
                build_dir = self.key_cache_lookup(build_id) or self.build_from_id(id_number=build_id, cache=self._cache)
                if build_dir is not None:
                    self._autolookup_key = build_id
            if build_dir is None:
                self._failed_build_ids.add(build_id)
                raise BuildDirNotFoundError("Couldn't find build directory "
                                            "from fw id %s" % str(build_id))
            elif build_dir.endswith(".elf"):
                # We got the file rather than the directory it's in
                build_dir, self._supplied_elf_basename = os.path.split(
                    build_dir)
            self._build_dir = build_dir
        return self._build_dir

    def key_cache_lookup(self, key):
        """
        See if the supplied key matches anything in the ELF cache (if there is
        one)
        """
        if self._cache is None:
            return None
        key_mapping = self._cache.key_mapping(brief=True)
        try:
            return key_mapping[key]
        except KeyError:
            return None
        
        

    # IGenericHydraFirmwareBuildInfo compliance
    @property
    def panic_and_fault_info(self):
        try:
            self._panic_and_fault_info
        except AttributeError:
            self._panic_and_fault_info = self._create_panic_and_fault_info()
            
        return self._panic_and_fault_info
    
    @property
    def debug_strings(self):
        
        return self._read_debug_strings()
    
    @property
    def xuv_file(self):
        return os.path.join(self.build_dir, self._xuv_file_basename)
    
    @property
    def elf_slt(self):
        return self.slt_type.generate(self.elf_code)
    
    # Protected / overrideable
    @property
    def _elf_file(self):
        """
        Can raise NoElfError if no ELF available.
        Can of course raise AttributeError if dependent attributes not available.
        """
        elf_path = os.path.join(self.build_dir, self._elf_file_basename)
        # If the build was autolocated we don't waste our time checking it's a
        # genuine path.
        if self._autolookup_key is None and not os.path.isfile(elf_path):
            raise NoElfError("Requesting ELF file but there is no ELF available for this build ({})".format(elf_path))
        return elf_path

    @property
    def _elf_file_basename(self):
        """
        The actual ELF basename. If a full ELF file path has been supplied, this
        is the filename part. If not, it's the standard known name for builds of
        this subsystem/core.
        """
        try:
            return self._supplied_elf_basename
        except AttributeError:
            return self._elf_file_default_basename

    @property
    def _build_file_nameroot(self):
        try:
            return self._supplied_build_file_nameroot
        except AttributeError:
            return self._default_build_file_nameroot
    
    @property
    def _default_build_file_nameroot(self):
        return "proc"
    
    @property
    def _elf_file_default_basename(self):
        return self._build_file_nameroot + ".elf"
    
    @property
    def _asm_listing_basename(self):
        return self._build_file_nameroot + ".lst"

    @property
    def _xuv_file_basename(self):
        return self._build_file_nameroot + ".xuv"

    @property
    def _path_anchor(self):
        """
        A path fragment that is likely to be found in any copy of the
        subsystem's source tree, used to simplify path mapping between
        different environments
        """
        return "fw"
    
    @property
    def _log_firm_basename(self):
        return "hydra_log_firm.c"

    @property
    def _common_dir(self):
      """
      Path to the hydra common folder; 
      This is determined from the _root_dir which may be different
      for some subsystems.
      """
      return os.path.join(self._root_dir, 'common')

    @property
    def _mib_dir(self):
      """
      Path to the hydra mib folder; 
      This is determined from the _root_dir which may be different
      for some subsystems.
      """
      return os.path.join(self._common_dir, 'mib')

    def _create_panic_and_fault_info(self):
        return HydraPanicAndFaultInfo(self.dwarf_full)

    def _read_debug_strings(self):
        return self.elf_reader.get_debug_strings(1,False)
    
    def _create_asm_listing(self):
        """
        Assume by default we have a XAP
        """
        asm_path = os.path.join(self._dir, self._asm_listing_basename)
        return XapAsmListing(asm_path)
        
    def _get_config_db(self):
        'Overridable routine to provide full path to config sdb'
        raise PureVirtualError(self)  
    
    def _create_mibdb(self):
        try:
            try:
                mibmeta_path = os.path.join(self._mib_dir, self._mibdb_basename)
            except PathNotFoundError:
                raise IOError
            return MIBDB(mibmeta_path)
        except IOError:
            # File doesn't exist in the conventional mapping from the elf
            # location so try in the directory the elf is in.
            try:
                return MIBDB(os.path.join(self._dir, self._mibdb_basename))
            except IOError:
                return None
                
    def _get_build_id(self):
        '''
        Utility function to find the build id from path.
        By default not implemented.
        '''
        raise PureVirtualError(self)

    def _get_build_ver(self):
        '''
        Utility function to supply the build version.
        By default implemented in terms of _get_build_id()
        which tries to get it from the path name.
        '''
        if getattr(self, '_build_ver', None) is None:
            setattr(self, '_build_ver', self._get_build_id())
        return self._build_ver

    def _match_elf_version(self, build_id_func, build_dir, require_elf):
        """
        Find the elf file in the given build paths which matches the
        given build id. Raises exception if no match found.
        """

        build_string, build_id = build_id_func()

        if build_string:
            iprint("(Attempting to find fw build '{}' in {})".format(build_string,
                                                                  build_dir))
        else:
            iprint("(Attempting to find fw build '{}' in {})".format(build_id,
                                                                  build_dir))
        elf_files = glob.glob(os.path.join(build_dir, "*.elf"))

        matching_elf_files = []
        for elf_file in elf_files:
            elf_reader = Elf_Reader(elf_file, self.toolchain)
            elf_code = ElfCodeReader(elf_reader, self._layout_info)

            try:
                elf_slt = self.slt_type(elf_code)
                if (build_string is not None and elf_slt.build_id_string == build_string or
                    build_id == elf_slt.build_id_number):
                    matching_elf_files.append(os.path.split(elf_file))
            except RawSLT.BadFingerprint:
                # Assume this elf is not for this subsystem
                pass

        if len(matching_elf_files) == 1:
            iprint("Found matching file: {}".format(os.path.join(*matching_elf_files[0])))
            return matching_elf_files[0]
        elif len(matching_elf_files) > 1:
            if require_elf:
                matching_file_names = [m[1] for m in matching_elf_files]
                raise ValueError("Found multiple matching elf files ({}) that match current "
                                 "fw in given directory".format(", ".join(matching_file_names)))
        else:
            if require_elf:
                raise ValueError("Could not find a matching elf file in given directory")

    @property
    def _src_root_for_rebuild(self):
        """
        returns tuple (_root_dir, rel_path_to_rootdir_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """
        raise PureVirtualError(self)

    # Protected / provided

    def _build_rel_path(self, path):
        """
        return a path that ought to be relative to the self._root_dir
        (though this routine does not check that, see map_file)
        by making use of (one of) the self._path_anchor(s)
        or else raise ValueError if (one of) the anchor(s) is not found in path.
        This routine now copes with self_path_anchor being either a single or
        list of values; (the need arises due to several firmware tree relayouts
        during development lifetime of a firmware common platform)
        """

        def build_one_rel_path(anchor, path_cmpts):
            """
            return path from path_cmpts made relative to (one value from) the
            self._path_anchor or None if no match.
            """

            anchor_cmpts = anchor.split("/")
            cur = 0
            for ibp, cmpt in enumerate(path_cmpts):
                if anchor_cmpts[cur] == cmpt:
                    cur += 1
                    if cur == len(anchor_cmpts):
                        rel_cmpts = path_cmpts[ibp+1-len(anchor_cmpts):]
                        return os.path.sep.join(rel_cmpts)
                else:
                    # Didn't match: back to the beginning
                    cur = 0
                    if anchor_cmpts[cur] == cmpt:
                        cur += 1
            return None

        # We don't split on os.path.sep because path may not be tied to the OS;
        # split on what works.
        sep = "/"
        path_cmpts = path.split(sep)
        if path_cmpts[0] == path:
            sep = "\\"
            path_cmpts = path.split(sep)
        # _path_anchor may be a list instead of a single value
        path_anchor = self._path_anchor
        if isinstance(path_anchor, list):
            for anchor in path_anchor:
                result = build_one_rel_path(anchor, path_cmpts)
                if result:
                    break
            else: # exhausted all anchors
                msg = "an anchor from '{}'".format(path_anchor)
        else:
            result = build_one_rel_path(path_anchor, path_cmpts)
            msg = "anchor '{}'".format(path_anchor)
        if not result:
            raise ValueError("Didn't find {} in the supplied path '{}'".format(
                msg, path))
        return result

    @property
    def _root_dir(self):
        """\
        Development tree root for this development build.
        """
        # firmware build is 8 levels deep
        root_dir = os.path.join(self._dir, 
                            '..', '..', '..', '..', '..', '..', '..', '..')
        root_dir = os.path.normpath(root_dir)
        return root_dir
        
    def _set_build_ver_check(self, value):
        '''
        set build_ver if you really know better than what
        self._get_build_id can determine (e.g. non release build)
        '''
        try:
            if self._build_ver and (self._build_ver != value):
                iprint('Warning: changing {}.build_ver from {} to {}'.
                      format(self.__class__.__name__, self._build_ver, value))
        except AttributeError:
            pass
        self._build_ver = value

    def _validate_asm_listing(self, listing, elf_addr_to_pc=lambda x:x, 
                              verbose=True):
        """
        Look up the ASM listing functions in the DWARF and check that all the
        addresses match (it's possible the set of functions won't match
        exactly, but as long as there's only a handful that aren't found we'll
        accept it)
        """
        count = 0
        not_found = []
        for func, addr in listing.functions.items():
            count += 1
            try:
                if elf_addr_to_pc(self.dwarf_sym.get_function(func).address) != addr:
                    if verbose:
                        iprint("DWARF has '%s' at 0x%x, but ASM listing has it "
                               "at 0x%x" % (func, 
                                         self.dwarf_sym.get_function(func).address, 
                                         addr))
                    return False
            except DwarfNoSymbol:
                not_found.append(func)
        if len(not_found) > count/10:
            if verbose:
                iprint("Failed to find %d of %d ASM functions in the "
                       "DWARF: %s" % (len(not_found), count, not_found))
            return False
        return True


    #Extensions

    @property
    def asm_listing(self):
        """
        Get hold of an object interfacing to the assembly listing
        """
        try:
            self._asm_listing
        except AttributeError:
            self._asm_listing = self._create_asm_listing()
        return self._asm_listing

    @property
    def config_xml_db(self):
        try:
            mibmeta_path = os.path.join(self._mib_dir, self._mibdb_basename)
        except PathNotFoundError:
            mibmeta_path = None
        if mibmeta_path is None or not os.path.exists(mibmeta_path):
            # File doesn't exist in the conventional mapping from the elf
            # location so try in the directory the elf is in.
            mibmeta_path = os.path.join(self._dir, self._mibdb_basename)
            if not os.path.exists(mibmeta_path):
                return None
        return mibmeta_path

    @property
    def config_db(self):
        """
        Path to database that defines the .sdb file used by configcmd 
        containing keys for a developer system label of dev_system_label.  
        Typically this is mib.sdb in the same folder as the executable.
        """
        try:
            self._config_db
        except AttributeError:
            self._config_db = self._get_config_db()
        return self._config_db

    @config_db.setter
    def config_db(self, value):
        'set config_db if you really know better'
        self._config_db = value

    @property
    def build_dir(self):
        '''
        Accessor for xIDE macros that need to do their own loading of stuff
        '''
        return self._dir

    @property
    def build_path(self):
        """
        Returns the build path supplied either on command line, or in
        a pydbg config file as used by pydbg_config.Config.  If a elf filename
        was supplied then it is also included in the returned path, which is
        how this may differ from the build_dir property.
        """
        if hasattr(self, '_supplied_elf_basename'):
            return os.path.join(self.build_dir, self._supplied_elf_basename)
        return self.build_dir

    @property
    def build_type(self):
        '''
        Accessor for determining the build type based on leaf of build_dir
        '''
        return os.path.basename(self._dir)

    @property
    def build_ver(self):
        '''
        Accessor for obtaining the build ver
        '''
        return self._get_build_ver()

    @build_ver.setter
    def build_ver(self, value):
        '''
        set build_ver if you really know better than what
        self._get_build_id can determine (e.g. non release build)
        '''
        self._set_build_ver_check(value)
        
    @property
    def debug_string_packing_info(self):
        return {"section_name" : "debug_strings",
                "packed" : False,
                "field_width" : 16,
                "bigendian" : True}
        
    @property
    def make_root(self):
        """
        returns rel_path_for_make providing
        directory relative to executable's src_root in which to 
        invoke make to rebuild it
        """
        return self._src_root_for_rebuild[1]
        
    def map_path(self, to_map):
        """
        Map the supplied path into the correct format for the local session.
        E.g. the DWARF for a release build of the Curator might contains paths
        in the form
        
        /home/cursw/curator_builds/builds/main_123/<build path>
        
        whereas a local path might look like
        
        G:\cursw\curator_builds\builds\main_123\<build_path_with_backslashes>
        
        We use the supplied path_anchor to try to find ourselves a match 
        """
        
        rel_path = self._build_rel_path(to_map)
        
        return os.path.sep.join([self._root_dir, rel_path])

    @property
    def mibdb(self):
        try:
            self._mibdb
        except AttributeError:
            self._mibdb = self._create_mibdb()
        return self._mibdb
               
    @classmethod
    def munge_build_dir(cls, dirname):
        """
        No munging needed by default
        """
        return dirname

    def source_file_path(self, srcfile):
        """
        Return the path for the specified source file, mapping paths as
        required.

        If you want the contents of the file then use source_file() or
        source_code().
        """
        # See if they gave us an absolute path or a path relative to a
        # well known location.
        paths = [
            os.path.normpath(srcfile),
            os.path.normpath(os.path.join(self.build_dir, srcfile)),
            os.path.normpath(os.path.join(self.src_root, srcfile))]

        for path in paths:
            if os.path.isfile(path):
                return path

        tried = set(paths)
        info = []

        # Can we manipulate the path to correct for build location versus
        # storage location differences in Dwarf file locations?
        try:
            mfile = self.map_path(srcfile)
            if os.path.isfile(mfile):
                return mfile
            tried.add(mfile)
        except ValueError as mapfail:
            info.append("Couldn't remap: " + str(mapfail))

        tried.discard(srcfile)

        if tried:
            info.append("also tried: " +
                        ", ".join("'" + path + "'" for path in tried))

        msg = "Can't find '%s'" % srcfile
        if info:
            msg += " (" + "; ".join(info) + ")"

        raise ValueError(msg)

    def source_file(self, srcfile):
        """
        Returns the entire contents of the given source file, mapping paths as
        required.

        If you want just a few lines from the file and/or you want decoration
        with headers and line numbers then use source_code().
        """
        srcfile = self.source_file_path(srcfile).lstrip(":")

        with open_file_unknown_encoding(srcfile) as src:
            return src.read()

    def source_code(self, srcfile, lineno, context_lines):
        """
        Return the specified lines from the specified source file, mapping paths
        as required.

        If you want the entire contents of the file then use source_file().
        """
        srcfile = self.source_file_path(srcfile)

        text = ""
        text += " (%s:%d)" % (os.path.sep.join(
                                    srcfile.split(os.path.sep)[-2:]), lineno)
        if context_lines:
            text += "\n"
            extra_lines = context_lines - 1
            srcfile = srcfile.lstrip(":")
            with open(srcfile) as src:
                lines = src.readlines()
                start_line = max(lineno - extra_lines, 1)
                end_line = min(lineno + extra_lines, len(lines))
                for line in range(start_line - 1,
                                   end_line):
                    text += "\n%d: %s" % (line, lines[line].rstrip())
            text += "\n"
        return text

    @property
    def src_root(self):
        """
        returns tuple (_rootdir, rel_path_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """
        return self._src_root_for_rebuild[0]

    @property
    def src_ver(self):
        '''
        Accessor for obtaining the source repository label for this build
        if known otherwise None.
        '''
        raise PureVirtualError(self)  
        
    @property
    def supports_elf(self):
        "Whether has an elf file"
        try:
            return os.path.exists(self.elf_file)
        except NoElfError:
            return False

    @property
    def _bad_var_names(self):
        """
        Hydra firmware typically contains lots of "log_fmt" variables that 
        aren't present in the running firmware. We need to filter them out of
        the DWARF
        """
        return set(["log_fmt"])
    
    @property
    def _bad_func_names(self):
        return set()


class HydraStubFirmwareBuildInfo (BaseGenericHydraFirmwareBuildInfo):
    """
    Interface to debugging information applicable to a specific development
    build (xuv + friends) of hydrastub firmware.
    """
    @property
    def _mibdb_basename(self):
        return "hydra_mib.xml"

    @property
    def slt_type(self):
        return HydraStubBaseSLT
        
class HydraAudioStubFirmwareBuildInfo(HydraStubFirmwareBuildInfo):
    
    @property
    def toolchain(self):
        return KCC24Toolchain

    @property
    def _build_file_nameroot(self):
        return "audio-cpu"

class KalAsmMixin(object):
    def _create_asm_listing(self):
        """
        Override the default XAP ASM parser with a Kalimba one
        """
        asm_path = os.path.join(self._dir, self._asm_listing_basename)
        if not os.path.exists(asm_path):
            raise ValueError("%s doesn't exist" % asm_path)
        listing = KalAsmListing(asm_path)
        if not self._validate_asm_listing(
                   listing,
                   elf_addr_to_pc=
                    (lambda x: x & (~self.toolchain.FUNC_ADDR_FLAG)),
                   verbose=False):
            raise AsmOutOfSyncError(".lst file has function addresses that "
                                    "don't match the DWARF")
        return listing


class HydraAppsFirmwareBuildInfo(KalAsmMixin, BaseGenericHydraFirmwareBuildInfo):

    @property
    def toolchain(self):
        return KCC32Toolchain

    @property
    def slt_type(self):
        return AppsBaseSLT

    @property
    def _root_dir(self):
        """\
        Development tree root for this development build.
        """
        # firmware build is 3 levels deep
        root_dir = os.path.join(self._dir, '..', '..', '..') 
        root_dir = os.path.normpath(root_dir)
        return root_dir

    def _get_config_db(self):
        """
        Path to database that defines the .sdb file used by configcmd 
        containing keys for a developer system label of dev_system_label.  
        Typically this is mib.sdb in the same folder as the executable.
        """
        return os.path.join(self.build_dir, 'core', 'mib', 'mib.sdb')

    @property
    def _mibdb_basename(self):
        return "app_mib.xml"

    @staticmethod
    def _munge_build_dir(build_dir, icore):
        """
        This is a simple-minded attempt to replace the integer that represents the
        core in the final component of the supplied build dir with the one 
        corresponding to this core.
        
        Assumptions:
         - there are exactly two cores
         - the build directory component contains "p<n>" to indicate core n
        """
        basename = os.path.basename(build_dir)
        if not basename:
            build_dir = os.path.dirname(build_dir)
            basename = os.path.basename(build_dir)
        
        if ("p%d" % (1-icore)) in build_dir:
            new_basename = basename.replace("p%d" % (1-icore), "p%d" % icore)
            return build_dir.replace(basename, new_basename)
            
        return build_dir

    def _build_file_nameroot_helper(self, core_nr):
        if self._chip_name:
            app_elf_prefix = self._chip_name.lower()
        else:
            found  = re.search('\%s([a-z]+)_p%d_d' % (os.path.sep, core_nr), self._dir)
            if found :
                app_elf_prefix = found.group(1)
        return "%s_app_p%d" % (app_elf_prefix, core_nr)

    @property
    def _src_root_for_rebuild(self):
        """
        returns tuple (_rootdir, rel_path_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """
        return (self._root_dir, os.path.join('fw', 'src'))

    @property
    def src_ver(self):
        '''
        Accessor for obtaining the source repository label for this build
        if known otherwise None.
        '''
        src_ver = getattr(self, '_src_ver', None)
        if src_ver is not None:
            return src_ver
            
        # Create the perforce label string
        self._src_ver = self.build_type +'_' + str(self.build_ver)

        if self.build_ver >= 390:
            self._src_ver = 'hydra_apps_' + self._src_ver
        return self._src_ver

    @src_ver.setter
    def src_ver(self, value):
        'set src_ver if you really know better'
        self._src_ver = value        
        
    def _get_build_id(self):
        '''
        Utility function to find the build id from path
        '''
        regex = r'.*_(\d+)' 
        pattern = re.compile(regex)
        match = pattern.match(self._root_dir)
        return int(match.group(1)) if match else None

    # From this build ver there can be multiple build targets in the same build
    # folder (different cores) and even ones with differing chip_name values.
    _MULTIPLE_BUILDS_VER_THRESHOLD = 2725

    def build_from_id(self,
                      # pylint: disable=invalid-name,redefined-builtin
                      id=None,
                      id_number=None, cache=None,
                      chip_name=None):
        """
        Find the Apps fw build with the given ID, if any; or use any
        build number passed in id_number or id.
        chipname is a value like "QCC514X_QCC304X" (case is ignored) or "*".
        From build 2725 there can be multiple build targets in the same build
        folder (different cores) and even ones with differing chip_name values.
        """

        if not id and id_number is None:
            raise ValueError("Cannot look up build without an id or id_number")
        if id_number:
            id = id_number
        if isinstance(id, (int, long)):
            id_string = str(id)
            # Below this version, we only ever match at most one build
            if id < self._MULTIPLE_BUILDS_VER_THRESHOLD:
                core = '?'
                chip_name = '*'
            else:
                try:
                    core = str(self.CORE_ID)
                except AttributeError:
                    raise NotImplementedError(
                        "You have to call this method on "
                        "HydraAppsP0FirmwareBuildInfo "
                        "or HydraAppsP1FirmwareBuildInfo, "
                        "not HydraAppsFirmwareBuildInfo")
                if chip_name is None:
                    chip_name = self._chip_name
            glob_folder = "*%s_p%s_d*[!lib]" % (chip_name.lower(), core)
        else:
            id_string = id.split()[3]
            glob_folder = id.split()[2]
        iprint(" (Attempting to find Apps firmware build %s)" % id_string)
        build_root = os.path.join(get_network_homes(), "appsw", "app_ss_builds",
                                  "builds")

        if cache:
            # Can we find a matching path amongst the cached ELF files?  If so
            # we avoid having to do a listing of a very large remote directory,
            # which can be slow in some circumstances.
            cached_build_dirs = list(
                set(os.path.dirname(cached_path)
                    for (cache_file, cached_path, _) in
                    cache.list(
                        os.path.join(build_root, "*_{}".format(id_string),
                                     "fw", "build", glob_folder, "*"))))
            if len(cached_build_dirs) == 1:
                dir, _ = cached_build_dirs[0].split(os.path.join("fw", "build"))
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
        if len(build_dirs) > 1:
            # Multiple matching builds?!?
            raise ValueError("Multiple Apps firmware build directories matched "
                             "fw ID %s: %s" % (id_string, build_dirs))
        dir = os.path.join(build_root, build_dirs[0])
        glob_pat = os.path.join(dir, "fw", "build", glob_folder)
        matches = list(filter(os.path.isdir, glob.glob(glob_pat)))
        if len(matches) != 1:
            # Glob didn't work properly
            raise ValueError("glob %s didn't match a single directory as "
                             "expected; it matched %d; consider using the "
                             "chip_name parameter. Matched: [%s]" %
                             (glob_pat, len(matches),
                              ",".join([str(x) for x in matches])))
        iprint(" (Found build at %s)" % dir)
        return matches[0]

    @property
    def _build_file_nameroot(self):
        return self._build_file_nameroot_helper(self.CORE_ID)

    @classmethod
    def munge_build_dir(cls, build_dir):
        """
        Replace p1/0 with p0/1 if necessary
        """
        return cls._munge_build_dir(build_dir, cls.CORE_ID)

    @property
    def _bad_func_names(self):
        return set(["dorm_wake"])
                
class HydraAppsP0FirmwareBuildInfo(HydraAppsFirmwareBuildInfo):
    CORE_ID = 0

class HydraAppsP1FirmwareBuildInfo(HydraAppsFirmwareBuildInfo):
    CORE_ID = 1

class HydraAudioFirmwareBuildInfo(KalAsmMixin,
                                  BaseGenericHydraFirmwareBuildInfo):

    @property
    def _path_anchor(self):
        return "kymera"

    @property
    def _common_dir(self):
        """
        Path to the hydra common folder;
        This is determined from the _root_dir which may be different
        for some subsystems and may not exist for a patch.
        """

        path = os.path.join(self._root_dir, 'common')
        if os.path.isdir(path):
            return path
        raise PathNotFoundError("path not found: {}".format(path))

    @property
    def _mib_dir(self):
      """
      Path to the hydra mib folder; 
      This is determined from the _root_dir which may be different
      for some subsystems.
      """
      return os.path.join(self._common_dir, 'hydra', 'mib')

    @property
    def _mibdb_basename(self):
        return "audio_mib.xml"

    def _get_config_db(self):
        """
        Path to database that defines the .sdb file used by configcmd 
        containing keys for a developer system label of dev_system_label.  
        Typically this is mib.sdb in the same folder as the executable.
        """
        # Correct for development builds but not release_builds
        return os.path.join(self.build_dir, 'audio_mib.sdb')
        
    def _create_interface_info(self):
        """
        Create interface to interface info.
        Uses different path from the base class method
        to find the interface files, but if supplied to constructor
        then use that one.
        """ 
        from csr.interface.hydra_interface_info import HydraInterfaceInfo
        if self._interface_dir is None:
            hip_dir = os.path.join(self._common_dir, 'hydra', 'interface')
        else:
            hip_dir = self._interface_dir
        return HydraInterfaceInfo(hip_dir)

    @property
    def toolchain(self):
        return KCC32Toolchain

    @property
    def slt_type(self):
        return AudioBaseSLT

    @property
    def _root_dir(self):
        """\
        Development tree root for this development build.
        """
        # firmware build is 8 levels deep
        if r"jenkins21" in self._dir:
            # Fats builds have a different folder structure and we need
            # to define a different root dir.
            # expected path is something like
            # audio:\\root.pri\fileroot\HomeFolders\S\svc-audio_dsp\Scratch\jenkins21\artifacts\csra68100_dev__kymera__trig\519\csra68100_sqif
            root_dir = self._dir
        else:
            root_dir = os.path.join(self._dir, '..', '..', '..', '..')
        root_dir = os.path.normpath(root_dir)
        return root_dir

    @property
    def _src_root_for_rebuild(self):
        """
        returns tuple (_rootdir, rel_path_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """       
        return (self._root_dir, 'build')

    def _get_build_ver(self):
        '''
        Utility function to supply the build version.
        '''
        # Cannot be implemented in terms of _get_build_id()
        # which tries to get it from the path name.
        if getattr(self, '_build_ver', None) is None:
            return None
        return self._build_ver

    @staticmethod
    def build_from_id(id=None, id_number=None, cache=None):
        """
        Determine a build path from the firmware build ID string or id_number
        integer.
        WARNING: this function only currently handles build ID strings of the 
        following form: 'kymera_<DATESTAMP>_<TAG>[_rel] YYYY-MM-DD'
        """
        if not id and id_number is None:
            raise ValueError("Cannot look up build without an id or id_number")

        if id_number:
            id = id_number
        if isinstance(id, (int, long)):
            id = HydraAudioFirmwareBuildInfo._id_from_build_ver(id)

        iprint(" (Attempting to find Audio firmware build '%s')" % id)
        root = os.path.join(get_network_homes(), "svc-audio-dspsw",
                            "kymera_builds","builds")
        if not os.path.isdir(root):
            iprint(" (No directory %s: can't look up build)" % root)
            return None
        try:
            name, date = id.split()
            year = date.split("-",1)[0]
            cmpts = name.split("_")
            name = "_".join(cmpts[:2]) # kymera_YYMMDDMMSS
            type = "_".join(cmpts[2:]) # 
            if type.endswith("_rel"):
                type = type[:-4]
            pth = os.path.join(root, year, name, "kalimba", "kymera", "output",
                                type, "build", "debugbin")
        except (ValueError, KeyError):
            # Build string is a different shape than expected
            return None
        if not os.path.isdir(pth):
            return None
        iprint(" (Found build at %s)" % pth)
        return pth

    @staticmethod
    def _id_from_build_ver(id_number):
        "Look up long form of build id from id_number"
        if platform.system() == "Windows":
            build_root = r"\\root.pri\FileRoot\UnixHomes\home"
        else:
            build_root = "/home"
        build_root = os.path.join(build_root, "svc-audio-dspsw", "kymera_builds")
        iprint(" (Attempting to find Audio firmware build %s)" % (id_number))
        if not os.path.isdir(build_root):
            iprint(" (No directory %s: can't look up build)" % build_root)
            return None

        # Read ID.txt to find it from the id string
        try:
            with open(os.path.join(build_root, "..", "ID.txt")) as ids:
                number_to_string = {}
                for line in ids:
                    if line.lstrip().startswith("#") or not line.strip():
                        continue
                    try:
                        build_num, build_id = line.split(None,1)
                    except ValueError:
                        iprint(line)
                    number_to_string[int(build_num.strip())] = build_id.strip()
        except IOError as e:
            iprint("Couldn't open ids.txt: %s" % e)
            return None
        return number_to_string[id_number]

class HydraAudioP0FirmwareBuildInfo(HydraAudioFirmwareBuildInfo):
    @property
    def _build_file_nameroot(self):
        #This is a little horrible thing that is copied from apps.
        return "kymera_%s_audio" % self._chip_name.lower()

    @property
    def _log_firm_basename(self):
        return "audio_log_firm.c"

class HydraAudioP1FirmwareBuildInfo(HydraAudioFirmwareBuildInfo):
    @property
    def _build_file_nameroot(self):
        #This is a little horrible thing that is copied from apps.
        return "kymera_%s_audio" % self._chip_name.lower()
    
    @property
    def _log_firm_basename(self):
        return "audio_log_firm.c"

class HydraAudioP0PatchBuildInfo(HydraAudioFirmwareBuildInfo):
    """
    Provide build info for an audio0 patch file
    """

    @property
    def _path_anchor(self):
        return "patches"

    @property
    def _get_config_db(self):
        """
        May not exist for a patch
        """

        path = HydraAudioFirmwareBuildInfo._get_config_db(self)
        if os.path.isfile(path):
            return path
        raise PathNotFoundError("path not found: {}".format(path))

    @property
    def hcf_dir(self):
        """
        Folder containing the patch .hcf file, which is not same as
        the .elf location.
        """
        return  os.path.normpath(
            os.path.join(self.build_dir, *['..']*4)).replace(
            'output', 'release', 1)

    @property
    def _build_file_nameroot(self):
        return "kymera-patch"
        
    @staticmethod
    def build_from_patch_id(id_number, id=None):
        """From the supplied id_number return path to patch build dir or None"""
        if id:
            raise ValueError("Lookup of patch by id string is not supported")
        if id_number is None:
            raise ValueError("Cannot look up patch without an id_number")
        iprint(" (Attempting to find Audio patch build {})".format(id_number))
        root = os.path.join(get_network_homes(), "svc-audio-dspsw",
                            "patch_releases", "output")
        if not os.path.isdir(root):
            iprint(" (No directory %s: can't look up build)" % root)
            return None           
        # glob to find the audio ROM fw build id (*)
        pth = os.path.join(root, "patches_*",
                           "version_"+str(id_number),
                           "build_output", "build", "patches", "debugbin")
        matches = glob.glob(pth)
        if len(matches) == 1:
            pth = matches[0]
            if not os.path.isdir(pth):
                return None
            iprint(" (Found patch build dir at %s)" % pth)
            return pth
        elif len(matches) > 0:
            raise ValueError("Unexpectedly got %d matches for audio patch "
                             "builds: %s" % (len(matches), pth))
        return None

    @property
    def _src_root_for_rebuild(self):
        """
        returns tuple (_rootdir, rel_path_for_make) providing
        directory relative to executable's _root_dir in which to 
        invoke make to rebuild it
        """   
        return (self._root_dir, os.path.join('build_output', 'build'))

class SingleFirmwareElf(FirmwareBuildInfoElfMixin):
    """
    MicroEnergy firmware comes in standard XAP-shaped ELF files
    """    
    def __init__(self, elf_path, use_cache=False):
        self._elf_path = elf_path
        self._layout_info = self.DATA_INFO_TYPE()
        self._cache = get_elf_cache(None) if use_cache else None
        self._autolookup_key = None
        
    @property
    def _elf_file(self):
        return self._elf_path


class OverlaidFirmwareBuildInfo (IFirmwareBuildInfo):
    """
    IFirmwareBuildInfo specialisation for a dual ELF world (app linked against ROM).
    Supports the case that only one or the other is supplied. 
    """
    APP_OBFUSCATES_ROM = False
    
    def __init__(self, elf_files):
        try:
            self._rom_elf = self.SINGLE_FIRMWARE_ELF_TYPE(
                             os.path.realpath(os.path.expanduser(elf_files["rom"])))
        except KeyError:
            self._rom_elf = None
        try:
            self._app_elf = self.SINGLE_FIRMWARE_ELF_TYPE(
                             os.path.realpath(os.path.expanduser(elf_files["app"])))
        except KeyError:
            self._app_elf = None
            
        if self._rom_elf is None and self._app_elf is None:
            # Failed to get hold of any build info: need to signal this to 
            # caller
            raise IFirmwareBuildInfo.FirmwareSetupException("No ELFs found")
            
        self._layout_info = self.SINGLE_FIRMWARE_ELF_TYPE.DATA_INFO_TYPE()
        self._build_id_func = None
        self._expect_matching_elf = False

    @property
    def elf_reader(self):
        try:
            self._elf_reader
        except AttributeError:
        
            if self._rom_elf and self._app_elf:
                
                self._elf_reader = Elf_Reader([self._rom_elf.elf_file,
                                               self._app_elf.elf_file],
                                               self.toolchain)
            elif self._rom_elf:
                self._elf_reader = Elf_Reader(self._rom_elf.elf_file,
                                              self.toolchain)
            elif self._app_elf:
                self._elf_reader = Elf_Reader(self._app_elf.elf_file,
                                              self.toolchain)
            else:
                self._elf_reader = None

        return self._elf_reader
        

    @property
    def _elf_file(self):
        return self._elf_file_name
    
    
    @property
    def dwarf_sym(self):
        try:
            self._dwarf_sym
        except AttributeError:
            if self.elf_reader:
                # Note: DWARF uses byte to mean 8 bits regardless of the size
                # of addressable units on the target platform
                self._dwarf_sym = Dwarf_Reader(self.elf_reader,
                                               ptr_size=self._layout_info.data_word_bits//8)
            else:
                self._dwarf_sym = None
        return self._dwarf_sym

    @property
    def dwarf_full(self):
        return self.dwarf_sym

    @property
    def rom_build_loaded(self):
        return self._rom_elf is not None

    @property
    def app_build_loaded(self):
        return self._app_elf is not None


class MicroEnergySingleFirmwareElf(SingleFirmwareElf):
    DATA_INFO_TYPE = XapDataInfo
    @property
    def toolchain(self):
        return XAPGCCToolchain

class MicroEnergyFirmwareBuildInfo(OverlaidFirmwareBuildInfo):
    SINGLE_FIRMWARE_ELF_TYPE = MicroEnergySingleFirmwareElf
    @property
    def toolchain(self):
        return XAPGCCToolchain

        
class FlashheartFirmwareBuildInfo(MicroEnergyFirmwareBuildInfo, FirmwareBuildInfoElfMixin):
    """
    Specialisation that knows how to find flashheart ROM ELF files
    """
    APP_OBFUSCATES_ROM = True
    
    def __init__(self, elf_files):
        if "rom" in elf_files and isinstance(elf_files["rom"], (int, long)):
            rom_elf = self._get_rom_elf_from_id(elf_files["rom"])
            if rom_elf is not None:
                elf_files["rom"] = rom_elf
            else:
                del elf_files["rom"]
        
                
        super(FlashheartFirmwareBuildInfo, self).__init__(elf_files)

    def _get_rom_elf_from_id(self, id):
        
        root = os.path.join(get_network_homes(),"svc_blesw","ble_builds",
                            "build_ids")
        iprint(" (Looking for Flashheart build %04x)" % id)
        build_path_glob = os.path.join(root, "%04x" % id, "builds",
                                       "flashheart_d*", "rom_img", "dev", 
                                       "rom_img.elf")
        matches = glob.glob(build_path_glob)
        if len(matches) == 1:
            iprint(" (ROM ELF is %s)" % matches[0])
            return matches[0]
        elif len(matches) > 0:
            raise ValueError("Unexpectedly got %d matches for flashheart ROM "
                             "builds: %s" % (len(matches), build_path_glob))
        return None
        
    
    def load_program_cache(self, prog_cache):

        if self._rom_elf:
            for loadable_section in self._rom_elf.elf_code.sections:
                addr = loadable_section.paddr
                section_data = loadable_section.data
                prog_cache[addr: addr + len(section_data)] = section_data
        if self._app_elf:
            for loadable_section in self._app_elf.elf_code.sections:
                addr = loadable_section.paddr
                section_data = loadable_section.data
                prog_cache[addr: addr + len(section_data)] = section_data

    @property
    def slt_type(self):
        return FlashheartFirmwareBuildInfo

            
class SimpleFirmwareBuildInfo(IFirmwareBuildInfo, SingleFirmwareElf):
    """
    Build info for the simple case where there's just one ELF and no other 
    build files that we know or care about
    """
    
    def __init__(self, elf_path, data_info_type,
                 use_cache=True):
        self.DATA_INFO_TYPE = data_info_type
        SingleFirmwareElf.__init__(self, elf_path, use_cache=use_cache)
