############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
This module contains generic variable types and variable containers for self-
describing access to the firmware.
"""
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint, wprint
from csr.dev.env.i_env import IInt, IEnum, IArray, IStruct,\
    ICompilationUnit, IVariable
from csr.wheels.subarray import SubArray
from csr.wheels.bitsandbobs import FrozenNamespace, to_signed, from_signed, \
display_hex, HexDisplayInt, SliceableDict, unique_subclass
from csr.dwarf import read_dwarf
from csr.dev.framework.meta.elf_firmware_info import DwarfVariableInfo, \
                                DwarfArrayElementVariableInfo,\
                                DwarfPointerTargetVariableInfo,\
                                DwarfSubarrayVariableInfo, \
                                CUPathJoinWrapper,\
                                varinfo_factory, \
                                is_local, BadPCLow,\
                                ElfUnsignedIntInfo, ElfUnsignedIntArrayInfo,\
                                ElfUnsignedCharInfo
from csr.dev.hw.address_space import AddressSpace, AddressRange, \
AddressMultiRange
from csr.dev.hw.core.base_core import BaseCore
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.model.interface import Group, Table, Text, Code
from .env_dicts import SymbolDict, VarDict, CUDict, FunctionDict, TypeDict,\
EnumDict, EnumConstDict, build_scope_container, Enums, EnumConsts

import functools
import sys
import collections
import os
import re
from pprint import pformat
import copy
from contextlib import contextmanager

try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int

DEBUG_COMPARES_EQUAL=False

def var_factory(env, data_space, layout_info):
    """
    Meta-factory returning functions that can create Variables from key+symbol
    info by picking up core-specific metadata from the closure
    """
    _info_factory = varinfo_factory(layout_info)
    def _factory(key, base_info):
        """
        Factory function returning a _Variable object given appropriate "base" (i.e.
        DWARF) info
        """
        return _Variable.factory(_info_factory(key, base_info), 
                                 data_space, data_space, env=env)
    _factory.data_space = data_space # it's convenient to get access to this
    return _factory

def is_global(key, dwarf_symbol):
    """
    Global symbol filter
    """
    return read_dwarf.is_global(key, dwarf_symbol)

def is_local(key, dwarf_symbol):
    """
    Local symbol filter
    """
    return not read_dwarf.is_global(key, dwarf_symbol)


class FirmwarePresenter(object):
    """
    Generic API for firmware details provided via dictionary-like and 
    attribute-like objects
    """
    
    def __init__(self, dwarf, elf, var_factory, layout_info, toolchain,
                 cus, sym_tab_dwarf=None, asm_sym=None):
      
        if cus != True:
            cu_dict = cus
            is_cu = False
        else:
            cu_dict = None
            is_cu = True
      
        if dwarf:
            self._vars_symbol_dict = dwarf.vars
            funcs_symbol_dict = dwarf.funcs
            try:
                cus_symbol_dict = dwarf.cus
            except AttributeError:
                # The DWARF has no notion of CUs so it must be a CU itself
                cus_symbol_dict = None
        else:
            self._vars_symbol_dict = ScopedSymbolDict({}, None)
            funcs_symbol_dict = ScopedSymbolDict({}, None)
            cus_symbol_dict = None
        if elf:
            self._elf_var_factory = functools.partial(var_factory, None)
            vars_aux_dict = elf.aux_vars(self._vars_symbol_dict,
                                         cus_symbol_dict, 
                                         self._elf_var_factory)
            funcs_aux_dict = elf.aux_funcs(funcs_symbol_dict,
                                           cus_symbol_dict,
                                           supplement=asm_sym)
            extra_function_syms = elf.extra_function_syms
            minim_ranges = elf.minim_ranges
        else:
            vars_aux_dict = None
            funcs_aux_dict = None
            extra_function_syms = {}
            minim_ranges = []

        if sym_tab_dwarf is None:
            sym_tab_dwarf_funcs = None
        else:
            sym_tab_dwarf_funcs = sym_tab_dwarf.funcs

        self._vars = VarDict(self._vars_symbol_dict, layout_info, cu_dict, 
                             toolchain,
                             factory=var_factory, 
                             aux_dict=vars_aux_dict,
                             is_cu=is_cu)
        self._funcs = FunctionDict(funcs_symbol_dict,  
                                   funcs_aux_dict,
                                   extra_function_syms,
                                   minim_ranges,
                                   cu_dict,
                                   layout_info,
                                   toolchain,
                                   sym_tab_dwarf_funcs=sym_tab_dwarf_funcs,
                                   is_cu=is_cu)

        self._types = TypeDict(dwarf) if dwarf else {}
        self._enums = EnumDict(dwarf) if dwarf else {}
        self._enum_consts = EnumConstDict(dwarf) if dwarf else {}
            
        self._dwarf = dwarf
        self._elf = elf
        self._layout_info = layout_info

    @property
    def vars(self):
        return self._vars
    
    @property
    def localvars(self):
        return self.vars
    
    @property
    def funcs(self):
        return self._funcs

    functions = fns = funcs
    
    @property
    def localfuncs(self):
        return self.funcs
    
    @property
    def types(self):
        return self._types
    
    @property
    def enums(self):
        return self._enums
    
    @property
    def enum_consts(self):
        return self._enum_consts
    
    @property
    def var(self):
        try:
            self._var
        except AttributeError:
            self._var = build_scope_container(self.vars)
        return self._var
    
    @property
    def func(self):
        try:
            self._func
        except AttributeError:
            self._func = build_scope_container(self.funcs)
        return self._func
    
    @property
    def enum(self):
        try:
            self._enum
        except AttributeError:
            self._enum = unique_subclass(Enums)(self)
        return self._enum
    
    @property
    def econst(self):
        try:
            self._econst
        except AttributeError:
            self._econst = unique_subclass(EnumConsts)(self)
        return self._econst

    @property
    def local(self):
        return self.var

    def cast(self, addr, type, module=None, array_len=None, data_mem=None):
        """
        Return a Variable interface object for the given type at the given address,
        or an array of such at the given address if array_len is provided.
        
        :param addr: Memory address of the data to cast. This can be a Variable 
         rather than an address; the effect is as if the variable's address had 
         been supplied.
        :param type: Name of the type it should be case to
        :param module, deprecated: Retained for backwards compatibility
        :param array_len, optional: If supplied return an array of elements of
         the given type, of this length
        :param data_mem, optional: indexable object returning the data memory
         corresponding to the object.  Defaults to the associated core's data
         space
        :return: Variable object of the appropriate type pointing at the given
         address in memory
        
        """
        data_mem = data_mem if data_mem is not None else self._data

        # Go to the DWARF directly for the struct dict
        struct_dict = self.types[type]
        
        if module is not None:
            iprint("Note: the 'module' parameter is unused and is deprecated")

        if isinstance(addr, _Pointer):
            addr = addr.value
        elif isinstance(addr, _Variable):
            addr = var_address(addr)

        # Construct an ad hoc variable
        if array_len is None:
            var = _Variable.create_from_type(struct_dict, addr, data_mem,
                                             self._layout_info, env=self)
        else:
            array_dict = {"type_tag" : read_dwarf.DW_TAG["array_type"],
                          "type_name" : "%s[%d]" % (struct_dict["type_name"],
                                                  array_len),
                          "num_elements" : array_len,
                          "element_type" : struct_dict,
                          "byte_size" : struct_dict["byte_size"]*array_len}
            var = _Variable.create_from_type(array_dict, addr, data_mem,
                                             self._layout_info, env=self)
            
        return var


class GlobalFirmwarePresenter(FirmwarePresenter):
    """
    A FirmwarePresenter that provides access to a set of compilation units
    """
    def __init__(self, dwarf, elf, var_factory, layout_info, toolchain,
                 sym_tab_dwarf=None, asm_sym=None):
        
        self._cus = CUDict(dwarf.cus, aux_dict=elf.aux_cus(dwarf.cus),
                           factory=self._cu_firmware_presenter_factory)
        FirmwarePresenter.__init__(self, dwarf, elf, var_factory, layout_info,
                                   toolchain, sym_tab_dwarf=sym_tab_dwarf,
                                   cus=self._cus, asm_sym=asm_sym)
        gbl_vars_aux_dict = elf.aux_gbl_vars(self._vars_symbol_dict, 
                                             self._elf_var_factory) if elf else None

        self._globvars = SymbolDict(self._vars_symbol_dict, self._cus,
                                    filter=is_global,
                                    factory=var_factory,
                                    elide_top_level_scope=True,
                                    aux_dict=gbl_vars_aux_dict,
                                    is_cu=False)
         
        self._dwarf = dwarf
        self._elf = elf
        self._var_factory = var_factory
        self._layout_info = layout_info
        self._toolchain = toolchain

    @property
    def cus(self):
        return self._cus
    
    @property
    def globalvars(self):
        return self._globvars
    
    gv = globalvars
    
    @property
    def cu(self):
        try:
            self._cu
        except AttributeError:
            self._cu = build_scope_container(self.cus, env=self)
        return self._cu
    
    @property
    def gbl(self):
        try:
            self._gbl
        except AttributeError:
            self._gbl = build_scope_container(self.globalvars,
                                              elide_top_level_scope=True)
        return self._gbl
    
    @property
    def abs(self):
        return self._elf.abs
    
    @property
    def _func_name_remappings(self):
        try:
            self._func_name_remappings_
        except AttributeError:
            self._func_name_remappings_ = self.funcs.ambiguous_name_remapping(self.cus)
        return self._func_name_remappings_

    @property
    def _var_name_remappings(self):
        try:
            self._var_name_remappings_
        except AttributeError:
            self._var_name_remappings_ = self.vars.ambiguous_name_remapping(
                                    self.cus,
                                    scope_remappings=self._func_name_remappings)
        return self._var_name_remappings_
    
    @property
    def var(self):
        try:
            self._var
        except AttributeError:
            self._var = build_scope_container(self.vars,
                                              name_remapping=self._var_name_remappings,
                                              elide_top_level_scope=True)
        return self._var
    
    @property
    def func(self):
        try:
            self._func
        except AttributeError:
            self._func = build_scope_container(self.funcs,
                                               name_remapping=self._func_name_remappings,
                                               elide_top_level_scope=True)
        return self._func


    def find_matching_symbols(self, sym_regex, types=False, vars=True, 
                              funcs=False, enum_fields=True):
        """
        Search the ELF and DWARF for symbols of the indicated types that match
        the given regular expression

        @param sym_regex Compiled regular expression.  Matching is done from the 
        beginning of the string.
        @param types Search the types (there are no separate namespaces for
        enum_fields, structs and unions)
        @param vars Search the variables (globals and statics, reported 
        separately and with compilation unit details for the latter)
        @param funcs Search the functions
        @param enum_fields Search the fields of enumerations (the names of
        enumerations themselves are covered by the search through types)
        """
        
        res = {}
        if vars:
            res["vars"] = self.vars.search(sym_regex)
        if funcs:
            res["funcs"] = self.funcs.search(sym_regex)
        if types:
            # Do special things with dwarf.types
            res["types"] = self.types.search(sym_regex)
        if enum_fields:
            # Do special things with dwarf.enum_consts
            res["enum_fields"] = self.enums.search_fields(sym_regex)
        
        return res


    def _cu_firmware_presenter_factory(self, cu_key, dwarf_cu):
        
        try:
            elf_cu = self._elf.cus.lookup_symbol(cu_key)
        except self._elf.cus.UnknownNameExcep:
            # Try just looking up the basename in case the symbol table doesn't
            # give full paths
            try:
                elf_cu = self._elf.cus.lookup_symbol(cu_key[-1])
            except (self._elf.cus.UnknownNameExcep,
                    self._elf.cus.AmbiguousNameExcep):
                elf_cu = None
        return FirmwarePresenter(dwarf_cu, elf_cu, self._var_factory,
                                 self._layout_info, self._toolchain, cus=True)
    
def _passes_type_filter(filter_type_list, incl_not_excl, typename):
    return filter_type_list is None or (
            any(t.match(typename) for t in filter_type_list) == incl_not_excl)


class _Variable (IVariable):
    """
    Base class of variable descriptors.  These provide "natural" interfaces to
    real variables by using ELF/DWARF metadata (see elf_firmware_info.py) to
    guide access to raw underlying data memory.  Structures, Pointers and Arrays
    are fully recursive, although recursion is only performed on demand.
    
    As well as providing appropriate value access, each class provides a display
    method which can be used to recursively "pretty-print" the variable.  This 
    supports several advanced features, including access to a specific member of
    a structure, and circular dereference prevention.  The primary use of this
    method is the struct() macro (implemented in standalone_env.py), but there
    are other users too, such as IPC message decoding.
    
    _Variables are constructed from a VariableInfo object of some specific type
    derived from IVariableInfo (but preferably from IDwarfVariableInfo), which
    provides metadata such as the start address, byte width, etc., an object
    representing the variable's data space (this can be a processor core object
    or an AddressSpace), and optionally a separate object representing the space
    that pointers within the structure should be dereferenced in.  This latter
    argument allows for example Variables created on the fly from a small array
    of bytes to contain pointers to real data memory.
    
    A factory method is provided in _Variable as the public construction point
    for variables.  It uses DWARF information to determine which concrete type
    of _Variable to construct.
    """              
    
    # Future extension: set this to true to get rid of the ".value" that's
    # required when setting and getting the value of integer variables.
    # This isn't switched on yet as it will break a bunch of existing code that
    # uses .value.
    USE_PROPERTIES_FOR_INTS = False
        
    @staticmethod
    def factory(var_info, core_or_dspace, ptd_to_space=None,
                semantics=None, env=None):
        '''
        Factory method to create objects to represent firmware variables.
        The type of representative object that is created depends on the 
        "type of type".  
        
        The argument "dspace" is the one that the 
        start_address and stop_address members in the var_info object refer to;
        additionally, it may be necessary to supply a separate "pointed-to
        space" so that stack variables (for which an artifical data space is 
        requred to have been specially created since the values may live in 
        registers) that are, or that contain, pointers - which obviously will
        refer to main data space, not the stack variable's artifical space -
        can be successfully dereferenced.
        '''
        if isinstance(core_or_dspace, BaseCore):
            dspace = core_or_dspace.data
            env = env if env is not None else core_or_dspace.fw.env
        else:
            dspace = core_or_dspace
        
        try:
            tag = var_info.type_tag
            # For DWARF-based variables we want to look up any semantics that have
            # been provided for the variable's type.
            retrieve_type_semantics = True
        except AttributeError:
            if var_info.size != 0:
                if var_info.size % var_info._layout_info.addr_units_per_data_word == 0:
                    # If we have size data and the symbol is a whole number of 
                    # words, set it up as an unsigned int or array of unsigned ints.
                    layout_info = var_info._layout_info
                    word_size_bytes = layout_info.addr_units_per_data_word
                    if var_info.size == word_size_bytes:
                        var_info = ElfUnsignedIntInfo(var_info._elf_symbol, layout_info)
                        tag = read_dwarf.DW_TAG["base_type"]
                    else:
                        var_info = ElfUnsignedIntArrayInfo(var_info._elf_symbol, layout_info)
                        tag = read_dwarf.DW_TAG["array_type"]
                elif var_info.size == 1:
                    layout_info = var_info._layout_info
                    var_info = ElfUnsignedCharInfo(var_info._elf_symbol, layout_info)
                    tag = read_dwarf.DW_TAG["base_type"]
                else:
                    return _Variable(var_info, dspace, env=env)
                # For ELF-based variables we know there won't be any semantics for the
                # type because the type is just unsigned int or array of unsigned int.
                # In fact even trying to look this up can lead to infinite loops because
                # it introduces a dependency on the firmware environment that wouldn't
                # otherwise exist in their construction.
                retrieve_type_semantics = False
            else:
                return _Variable(var_info, dspace, env=env)

        if tag == read_dwarf.DW_TAG["structure_type"] or \
           tag == read_dwarf.DW_TAG["union_type"]:
            return _Structure.factory(var_info, dspace, ptd_to_space,
                                      tag == read_dwarf.DW_TAG["union_type"],
                                      semantics=semantics, env=env, 
                                      retrieve_type_semantics=retrieve_type_semantics)
        elif tag == read_dwarf.DW_TAG["array_type"]:
            #Not all arrays have a known number of elements in the DWARF.
            #We probably want to create a pointer object instead.
            if "num_elements" in var_info.struct:
                return _Array(var_info, dspace, ptd_to_space, env=env,
                             retrieve_type_semantics=retrieve_type_semantics)
            else:
                return _Pointer(var_info, dspace, ptd_to_space, env=env)
        elif tag == read_dwarf.DW_TAG["base_type"]:
            return _Integer(var_info, dspace, env=env)
        elif tag == read_dwarf.DW_TAG["enumeration_type"]:
            return _Enum(var_info, dspace, env=env)
        elif tag == read_dwarf.DW_TAG["pointer_type"]:
            return _Pointer(var_info, dspace, ptd_to_space,semantics=semantics, env=env)
        elif tag == read_dwarf.DW_TAG["subroutine_type"]:
            return _Function(var_info, dspace, env=env)

        return _Variable(var_info, dspace, env=env)
            
    @staticmethod
    def create_from_type(struct_dict, addr, core_or_dspace, layout_info,
                         ptd_to_space=None, env=None):
        """
        Helper for creating ad hoc variables
        """
        if isinstance(core_or_dspace, BaseCore):
            dspace = core_or_dspace.data
            env = env if env is not None else core_or_dspace.fw.env
        else:
            dspace = core_or_dspace
        
        var_info = DwarfVariableInfo(struct_dict, addr, layout_info)
        return _Variable.factory(var_info, dspace, ptd_to_space=ptd_to_space, env=env)



    def __init__(self, info, dspace, env=None):
        self._py_info = info
        # Save core_or_dspace to enable recursive construction of structure members
        self._py_data = dspace
        self._py_env = env
        
    def __ne__(self, other):
        return not (self == other)

    @property
    def address(self):
        '''
        Returns start address of the variable in memory
        '''
        return self._py_info.start_address
    
    @property
    def size(self):
        '''
        Returns size of the variable in memory
        '''
        return self._py_info.size
    
    @property
    def typename(self):
        """
        Returns the name of the variable's type
        """
        return self._py_info.datatype
    
    # IVariable compliance
    
    @property 
    def mem(self):
        # On the fly...
        return SubArray(self._py_data, 
                        self.address, 
                        self.address + self.size)

    @property
    def value_string(self):
        return "\n".join(self.display(""," |", [], []))

    def display_depth(self, depth_limit, show=True):
        """
        Display the Variable to a limited depth, as defined by the supplied 
        depth limit.  Depth increases every time we expand a structure's members,
        an array or linked list's elements, or a pointer's target.
        :param depth_limit: Limit on depth to show
        :param show, optional: If False, returns a string instead of printing it
        """
        display_str = "\n".join(self.display(""," |", [], [], depth_limit=depth_limit))
        if show:
            TextAdaptor(Code(display_str), gstrm.iout)
        else:
            return display_str

    def __repr__(self):
        return self.value_string


    def display(self, name, prefix, target, derefed, ll_expand=False, last=False,
                depth_limit=None):
        return ["<no type information>"]

    def report_addr(self, name):
        """
        Helper function that prints the name and address of a variable using the
        right width for pointers
        """
        ptr_fmt = r"0x%" + ("%02dx" % (self._py_info._layout_info.data_word_bits // 4))
        fmt = r"%s at " + ptr_fmt
        return fmt % (name, self.address)

    def _get_struct_attr_property(self):
        """
        Return a property object that actually looks up *this* object, not the
        instance of the class whose property it becomes
        """
        # We definitely have a getter
        prop = property(lambda o: self.get())
        # If we have a setter, add it in
        if self.set is not None:
            def _set(o, v):
                self.set(v)
            prop.setter(_set)
        return prop
        

    @property
    def _info(self):
        return self._py_info

    def set_semantics(self, semantics_dict, update=False):
        pass

    def clone(self, dspace=None, ptd_to_space=None, env=None):
        """
        Create an identical variable access object, possibly pointing at 
        different data spaces.  This is overridden in the _Pointer and _Structure
        subclasses to enable semantics to be cloned too.
        """
        env = env if env is not None else self._py_env
        dspace = dspace or self._py_data
        return _Variable.factory(var_info(self), dspace, ptd_to_space, env=env)

    def capture(self, full=True):
        """
        Create a cloned variable (see the "clone" method) based on a copy of
        the variable's memory footprint held in a local structure.  The
        captured variable is then independent of the chip.
        
        :p full If true, capture the entire direct and indirect memory footprint
        by following dereference-able pointers.  If false, only capture the
        direct memory footprint: pointers will refer to the original variable's
        data space.
        """
        if full:
            # capture main struct memory and follow pointers
            self._py_data.start_capturing()
            self.display("","", [], [])
            captured_mem = self._py_data.stop_capturing()
            pointed_to_mem = captured_mem
        else:
            # just capture main struct memory
            captured_mem = SliceableDict()
            mem_slice = slice(var_address(self),var_address(self)+var_size(self))
            captured_mem[mem_slice] = self._py_data[mem_slice]
            pointed_to_mem = self._py_data

        return self.clone(dspace=captured_mem, ptd_to_space=pointed_to_mem)
    
    @contextmanager
    def footprint_prefetched(self):
        """
        Wrap up the range prefetcher for an arbitrary Variable read
        """
        try:
            self._py_data.address_range_prefetched
        except AttributeError:
            # A substitute py_data that doesn't have (or presumably need) range
            # prefetching - so use a null context guard
            yield
        else:
            with self._py_data.address_range_prefetched(*var_start_stop(self)) as context_var:
                yield context_var

    
    def compares_equal_to(self, other, seen, filter_type_list=None, incl_not_excl=False):
        """
        Internal method used to implement a given object's equality check, while
        remembering what variables have already been seen by a checker (this
        avoids infinite loops and excessive re-checking of shared resources)
        """
        seen.add((var_address(self), var_address(other), var_typename(self)))
        
        # default notion of equality is identity
        if DEBUG_COMPARES_EQUAL:
            if var_address(self) != var_address(other):
                iprint("unequal as non-identical")
        return var_address(self) == var_address(other)
    
    def equals(self, other, filter_type_list=None, incl_not_excl=False):
        """
        Public API for qualified equality checking
        """
        
        return self.compares_equal_to(other, set(), filter_type_list=filter_type_list,
                                      incl_not_excl=incl_not_excl)
    
    def __eq__(self, other):
        if not isinstance(other, _Variable):
            return False
        # Full equality is just compares_equal_to with no filtering
        return self.equals(other)
        
    def __ne__(self, other):
        return not (self == other)


# -----------------------------------------------------------------------------
# BELOW HERE are undeveloped placeholders copied from xide_env implementation.
# -----------------------------------------------------------------------------

class _Function (_Variable):
    """
    Self-describing Function Variable 
    """
       
    def __init__(self, info, dspace, env=None):
        
        _Variable.__init__(self, info, dspace, env=env)
        
class _Integer (_Variable, IInt):
    """
    Self-describing Integer Variable 
    """
    wrapping_enabled = False
       
    def __init__(self, info, dspace, env=None):
        
        _Variable.__init__(self, info, dspace, env=env)
        
        self._py_address_slice = slice(self.address,
                                    self.address + self.size)

        if "bit_size" in self._py_info.struct:
            self._py_is_bitfield = True
            total_bits = self._py_info.struct["byte_size"] * 8
            
            self._py_shift = total_bits - self._py_info.struct["bit_offset"] - \
                    self._py_info.struct["bit_size"]
            self._py_mask = ((1 << self._py_info.struct["bit_size"]) - 1) << self._py_shift
        else:
            self._py_is_bitfield = False
        


    def compares_equal_to(self, other, seen, filter_type_list=None, incl_not_excl=False):
        
        # Note: we don't consider integers of different declared types to be 
        # equal even if their numerical values are equal.  This is debatable
        # given C's willingness to implicitly convert integral types, but we've 
        # taken a conservative approach as the safest way to avoid stepping into
        # that minefield.
        
        seen.add((var_address(self), var_address(other), var_typename(self)))
        
        if not isinstance(other, _Integer) or var_typename(self) != var_typename(other):
            if DEBUG_COMPARES_EQUAL:
                iprint("unequal because types don't match")
            return False
        if not _passes_type_filter(filter_type_list, incl_not_excl, var_typename(self)):
            return True
        
        if DEBUG_COMPARES_EQUAL:
            if self.value != other.value:
                iprint("unequal because values differ")
        return self.value == other.value

    @classmethod
    def set_wrap_policy(cls, wrapping):
        """
        Method to set the wrapping policy for integers in the current session.

        :p wrapping If False, then integers will not wrap. Rather, an exception will
        be raised if an attempt to set an out of range value is made. This is the default
        behaviour of a pydbg session. If True, then integer will overflow.
        """
        cls.wrapping_enabled = wrapping

    # IInt compliance
    def get_value(self):
        try:
            words = self._py_data[self._py_address_slice]
        except AddressSpace.NoAccess:
            return None
        unsigned = self._py_info._layout_info.deserialise(words)
        if self._py_is_bitfield:
            return (unsigned & self._py_mask) >> self._py_shift
        if self._py_info.signed:
            return to_signed(unsigned, self._py_info.byte_size * 8)
        return unsigned
    
    def _populate_footprint(self, value, byte_offset, byte_list):
        """
        Populate the memory footprint of this integer with the given value.
        If the integer is a bitfield, we OR the footprint bytes into the given
        byte list because at least some of them may share bits with other
        values (these bits will be set separately)
        """
        if self._py_is_bitfield:
            
            if value & (self._py_mask >> self._py_shift) != value:
                raise ValueError("{} is too wide for a {}-bit field".format(value, self._py_info.struct["bit_size"]))
            
            value = (value << self._py_shift) & self._py_mask
            bytes = self._py_info._layout_info.serialise(value, self._py_info.size,
                                                         self.wrapping_enabled)
            
            # Now we need to "or" these bytes into the byte_list
            for i,byte in enumerate(bytes):
                byte_list[byte_offset + i] |= byte
        
        else:
            if self._py_info.signed:
                value = from_signed(value, self._py_info.byte_size * 8, self.wrapping_enabled)

            bytes = self._py_info._layout_info.serialise(value, self._py_info.size,
                                                         self.wrapping_enabled)
            byte_list[byte_offset:byte_offset+len(bytes)] = bytes
    
    def set_value(self, new_value):
        if self._py_is_bitfield:
            # Read the value to initialise the byte_list that _populate_footprint writes
            # into with the bits that aren't ours to set.
            
            words = self._py_data[self._py_address_slice]
            value = self._py_info._layout_info.deserialise(words)
            value &= ~self._py_mask
            byte_list = self._py_info._layout_info.serialise(value, self._py_info.size, self.wrapping_enabled)
            

        else:
            # For a non-bit-offset value, we don't OR-in the values, so we can
            # just set the byte list to all zeros.
            byte_list = [0]*var_size(self)
            
        self._populate_footprint(new_value, 0, byte_list)
            
        # Finally we write the footprint to the chip.
        self._py_data[self._py_address_slice] = byte_list
      
    if _Variable.USE_PROPERTIES_FOR_INTS:
        get = get_value
        set = set_value
    def get(self):
        return self

    # _Variable compliance
    
    @property
    def value_string(self):
        """
        Construct a string displaying the integer's value in hex
        """
        try:
            value = self.value
            if self._py_is_bitfield:
                bit_size = self._py_info.struct["bit_size"]
                # Work out the number of nibbles needed to represent the bitfield
                if bit_size < 4:
                    # In this case, don't use hex notation at all - it's redundant
                    num_nibbles = None
                else:
                    num_nibbles = (bit_size + 3) // 4
            else:
                num_nibbles =  (2*self._py_info.byte_size)
            if value >= 0:
                value_fmt = r"0x%0" + "%dx" % num_nibbles if num_nibbles is not None else "%d"
                return value_fmt % value
            elif value is not None:
                value_fmt = r"-0x%0" + "%dx" % num_nibbles if num_nibbles is not None else "-%d"
                return value_fmt % (-1*value)
            else:
                return "<Integer at 0x%x is in inaccessible space>" % self.address
        except AddressSpace.NoAccess:
            # This can happen when mapping in a buffer, for instance
            return "<Integer at 0x%x is in inaccessible space>" % self.address

    def display(self, name, prefix, target, derefed, ll_expand=False, last=False,
                depth_limit=None):

        #If we're displaying an integer it makes no sense to still have a target
        #member to display
        assert len(target) < 2
        
        ostr = []
        if len(target) == 1:
            ostr.append(self.report_addr(name))
        if self._py_is_bitfield:
            name += ":{}".format(self._py_info.struct["bit_size"])
        if "%v" in self._py_info.datatype:
            decl_string = self._py_info.datatype.replace("%v", name)
        else:
            decl_string = "{} {}".format(self._py_info.datatype, name)
            
        ostr.append("%s-%s : %s" % (prefix, decl_string, self.value_string))
        return ostr
    

class _Enum(_Integer, IEnum):
    """\
    Self-describing Enumeration Variable 
    """
    
    def __init__(self, info, dspace, env=None):
        
        _Integer.__init__(self, info, dspace, env=env)
        
        self._py_sym_dict = self._py_info.struct["enumerators"]
        
    def get_symbolic_value(self):
        """
        There could be multiple enumerators with the same value
        """
        value_list = [sym for (sym, val) in self._py_sym_dict.items() \
                                                          if val == self.value]
        if len(value_list) == 1:
            return value_list[0]
        elif value_list:
            return value_list
        else:
            # Return value if it isn't in the enumeration dictionary
            return self.value
    
    def set_symbolic_value(self, v_str):
        
        self.value = self._py_sym_dict[v_str]
    
    @property
    def value_string(self):
    
        try:
            val = self.value
            if val is not None:
                hex_width = 2*self._py_info.byte_size
                value_fmt = r"%s (0x%" + "%02dx)" % hex_width
                return value_fmt % (self.symbolic_value, self.value)
            else:
                raise AddressSpace.NoAccess
        except AddressSpace.NoAccess:
            # This can happen when mapping in a buffer, for instance
            return "<Enumeration at 0x%x is in inaccessible space>" % self.address


    def display(self, name, prefix, target, derefed, ll_expand=False, last = False,
                depth_limit=None):
        
        #If we#'re displaying an integer it makes no sense to still have a target
        #member to display
        assert len(target) < 2
        
        ostr = []
        if len(target) == 1:
            ostr.append(self.report_addr(name))
        ostr.append("%s-%s %s : %s" % (prefix, self._py_info.datatype, name,
                                       self.value_string))
        return ostr


def get_struct_semantics(type_dict, py_env):
    """
    Helper function to look up the semantics of a given type, represented by
    type_dict, using the environment py_env (which might be None).  Tries to 
    find semantics data using either the type_name or the base_type_name, with
    irrelevant qualifiers (const, volatile) stripped off.
    """
    if py_env is None:
        return {}

    def disqualify(raw_typename):
        if raw_typename in ("<anonymous>", None):
            return False
        # Note: trailing space deliberately included to simplify matching
        for qual in ("const ", "volatile "):
            if raw_typename.startswith(qual):
                raw_typename = raw_typename[len(qual):]
        return raw_typename

    typename, base_typename = (disqualify(type_dict.get("type_name")), 
                               disqualify(type_dict.get("base_type_name")))
    
    if typename is not False:
        # self._py_env might be a plain FirmwarePresenter, which doesn't
        # have any connection with the calling context, so in particular
        # doesn't have a struct_semantics attribute.  It might also be
        # None.
        try:
            py_env.struct_semantics
        except AttributeError:
            struct_semantics = {}
        else:
            struct_semantics = py_env.struct_semantics.lookup(typename, 
                                                              alt_name=base_typename).data
    else:
        struct_semantics = {}

    return struct_semantics

def equal_semantics(sem1, sem2):
    
    # Potential extension: Technically we should probably consider variables with unequal
    # semantics to be unequal, but in practice this is unlikely to be encountered
    # so it's not a priority to get this done. 
    return True
    


class InvalidDereference(RuntimeError):
    """
    Indicates an attempt to dereference a pointer that isn't valid
    """


class _Pointer(_Integer):
    """
    Class that describes pointers.  It allows dereferencing, unless
     - the pointer's value is 0
     - ptd_to_space is False
     - accessing the address in ptd_to_space leads to a NoAccess exception
    If ptd_to_space is not specified, the same data space that the _Pointer is
    evaluated is used for dereferencing.
    Hence to construct _Pointers that you don't want dereferenced, pass 
    ptd_to_space=False in the factory method.
     
    The dereferenced pointer is a new _Variable of the appropriate concrete type
    constructed on the fly.
    """

    suppress_nonzero_pointer_value_display = False

    def __init__(self, info, dspace, ptd_to_space=None,
                 semantics=None, env=None):

        _Integer.__init__(self, info, dspace, env=env)

        self._py_ptd_to_space = ptd_to_space if ptd_to_space is not None else self._py_data
            
        var_set_semantics(self, semantics)

    def clone(self, dspace=None, ptd_to_space=None, env=None):
        """
        Creae a clone of self by reconstructing the arugments passed to the
        constructor
        """
        info = self._py_info
        env = env if env is not None else self._py_env
        dspace = dspace or self._py_data
        ptd_to_space = ptd_to_space or self._py_ptd_to_space
        return _Pointer(info, dspace, ptd_to_space=ptd_to_space, 
                        semantics=copy_semantics(self._semantics), env=env)

    def set_semantics(self, semantics, update=False):
        # Split out semantics to be applied to the pointer itself to those to
        # be applied to the pointed-to object
        
        if not update:
            self._semantics = {}
        
        if semantics is None:
            semantics = {}
            
        self._semantics.update(semantics)

        if not update:
            # Now pull in intrinsic semantics of this structure (these will be 
            # specified on a per-field basis - there are no intrinsic semantics that
            # are meaningful for the structure as a whole)
            try:
                pointed_to = self._py_info.struct["pointed_to"]
            except KeyError:
                # void pointer
                pass
            else:
                ptd_to_semantics = get_struct_semantics(pointed_to, self._py_env)
                # Reset the keys to account for the dereference
                struct_semantics = {"deref."+k:v for (k,v) in ptd_to_semantics.items()}
                
                # Merge in the semantics of fields inherited from outer structures: if
                # the key alread exists, merge the dicts, otherwise add a new entry 
                # (i.e. merge with an empty dict)
                for k in self._semantics:
                    struct_semantics.setdefault(k, {}).update(self._semantics[k])
                self._semantics = struct_semantics

        if self._semantics.get("",{}).get("depth", None) is not None:
            # We aren't allowed to expand this pointer as a linked list because
            # the owning structure doesn't treat it as one
            self._ll = None
        else:
            # Now figure out if there are any linked list fields in the dereferenced
            # structure, taking into account typing and explicit depth-limit semantics
            self._ll_fields = set(self._py_info.linked_list)
            # If the pointed-to object limits the expansion of a linked-list-like
            # field, then it's not to be treated as a linked list
            ll_excluded = {ll for ll in self._ll_fields if 
                           self._semantics.get("deref."+ll,{}).get("depth",None) != None}
            # What do we have left after that?
            ll_remaining = self._ll_fields - ll_excluded
            if len(ll_remaining) == 1:
                self._ll = list(ll_remaining)[0]
            else:
                self._ll = None

    def add_semantics(self, semantics):
        var_set_semantics(self, semantics, update=True)

    def get_semantics(self):
        return self._semantics

    @property
    def _deref_type_dict(self):
        #The pointer's value is (of course) the pointed-to type.  However,
        #it's possible to construct a _Pointer from *array* variable info,
        #in which case we want the "element_type"
        try:
            pointed_to = self._py_info.struct["pointed_to"]
            if "byte_size" in pointed_to:
                # If the structure declaration contains an opaque pointer, look it
                # up in the global type information so we can decode it
                try:
                    if (not isinstance(pointed_to["byte_size"], int_type) and
                            pointed_to["type_name"]):
                        pointed_to = \
                    self._py_env.types[pointed_to["type_name"]]
                except AttributeError:
                    # self._py_env is None, so we have no access to
                    # the global type information
                    pass
                except read_dwarf.DwarfNoSymbol:
                    # the type is probably literally opaque, i.e. only exists
                    # as a declaration
                    pass
        except KeyError:
            try:
                pointed_to = self._py_info.struct["element_type"]
            except KeyError:
                raise ValueError("_Pointer constructed on neither pointer "
                                 "nor array info!")
                
        return pointed_to
    
    def __getitem__(self, index):
        
        if not self._py_valid:
            raise InvalidDereference("Attempting to dereference an invalid pointer: %s" % self.value_string)
        
        if not self.is_linked_list or index == 0:
            
            # Get the element size by creating a VariableInfo with an arbitrary
            # address and then using its size method
            dummy_ptd_to_info = DwarfPointerTargetVariableInfo(0,
                                                   self._deref_type_dict,
                                                   self._py_info._layout_info)

            deref_semantics = {k[6:]:v for (k,v) in list(self._semantics.items()) 
                                                    if k.startswith("deref")}
            return _Variable.factory(
                    DwarfPointerTargetVariableInfo(self.value + 
                                                   index*dummy_ptd_to_info.size,
                                                   self._deref_type_dict,
                                                   self._py_info._layout_info),
                    self._py_ptd_to_space,
                    semantics=deref_semantics, env=self._py_env).get()
        else:
            linked_list = self.expand_linked_list()
            
            if index >= len(linked_list):
                raise IndexError("Can't access element %d of linked list: only "
                                 "%d elements long" % (index, len(linked_list)))
            
            return linked_list[index].deref
            
    @property
    def is_linked_list(self):
        return self._ll != None

    @property
    def deref(self):
        return self[0]
     
    def expand_linked_list(self):
        '''
        If this object is a linked list pointer then this method returns a
        list of objects (of the type this Pointer points to) by following
        the 'next' element of the pointed to structure.
        '''
        enable_linked_list_prefetch = False
        
        def fetch_range(data, range):
            if enable_linked_list_prefetch:
                try:
                    data.prefetch_address_range
                except AttributeError:
                    pass
                else:
                    data.prefetch_address_range(*range)
        def flush_range(data, range):
            if enable_linked_list_prefetch:
                try:
                    data.discard_prefetched
                except AttributeError:
                    pass
                else:
                    data.discard_prefetched(*range)
            
        
        
        expanded_list = []
        pointer = self.clone() # clone self because we're going to fiddle with
        # the semantics later and we don't want to affect the original object
        pointer_range = var_start_stop(pointer)
        fetch_range(pointer._py_data, pointer_range)
        try:
            values_seen = set()
            if var_is_linked_list(self):
                while (pointer._py_valid and pointer.value not in values_seen and
                       pointer._semantics.get("",{}).get("depth", None) != 0):
                    expanded_list.append(pointer)
                    values_seen.add(pointer.value)
                    old_pointer = pointer
                    pointer = pointer.deref[self._ll]
                    # Replicate any custom semantics from the head of the linked list
                    # to the following element
                    pointer.add_semantics(old_pointer.get_semantics())
                    flush_range(old_pointer._py_data, pointer_range)
                    pointer_range = var_start_stop(pointer)
                    fetch_range(pointer._py_data, pointer_range)
                for ptr in expanded_list:
                    # Now set the semantics on each pointer in the list so it just
                    # expands the entry it's pointing to and then stops
                    var_add_semantics(ptr, {"deref.%s" % self._ll : {"depth" : 0}})
                
            else:
                raise ValueError("Trying to expand non-linked-list of type %s as a "
                                 "linked list" % var_typename(self))
        finally:
            flush_range(pointer._py_data, pointer_range)
            
        return expanded_list

    @property
    def _py_valid(self):
        if self.is_func_ptr:
            return True # Potential extension: Ideally we need a function space attribute so 
        # we can test this properly

        if (self.value != 0 and 
            self.value is not None and  
            self._py_ptd_to_space is not False and 
            self._py_ptd_to_space is not None and
            ("byte_size" in self._deref_type_dict and 
               isinstance(self._deref_type_dict["byte_size"], int_type)
               )
            ):
            try:
                self._py_ptd_to_space[self.value]
                return True
            except AddressSpace.NoAccess:
                return False
            except AddressSpace.ReadFailure:
                return False
            except (KeyError, IndexError):
                # These can occur if the variable's underlying memory is a list
                # or dictionary (e.g. a captured variable)
                return False
        return False

    def _py_print_c_string(self):
        addr = self.value
        ostr = ""
        while 1:
            word = self._py_ptd_to_space[addr]
            if word > 0xff:
                return " <Bad string>"
            char0 = word & 0xff
            if char0 == 0:
                break
            ostr += chr(char0)
            addr += 1
        return ostr

    @property
    def is_func_ptr(self):
        try:
            return self._deref_type_dict["type_tag"] == read_dwarf.DW_TAG["subroutine_type"]
        except KeyError:
            # deref_type_dict might simply be {"type_name" : "void"}
            return False

    @property
    def interpret_as_string(self):
        """
        Should this pointer's contents be interpreted as a null-terminated ascii
        string?
        """
        # If "is_string" isn't set, default True if it's const_char_star and
        # False otherwise
        is_const_char_star = self._deref_type_dict["type_name"] == "const char"
        return self._semantics.get("",{}).get("is_string", is_const_char_star)
    
    def display(self, name, prefix, target, derefed, ll_expand=False, last=False,
                depth_limit=None):
        #If pointer deref gets a "*" top-level target, it means the caller is
        #requesting an immediate dereference and doesn't want the pointer's
        #value displayed.
        
        if "%v" in self._py_info.datatype:
            decl_string = self._py_info.datatype.replace("%v", name) 
        else:
            decl_string = "{} {}".format(self._py_info.datatype, name)
        
        if len(target) > 1:
            if target[1] != "*":
                raise ValueError("Bad display target: pointer '%s' requested "
                                 "to display '%s'" % (name, target[1]))
            ostr = []
        elif self.value != 0 and self.suppress_nonzero_pointer_value_display:
            ostr = ["%s-%s" % (prefix, decl_string)]
        elif self.is_func_ptr and "*" in self._py_info.datatype and self._deref_type_dict["type_name"] == "%v":
            try:
                ret_type = self._deref_type_dict["type"].typename
            except KeyError:
                ret_type = "void"
            param_string = ", ".join(p[1].typename for p in self._deref_type_dict["params"])
            ostr = ["%s-%s (*%s)(%s) %s" % (prefix, ret_type, name, param_string, self.value_string)]
        else:
            ostr = _Integer.display(self, name, prefix, target, derefed)
            if last:
                prefix = "%s " % prefix[:-1]

        deref_depth_limit= depth_limit - 1 if depth_limit is not None else None
        depth = self._semantics.get("",{}).get("depth",None)
        if self._py_valid and depth != 0:
            if not self.value in derefed:
                if not self.is_linked_list:
                    if self._deref_type_dict["type_name"] != "void":
                        real_array_len = depth
                        if self.is_func_ptr:
                            try:
                                if self._py_env is not None and self.value != 0:
                                    func_name = self._py_env.functions[self.value]
                                else:
                                    func_name = None
                            except KeyError:
                                func_name = "not found"
                            except AttributeError:
                                func_name = "can't look up"
                            if func_name is not None:
                                ostr[-1] += " (%s)" % func_name
                        elif self.interpret_as_string:
                            ostr[-1] += " ('%s')" % self._py_print_c_string()
                        elif real_array_len is not None and real_array_len > 1 and depth_limit != 0:
                            # Print the dereferenced values out as an array 
                            for i in range(real_array_len):
                                ostr += var_display(self[i],
                                                    "%s[%d]" % (name,i),
                                                    "%s   |" % prefix,
                                                    target[1:],
                                                    derefed + [self.value],
                                                    depth_limit=deref_depth_limit)
                        else:
                            var_name = (None if self._py_env is None else 
                                        self._py_env.vars.find_by_address(self.value))
                            if var_name is not None:
                                var_obj = self._py_env.vars[var_name]
                                var_addr = var_address(var_obj)
                                if var_addr == self.value: # check we're 
                                    #pointing to the start of the variable, not 
                                    # at some subfield
                                    try:
                                        # qualify the variable name with the
                                        # CU name if it's not a unique var name
                                        var_name, cu_name = self._py_env.vars.name_cu_from_key(var_name)
                                        try:
                                            self._py_env.vars[var_name]
                                            unique=True
                                        except KeyError:
                                            unique=False
                                        ostr[-1] += " (%s" % var_name
                                        ostr[-1] += ")" if unique else (":%s)" % cu_name)
                                    except ValueError:
                                        # global
                                        ostr[-1] += " (%s)" % var_name
                                else:
                                    # We're pointing inside a known var, so we're
                                    # not actually pointing *at* one
                                    var_name = None
                            # In compressed display mode, only display pointed-
                            # to variables that aren't found amongst the 
                            # globals and statics
                            if depth_limit != 0 and (var_name is None or 
                                                     self._py_env is None or 
                                                     not self._py_env.compressed_var_display):
                                ostr += var_display(self.deref,
                                                    "*%s" % name, "%s   |" % prefix,
                                                    target[1:], 
                                                    derefed + [self.value],
                                                    # Pretend to the pointed-to struct
                                                    # that we're already doing linked
                                                    # list display in case it would 
                                                    # try to do so itself
                                                    ll_expand=True,
                                                    depth_limit=deref_depth_limit)
                elif not ll_expand and depth_limit != 0:
                    prev_value = self.value
                    for i,elem in enumerate(self.expand_linked_list()):
                        if self.suppress_nonzero_pointer_value_display:
                            ostr += ["%s- %s[%d] :" % (prefix, name, i)]
                        else:
                            ostr += ["%s- %s[%d] : 0x%x" % (prefix, name, i, elem.value)]
                        ostr += var_display(elem.deref,
                                            "*%s" % name, 
                                            "%s   |" % prefix,
                                            target[1:], 
                                            derefed + [prev_value],
                                            ll_expand=True,
                                            depth_limit=deref_depth_limit)
                        prev_value = elem.value
                    
            else:
                ostr[-1] += " (circular dereference)"
        elif len(target) > 1:
            # If this pointer was specifically targeted, we need to return
            # something
            ostr += ["Invalid pointer (0x%04x): cannot dereference" % \
                                                                    self.value]

        return ostr

    def compares_equal_to(self, other, seen, filter_type_list=None, incl_not_excl=False):
        
        if (var_address(self), var_address(other), var_typename(self)) in seen:
            return True
        seen.add((var_address(self), var_address(other), var_typename(self)))
                
        if (not isinstance(other, _Pointer) or 
            var_typename(self) != var_typename(other) or
            not equal_semantics(self._semantics, other._semantics)):
            if DEBUG_COMPARES_EQUAL:
                if not isinstance(other, _Pointer):
                    iprint("unequal because other is not a _Pointer but a {}".format(other.__class__.__name__))
                elif var_typename(self) != var_typename(other):
                    iprint("unequal because typenames are: {} and {}".format(
                            var_typename(self),var_typename(other)))
                else:
                    iprint("unequal because semantics differ")
            return False
        
        if not _passes_type_filter(filter_type_list, incl_not_excl,
                                   self._py_info.struct["pointed_to"]["type_name"]):
            # An pointer to a type we don't care about: any differences are unimportant
            return True

        depth = self._semantics.get("",{}).get("depth")
        # We do a special case for zero depth here to avoid invoking the validity 
        # checks for pointers we're just intending to ignore
        if depth == 0:
            return True

        if not other._py_valid or not self._py_valid:
            return _Integer.compares_equal_to(self, other, seen, filter_type_list,
                                              incl_not_excl)
            
        if depth is not None:
            # Explicit non-zero depth semantic: compare the pointers as arrays
            #if DEBUG_COMPARES_EQUAL:
            #    print("(checking pointer as array)")
            return all(self[i].compares_equal_to(other[i], seen, 
                                                 filter_type_list, 
                                                 incl_not_excl) for i in range(depth))
            
        # Apply the implicit (or explicit) "this pointer is a null-terminated
        # string" semantic
        if self.interpret_as_string and other.interpret_as_string:
            if DEBUG_COMPARES_EQUAL:
                if self._py_print_c_string() != other._py_print_c_string():
                    iprint("unequal because string interpretations differ")
            return self._py_print_c_string() == other._py_print_c_string()
        
        #if DEBUG_COMPARES_EQUAL:
        #    print("(checking referents for equality)")
        return self.deref.compares_equal_to(other.deref, seen,
                                            filter_type_list=filter_type_list,
                                            incl_not_excl=incl_not_excl)
        


class _Array(_Variable, IArray):
    """\
    Firmware array descriptor.  The supplied IVariableInfo's struct must have a
    "num_elements" member.
    """

    def __init__(self, info, dspace, ptd_to_space, semantics=None, env=None,
                 retrieve_type_semantics=True):

        _Variable.__init__(self, info, dspace, env=env)
        self._py_ptd_to_space = ptd_to_space
        if semantics is None:
            semantics = {}
        self.set_semantics(semantics, update=not retrieve_type_semantics)

    def set_semantics(self, semantics_dict, update=False):
        
        # Standard shape for an array semantics dict is to have keys "" for the
        # array itself and "[]" for elements. But if neither is present, assume
        # that "" is meant.
        if "" not in semantics_dict and "[]" not in semantics_dict:
            self._semantics = {"" : semantics_dict}
        else:
            self._semantics = semantics_dict

        if not update:
            # Now pull in intrinsic semantics of this array 
            struct_semantics = get_struct_semantics(self._py_info.struct, self._py_env)
                
            # Merge in the semantics inherited from outer structures: if
            # the key alread exists, merge the dicts, otherwise add a new entry 
            # (i.e. merge with an empty dict)
            for k in self._semantics:
                struct_semantics.setdefault(k, {}).update(self._semantics[k])
            self._semantics = struct_semantics


    def compares_equal_to(self, other, seen, filter_type_list=None, incl_not_excl=False):
        if (var_address(self), var_address(other), var_typename(self)) in seen:
            return True
        seen.add((var_address(self), var_address(other), var_typename(self)))

        if not isinstance(other, _Array) or var_typename(self) != var_typename(other):
            if DEBUG_COMPARES_EQUAL:
                iprint("unequal as arrays because not both arrays or typenames differ")
            return False

        if not _passes_type_filter(filter_type_list, incl_not_excl,
                                  self._py_info.struct["element_type"]["type_name"]):
            # An array of elements of a type we don't care about - any differences
            # are unimportant
            return True
        
        #if DEBUG_COMPARES_EQUAL:
        #    print("(checking array elements for equality)")
        return all((e1.compares_equal_to(e2, seen, filter_type_list=filter_type_list,
                                         incl_not_excl=incl_not_excl) for (e1,e2) in zip(self.elements, other.elements)))
        
    @property
    def value(self):
        with self.footprint_prefetched():
            return [var_value(elt) for elt in self]
        
    @property
    def _length(self):
        length = self._semantics.get("",{}).get("length", self._py_info.struct["num_elements"])
        if isinstance(length, _Integer):
            return length.value
        return length
    
    def _populate_footprint(self, value, byte_offset, byte_list):
        """
        Write the bytes required to represent the given value for the array into
        the byte list at the given offset. This recursively calls each of the
        elements' _populate_footprint methods.
        """
        value = list(value) # make a list in case value is merely an iterable.
        
        elt_type_dict = self._py_info.struct["element_type"]
        layout_info = self._py_info._layout_info
        elt_byte_size = elt_type_dict["byte_size"]
        elt_addr_unit_size = elt_byte_size // (layout_info.addr_unit_bits//8)

        for i, elt in enumerate(self):
            elt_offset = i*elt_addr_unit_size
            elt._populate_footprint(value[i], byte_offset + elt_offset, byte_list)
        
    @value.setter
    def value(self, values):
        raw_values = [0]*var_size(self)
        self._populate_footprint(values, 0, raw_values)
        self._py_data[self.address:self.address+len(raw_values)] = raw_values
    
    
    # IArray compliance

    @property
    def elements(self):
        try:
            self._py_elements
        except AttributeError:
            elem_semantics = self._semantics.get("[]", {})
            self._py_elements = self._ElementArray(self,  
                                                   self._py_data,
                                                   self._py_ptd_to_space,
                                                   semantics=elem_semantics,
                                                   env=self._py_env)
        return self._py_elements

    @property
    def num_elements(self):
        return len(self.elements)

    def override_num_elements(self, num_elements):
        ''' 
        Force the number of elements of an array to the given value.
        This can be useful for variable sized arrays where the C structure
        has a defined fixed default size.
        ''' 
        self._py_info.struct["num_elements"] = num_elements
        try:
            del self._py_elements
        except AttributeError:
            pass
        
    # _Variable compliance

    def display(self, name, prefix, target, derefed, ll_expand=False, last=False, 
                depth_limit=None):
        """
        Display the array and all or some of its elements
        """
        with self.footprint_prefetched():

            ostr = [] #Just to get us started
            #Construct the appropriate element slice of the array
            if len(target) > 1:
                #If we haven't fully resolved the target at this level, the next
                #resolution should be an indexing entity
                if isinstance(target[1], int_type):
                    elem_slice = slice(target[1], target[1] + 1)
                elif isinstance(target[1], slice):
                    elem_slice = target[1]
                else:
                    raise ValueError("Bad display target: array '%s' requested to "
                                     "display '%s'" % name, target[1])
            else:
                #If we are fully resolved, print out the
                #array name and update the prefix to indent the element display
                if len(target) == 1:
                    #If we newly resolved at this level, print a header string
                    ostr.append(self.report_addr(name))
    
                if "%v" in self._py_info.datatype:
                    if "[" in name:
                        # It's the name of subarray, so we should parenthesise
                        sub_name = "("+name+")"
                    else:
                        sub_name = name
                    decl_string = self._py_info.datatype.replace("%v", sub_name)
                else:
                    decl_string = "{} {}".format(self._py_info.datatype, name)
                ostr.append("%s-%s" % (prefix, decl_string))
                if last:
                    prefix = "%s " % prefix[:-1]
                prefix = "%s   |" % prefix
                elem_slice = slice(0, len(self.elements))
    
            #Display the slice.
            prev_elem = None
            prev_ostr = None
    
            if depth_limit != 0:
                elem_depth_limit = depth_limit - 1 if depth_limit is not None else None
        
                for index, elem in (list(zip(list(range(len(self.elements))), 
                                             self.elements)))[elem_slice]:
                    if len(target) > 1:
                        #We're not resolved, so pass the requested name down as is
                        elem_name = name
                    else:
                        #We are already resolved, so extend the requested name with
                        #the index
                        elem_name = "%s[%d]" % (name, index)
                        
                    if not self._py_env or not self._py_env.compressed_var_display or prev_elem is None or elem != prev_elem:
                        if prev_ostr is not None:
                            # Write the last element in the sequence of same values
                            ostr += prev_ostr
                            prev_ostr = None
                        # Write the new, different element
                        ostr += var_display(elem, elem_name, prefix, target[1:], derefed,
                                            depth_limit=elem_depth_limit)
                        count_same = 1
                    else:
                        # Same as the last.  Print a continuation character once we've seen
                        # the third identical value in a row
                        count_same += 1
                        if count_same == 3 :
                            ostr.append("%s  :" % prefix)
                        # Remember this element in case it's the last one before a change,
                        # in which case we'll want to print it later 
                        prev_ostr = var_display(elem, elem_name, prefix, target[1:], derefed,
                                                depth_limit=elem_depth_limit)
                    prev_elem = elem
                if prev_ostr is not None:
                    ostr += prev_ostr
            return ostr
    
    # Private
    
    class _ElementArray(object):
        """
        Private class providing an array-like description of the elements of an
        Array variable        
        """
        def __init__(self, array, dspace, ptd_to_space=None,
                     semantics=None, env=None):
            self._array = array
            self._array_info = array._py_info
            self._env = env
            self._dspace = dspace
            self._ptd_to_space = ptd_to_space
            self._elem_semantics = semantics
            
        def __len__(self):
            return self._array._length
        
        def __getitem__(self, i):
            #Does the framework automatically do this?
            if isinstance(i, slice):
                step = 1 if i.step is None else i.step
                if i.stop is None or i.stop > len(self):
                    stop = len(self)
                else:
                    stop = i.stop
                start = 0 if i.start is None else i.start
                elems = [0] * int((stop - start) // step)
                for ii in range(start,stop,step):
                    elems[(ii-start)//step] = self[ii]
                return elems
            else:
                if i < 0 or i >= len(self):
                    raise IndexError
                return _Variable.factory(\
                         DwarfArrayElementVariableInfo(i, self._array_info,
                                                   self._array_info._layout_info),
                         self._dspace,
                         self._ptd_to_space,
                         semantics=self._elem_semantics, env=self._env)
                    
        def __iter__(self):
            return self._Iter(self)
            
        class _Iter(object):
            
            def __init__(self, array):
                self._a = array
                self._i = 0
                
            def __next__(self):                    
                try:
                    element = self._a[self._i]
                    self._i = self._i + 1 
                except IndexError:                    
                    raise StopIteration
                
                return element
            
            def next(self):
                return self.__next__()

    def __len__(self):
        return len(self.elements)

    def __getitem__(self, index_or_slice):
        if isinstance(index_or_slice, slice):
            sl = index_or_slice
            # info, dspace, ptd_to_space, semantics=None, env=None
            info = DwarfSubarrayVariableInfo(self._py_info, sl.start, sl.stop)
            return _Array(info, self._py_data, self._py_ptd_to_space, 
                          semantics=self._semantics, env=self._py_env)
        return self.elements[index_or_slice]
            

def copy_semantics(semantics_dict):
    """
    When cloning Structures and Pointers we pass the semantics to the new object,
    of course.  However, we need to be able to independently modify entries in 
    the field-wise subdictionaries, so the semantics dict in the cloned object
    needs to be a completely new parent dictionary containing shallow copies of 
    the field dictionaries.  That way we can independently overwrite entries
    in the field dictionaries in the original and cloned objects.  (A true 
    deep copy is too deep, because the entries may contain references to 
    arbitrary objects, e.g. when the union discrimination function is bound to
    a particular structure instance)
    """
    new_dict = {}
    for field, fdict in list(semantics_dict.items()):
        new_dict[field] = fdict.copy()
    return new_dict

class _Structure (_Variable, IStruct):
    """\
    Generic firmware structure descriptor.  This is automatically subclassed
    when constructed via the factory() method so that property attributes can
    be added corresponding to the wrapped C structure's attributes
    """
    _struct_id = 0

    @staticmethod
    def factory(*args, **kw_args):

        _Structure_Sub = type("_Structure_%d" % _Structure._struct_id,
                              (_Structure,), {})
        _Structure._struct_id += 1
        return _Structure_Sub(*args, **kw_args)
    
    def __init__(self, info, dspace, ptd_to_space=None, is_union=False,
                 semantics=None, env=None, retrieve_type_semantics=True):
        """
        :param info: DwarfVariableInfo object of some suitable kind
        :param dspace: Data space to which the struct type layout metadata refers
        :param ptd_to_space: Data space to which pointers within the struct refer
         (usually the same as dspace but can be different in "capture" scenarios)
        :param is_union: This is actually a union, rather than a struct
        :param semantics: Dictionary of semantics to apply
        :param env: Firmware environment.  Optional but best supplied if possible.
        :param retrieve_type_semantics: Flag allowing the caller to avoid having the
         semantic set-up logic look up the semantics that have been provided
         separately for this struct's type.  Should not normally be used (i.e.
         should be True), but is necessary when constructing faked-up non-DWARF
         variables, because these are constructed during construction of the
         environment, so trying to access the environment within that process
         leads to an infinite recursion.  In any case, we know that non-DWARF
         variables won't have any type-based semantics, since they don't even 
         have properly defined types.
        """
        _Variable.__init__(self, info, dspace, env=env)
        self._py_ptd_to_space = ptd_to_space
        self._py_is_union = is_union
        var_set_semantics(self, semantics, update=not retrieve_type_semantics)
        for name, sym in var_member_list(self):
            prop = sym._get_struct_attr_property()
            setattr(type(self), name, prop)
        
    def clone(self, dspace=None, ptd_to_space=None, env=None):
        """
        Creae a clone of self by reconstructing the arugments passed to the
        constructor
        """
        info = self._py_info
        env = env if env is not None else self._py_env
        dspace = dspace if dspace is not None else self._py_data
        ptd_to_space = ptd_to_space if ptd_to_space is not None else self._py_ptd_to_space
        return _Structure.factory(info, dspace, 
                                  ptd_to_space=ptd_to_space, 
                                  semantics=copy_semantics(self._semantics),
                                  env=env)

    @property
    def value(self):
        """
        Retrieve the struct's field values as a dictionary
        """
        with self.footprint_prefetched():
            # Walk through the member list populating a dict
            return {mbr : var_value(self[mbr]) for mbr in self.members}

    def _populate_footprint(self, value, base_offset, byte_list):
        """
        Populate the memory footprint of this structure at the
        given offset into the byte list by recursively populating the footprints
        of each of the members, at the appropriate offsets into the byte list.
        """
        raw_mbr_list = self._py_info.struct["members"]
        for name, offset, _ in raw_mbr_list:
            try:
                mbr_value = value[name]
            except KeyError:
                # No value given: leave the bytes alone
                pass
            else:
                self[name]._populate_footprint(mbr_value, base_offset + offset, byte_list)

    @value.setter
    def value(self, value_dict):
        
        byte_list = [0] * var_size(self)
        
        self._populate_footprint(value_dict, 0, byte_list)
        
        # Finally, write the new footprint to memory
        self._py_data[slice(*var_start_stop(self))] = byte_list
            


    def compares_equal_to(self, other, seen, filter_type_list=None, incl_not_excl=False):
        
        if (var_address(self), var_address(other), var_typename(self)) in seen:
            return True
        seen.add((var_address(self), var_address(other), var_typename(self)))

        if (not isinstance(other, _Structure) or 
            var_typename(self) != var_typename(other) or
            not equal_semantics(self._semantics, other._semantics)):
            if DEBUG_COMPARES_EQUAL:
                iprint("unequal as structures because not both structures or typenames or semantics differ")
            return False
        
        if not _passes_type_filter(filter_type_list, incl_not_excl, var_typename(self)):
            # This is a type we don't care about: differences are unimportant
            return True

        #if DEBUG_COMPARES_EQUAL:
        #    print("(checking members for equality)")
        mbr_equal_dict = {m1[0]:m1[1].compares_equal_to(m2[1], seen, filter_type_list, incl_not_excl) 
                for (m1,m2) in zip(self.member_list, other.member_list)}
        if DEBUG_COMPARES_EQUAL:
            diff_mbrs = [m for (m,eql) in mbr_equal_dict.items() if not eql]
            if diff_mbrs:
                iprint("(differing members are: {})".format(diff_mbrs))
        return all(mbr_equal_dict.values())

    def set_semantics(self, semantics, update=False):
        # Split the supplied semantics dictionary into the info to be applied
        # at this level, and the info that refers to subfields.
        if not update:
            self._semantics = {}
            
        if semantics is None:
            semantics = {}

        self._semantics.update(semantics)

        if not update:
            # Now pull in intrinsic semantics of this structure (these will be 
            # specified on a per-field basis - there are no intrinsic semantics that
            # are meaningful for the structure as a whole)
            struct_semantics = get_struct_semantics(self._py_info.struct, self._py_env)
                
            # Merge in the semantics of fields inherited from outer structures: if
            # the key alread exists, merge the dicts, otherwise add a new entry 
            # (i.e. merge with an empty dict)
            for k in self._semantics:
                struct_semantics.setdefault(k, {}).update(self._semantics[k])
            self._semantics = struct_semantics

        # Now figure out how to treat any linked-list-like fields in the
        # structure
        
        # First detect the logical linked list fields
        self._ll_fields = set(self._py_info.linked_list)
        # Now find those excluded from consideration by explicit semantics limiting
        # the depth (to 0 or 1)
        ll_excluded = {field for (field, sem) in list(self._semantics.items()) 
                             if field in self._ll_fields and sem.get("depth",None) != None}
        # And construct the set of linked list fields we can actually treat as such
        ll_allowed = self._ll_fields - ll_excluded
        # If there are more than one of these, or we've been explicitly told not
        # to treat ourselves as a linked list, we can't use any of them
        if len(ll_allowed) != 1:
            self._ll = None
        else:
            self._ll = list(ll_allowed)[0]
            
        try:
            self._py_member_list
            self.set_member_semantics(update=update)
        except AttributeError:
            pass # Members not constructed yet.  set_member_semantics will be
                 # called then.
            
    def set_member_semantics(self, update=False):
        """
        Write the semantics given in the object's sub_semantics dict into the
        appropriate member objects.  WARNING: this method forces creation of
        members.
        """
        for name, member in var_member_list(self):
            # Look up the entries in the parent semantics dict that refer to this
            # field or its children, and create a new dictionary containing those
            # entries with the name (and the following dot for children) removed
            field_semantics = {k[len(name)+1:]:v 
                                      for (k,v) in list(self._semantics.items()) 
                                          if k.startswith(name)}
            # If the linked list field exists and has a depth limit, reduce the
            # depth limit by one in the pointed-to structure
            if name == self._ll:
                depth_limit = field_semantics.get("", {}).get("depth",None) 
                if depth_limit is not None and depth_limit > 0:
                    field_semantics.setdefault("deref."+name,{})["depth"] = depth_limit-1
            
            for field, sem in list(field_semantics.items()):
                if (self._py_env is not None and 
                    "discrim" in sem and 
                    "bound_discrim" not in sem):
                    sem["bound_discrim"] = functools.partial(sem["discrim"], 
                                                             self._py_env,
                                                             self, name)
            if os.getenv("PYDBG_NO_DEFAULT_UNION_DISPLAY"):
                if (("" not in field_semantics or "discrim" not in field_semantics[""]) and 
                    isinstance(member, _Structure) and member._py_is_union):
                    # If an explicit discriminant evaluator hasn't been supplied,
                    # we can't discriminate the union
                    field_semantics.setdefault("",{})["bound_discrim"] = lambda : None
                
                
            if isinstance(member, _Array):
                 array_len_semantic = field_semantics.get("",{}).get("length",None)
                 if isinstance(array_len_semantic, str):
                     field_semantics[""]["length"] = self.members[array_len_semantic]

            var_set_semantics(member, field_semantics,update=update)

    def add_semantics(self, semantics):
        var_set_semantics(self, semantics, update=True)
    
    def get_semantics(self):
        return self._semantics

    @property
    def is_linked_list(self):
        """
        We treat this object as a linked list if there's exactly one 
        linked-list-like field that we're allowed to expand
        """
        return self._ll != None

    # IStructure compliance
    
    @property
    def members(self):
        try:
            self._py_member_dict
        except AttributeError:
            self._py_member_dict = self._MemberDict(var_member_list(self))
        return self._py_member_dict

    # Private
    
    @property
    def member_list(self):
        try:
            self._py_member_list
        except AttributeError:
            self._py_member_list = self._create_members()
            # Pass any explicit semantics from this object down to the member 
            # objects, updating their automatically-supplied type semantics
            self.set_member_semantics(update=True)
        return self._py_member_list
    
    class _MemberDict(object):
        
        def __init__(self, mbr_list):
            self._m_list = mbr_list
            self._index_dict = dict((m_entry[0], i) \
                                    for i, m_entry in enumerate(self._m_list))

        def __getitem__(self, key):
            """
            Dictionary-like indexing
            """
            member_index = self._index_dict[key]
            return self._m_list[member_index][1]

        def items(self):
            """
            Dictionary-like iteration
            """
            for (name, var) in self._m_list:
                yield (name, var)

        def keys(self):
            for (name, dummy) in self._m_list:
                yield name

        def values(self):
            return (var for (_, var) in self._m_list)
                
        def __contains__(self, key):
            for name, _ in self._m_list:
                if name == key:
                    return True
            return False
        
        def __iter__(self):
            return self.keys()

    def _create_members(self):
        """\
        Populate the dictionary of member variables.  We need to calculate
        each member's address as we go as the "stop_address" of the previous
        member
        """
        member_list = []
        try:
            self._py_info.struct["members"]
        except KeyError:
            # A structure with no members... probably means a forward declaration...
            # Try looking up a type of the same name in the environment.. If it
            # exists, replace this variable's type info
            if self._py_env is None:
                return member_list
            try:
                full_type = self._py_env.types[self._py_info.struct["type_name"]]
                self._py_info._struct = full_type
            except DwarfNoSymbol:
                return member_list
            
        raw_list = self._py_info.struct["members"]
        addr = var_address(self)
        for (name, offset, dwarf_var) in raw_list:
            if offset is not None:
                # If we have an explicit offset available, use that instead of
                # assuming the members are packed.
                # Note that the offset is given in the DWARF in native 
                # addressing units (see DWARF 2 spec sec 2.4.3).
                addr = var_address(self) + offset
                
            var_info = DwarfVariableInfo(dwarf_var, addr, 
                                         self._py_info._layout_info)
            member_list.append((name, _Variable.factory(var_info, 
                                                        self._py_data,
                                                        self._py_ptd_to_space,
                                                        env=self._py_env)))
            if not self._py_is_union:
                # Provisionally set the next member's address to the end of
                # the current member in case we've got no explicit offset info
                addr = var_info.stop_address

        return member_list

    def display(self, name, prefix, target, derefed, ll_expand=False, last=False,
                depth_limit=None):

        ostr = []
        if len(target) > 1:
            try:
                #Find the target member in the list
                member = self.members[target[1]]
            except KeyError:
                raise ValueError(
"""Bad display target: requested to display member '%s'
Valid members are: %s)""" % (target[1], [key for key in list(self.members.keys())]))

            ostr += var_display(member, name, prefix, target[1:], derefed,
                                   ll_expand=ll_expand, last=last)
        else:
            # We are fully resolved: display the structure and its contents
            if len(target) == 1:
                #This is bottom of the target tree
                ostr.append(self.report_addr(name))

            ostr.append("%s-%s %s : %s" % (prefix, self._py_info.datatype, name,
                                   ("union" if self._py_is_union else "struct")))

            if last:
                prefix = "%s " % prefix[:-1]

            ostr += self._display(name, prefix, target, derefed, ll_expand=ll_expand,
                                  depth_limit=depth_limit)
            
            return ostr
    
    def expand_linked_list(self):
        '''
        If this object is a linked list pointer then this method returns a
        list of objects (of the type this Pointer points to) by following
        the 'next' element of the pointed to structure.
        '''
        
        # WARNING: don't be too quick to try to merge this method with _Pointer.
        # expand_linked_list.  The problem is that there may be semantics on the
        # structure that make it treatable as a linked list, and passing these
        # correctly to the _Pointer member is a subtle business.  It's not that
        # it can't be done, it's just very tricky.
        
        enable_linked_list_prefetch = False
        
        def fetch_range(data, range):
            if enable_linked_list_prefetch:
                try:
                    data.prefetch_address_range
                except AttributeError:
                    pass
                else:
                    data.prefetch_address_range(*range)
        def flush_range(data, range):
            if enable_linked_list_prefetch:
                try:
                    data.discard_prefetched
                except AttributeError:
                    pass
                else:
                    data.discard_prefetched(*range)
            
        expanded_list = []
        structure = self.clone()
        struct_range = var_start_stop(structure)
        pointer_range = None
        try:
            fetch_range(structure._py_data, struct_range)
            values_seen = {self.address}
            if var_is_linked_list(self):
                while True:
                    expanded_list.append(structure)
                    if pointer_range is not None:
                        flush_range(pointer._py_data,pointer_range)
                        pointer_range = None
                    pointer = var_members(structure)[self._ll]
                    pointer_range = var_start_stop(pointer)
                    fetch_range(pointer._py_data,pointer_range)
                    if pointer.value in values_seen:
                        break
                    if not pointer._py_valid:
                        break
                    if pointer._semantics.get("",{}).get("depth",None) == 0:
                        break
                    old_structure = structure
                    structure = pointer.deref
                    # Replicate any custom semantics from the head of the linked list
                    # to the following element
                    structure.add_semantics(old_structure.get_semantics())
                    flush_range(old_structure._py_data,struct_range)
                    struct_range = var_start_stop(structure)
                    fetch_range(structure._py_data,struct_range)
                    # Make sure miscellaneous semantics in the list parent are 
                    # propagated to each list element
                    values_seen.add(pointer.value)
                for struct in expanded_list:
                    # Now set the semantics on each pointer in the list so it just
                    # expands the entry it's pointing to and then stops
                    var_add_semantics(struct, {self._ll : {"depth" :  0}})
    
            else:
                raise ValueError("Trying to expand non-linked-list of type %s as a "
                                 "linked list" % var_typename(self))
            
                
        finally:
            if pointer_range is not None:
                flush_range(pointer._py_data,pointer_range)
            if struct_range is not None:
                flush_range(structure._py_data,struct_range)
        return expanded_list

    def _display(self, name, prefix, target, derefed, ll_expand=False,
                 depth_limit=None):
        """
        Core of the display method which can be overridden by subclasses for
        type-specific pretty-printing
        """
        if depth_limit == 0:
            return []
        mbr_depth_limit = depth_limit - 1 if depth_limit is not None else None
        
        ostr = []
        with self.footprint_prefetched():
            if var_is_linked_list(self) and not ll_expand:
                # Start expanding the linked list
                prev_address = self.address
                for i,elem in enumerate(var_expand_linked_list(self)):
                    if _Pointer.suppress_nonzero_pointer_value_display:
                        ostr += ["%s- %s[%d] :" % (prefix, name, i)]
                    else:
                        ostr += ["%s- %s[%d] : 0x%x" % (prefix, name, i, var_address(elem))]
                    ostr += var_display(elem, name, "%s   |" % prefix,
                                        target[1:], 
                                        derefed + [prev_address],
                                        ll_expand=True,
                                        depth_limit=mbr_depth_limit)
                    prev_address = var_address(elem)
            else:
                if var_member_list(self):
                    
                    # Display members fully unless they are the linked list pointer,
                    # in which case just show the address by displaying them as raw
                    # integers
                    if "bound_discrim" in self._semantics.get("",{}):
                        mname = self._semantics[""]["bound_discrim"]() # evaluate the union discriminant now
                        if mname is not None:
                            member = var_members(self)[mname]
                            ostr += var_display(member, mname, "%s  |" % prefix, target[1:],
                                                derefed, depth_limit=mbr_depth_limit)
                    else:
                        
                        for mname, member in var_member_list(self):
                            depth = self._semantics.get(mname,{}).get("depth",None) 
                            if (mname not in self._ll_fields or depth != None):
                                # Let the pointer display do what it does
                                ostr += var_display(member, mname, "%s   |" % prefix,
                                                       target[1:], derefed, 
                                                       depth_limit=mbr_depth_limit)
                            else:
                                # Pointer display will expand pointers we don't want
                                # it to, so force integer display
                                ostr += _Integer.display(member, mname, "%s   |" % prefix, 
                                                         target[1:], derefed,
                                                         last=(member is self.member_list[-1][1]), 
                                                         depth_limit=mbr_depth_limit)
    
        return ostr

# These functions are designed to help avoid clashes between the names of
# structure/union fields, and the names of attributes in the _Structure class.
# Attributes with leading underscores are not covered.
# Note that any name in this module that starts "var_" will be imported into
# the global namespace by PydbgFrontEnd.

@display_hex
def var_address(var):
    if not isinstance(var, _Structure):
        return var.address
    return super(type(var), var).address

def var_size(var):
    if not isinstance(var, _Structure):
        return var.size
    return super(type(var), var).size

@display_hex
def var_start_stop(var):
    return var_address(var),var_address(var)+var_size(var)

def var_info(var):
    return var._py_info

def var_typename(var):
    if not isinstance(var, _Structure):
        return var.typename
    return super(type(var), var).typename

def var_value_string(var):
    if not isinstance(var, _Structure):
        return var.value_string
    return super(type(var), var).value_string

def var_display(var, *args, **kwargs):
    if not isinstance(var, _Structure):
        return var.display(*args, **kwargs)
    return super(type(var), var).display(*args, **kwargs)

def var_members(var):
    if not isinstance(var, _Structure):
        raise TypeError("Variable of type %s does not have members" % var.__class__.__name__)
    return super(type(var), var).members

def var_member_list(var):
    if not isinstance(var, _Structure):
        raise TypeError("Variable of type %s does not have members" % var.__class__.__name__)
    return super(type(var), var).member_list

def var_is_linked_list(var):
    if not isinstance(var, _Structure):
        return var.is_linked_list
    return super(type(var), var).is_linked_list

def var_expand_linked_list(var):
    if not isinstance(var, _Structure):
        return var.expand_linked_list()
    return super(type(var), var).expand_linked_list()

def var_set_semantics(var, semantics, **kwargs):
    if not isinstance(var, _Structure):
        var.set_semantics(semantics, **kwargs)
    super(type(var), var).set_semantics(semantics, **kwargs)

def var_add_semantics(var, semantics, **kwargs):
    if not isinstance(var, _Structure):
        return var.add_semantics(semantics, **kwargs)
    return super(type(var), var).add_semantics(semantics, **kwargs)

def var_is_bitfield(var):
    return isinstance(var, _Integer) and var._py_is_bitfield

def var_value(var):
    if not isinstance(var, _Structure):
        return var.value
    return super(type(var), var).value