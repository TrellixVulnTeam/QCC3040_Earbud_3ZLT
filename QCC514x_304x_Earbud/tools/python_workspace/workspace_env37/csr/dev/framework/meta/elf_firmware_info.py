##############################################################lobvar##############
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.dev.framework.meta.i_firmware_info import IFirmwareInfo,\
    IVariableInfo, ICompilationUnitInfo, IDwarfVariableInfo
from csr.wheels.bitsandbobs import TypeCheck, display_hex, HexDisplayInt, \
                                   unique_subclass, unique_basenames, LazyAttribEvaluator, \
                                   FrozenNamespace
from csr.dwarf.read_dwarf import Dwarf_Symbol, DW_TAG, Dwarf_Func_Symbol, \
                                 DwarfNoSymbol, DwarfAmbiguousName, DW_TAG_LOOKUP,\
                                 is_global as dwarf_is_global
from csr.dev.hw.address_space import AddressRange, AddressMultiRange
from csr.dev.model.interface import Group, Text, Table
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels.scoped_symbol_dict import ScopedSymbolDict


class BadPC(ValueError):
    """
    Indicates a dubious PC value, in that it lies outside any function 
    """
class BadPCHigh(BadPC):
    """
    A bad PC that is too high to be in any function
    """
class BadPCLow(BadPC):
    """
    A bad PC this is too low to be in any functino
    """

class AmbiguousCUName(KeyError):
    """
    A CU was looked up with too short a path fragment to uniquely identify it
    """
    def __init__(self, matches):
        matches = ["/".join(m) for m in matches]
        self.matches = matches
        super(AmbiguousCUName, self).__init__("Multiple possible CU matches: %s and %s" % 
                                              (", ".join(matches[:-1]), matches[-1]))

def varinfo_factory(layout_info):

    def _factory(key, dwarf_or_elf_symbol):
        """
        Factory method that turns Dwarf_Var_Symbols corresponding to static variables
        into IVariableInfo objects
        """
        try:
            dwarf_or_elf_symbol.static_location
            dwarf_symbol = dwarf_or_elf_symbol
        except AttributeError:
            elf_symbol = dwarf_or_elf_symbol
            return ElfVariableInfo(elf_symbol, layout_info)
        return DwarfVariableInfo(dwarf_symbol, dwarf_symbol.static_location, layout_info)
    return _factory

def is_global(dwarf_symbol):
    return dwarf_is_global(dwarf_symbol)

def is_local(dwarf_symbol):
    return not dwarf_is_global(dwarf_symbol)

class NotInCU(Exception):
    """
    The supplied PC is not inside any function from this CU
    """

class CUPathJoinWrapper(object):
    """
    Wrapper to iterate over a CU dictionary joining the minimal path
    tuples together in specified ways.
    
    Because munging may not be easily reversible in all circumstances (for 
    example because a specified separator is a character string that might 
    appear independently in the joined string fragments) it is only 
    possible to look items up via __getitem__ that have previously been iterated
    over via this wrapper.  But this doesn't seem like a major restriction 
    because the munged keys have to be created before they can be used.
    """
    def __init__(self, cu_dict, sep, basename_first=False,
                 strip_suffices=True, basename_sep=None):
        
        self._cu_dict = cu_dict
        self._sep = sep
        self._basename_first = basename_first
        self._basename_sep = basename_sep if basename_sep is not None else sep
        self._strip_suffices = strip_suffices
        
        self._reverse_dict = {}

    def _suffix_strip(self, name):
        if self._strip_suffices:
            first_dot = name.find(".")
            if first_dot != -1:
                return name[:first_dot]
        return name

        
    def _tuple_munge(self, tpl):
        if len(tpl) == 1:
            val = self._suffix_strip(tpl[0])
        else:
            if self._basename_first:
                val = self._basename_sep.join((self._suffix_strip(tpl[-1]), 
                                            self._sep.join(tpl[:-1])))
            else:
                val = self._sep.join(tpl[:-1] + (self._suffix_strip(tpl[-1]),))
        self._reverse_dict[val] = tpl
        return val
        
    def keys(self):
        return [self._tuple_munge(k) for k in list(self._cu_dict.keys())]
    
    def items(self):
        for k,v in self._cu_dict.items():
            yield self._tuple_munge(k), v

    def __getitem__(self, key):
        return self._cu_dict[self._reverse_dict[key]]

class ElfVariableInfo (IVariableInfo):
    """\
    ELF-based Firmware Variable (extern or static) MetaData Interface
    
    Implementation is somewhat limited due to lack of type information in ELF
    (c.f. DWARF).
    
    For this reason there are no subclasses: All ELF variables are blobs!
    """
    
    @staticmethod
    def create_elf_variable_info(elf_symbol, dwarf_var, layout_info):
        """
        Factory method based on whatever information is available via the 
        dwarf_var, possibly none.
        """
        
        if dwarf_var:
        
            try:
                return ElfDwarfVariableInfo(elf_symbol, dwarf_var, layout_info)
            except ValueError:
                iprint("Something went wrong creating %s" % elf_symbol)
                #Something went wrong; just fall through
                pass
        
        #Worst case: no DWARF info, so fall back to a plain vanilla 
        # ElfVariableInfo
        return ElfVariableInfo(elf_symbol, layout_info);

    
    
    def __init__(self, elf_symbol, layout_info):
        if isinstance(elf_symbol, ElfVariableInfo):
            raise TypeError
        self._elf_symbol = elf_symbol
        self._layout_info = layout_info
    
    # IVariableInfo compliance
    
    @property
    def symbol(self):
        return self._elf_symbol.name

    @property
    def datatype(self):
        # No type information in ELF
        return "<Unknown>"

    @property
    def is_external(self):
        return self._elf_symbol.is_global

    @property
    def start_address(self):
        return self._elf_symbol.address

    @property
    def stop_address(self):
        # Derived
        return self.start_address + self.size
        
    @property
    def size(self):
        return self._elf_symbol.size
    
    @property
    def byte_size(self):
        return (self.size * self._layout_info.addr_unit_bits) // 8
    

class ElfUnsignedIntegralTypeInfo(ElfVariableInfo):
    signed = False

    @property
    def type_tag(self):
        return 0x24 # DW_TAG_base_type
    @property
    def struct(self):
        return {"type_tag" : self.type_tag,
                "type_name" : self.datatype,
                "byte_size" : self.byte_size}

class ElfUnsignedIntInfo(ElfUnsignedIntegralTypeInfo):
    """
    ElfVariableInfo extension for the case where we presume that
    a given symbol represents an unsigned int, even though we don't
    have a DWARF symbol to tell us that.
    """
    @property
    def datatype(self):
        return "unsigned int"

class ElfUnsignedCharInfo(ElfUnsignedIntegralTypeInfo):
    @property
    def datatype(self):
        return "unsigned char"

class ElfUnsignedIntArrayInfo(ElfVariableInfo):
    """
    ElfVariableInfo extension for the case where we presume that
    a given symbol represents an array of unsigned ints, even though 
    we don't have a DWARF symbol to tell us that.
    """
    
    @property
    def type_tag(self):
        return 0x1 # DW_TAG_array_type
    @property
    def datatype(self):
        return "unsigned int %v[{}]".format(
                        self.size // self._layout_info.addr_units_per_data_word)

    @property
    def struct(self):
        num_elts = self.size // self._layout_info.addr_units_per_data_word
        return {"num_elements" : num_elts,
                "element_type" : {"type_tag" : 0x24, 
                                  "type_name" : "unsigned int",
                                  "byte_size" : self._layout_info.addr_units_per_data_word},
                "type_tag" : self.type_tag,
                "type_name"  : self.datatype}


class DwarfVariableInfoMixin(IDwarfVariableInfo):
    """
    Can be passed either a "struct" dictionary or a Dwarf_Symbol;
    """
    def __init__(self, dwarf_var_or_dict):
        if isinstance(dwarf_var_or_dict, Dwarf_Symbol):
            self._dwarf_var = dwarf_var_or_dict
        else:
            self._struct = dwarf_var_or_dict
    
    @property 
    def struct(self):
        try:
            self._struct
        except AttributeError:
            self._struct = self._build_struct()
            
        return self._struct
      
    @property
    def is_external(self):
        return self._dwarf_var.is_global
        
    @property
    def type_tag(self):
        try:
            return self.struct["type_tag"]
        except KeyError:
            return None

    def datatype(self):
        """\
        Type information (ITypeInfo)
        """
        return self.struct["type_name"]
    
    def byte_size(self):
        '''
        Override the ELF size information in case it's spurious.
        '''
        return self.struct["byte_size"]

    def __repr__(self):
        return ("%s(type_tag='%s', datatype='%s', byte_size=%d)" % 
                (self.__class__.__name__,
                 DW_TAG_LOOKUP[self.type_tag].replace("_type",""),
                 self.datatype,
                 self.byte_size))

    def signed(self):
        """
        Is the variable signed or not (not, if that isn't a meaningful question,
        of course)
        """
        return "signed" in self.struct and self.struct["signed"]

    @property
    def linked_list(self):
        """
        Algorithm to determine if a type represents a linked list.  To be a 
        linked list the following must be true:
         - Must be a named non-empty structure or a pointer to one
         - The (pointed-to) structure must have exactly one pointer member 
         pointing to the same fundamental type.
         This method simply returns a complete list of the linked-list-type
         pointers.  If there is exactly one of these, it is a singly-linked list,
         but it is left to the client of this function to to use the information
         correctly.
        """
        list_mbrs = []
        try:
            if self.type_tag == DW_TAG["pointer_type"]:
                # Weed out cases where the pointed-to object isn't a structure
                if (self.struct["pointed_to"]["type_tag"] != 
                                                    DW_TAG["structure_type"]):
                    return []
                struct = self.struct["pointed_to"]
            elif self.type_tag == DW_TAG["structure_type"]:
                struct = self.struct
            else:
                # Not a pointer or a structure
                return []
        except KeyError:
            # Any of the looked-up struct dictionary elements not being present
            # is sufficient to prove this isn't a linked list
            pass
        else:
            # Weed out cases where the structure isn't named
            if "<anonymous>" in struct["base_type_name"]:
                return []
            
            # Look for a pointer member pointing to the same type 
            for name, _, mbr in struct.get("members",[]):
                if mbr.struct_dict["type_tag"] == DW_TAG["pointer_type"]:
                    mbr_ptd_to = mbr.struct_dict["pointed_to"]
                    # not every struct_dict has "base_type_name" - in particular
                    # that of the target "type" of void pointers doesn't. 
                    if (mbr_ptd_to.get("base_type_name", mbr_ptd_to["type_name"]) == 
                        struct["base_type_name"]):
                        list_mbrs.append(name)
        return list_mbrs

    #Extend the interface for struct-specific information
    def _build_struct(self):
        """
        Return structure information in a suitable form
        """
        return self._dwarf_var.struct_dict


class ElfDwarfVariableInfo(ElfVariableInfo, DwarfVariableInfoMixin):
    """
    This class represents information for ELF variables with DWARF symbol
    information
    """
    def __init__(self, elf_symbol, dwarf_var, layout_info):
        
        ElfVariableInfo.__init__(self, elf_symbol, layout_info)
        
        DwarfVariableInfoMixin.__init__(self, dwarf_var)

    #We have to have this here because otherwise for some reason ElfVariableInfo's
    #datatype appears to take precedence over DwarfVariableInfoMixin's        
    @property
    def datatype(self):
        """\
        Type information (ITypeInfo)
        """
        return DwarfVariableInfoMixin.datatype(self)
    
    @property
    def byte_size(self):
        return DwarfVariableInfoMixin.byte_size(self)

    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits
    
    @property
    def signed(self):
        return DwarfVariableInfoMixin.signed(self)
    
class DwarfVariableInfo(DwarfVariableInfoMixin):
    """
    Implements the IVariableInfo for DWARF entities that are not ELF entities,
    i.e. structure and array members.
    """

    def __init__(self, dwarf_var, start_addr, layout_info):
                
        DwarfVariableInfoMixin.__init__(self, dwarf_var)
        
        self._addr = start_addr
        self._layout_info = layout_info
        
    @property
    def start_address(self):
        return self._addr
    
    @property
    def stop_address(self):
        return self.start_address + self.size
    
    @property
    def symbol(self):
        #Do we want this or self._parent.symbol?
        return None
    
    @property
    def datatype(self):
        return DwarfVariableInfoMixin.datatype(self)
    
    @property
    def byte_size(self):
        return DwarfVariableInfoMixin.byte_size(self)

    @property
    def signed(self):
        return DwarfVariableInfoMixin.signed(self)

    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits


class StackVariableInfo(DwarfVariableInfoMixin):
    '''
    Info for a stack variable, for which a memory slice must be explicitly
    constructed using the DWARF variable location rules
    '''
    def __init__(self, dwarf_var, layout_info):

        DwarfVariableInfoMixin.__init__(self, dwarf_var)
        self._layout_info = layout_info

    @property
    def start_address(self):
        '''
        StackVariables point to a specially constructed memory slice, so the
        info always starts at 0
        '''
        return 0

    @property
    def stop_address(self):
        return self.start_address + self.size

    @property
    def symbol(self):
        #Do we want this or self._parent.symbol?
        return None

    @property
    def is_external(self):
        #Is this right or should it be self._parent.is_external?
        return False

    @property
    def datatype(self):
        return DwarfVariableInfoMixin.datatype(self)

    @property
    def signed(self):
        return DwarfVariableInfoMixin.signed(self)

    @property
    def byte_size(self):
        return DwarfVariableInfoMixin.byte_size(self)
    
    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits


class DwarfPointerOrArrayVariableInfoBase(DwarfVariableInfo):
    """
    Common base class for DwarfPointerTargetVariableInfo and DwarfArrayVariableInfo
    """
    
    @property
    def symbol(self):
        """\
        Source symbol (str)
        """
        return None

    @property
    def datatype(self):
        """\
        Type information (ITypeInfo)
        """
        return self.struct["type_name"]

    @property
    def is_external(self):
        """\
        Is this variable public/private to the defining CU?
        """
        return False
    
    @property
    def struct(self):
        """
        Structure dictionary access
        """
        return self._struct
    
    @property
    def type_tag(self):
        """
        Return the DWARF tag of the variable's resolved type to enable the
        correct IVariable subclass to be instantiated for the variable
        """
        try:
            return self.struct["type_tag"]
        except KeyError:
            return None

    @property
    def byte_size(self):
        """\
        Number of data-space words occupied.
        """
        if isinstance(self.struct["byte_size"], str):
            # If there's no byte_size attribute in the DWARF, it's most likely 
            # a pointer_type
            if self.type_tag == DW_TAG["pointer_type"]:
                return self._layout_info.data_word_bits // 8
            elif self.type_tag == DW_TAG["subroutine_type"]:
                # There's no known reason for the debugging environment needing
                # to know the size of an object a function pointer points to so
                # it's safe to return 0 silently.
                return 0
            iprint("WARNING: no size information for '%s' (type_tag 0x%02x)!" %
                                                   (self.symbol, self.type_tag))
        return self.struct["byte_size"]

    @property
    def signed(self):
        return "signed" in self.struct and self.struct["signed"]

    
class DwarfPointerTargetVariableInfo(DwarfPointerOrArrayVariableInfoBase):
    """
    Specialisation that creates an IVariable for the variable a pointer points
    to.  This has no ELF parent; the base address has to be supplied explicitly.
    """
    
    def __init__(self, address, pointed_to_struct, layout_info):
        
        self._address = address
        self._struct = pointed_to_struct
        self._layout_info = layout_info
        
    @property
    def start_address(self):
        """\
        Start address in data-space (inclusive).
        """
        return self._address

    @property
    def stop_address(self):
        """\
        Stop address in data-space (exclusive).
        """
        return self._address + self.size
        
    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits
        

class DwarfArrayElementVariableInfo(DwarfPointerOrArrayVariableInfoBase):
    """
    Specialisation that dynamically creates IVariables corresponding to 
    a particular array element
    """
    def __init__(self, index, array_info, layout_info):
        
        self._index = index
        self._array = array_info
        self._element_struct = array_info.struct["element_type"]
        self._layout_info = layout_info 
        
    @property
    def start_address(self):
        """\
        Start address in data-space (inclusive).
        """
        return self._array.start_address + self.size*self._index

    @property
    def stop_address(self):
        """\
        Stop address in data-space (exclusive).
        """
        return self._array.start_address + self.size*(self._index + 1)
        
    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits

    @property
    def struct(self):
        """
        Structure dictionary access
        """
        return self._element_struct
    
    @property
    def signed(self):
        return "signed" in self.struct and self.struct["signed"]
    
    @property
    def type_tag(self):
        """
        Return the DWARF tag of the variable's resolved type to enable the
        correct IVariable subclass to be instantiated for the variable
        """
        return self.struct["type_tag"]

class DwarfSubarrayVariableInfo(IDwarfVariableInfo):
    """
    VariableInfo for a slice derived from a parent array.  It's a bit of a hack
    because of the horrible way we both have an API and expose the underlying
    struct dictionary as a public property, but I think it basically hangs 
    together.
    """
    
    def __init__(self, parent_array_info, start=None, stop=None):

        self._parent_info = parent_array_info
        self._layout_info = self._parent_info._layout_info
        self._start = start if start is not None else 0
        self._stop = stop if stop is not None else self._parent_info.struct["num_elements"]
        self._element_byte_size = self._parent_info.struct["element_type"]["byte_size"]
        self._element_size = self._element_byte_size*8//self._layout_info.addr_unit_bits

    @property
    def struct(self):
        d = self._parent_info.struct.copy()
        d["num_elements"] = self._stop - self._start
        d["byte_size"] = self._element_byte_size * (self._stop - self._start)
        return d

    @property
    def start_index(self):
        return self._start

    @property
    def start_address(self):
        return self._parent_info.start_address + self._start * self._element_size
    
    @property
    def stop_address(self):
        return self.start_address + self.size
    
    @property
    def symbol(self):
        return None
    
    @property
    def datatype(self):
        return self._parent_info.datatype+"([%d:%d])" % (self._start, self._stop)
    
    @property
    def byte_size(self):
        self.size * self._layout_info.addr_unit_bits // 8

    @property
    def signed(self):
        return DwarfVariableInfoMixin.signed(self)

    @property
    def size(self):
        return self._element_size * (self._stop - self._start)


class AdHocVariableInfo(IDwarfVariableInfo):
    """
    Treat an arbitrary region of memory as the specified type
    """
    
    def __init__(self, address, dwarf_info_struct, layout_info):
        
        self._address = address
        self._layout_info = layout_info
        
        try:
            dwarf_info_struct["type_tag"]
            dwarf_info_struct["datatype"]
            dwarf_info_struct["byte_size"]
        except KeyError:
            raise ValueError("dwarf_info_struct must at least contain " 
                             "'type_tag', 'byte_size' and 'datatype'")
        
        self._struct = dwarf_info_struct
        
    @property
    def symbol(self):
        """\
        Source symbol (str)
        """
        return None

    @property
    def datatype(self):
        """\
        Type information (ITypeInfo)
        """
        return self._struct["datatype"]

    @property
    def is_external(self):
        """\
        Is this variable public/private to the defining CU?
        """
        return True

    @property
    def start_address(self):
        """\
        Start address in data-space (inclusive).
        """
        return self._address

    @property
    def stop_address(self):
        """\
        Stop address in data-space (exclusive).
        """
        return self._address + self.size
        
    @property
    def byte_size(self):
        """\
        Number of bytes (octets) occupied.
        """
        if isinstance(self.struct["byte_size"], str):
            # read_dwarf writes a "not found" string for this field if the
            # attribute wasn't found in the DWARF
            iprint("WARNING: no size information for '%s'!" % self.symbol)
            return 1
        return self.struct["byte_size"]

    @property
    def size(self):
        return (self.byte_size * 8) // self._layout_info.addr_unit_bits

    @property
    def signed(self):
        return "signed" in self.struct and self.struct["signed"]

    @property
    def struct(self):
        """
        Structure dictionary access
        """
        return self._struct
    
    @property
    def type_tag(self):
        """
        Return the DWARF tag of the variable's resolved type to enable the
        correct IVariable subclass to be instantiated for the variable
        """
        return self._struct["type_tag"]

    @property
    def parent_info(self):
        """
        Every DWARF variable has an ELF symbol of which it is a member, which 
        may be itself
        """
        return None


