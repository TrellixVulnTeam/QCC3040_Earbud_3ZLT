############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.global_streams import iprint
from csr.wheels import UnimportableObjectProxy
import os
import sys
import platform
from importlib import import_module
from collections import namedtuple
from ..wheels.scoped_symbol_dict import ScopedSymbolDict

if sys.version_info > (3,):
    # Py3
    str_type = (str, bytes)
else:
    str_type = (str, unicode)

try:
    # Are we in a distribution or in the Perforce tree?
    try:
        from . import _read_dwarf
    except ImportError:
        import _read_dwarf

    if os.path.basename(_read_dwarf.__file__).startswith("__init__.py"):
        # _read_dwarf is a package containing system-specific files which we need to
        # select from
        system = platform.system()
        is_32bit = sys.maxsize == (1 << 31) - 1
        win_bits = "win32" if is_32bit else "win64"
        vc_dir = "vc10" if sys.version_info <= (3, 4) else "vc14"
        
        if system == "Windows":
            if is_32bit:
                if vc_dir == "vc10":
                    from csr.dwarf._read_dwarf.win32.vc10 import _read_dwarf as c_read_dwarf
                else:
                    from csr.dwarf._read_dwarf.win32.vc14 import _read_dwarf as c_read_dwarf
            else:
                if vc_dir == "vc10":
                    from csr.dwarf._read_dwarf.win64.vc10 import _read_dwarf as c_read_dwarf
                else:
                    from csr.dwarf._read_dwarf.win64.vc14 import _read_dwarf as c_read_dwarf

        elif system == "Linux":
            if is_32bit:
                raise ImportError("32-bit Python on Linux is not supported")
            else:
                if sys.version_info >= (3,):
                    from ._read_dwarf.linux_x86_64.py3 import _read_dwarf as c_read_dwarf
                else:
                    #Python 2
                    from ._read_dwarf.linux_x86_64 import _read_dwarf as c_read_dwarf
        else:
            raise ImportError
    else:
        try:
            from . import _read_dwarf as c_read_dwarf
        except ImportError:
            import _read_dwarf as c_read_dwarf
except ImportError as exc:
    c_read_dwarf = UnimportableObjectProxy(exc)


class ElfToolchain(object):
    
    XAPGCC    = 0
    KCC       = 1
    ARMNATIVE = 2
    ARMGCC    = 3

# Looks and feels like a namedtuple, but we want fields to be updateable (e.g. if
# the symtab doesn't have sizes, but we gather them from the listing file)
class ElfSymType(object):
    def __init__(self, name, address, size, is_global):
        self.name = name
        self.address = address
        self.size = size
        self.is_global = is_global
    def __len__(self):
        return 4
    def __getitem__(self, i):
        return getattr(self, ["name", "address", "size", "is_global"][i])
    def __repr__(self):
        return "ElfSymType(name={}, address=0x{:x}, size={}, is_global={}".format(
                            self.name, self.address, 
                            (hex(self.size) if self.size is not None else None), self.is_global)

KCC_LARGE_SYM_SENTINEL = 0x0fffffff


class ElfError(ValueError):
    """
    Something wrong with an ELF look-up
    """

class ElfNoSymbol(ElfError):
    """
    No symbol of the given name
    """
class ElfAmbiguousName(ElfError):
    """
    Multiple symbols of the given name
    """

def combine_rom_ram_elf_symbols(rom_symbols, ram_symbols, ram_obfuscates_rom=False):
    # Useful debug code: worth leaving in for future convenience but
    # shouldn't be called normally.
    if False:
        check_duplication(rom_symbols,
                          ram_symbols)

    # Combine the symbols by overwriting any name clashes with the App
    # ELF's symbol.
    elf_symbols = []
    (rom_gbls, rom_funcs, rom_cus, rom_abs, rom_minim) = \
                                           rom_symbols
    (ram_gbls, ram_funcs, ram_cus, ram_abs, ram_minim) = \
                                           ram_symbols

    if ram_obfuscates_rom:
        # Delete any function from the RAM ELF that is at the same
        # address as a function from the ROM ELF.  We assume the former is
        # just an (obfuscated) reference to the latter, and we'd rather
        # just have the non-obfuscated symbol knocking around.
        rom_func_addrs = {v:k for (k,v) in rom_funcs.items()}
        retained_ram_funcs = {}
        start_num_ram_funcs = len(ram_funcs)
        for name, addr in ram_funcs.items():
            if addr not in rom_func_addrs:
                retained_ram_funcs[name] = addr
        ram_funcs = retained_ram_funcs
        
    for (d_rom, d_ram) in ((rom_gbls, ram_gbls), (rom_funcs, ram_funcs)):
        if isinstance(d_rom, dict):
            d_rom.update(d_ram)
            elf_symbols.append(d_rom)
            
    # It's possible there are still duplicates, but experience suggests
    # these are just aliases for a primary function name.  
    
    # Update the CUs on a per-CU basis where filenames clash
    for cu in ram_cus:
        if cu not in rom_cus:
            rom_cus[cu] = ram_cus[cu]
        else:
            rom_cus[cu].update(ram_cus[cu])
    elf_symbols.append(rom_cus)
    rom_abs.update(ram_abs)
    elf_symbols.append(rom_abs)
    # The elf_symbols list includes "minim_ranges", which is a Kalimba-
    # specific concept.  For XAP/ARM ELFs we want an empty list.
    elf_symbols += [[]]

    return elf_symbols

def check_duplication(rom_symbol_list, app_symbol_list):
    """
    Debugging function which takes a pair of ELF symbol lists and checks what
    duplication of names exists between them, and what DWARF info is available
    about different categories of name in the two ELFs 
    """
    def check_symbol_duplication(rom_syms, app_syms, sym_type,
                                 get_method_name):
    
        duplicates = set(rom_syms.keys()).intersection(list(app_syms.keys()))
        only_in_rom = set(rom_syms.keys()) - set(app_syms.keys())
        only_in_app = set(app_syms.keys()) - set(rom_syms.keys())
        rom_dwarf = Dwarf_Reader(self._rom_elf._elf_reader)
        app_dwarf = Dwarf_Reader(self._app_elf._elf_reader)
        rom_dwarf_count = 0
        app_dwarf_count = 0
        for dup in duplicates:
            try:
                rom_dwarf_var = getattr(rom_dwarf,get_method_name)(dup)
                rom_dwarf_count += 1
            except DwarfNoSymbol:
                pass
            try:
                app_dwarf_var = getattr(app_dwarf,get_method_name)(dup)
                app_dwarf_count += 1
            except DwarfNoSymbol:
                pass
        iprint("Of %d duplicated %ss, %d had DWARF info in ROM ELF and "
        "%d had DWARF info in App ELF") % (len(duplicates), sym_type, rom_dwarf_count, app_dwarf_count)
        rom_dwarf_count = 0
        for sym in only_in_rom:
            try:
                rom_dwarf_var = getattr(rom_dwarf,get_method_name)(sym)
                rom_dwarf_count += 1
            except DwarfNoSymbol:
                pass
        iprint("Of %d %ss only in the ROM, %d had DWARF info in ROM ELF"
               % (len(only_in_rom), sym_type, rom_dwarf_count))
        app_dwarf_count = 0
        for sym in only_in_app:
            try:
                app_dwarf_var = getattr(app_dwarf,get_method_name)(sym)
                app_dwarf_count += 1
            except DwarfNoSymbol:
                pass
        iprint("Of %d %ss only in the APP ELF, %d had DWARF info in APP ELF"
               % (len(only_in_app), sym_type, app_dwarf_count))

    # Find duplicate symbols
    check_symbol_duplication(rom_symbol_list[0], 
                             app_symbol_list[0],
                             "global",
                             "get_global_var")
    check_symbol_duplication(rom_symbol_list[1], 
                             app_symbol_list[1],
                             "function",
                             "get_function")
    check_symbol_duplication(rom_symbol_list[2], 
                             app_symbol_list[2],
                             "CU",
                             "get_cu")
            
            
def try_cu_name_remap(dwarf_cus, cu_name):
    # remap the name if it's a known CU (i.e. it's in the
    # DWARF but contains no variables)
    full_cu_name = dwarf_cus.lookup_key(cu_name)
    if full_cu_name:
        return "/".join(dwarf_cus.lookup_key(cu_name))
    # It's not in the DWARF - the CU name we've got is the
    # best we're getting
    return cu_name


class Elf_Reader(object):
    """
    Wraps the Elf_Reader class implemented in C, which can return details
    from the symbol table, and read the debug strings into a dictionary
    indexed by string address 
    """
    def __init__(self, elf_path_or_path_list, toolchain,
                 ram_obfuscates_rom=False,
                 allow_abs_variables=False):
        """
        Initialise the Elf_Reader with the full path to the target
        ELF file.
        Note that instances of this class have a separate handle on the
        ELF file from the Dwarf_Reader.
        """
        if isinstance(elf_path_or_path_list, str_type):
            # elf_path required to be a string so ensure that here
            if type(elf_path_or_path_list) is not str:
                if type(elf_path_or_path_list) is bytes:
                    elf_path = elf_path_or_path_list.decode()
                else:
                    # py2 Unicode type
                     elf_path = elf_path_or_path_list.encode()
            else:
                elf_path = elf_path_or_path_list
            self._c_readers = [c_read_dwarf.Elf_Reader(elf_path)]
            self.elf_files = [elf_path]
            
        else:
            elf_paths_or_readers = list(elf_path_or_path_list)
            if all(isinstance(entry,str) for entry in elf_paths_or_readers):
                self._c_readers = [c_read_dwarf.Elf_Reader(elf_path) 
                                    for elf_path in elf_path_or_path_list]
                self.elf_files = elf_path_or_path_list
            else:
                self._c_readers = sum((entry._c_readers for entry in elf_paths_or_readers), [])
                self.elf_files = sum((entry.elf_files for entry in elf_paths_or_readers), [])
            
        self._toolchain = toolchain
        self._ram_obfuscates_rom = ram_obfuscates_rom
        self._allow_abs_variables = allow_abs_variables
        
    def _get_symbols(self):
        """
        Return a tuple of data structures holding different types of
        symbols:
         - a dictionary of global variables, name mapping to (address, size)
          tuples
         - a dictionary of global functions, name mapping to address
         - a dictinoary of compilation units, name mapping to a dictionary 
         with two entries, "vars" and "funcs", which contain the same types
         of element as the global dictionaries above
         - a dictionary of absolute symbols, name mapping to value
         - a list of start address, end address pairs delineating the 
         executable sections that are marked as MINIM (this is a Kalimba
         feature; other processors will get an empty list)
         
         Note that no name-munging is performed at this level, meaning that
         functions and variables still have any prefixes added by the
         compiler (e.g. $_, L_) etc, and compilation unit names are still
         the full path.  However, some filtering is performed, so
         for example KCC's branch and debug symbols are not included.
        """
        
        if len(self._c_readers) == 1:
            
            (self._raw_gbl_vars_,
             self._raw_gbl_funcs_,
             self._raw_cus_,
             self._raw_abs_,
             self._minim_ranges) = self._c_readers[0].get_symbols(self._allow_abs_variables)
             
        elif len(self._c_readers) == 2:
            
            (self._raw_gbl_vars_,
             self._raw_gbl_funcs_,
             self._raw_cus_,
             self._raw_abs_,
             self._minim_ranges) = combine_rom_ram_elf_symbols(
                                    self._c_readers[0].get_symbols(),
                                    self._c_readers[1].get_symbols(),
                                    ram_obfuscates_rom=self._ram_obfuscates_rom)
        else:
            raise ValueError("Don't know how to combine %d sets of ELF "
                             "symbols" % len(self._c_readers))
    
    @property
    def _raw_gbl_vars(self):
        try:
            self._raw_gbl_vars_
        except AttributeError:
            self._get_symbols()
        return self._raw_gbl_vars_

    @property
    def _raw_gbl_funcs(self):
        try:
            self._raw_gbl_funcs_
        except AttributeError:
            self._get_symbols()
        return self._raw_gbl_funcs_

    @property
    def _raw_cus(self):
        try:
            self._raw_cus_
        except AttributeError:
            self._get_symbols()
        return self._raw_cus_

    @property
    def _raw_abs(self):
        try:
            self._raw_abs_
        except AttributeError:
            self._get_symbols()
        return self._raw_abs_
    
    @property
    def vars(self):
        # Put these in a ScopedSymbolDict
        return ScopedSymbolDict(self._vars_scoping_dict, lambda k,x:x, 
                                UnknownNameExcep=ElfNoSymbol,
                                AmbiguousNameExcep=ElfAmbiguousName)
        
    @property
    def _vars_scoping_dict(self):
        """
        The variables in the ELF symbol table as a ScopedSymbolDict
        """
        try:
            self._vars_scoping_dict_
        except AttributeError:
        
            self._vars_scoping_dict_ = {"<globals>" : {}}
            scoping_dict_gbls = self._vars_scoping_dict_["<globals>"]
            # Clean up the globals and put them into a "<globals>" subdirectory.  
    
            # clean up globalvar name
            for name, (addr, size) in list(self._raw_gbl_vars.items()):
                for substr in self._toolchain.GLOBVAR_PREFIXES:
                    # The Apps firmware build system does something strange with these two linker script symbol
                    # names, so we have to special case them to avoid them being incorrectly munged here
                    if name.startswith("$MEM_MAP_BSS_START") or name.startswith("$MEM_MAP_INITC_START"):
                        name = name[1:]
                    elif name.startswith(substr):
                        name = name[len(substr):]
                scoping_dict_gbls[name] = ElfSymType(name, addr, size, True)
            
            # Loop over raw CUs cleaning up the locals and putting them into CU
            # based subdirectories
    
            for cu, cu_dict in list(self._raw_cus.items()):
                cu = cu.replace("\\","/")
                if not any(cu.startswith(prefix) for prefix in self._toolchain.CU_PREFIXES):
                    if cu_dict.get("vars"):
                        self._vars_scoping_dict_[cu] = {}
                        for name, (addr, size) in list(cu_dict["vars"].items()):
                            self._vars_scoping_dict_[cu][name] = ElfSymType(name, addr, size, False)
    
        return self._vars_scoping_dict_
    
    @property
    def funcs(self):
        """
        The functions in the ELF symbol table as a ScopedSymbolDict
        """
        # Put these in a ScopedSymbolDict
        return ScopedSymbolDict(self._funcs_scoping_dict, lambda k,x:x, 
                                UnknownNameExcep=ElfNoSymbol,
                                AmbiguousNameExcep=ElfAmbiguousName)
        
    @property
    def _funcs_scoping_dict(self):
        
        try:
            self._funcs_scoping_dict_, self._extra_function_syms
        except AttributeError:
            REALLY_BAD_FUNC_CHARS = (chr(1),)
            BAD_FUNC_CHARS = (".", "?") + REALLY_BAD_FUNC_CHARS
    
            # Clean up the globals and put them into a "<globals>" subdirectory.  
            # Loop over raw CUs cleaning up the locals and putting them into CU
            # based subdirectories 
            
            # Put these in a ScopedSymbolDict with a factory that can construct
            # basic ELF function info API objects
    
            self._funcs_scoping_dict_ = {"<globals>" : {}}
            self._extra_function_syms = {}
            scoping_dict_gbls = self._funcs_scoping_dict_["<globals>"]
            # Clean up the globals and put them into a "<globals>" subdirectory.  
    
            # clean up globalvar name
            for name, (value, size) in list(self._raw_gbl_funcs.items()):
                if not size:
                    size = None
                if any(name.startswith(prefix) for prefix in self._toolchain.FUNC_IGNORE_PREFIXES):
                    continue
                for substr in self._toolchain.GLOBFUNC_PREFIXES:
                    if name.startswith(substr):
                        name = name[len(substr):]
                if not any(c in name for c in BAD_FUNC_CHARS):
                    scoping_dict_gbls[name] = ElfSymType(name, value, size, True)
                elif not any(c in name for c in REALLY_BAD_FUNC_CHARS):
                    # Remember these even though they may not be real function 
                    # symbols: we allow them for name-based function look-ups
                    # and also may use them as aliases if the name in the DWARF
                    # is even uglier (e.g. in Audio ELFs you see "namep1.namep2" in 
                    # the ELF corresponding to "$M.namep1.namep2" in the DWARF) 
                    self._extra_function_syms[name] = ElfSymType(name, value, size, True)
            
            # Loop over raw CUs cleaning up the locals and putting them into CU
            # based subdirectories
    
            for cu, cu_dict in list(self._raw_cus.items()):
                cu = cu.replace("\\","/")
                if not any(cu.startswith(prefix) for prefix in self._toolchain.CU_PREFIXES):
                    if cu_dict.get("funcs"):
                        self._funcs_scoping_dict_[cu] = {}
                        for name, (value, size) in list(cu_dict["funcs"].items()):
                            if not size:
                                size = None
                            if any(name.startswith(prefix) for prefix in self._toolchain.FUNC_IGNORE_PREFIXES):
                                continue
                            for substr in self._toolchain.LOCALFUNC_PREFIXES:
                                if name.startswith(substr):
                                    name = name[len(substr):]
                            if not any(c in name for c in BAD_FUNC_CHARS):
                                self._funcs_scoping_dict_[cu][name] = ElfSymType(name, value, size, 
                                                                                 False)
        return self._funcs_scoping_dict_
    
    @property
    def cus(self):
        """
        Return the list of ELF CUs
        """
        return ScopedSymbolDict(self._cus_scoping_dict, lambda k,x:x, 
                                unflatten_dict=True,
                                UnknownNameExcep=ElfNoSymbol,
                                AmbiguousNameExcep=ElfAmbiguousName)
        
    @property
    def _cus_scoping_dict(self):
        try:
            self._cus_scoping_dict_
        except AttributeError:
            self._cus_scoping_dict_ = {}
            for cu, cu_dict in list(self._raw_cus.items()):
                cu = cu.replace("\\","/")
                if not any(cu.startswith(prefix) for prefix in self._toolchain.CU_PREFIXES):
                    self._cus_scoping_dict_[cu] = None
                    
        return self._cus_scoping_dict_
        
    @property
    def abs(self):
        """
        Return the cleaned list of absolute symbols
        """
        try:
            self._abs
        except AttributeError:
            clean_abs = {}
            if self._toolchain.NAME == "KCC32":
                # Possible to-do: this is a little slow for CSRA68100 Audio ELFs -
                # takes around 0.5 secs on my machine, which is a fairly quick one.  
                # Maybe do it in C?
                num_abs = len(self._raw_abs)
                for name, value in self._raw_abs.items():
                    
                    if ("?int64_hi" not in name and 
                        not name.lstrip("$").startswith("_") and 
                        value != self._toolchain.LARGE_SYM_SENTINEL):
                        if name.startswith("$"):
                            name = name[1:]
                        if name.endswith("?int64_lo"):
                            name = name[:-9]
                        clean_abs[name] = value
                self._abs = clean_abs
            else:
                clean_abs = {}
                num_abs = len(self._raw_abs)
                for name, value in self._raw_abs.items():
                    # Throw away symbols starting with an underscore that are
                    # otherwise the same as an existing symbol (e.g. the register
                    # symbols in io_map.s)
                    if not name.startswith("_") or name[1:] not in self._raw_abs:
                        clean_abs[name] = value
                self._abs = clean_abs
        return self._abs
        
    @property
    def minim_ranges(self):
        """
        Return the minim ranges.  Perhaps we should subclass this for Kalimba?
        """
        return self._minim_ranges
    
    @property
    def extra_function_syms(self):
        self.funcs
        return self._extra_function_syms
    
    def aux_vars(self, dwarf_vars, dwarf_cus, factory):
        """
        Return a ScopedSymbolDict containing just the names that the DWARF
        doesn't contain
        """
        aux_vars_dict = {"<globals>" : {}}
        # Loop over self.vars looking for CUs that aren't present in the 
        # dwarf_vars, and global variables that aren't in the dwarf_vars.  Put 
        # all the CUs' contents and all the global variables found into a new
        # ScopedSymbolDict with the factory function applied
        for cu_name in self.vars.scope_keys():
            if cu_name != "<globals>":
                cu_matches_in_dwarf = dwarf_cus.scoped_name_matches(cu_name)
                if (len(cu_matches_in_dwarf) == 0 or
                    (len(cu_matches_in_dwarf) == 1 and 
                        not dwarf_cus[cu_matches_in_dwarf[0]].vars.any_keys())):
                    # Add this CU into the aux_vars wholesale
                    full_cu_name = try_cu_name_remap(dwarf_cus, cu_name)
                    aux_vars_dict[full_cu_name] = self._vars_scoping_dict[cu_name].copy()
                    for var in aux_vars_dict[full_cu_name]:
                        aux_vars_dict[full_cu_name][var] = factory(aux_vars_dict[full_cu_name][var])
                    
        for var in self._vars_scoping_dict["<globals>"]:
            if not dwarf_vars.scoped_name_matches(var):
                aux_vars_dict["<globals>"][var] = factory(self._vars_scoping_dict["<globals>"][var])
        
        return ScopedSymbolDict(aux_vars_dict, lambda k,x:x, 
                                UnknownNameExcep=None,
                                AmbiguousNameExcep=ElfAmbiguousName,
                                ignore_globals=False)
        
    def aux_gbl_vars(self, dwarf_vars, factory):
        """
        Return a ScopedSymbolDict containing just the names that the DWARF
        doesn't contain
        """
        # Loop over self.vars looking for global variables that aren't in the 
        # dwarf_vars.  Put all the global variables found into a new
        # ScopedSymbolDict with the factory function applied
        aux_vars_dict = {"<globals>" : {}}
        # Loop over self.vars looking for CUs that aren't present in the 
        # dwarf_vars, and global variables that aren't in the dwarf_vars.  Put 
        # all the CUs' contents and all the global variables found into a new
        # ScopedSymbolDict with the factory function applied
        for var in self._vars_scoping_dict["<globals>"]:
            if not dwarf_vars.scoped_name_matches(var):
                aux_vars_dict["<globals>"][var] = factory(self._vars_scoping_dict["<globals>"][var])
        
        return ScopedSymbolDict(aux_vars_dict, lambda k,x:x, 
                                UnknownNameExcep=None,
                                AmbiguousNameExcep=ElfAmbiguousName,
                                ignore_globals=False)
    
    def aux_funcs(self, dwarf_funcs, dwarf_cus, supplement=None):
        """
        Return a ScopedSymbolDict containing just the names that the DWARF
        doesn't contain
        """
        # Loop over self.funcs looking for CUs that aren't present in the 
        # dwarf_funcs, and global funcs that aren't in the dwarf_funcs.  Put 
        # all the CUs' contents and all the global functions found into a new
        # ScopedSymbolDict with the factory function applied
        aux_funcs_dict = {"<globals>" : {}}
        # Loop over self.funcs looking for CUs that aren't present in the 
        # dwarf_funcs, and global functions that aren't in the dwarf_funcs.  Put 
        # all the CUs' contents and all the global variables found into a new
        # ScopedSymbolDict with the factory function applied

        from csr.dev.fw.meta.i_firmware_build_info import ToolchainQuirks
        include_all = (ToolchainQuirks.DWARF_BAD_INLINE_ATTRIBUTES 
                                            in self._toolchain.QUIRKS)


        for cu_name in self.funcs.scope_keys():
            if cu_name != "<globals>":
                cu_matches_in_dwarf = dwarf_cus.scoped_name_matches(cu_name)
                if (include_all or (len(cu_matches_in_dwarf) == 0 or
                                   (len(cu_matches_in_dwarf) == 1 and 
                                    not dwarf_cus[cu_matches_in_dwarf[0]].funcs.any_keys()))):
                    # Add this CU into the aux_funcs wholesale
                    full_cu_name = try_cu_name_remap(dwarf_cus, cu_name)
                    aux_funcs_dict[full_cu_name] = self._funcs_scoping_dict[cu_name].copy()
                    
        for var in self._funcs_scoping_dict["<globals>"]:
            if include_all or not dwarf_funcs.scoped_name_matches(var):
                aux_funcs_dict["<globals>"][var] = self._funcs_scoping_dict["<globals>"][var]
        
        return ScopedSymbolDict(aux_funcs_dict, lambda k,x:x, 
                                UnknownNameExcep=None,
                                AmbiguousNameExcep=ElfAmbiguousName,
                                ignore_globals=False, supplement=supplement)


    def aux_cus(self, dwarf_cus):
        """
        Return a ScopedSymbolDict containing just the names that the DWARF
        doesn't contain (with a nominal value - this is essentially just a set
        of compilation unit names)
        """
        aux_cus = {}
        for cu in self._cus_scoping_dict:
            if not dwarf_cus.scoped_name_matches(cu):
                aux_cus[cu] = self._cus_scoping_dict[cu]
        return ScopedSymbolDict(aux_cus, lambda k,x:x, 
                                unflatten_dict=True,
                                UnknownNameExcep=None,
                                AmbiguousNameExcep=ElfAmbiguousName)

    
    def get_debug_strings(self, char_width=1, is_littleendian=True):
        """
        Return a dictionary mapping (virtual) string addresses to strings.
        This can be used directly during log decoding to look up the string
        from the address in the log buffer.  
        """
        return self._c_readers[0].get_debug_strings(char_width, is_littleendian)
    
    def get_program_sections(self):
        
        return self._c_readers[0].get_program_sections()

    def get_section_table(self):
        return self._c_readers[0].get_section_table()

    def get_program_entry_point(self):
        return self._c_readers[0].get_program_entry_point()