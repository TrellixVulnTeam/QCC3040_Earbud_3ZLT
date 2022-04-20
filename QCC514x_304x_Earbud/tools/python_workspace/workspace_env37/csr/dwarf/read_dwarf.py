############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import os
import time
import platform
import sys
import re
from importlib import import_module
from collections import OrderedDict
import operator
from ..wheels.scoped_symbol_dict import ScopedSymbolDict
from ..wheels.global_streams import iprint
from ..wheels import autolazy
from .read_elf import Elf_Reader
from csr.wheels import UnimportableObjectProxy

if sys.version_info > (3,):
    # Py3
    int_type = (int)
else:
    # Py2
    int_type = (int, long)


class DwarfError(ValueError): # need to derive from ValueError for legacy reasons
    pass

class DwarfNoStackFrameInfo(DwarfError):
    pass

class DwarfNoSymbol(DwarfError):
    pass

class DwarfAmbiguousName(DwarfError):
    """
    Exception indicating that a given name can't be resolved unambiguously to a
    symbol of the indicated type - e.g. get_functions("a_static") but there are
    two static functions of that name.
    """
    def __init__(self, symbol_name, appears_in):
        DwarfError.__init__(self, "Multiple matches found for '%s'" % symbol_name)
        self.matches = appears_in

class DwarfUndefinedType(RuntimeError):
    """
    Indicates that a type was encountered that appeared to have no definition at
    all.
    """

class DwarfFunctionAddressRangeNotPresent(RuntimeError):
    pass


def scoped_symbol_dict(base_dict, factory, combine_cus=False):
    """
    Helper for creating ScopedSymbolDicts with suitable configuration for use
    by read_dwarf functionality
    """
    return ScopedSymbolDict(base_dict, factory, scope_sep="::",
                          ignore_globals=True, UnknownNameExcep=DwarfNoSymbol,
                          AmbiguousNameExcep=DwarfAmbiguousName,
                          combine_outermost=combine_cus)

def path_dict(base_dict, factory):
    """
    Helper for creating ScopedSymbolDicts with suitable configuration for use
    as containers of path-keyed entities
    """

    return ScopedSymbolDict(base_dict, factory, scope_sep="/",
                            ignore_globals=True, UnknownNameExcep=None,
                            AmbiguousNameExcep=DwarfAmbiguousName, unflatten_dict=True,
                            alt_sep="\\")

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

#These values are given in the DWARF 2 spec, Figs 14 and 15
DW_TAG = {
    "array_type" : 0x01,
    "class_type" : 0x02,
    "entry_point" : 0x03,
    "enumeration_type" : 0x04,
    "formal_parameter" : 0x05,
    "imported_declaration" : 0x08,
    "label" : 0x0a,
    "lexical_block" : 0x0b,
    "member" : 0x0d,
    "pointer_type" : 0x0f,
    "reference_type" : 0x10,
    "compile_unit" : 0x11,
    "string_type" : 0x12,
    "structure_type" : 0x13,
    "subroutine_type" : 0x15,
    "typedef" : 0x16,
    "union_type" : 0x17,
    "unspecified_parameters" : 0x18,
    "variant" : 0x19,
    "common_block" : 0x1a,
    "common_inclusion" : 0x1b,
    "inheritance" : 0x1c,
    "inlined_subroutine" : 0x1d,
    "module" : 0x1e,
    "ptr_to_member_type" : 0x1f,
    "set_type" : 0x20,
    "subrange_type" : 0x21,
    "with_stmt" : 0x22,
    "access_declaration" : 0x23,
    "base_type" : 0x24,
    "catch_block" : 0x25,
    "const_type" : 0x26,
    "constant" : 0x27,
    "enumerator" : 0x28,
    "file_type" : 0x29,
    "friend" : 0x2a,
    "namelist" : 0x2b,
    "namelist_item" : 0x2c,
    "namelist_items" : 0x2c,
    "packed_type" : 0x2d,
    "subprogram" : 0x2e,
    "template_type_parameter" : 0x2f,
    "template_type_param" : 0x2f,
    "template_value_parameter" : 0x30,
    "template_value_param" : 0x30,
    "thrown_type" : 0x31,
    "try_block" : 0x32,
    "variant_part" : 0x33,
    "variable" : 0x34,
    "volatile_type" : 0x35,
    "dwarf_procedure" : 0x36,
    "restrict_type" : 0x37,
    "interface_type" : 0x38,
    "namespace" : 0x39,
    "imported_module" : 0x3a,
    "unspecified_type" : 0x3b,
    "partial_unit" : 0x3c,
    "imported_unit" : 0x3d,
    "mutable_type" : 0x3e,
    "condition" : 0x3f,
    "shared_type" : 0x40,
    "type_unit" : 0x41,
    "rvalue_reference_type" : 0x42,
    "template_alias" : 0x43} 

DW_TAG_LOOKUP = {v:k for (k,v) in list(DW_TAG.items())}

#These values are given in the DWARF 2 spec, Figs 22 and 23
DW_OP = {
    "addr" : 0x03,
    "deref" : 0x06,
    "const1u" : 0x08,
    "const1s" : 0x09,
    "const2u" : 0x0a,
    "const2s" : 0x0b,
    "const4u" : 0x0c,
    "const4s" : 0x0d,
    "const8u" : 0x0e,
    "const8s" : 0x0f,
    "constu" : 0x10,
    "consts" : 0x11,
    "dup" : 0x12,
    "drop" : 0x13,
    "over" : 0x14,
    "pick" : 0x15,
    "swap" : 0x16,
    "rot" : 0x17,
    "xderef" : 0x18,
    "abs" : 0x19,
    "and" : 0x1a,
    "div" : 0x1b,
    "minus" : 0x1c,
    "mod" : 0x1d,
    "mul" : 0x1e,
    "neg" : 0x1f,
    "not" : 0x20,
    "or" : 0x21,
    "plus" : 0x22,
    "plus_uconst" : 0x23,
    "shl" : 0x24,
    "shr" : 0x25,
    "shra" : 0x26,
    "xor" : 0x27,
    "bra" : 0x28,
    "eq" : 0x29,
    "ge" : 0x2a,
    "gt" : 0x2b,
    "le" : 0x2c,
    "lt" : 0x2d,
    "ne" : 0x2e,
    "skip" : 0x2f,
    "lit0" : 0x30,
    "lit1" : 0x31,
    "lit2" : 0x32,
    "lit3" : 0x33,
    "lit4" : 0x34,
    "lit5" : 0x35,
    "lit6" : 0x36,
    "lit7" : 0x37,
    "lit8" : 0x38,
    "lit9" : 0x39,
    "lit10" : 0x3a,
    "lit11" : 0x3b,
    "lit12" : 0x3c,
    "lit13" : 0x3d,
    "lit14" : 0x3e,
    "lit15" : 0x3f,
    "lit16" : 0x40,
    "lit17" : 0x41,
    "lit18" : 0x42,
    "lit19" : 0x43,
    "lit20" : 0x44,
    "lit21" : 0x45,
    "lit22" : 0x46,
    "lit23" : 0x47,
    "lit24" : 0x48,
    "lit25" : 0x49,
    "lit26" : 0x4a,
    "lit27" : 0x4b,
    "lit28" : 0x4c,
    "lit29" : 0x4d,
    "lit30" : 0x4e,
    "lit31" : 0x4f,
    "reg0" : 0x50,
    "reg1" : 0x51,
    "reg2" : 0x52,
    "reg3" : 0x53,
    "reg4" : 0x54,
    "reg5" : 0x55,
    "reg6" : 0x56,
    "reg7" : 0x57,
    "reg8" : 0x58,
    "reg9" : 0x59,
    "reg10" : 0x5a,
    "reg11" : 0x5b,
    "reg12" : 0x5c,
    "reg13" : 0x5d,
    "reg14" : 0x5e,
    "reg15" : 0x5f,
    "reg16" : 0x60,
    "reg17" : 0x61,
    "reg18" : 0x62,
    "reg19" : 0x63,
    "reg20" : 0x64,
    "reg21" : 0x65,
    "reg22" : 0x66,
    "reg23" : 0x67,
    "reg24" : 0x68,
    "reg25" : 0x69,
    "reg26" : 0x6a,
    "reg27" : 0x6b,
    "reg28" : 0x6c,
    "reg29" : 0x6d,
    "reg30" : 0x6e,
    "reg31" : 0x6f,
    "breg0" : 0x70,
    "breg1" : 0x71,
    "breg2" : 0x72,
    "breg3" : 0x73,
    "breg4" : 0x74,
    "breg5" : 0x75,
    "breg6" : 0x76,
    "breg7" : 0x77,
    "breg8" : 0x78,
    "breg9" : 0x79,
    "breg10" : 0x7a,
    "breg11" : 0x7b,
    "breg12" : 0x7c,
    "breg13" : 0x7d,
    "breg14" : 0x7e,
    "breg15" : 0x7f,
    "breg16" : 0x80,
    "breg17" : 0x81,
    "breg18" : 0x82,
    "breg19" : 0x83,
    "breg20" : 0x84,
    "breg21" : 0x85,
    "breg22" : 0x86,
    "breg23" : 0x87,
    "breg24" : 0x88,
    "breg25" : 0x89,
    "breg26" : 0x8a,
    "breg27" : 0x8b,
    "breg28" : 0x8c,
    "breg29" : 0x8d,
    "breg30" : 0x8e,
    "breg31" : 0x8f,
    "regx" : 0x90,
    "fbreg" : 0x91,
    "bregx" : 0x92,
    "piece" : 0x93,
    "deref_size" : 0x94,
    "xderef_size" : 0x95,
    "nop" : 0x96,
    "stack_value" : 0x9f
  }

DW_AT = {
    "sibling" : 0x01,
    "location" : 0x02,
    "name" : 0x03,
    "ordering" : 0x09,
    "subscr_data" : 0x0a,
    "byte_size" : 0x0b,
    "bit_offset" : 0x0c,
    "bit_size" : 0x0d,
    "element_list" : 0x0f,
    "stmt_list" : 0x10,
    "low_pc" : 0x11,
    "high_pc" : 0x12,
    "language" : 0x13,
    "member" : 0x14,
    "discr" : 0x15,
    "discr_value" : 0x16,
    "visibility" : 0x17,
    "import" : 0x18,
    "string_length" : 0x19,
    "common_reference" : 0x1a,
    "comp_dir" : 0x1b,
    "const_value" : 0x1c,
    "containing_type" : 0x1d,
    "default_value" : 0x1e,
    "inline" : 0x20,
    "is_optional" : 0x21,
    "lower_bound" : 0x22,
    "producer" : 0x25,
    "prototyped" : 0x27,
    "return_addr" : 0x2a,
    "start_scope" : 0x2c,
    "bit_stride" : 0x2e, # DWARF3 name 
    "stride_size" : 0x2e, # DWARF2 name 
    "upper_bound" : 0x2f,
    "abstract_origin" : 0x31,
    "accessibility" : 0x32,
    "address_class" : 0x33,
    "artificial" : 0x34,
    "base_types" : 0x35,
    "calling_convention" : 0x36,
    "count" : 0x37,
    "data_member_location" : 0x38,
    "decl_column" : 0x39,
    "decl_file" : 0x3a,
    "decl_line" : 0x3b,
    "declaration" : 0x3c,
    "discr_list" : 0x3d,
    "encoding" : 0x3e,
    "external" : 0x3f,
    "frame_base" : 0x40,
    "friend" : 0x41,
    "identifier_case" : 0x42,
    "macro_info" : 0x43,
    "namelist_item" : 0x44,
    "priority" : 0x45,
    "segment" : 0x46,
    "specification" : 0x47,
    "static_link" : 0x48,
    "type" : 0x49,
    "use_location" : 0x4a,
    "variable_parameter" : 0x4b,
    "virtuality" : 0x4c,
    "vtable_elem_location" : 0x4d,
    "allocated" : 0x4e, # DWARF3
    "associated" : 0x4f, # DWARF3
    "data_location" : 0x50, # DWARF3
    "byte_stride" : 0x51, # DWARF3f
    "stride" : 0x51, # DWARF3 (do not use)
    "entry_pc" : 0x52, # DWARF3
    "use_UTF8" : 0x53, # DWARF3
    "extension" : 0x54, # DWARF3
    "ranges" : 0x55, # DWARF3
    "trampoline" : 0x56, # DWARF3
    "call_column" : 0x57, # DWARF3
    "call_file" : 0x58, # DWARF3
    "call_line" : 0x59, # DWARF3
    "description" : 0x5a, # DWARF3
    "binary_scale" : 0x5b, # DWARF3f
    "decimal_scale" : 0x5c, # DWARF3f
    "small" : 0x5d, # DWARF3f
    "decimal_sign" : 0x5e, # DWARF3f
    "digit_count" : 0x5f, # DWARF3f
    "picture_string" : 0x60, # DWARF3f
    "mutable" : 0x61, # DWARF3f
    "threads_scaled" : 0x62, # DWARF3f
    "explicit" : 0x63, # DWARF3f
    "object_pointer" : 0x64, # DWARF3f
    "endianity" : 0x65, # DWARF3f
    "elemental" : 0x66, # DWARF3f
    "pure" : 0x67, # DWARF3f
    "recursive" : 0x68, # DWARF3f
    "signature" : 0x69, # DWARF4
    "main_subprogram" : 0x6a, # DWARF4
    "data_bit_offset" : 0x6b, # DWARF4
    "const_expr" : 0x6c  # DWARF4
    }

#These values are given in the DWARF 2 spec, Fig 24
DW_ATE = {
    "address" : 0x01,
    "boolean" : 0x02,
    "complex_float" : 0x03,
    "float" : 0x04,
    "signed" : 0x05,
    "signed_char" : 0x06,
    "unsigned" : 0x07,
    "unsigned_char" : 0x08,
    "lo_user" : 0x80,
    "hi_user" : 0xff
    }


def read_dwarf_arch_supported(arch_name):
    """
    Is the given architecture supported by the C layer?
    """
    if arch_name.lower() in ("arm",):
        return c_read_dwarf.arch_is_supported(40) # EM_ARM from elf_repl.h
    elif arch_name.lower() in ("xap",):
        return c_read_dwarf.arch_is_supported(0x9ba0)
    elif arch_name.lower() in ("kalimba","kal","k32"):
        return c_read_dwarf.arch_is_supported(0xdb)
    iprint("Unrecognised architecture name '%s'" % arch_name)
    return False

def leb_udecode(encoded):
    value = encoded & 0x7f
    offset = 0

    while encoded & 0x80:
        offset += 7
        encoded >> 8
        value |= (encoded & 0x7f) << offset
    return value;


class DwarfStackMachine(object):
    """
    Partially implements Section 2.5 of the DWARF5 standard.
    
    Note: we ignore type information at present and instead assume that we're
    only dealing with unsigned values.
    
    This stack machine is currently only used for calculating the addresses of
    static variables, which are usually trivial constant expressions involving
    unsigned addresses.  The only case I've seen that isn't simply 
    "DW_OP_addr <an address>" is with LLVM where "DW_OP_uconst" is used to
    adjust the value of the prior DW_OP_addr.
    
    If we need a more complete implementation of the stack machine (e.g. for
    evaluating automatic variables in stack backtraces) we can revisit this. It
    should reliably raise an exception if an unsupported operand is submitted.
    
    """
    _dwarf_py_ops = {"div" : operator.floordiv,
                     "minus" : operator.sub,
                     "mod" : operator.mod,
                     "mul" : operator.mul,
                     "or" : operator.or_,
                     "plus" : operator.add,
                     "shl" : operator.lshift,
                     "shr" : operator.rshift,
                     "xor" : operator.xor,
                     "and" : operator.and_
                     }
    
    
    def __init__(self, core=None):
        
        self._stack = []

    def _binary_op(self, op):
        a = self._pop()
        b = self._pop()
        self._stack.append(op(b,a))

    def simplify(self, op_list):
        """ 
        Remove potential redundant operation sequences from the op_list.  These
        are observed in LLVM Zeagle DWARF, e.g.
        DW_AT_location              len 0x000b: 03d0c4000006311e30229f: DW_OP_addr 0x0000c4d0 DW_OP_deref DW_OP_lit1 DW_OP_mul DW_OP_lit0 DW_OP_plus DW_OP_stack_value
        """
    
        # The following operations sequences have no effect
        #  - DW_OP_deref DW_OP_stack_value
        #  - DW_OP_lit0 DW_OP_plus/minus/shl/shr
        #  - DW_OP_lit1 DW_OP_mul/div
    
        op_list = list(op_list)
        op_list_cmds = [op for op,_,_,_ in op_list]
        
        zero_ident_ops = set(DW_OP[op] for op in ("plus","minus","shl","shr"))
        unit_ident_ops = set(DW_OP[op] for op in ("mul","div"))
        
        reexamine=True
        while reexamine:
            reexamine=False
            try:
                lit0_ind = op_list_cmds.index(DW_OP["lit0"])
            except ValueError:
                pass
            else:
                if lit0_ind + 1 < len(op_list_cmds) and op_list_cmds[lit0_ind + 1] in zero_ident_ops:
                    op_list = op_list[:lit0_ind] + op_list[lit0_ind+2:]
                    op_list_cmds = [op for op,_,_,_ in op_list]
                    reexamine = True
            try:
                lit1_ind = op_list_cmds.index(DW_OP["lit1"])
            except ValueError:
                pass
            else:
                if lit1_ind + 1 < len(op_list_cmds) and op_list_cmds[lit1_ind + 1] in unit_ident_ops:
                    op_list = op_list[:lit1_ind] + op_list[lit1_ind+2:]
                    op_list_cmds = [op for op,_,_,_ in op_list]
                    reexamine = True
            try:
                deref_ind = op_list_cmds.index(DW_OP["deref"])
            except ValueError:
                pass
            else:
                if deref_ind + 1 < len(op_list_cmds) and op_list_cmds[deref_ind + 1] == DW_OP["stack_value"]:
                    op_list = op_list[:deref_ind] + op_list[deref_ind+2:]
                    op_list_cmds = [op for op,_,_,_ in op_list]
                    reexamine = True

        # Collapse successive DW_OP_piece operations into one
        reexamine = True
        next_ind = 0
        while reexamine:
            try:
                piece_ind = op_list_cmds[next_ind:].index(DW_OP["piece"]) + next_ind
            except ValueError:
                reexamine = False
            else:
                # Loop through successive DW_OP_piece operands adding their
                # byte sizes, and replace with a single DW_OP_piece with the
                # combined byte size
                i = piece_ind + 1
                size = op_list[piece_ind][1]
                while i < len(op_list_cmds) and op_list_cmds[i] == DW_OP["piece"]:
                    size += op_list[i][1]
                    i += 1
                old_cmd = op_list[piece_ind]
                op_list = op_list[:piece_ind] + [(old_cmd[0], size, old_cmd[2], old_cmd[3])] + op_list[i:]
                op_list_cmds = [op for op,_,_,_ in op_list]
                next_ind = piece_ind + 1

        # Now look for op_lists involving successive DW_OP_addr/DW_OP_piece 
        # pairs at contiguous addresses
        if len(op_list_cmds) >= 2 and op_list_cmds[:2] == [DW_OP["addr"], DW_OP["piece"]]:
            base_addr = op_list[0][1]
            piece_size = op_list[1][1]
            next_ind = 2
            while len(op_list_cmds) >= next_ind+2:
                if (op_list_cmds[next_ind:next_ind+2] == [DW_OP["addr"], DW_OP["piece"]] and 
                    op_list[next_ind][1] == base_addr + piece_size):
                    # We can merge this piece into the first one because they are
                    # contiguous
                    piece_size += op_list[3][1]
                    next_ind += 2
                else:
                    break
            if next_ind == len(op_list_cmds):
                # The whole location expression was a set of contiguous addr/piece 
                # pairs, so we can collapse it down to a single address expression
                op_list = [op_list[0]]
                
        ## Filter out "DW_OP_addr 0" which looks like an error/indication of 
        ## a variable that should be ignored.
        #if op_list[0][0:2] == (DW_OP["addr"], 0):
        #    return None

        return op_list

    def execute(self, op):
        
        # read_dwarf returns 3 args for every location operation, but we 
        # only care about the first one.
        operand, arg, arg2, arg3 = op
         
        try:
            pyop = self._dwarf_py_ops[op]
        except KeyError:
            # handle manually

            # Literal encodings (2.5.1.1)
            #----------------------------
            # Only unsigned values supported

            if operand == DW_OP["addr"]:
                self._stack.append(arg)
            elif DW_OP["lit0"] <= operand <= DW_OP["lit31"]:
                self._stack.append(operand-DW_OP["lit0"])
            elif operand in set(DW_OP[opname] for opname in 
                                ("const1u","const2u","const4u","const8u")):
                self._stack.append(arg)
            elif operand == DW_OP["constu"]:
                self._stack.append(leb_udecode(arg))

            # Register values (2.5.1.2)
            # -------------------------
            
            # fbreg, bregN, bregx, regval_type not implemented yet!
            
            # Stack operations (2.5.1.3)
            # --------------------------
            elif operand == DW_OP["dup"]:
                self._stack.append(self._stack[-1])
            elif operand == DW_OP["drop"]:
                self._stack.pop()
            elif operand == DW_OP["pick"]:
                picked = self._stack[-1-arg]
                self._stack.append(picked)
            elif operand == DW_OP["over"]:
                self._stack.append(self._stack[-2])
            elif operand == DW_OP["swap"]:
                self._stack[-2:] = self._stack[-1:-3:-1]
            elif operand == DW_OP["rot"]:
                self._stack[-3:] = [self._stack[-1]] + self._stack[-3:-1]
            
            # deref, deref_size, deref_type, xderef, xderef_size, xderef_type,
            # push_object_address, form_tls_address, call_frame_cfa not implemented
            
            # Arithmetic and logical operations (2.5.1.4)
            # -------------------------------------------
            
            elif operand == DW_OP["abs"]:
                self._stack[-1] = abs(self._stack[-1])
            elif operand == DW_OP["not"]:
                self._stack[-1] = ~self._stack[-1]
            elif operand == DW_OP["plus_uconst"]:
                self._stack[-1] += leb_udecode(arg)
                
            # Control flow operations (2.5.1.5)
            # ---------------------------------
            # Not supported, except NOP
                
            elif operand == DW_OP["nop"]:
                pass
                
            else:
                operand_name = "DW_OP_" + {v:k for (k,v) in DW_OP.items()}[operand]
                raise NotImplementedError("Pydbg DWARF stack machine doesn't support "
                                          "'%s'" % operand_name)

        else:
            self._binary_op(pyop)
            
    def result(self):
        return self._stack[-1]
            
    def evaluate(self, op_list):
        op_list = self.simplify(op_list)
        if op_list is None:
            # bad op_list
            return None
        try:
            for op in op_list:
                self.execute(op)
        except NotImplementedError as exc:
            msg = str(exc)
            DW_OP_names = {v:k for (k,v) in DW_OP.items()}
            full_op_list = " ".join("DW_OP_%s %d" % (DW_OP_names[op[0]],op[1]) for op in op_list)
            raise NotImplementedError(msg + "\n (" + full_op_list + ")")
            
        return self.result()
            


class Dwarf_Symbol(object):
    """
    Interface to dwarf debugging information for a particular symbol
    """
    def __init__(self, symbol_ref, reader):
        
        self._symbol_ref = symbol_ref
        self._reader = reader
        
    @property
    def is_global(self):
        return self._reader.symbol_is_external(self._symbol_ref)

class Dwarf_Var_Type_Mixin(object):
    """
    Helper class providing some functionality that Dwarf_Var_Symbol and
    Dwarf_Type_Symbol share
    """
                                
    def _wrap_member_refs(self, member_refs):
        return [(name, offset, Dwarf_Var_Symbol(ref, self._reader)) \
                for (name, offset, ref) in member_refs]
        
    def _wrap_param_refs(self, param_refs):
        return [(name, Dwarf_Var_Symbol(ref, self._reader)) \
                for (name, ref) in param_refs]
    def _wrap_return_type(self, return_type):
        return Dwarf_Type_Symbol(return_type, self._reader)

    def _enhance_type_dict(self, d):
        """
        Helper function for adding extra info to the type dictionary 
        returned by the raw _read_dwarf calls get_var_struct_dict and
        get_type_struct_dict
        """
        
        if d.get("byte_size", None) == "<not present>":
            if d.get("type_tag",None) == DW_TAG["pointer_type"]:
                # We know what size pointers are
                d["byte_size"] = (self._reader.reader.ptr_size or 
                                  self._reader.reader.get_type("size_t").struct_dict["byte_size"])
            elif (d.get("type_tag",None) == DW_TAG["array_type"] and 
                    d["element_type"]["type_tag"] == DW_TAG["pointer_type"]):
                d["byte_size"] = ((self._reader.reader.ptr_size or 
                                  self._reader.reader.get_type("size_t").struct_dict["byte_size"]) * 
                                         d.get("num_elements",1))
            # Better just hope the client code doesn't need the byte size
            # as it wasn't in the DWARF and we have no other way of setting it.
        
        
        try:
            # these attributes are defined for bit fields only
            for attr in "bit_offset", "bit_size", "byte_size":
                val = self._reader.get_attr(self._symbol_ref, DW_AT[attr])
                if val is not None:
                    d[attr] = val 
        except RuntimeError:
            pass

        def wrap_inner_lists(d, name, wrap_func):
            try:
                d[name] = wrap_func(d[name])
            except KeyError:
                #No members list; nothing to do 
                pass

        wrap_inner_lists(d, "members", self._wrap_member_refs)
        wrap_inner_lists(d, "params", self._wrap_param_refs)
        wrap_inner_lists(d, "return_type", self._wrap_return_type)
        
        try:
            if d.get("type_tag", None) == DW_TAG["base_type"]:
                if d["base_type_name"] == d["type_name"]:
                    # C base type
                    try:
                        encoding = self._reader.get_attr(self._symbol_ref, 
                                                         DW_AT["encoding"])
                        if encoding in (DW_ATE["signed"], 
                                        DW_ATE["signed_char"]):
                            d["signed"] = True
                        else:
                            d["signed"] = False
                    except RuntimeError:
                        # Sometimes looking up this attribute goes a bit
                        # squiffy.  I don't know why.
                        pass
                else:
                    # typedef of a C base type
                    d["signed"] = self._reader.reader.get_type(
                                   d["base_type_name"]).struct_dict["signed"]
        except KeyError:
            pass

        if "element_type" in d:
            self._enhance_type_dict(d["element_type"])
        if "pointed_to" in d:
            self._enhance_type_dict(d["pointed_to"])

        return d
        
    @property
    def issigned(self):
        """
        Checks if the type is signed or unsigned.
        """
        return "signed" in self.struct_dict and self.struct_dict["signed"]

    @property
    def typename(self):
        return self.struct_dict["type_name"]
        
    @property
    def type_tag(self):
        return self.struct_dict["type_tag"]
        
    @property
    def byte_size(self):
        byte_size = self.struct_dict["byte_size"]
        if byte_size == "<not present>":
            byte_size = self._reader.reader.get_type("unsigned int").struct_dict["byte_size"]
        return byte_size
        
    @property
    def struct_dict(self):
        try:
            self._struct_dict
        except AttributeError:
            self._struct_dict = self._get_struct_dict()
        return self._struct_dict
    

class Dwarf_Raw_Type_Symbol(Dwarf_Symbol):
    
    def typed(self):
        return Dwarf_Type_Symbol.factory(self._symbol_ref, self._reader)

class Dwarf_Type_Symbol(Dwarf_Symbol, Dwarf_Var_Type_Mixin):
    """
    Interface to DWARF debugging information for a particular type
    """
    @staticmethod
    def factory(symbol_ref, reader, alias=None):
        d = reader.get_type_struct_dict(symbol_ref)
        if d["type_tag"] == DW_TAG["enumeration_type"]:
            if alias is not None:
                iprint("WARNING: type aliasing not currently supported for enumeration types")
            return Dwarf_Enum_Symbol(symbol_ref, reader)
        else:
            if alias is None:
                return Dwarf_Type_Symbol(symbol_ref, reader)
            else:
                return Dwarf_Aliased_Type_Symbol(symbol_ref, reader, alias=alias)
    
    
    def __init__(self, symbol_ref, reader):
        
        Dwarf_Symbol.__init__(self, symbol_ref, reader)
        
    @property
    def is_declaration(self):
        """
        Checks if the type has a size or not
        """
        return (self.struct_dict["type_tag"] == DW_TAG["structure_type"] and 
                ("byte_size" not in self.struct_dict or not
                isinstance(self.struct_dict["byte_size"], int_type)))

    def _get_struct_dict(self):
        """Wrap the raw Symbol_Refs returned by the C layer as 
        Dwarf_Type_Symbols but leave the plain information alone"""
        d = self._reader.get_type_struct_dict(self._symbol_ref)
        
        self._enhance_type_dict(d)

        return d

    def pointer_to(self):
        return Dwarf_Pointer_Type_Symbol(self)
    
    def array_of(self, nelems):
        return Dwarf_Array_Type_Symbol(self, nelems)

    
class Dwarf_Pointer_Type_Symbol(Dwarf_Type_Symbol):
    """
    Special type symbol that wraps a supplied type symbol and provides the
    details for a pointer to it.
    """
    def __init__(self, ptd_to):
        self._ptd_to = ptd_to
        self._reader = ptd_to._reader
        
    def _get_struct_dict(self):
        ptd_to_dict = self._ptd_to.struct_dict
        if self._ptd_to.typename.endswith("*"):
            suffix = "*"
        else:
            suffix = " *"
        return {"type_tag" : DW_TAG["pointer_type"],
                "type_name" : self._ptd_to.typename + suffix,
                # Cheat slightly to get the size of pointers
                "byte_size" : self._reader.reader.ptr_size or 
                                self._reader.reader.get_type("size_t"
                                                             ).struct_dict["byte_size"],
                "pointed_to" : self._ptd_to.struct_dict}
                

class Dwarf_Array_Type_Symbol(Dwarf_Type_Symbol):
    """
    Special type symbol that wraps a supplied type symbol and provides the
    details for an array of it, of the specified length
    """
    def __init__(self, elem_type, nelems):
        self._elem_type = elem_type
        self._nelems = nelems
        self._reader = elem_type._reader
        
    def _get_struct_dict(self):
        elem_type_dict = self._elem_type.struct_dict
        # If elem_type is itself an array, then we need to insert the new
        # dimension before the existing one(s)
        name_split = self._elem_type.typename.split("[",1)
        name = name_split[0]
        if len(name_split) == 1:
            old_suffix = ""
        else:
            old_suffix = "["+name_split[1]
        new_suffix = "[%d]" % self._nelems
        return {"type_tag" : DW_TAG["array_type"],
                "type_name" : name + new_suffix + old_suffix,
                "byte_size" : self._elem_type.byte_size * self._nelems,
                "element_type" : self._elem_type.struct_dict,
                "num_elements" : self._nelems}

    
class Dwarf_Enum_Symbol(Dwarf_Type_Symbol):
    """
    Return access to the value-symbol mapping of a given enumeration in
    addition to the standard type information
    """
    
    def __getitem__(self, name):
        
        return self._name_dict[name]

    def keys(self):
        return iter(self._name_dict.keys())

    def items(self):
        return iter(self._name_dict.items())

    def values(self):
        return iter(self._name_dict.values())

    def __repr__(self):
        return self._name_dict.__repr__()

    @property
    def _name_dict(self):
        try:
            self.__name_dict
        except AttributeError:
            self.__name_dict = self.struct_dict["enumerators"]

        return self.__name_dict

class Dwarf_Aliased_Type_Symbol(Dwarf_Type_Symbol, Dwarf_Var_Type_Mixin):
    """
    This is a very simple variant on a normal TypeSymbol.  It exists to support
    typedefs to opaque structures, which the C layer doesn't collapse directly
    into aliases like it does typedefs to visible structures.  The constructor
    takes the references to the aliased structure type plus the typedef name.
    This class just replaces the type_name field in the former's struct_dict. 
    """
    def __init__(self, opaque_symbol_ref, reader, alias):
        
        Dwarf_Symbol.__init__(self, opaque_symbol_ref, reader)
        self._alias = alias
        
    def _get_struct_dict(self):
        
        d = Dwarf_Type_Symbol._get_struct_dict(self)
        
        d["type_name"] = self._alias
        return d


class Dwarf_Var_Symbol(Dwarf_Symbol, Dwarf_Var_Type_Mixin):
    """
    Interface to DWARF debugging information for a particular variable
    """
    def __init__(self, symbol_ref, reader):
    
        Dwarf_Symbol.__init__(self, symbol_ref, reader)

    def _get_struct_dict(self):
        """
        Wrap the raw Symbol_Refs returned by the C layer as Dwarf_Var_Symbols
        but leave the plain information alone
        """
        d = self._reader.get_var_struct_dict(self._symbol_ref)
        self._enhance_type_dict(d)
        return d

    @property
    @autolazy
    def static_location(self):
        """
        Look up the address of the variable at the entry PC of the program.  If
        it's a static variable the location description will presumably be a 
        single "addr" operation, otherwise we just want to return None. 
        """
        loc_list = self._reader.get_local_loc(self._symbol_ref, 0, False)
        if loc_list is not None:
            try:
                stack = DwarfStackMachine()
                return stack.evaluate(loc_list)
            except NotImplementedError:
                pass

class Dwarf_Ranged_Symbol_Mixin(object):
    
    _allow_absent_range = False
    
    @property
    def ranges(self):
        """
        Return a list of the address ranges corresponding to this entity's
        code. List entries are (low PC, high PC) pairs where high PC is one-past
        -the-end) Most of the time the list has just one entry because the code
        is contiguous.  However there are situations where that's not true.
        
        WARNING: KCC ELFs are known to have broken CU low PC/high PC values
        (but the function values are fine) 
        """
        try:
            self._ranges
        except AttributeError:
            if not isinstance(self._symbol_ref, list):
                try:
                    self._ranges = self._reader.get_ranges(self._symbol_ref, self._allow_absent_range)
                except RuntimeError:
                    raise DwarfFunctionAddressRangeNotPresent
            else:
                # The CU comes in bits: we just gather the ranges from all the
                # bits and then clean them up
                self._ranges = []
                for ref in self._symbol_ref:
                    if ((self._reader.get_attr(ref, DW_AT["ranges"]),
                         self._reader.get_attr(ref, DW_AT["low_pc"])) != (None, None)):
                        range = self._reader.get_ranges(ref, self._allow_absent_range)
                        if range is not None:
                            self._ranges += range
                # Putting elements in a set eliminates duplicates; we then 
                # sort, lowest tuple first (tuples sorted on elements from 0)
                self._ranges = sorted(set(self._ranges)) 

        return self._ranges
    
    def is_in_range(self, pc):
        if self.ranges is not None:
            for low, high in self.ranges:
                if high < low:
                    # high is relative to low
                    high += low
                if low <= pc and pc < high:
                    return True
        return False
    
    def __contains__(self, pc):
        return self.is_in_range(pc)
    
    @property
    def address(self):
        """
        Returns the lowest address that the entity appears at.  Use
        self.ranges if it's possible the entity is in multiple segments and 
        you care about that.
        """
        return self.ranges[0][0]

    @property
    def end_address(self):
        """
        Returns the lowest address that is higher than any that the entity 
        appears at.  Use self.ranges if it's possible the entity is in multiple 
        segments and you care about that.
        """
        end_addr_or_len = self.ranges[-1][1]
        if end_addr_or_len < self.address:
            # high is relative to low
            return self.address + end_addr_or_len
        return end_addr_or_len


    @property
    def size(self):
        """
        Returns the symbol's size as range end-range start so long as 
        range end isn't smaller than range start - in that case it is assumed
        that "range end" is really the length.  Note that it is important that
        when the two are equal the size is reported as zero, as some DWARF files
        set low_pc and high_pc to -1 to indicate a non-linked function (e.g.
        XAPGCC).
        """
        return sum((r[1]-r[0] if r[1]>=r[0] else r[1]) for r in self.ranges)

class Dwarf_Func_Symbol(Dwarf_Symbol, Dwarf_Ranged_Symbol_Mixin):
    """
    Interface to DWARF debugging information for a particular function, 
    including inlined instances of functions.  (Note that inlined instances
    are not accessible directly via Dwarf_Reader.get_function(): they are only
    available from the parent Dwarf_Func_Symbol's inlined_calls list).
    
    An inlined function has distinct symbol_ref and abs_ref attributes: the
    symbol_ref is the debugging entry for the specific inlined instance of the
    source function, whose debugging entry is abs_ref. For non-inlined functions
    the abs_ref attribute is simply set equal to symbol_ref. 
    """
    
    def __init__(self, symbol_ref, reader):
        
        Dwarf_Symbol.__init__(self, symbol_ref, reader)
        self._abs_ref = reader.get_attr(symbol_ref, DW_AT["abstract_origin"])
        if self._abs_ref is None:
            self._abs_ref = symbol_ref
        
    @property
    def name(self):
        """
        Look up the name of the function that has been inlined in
        """
        return self._reader.get_attr(self._abs_ref, DW_AT["name"])
    
    @property
    def is_inline(self):
        """
        Is this an inlined function instance?  It is if the "abstract reference"
        is separate from the symbol reference
        """
        return self._abs_ref is not self._symbol_ref
    
    def get_frame_info(self, pc, executing_pc, regs):
        """
        Retrieve the stack frame information for the given *absolute* PC (which
        must be in this function's range)
        """
        return Dwarf_Stack_Frame(self._reader, self, pc - self.address, executing_pc, regs)
        
    @property
    def params(self):
        '''
        Look up the DWARF symbols for the formal parameters of the
        function.  This returns a list of (name, Dwarf_Symbol) tuples
        '''
        prms = self._reader.get_formal_params(self._symbol_ref)
        return ((name, Dwarf_Var_Symbol(sym, self._reader)) \
                    for (name, sym) in prms)
                          

    @property
    def locals(self):
        '''
        Look up the DWARF symbols for the local variables of the
        function.  This returns a list of (name, Dwarf_Symbol) tuples
        '''
        return ((name, Dwarf_Var_Symbol(sym, self._reader)) \
                    for (name, sym) in \
                          self._reader.get_local_variables(self._symbol_ref))

    @property
    def inline_calls(self):
        '''
        Return a list of inline function calls that are made from this function.
        The calls are not named: the name can be retrieved from the 
        Dwarf_Func_Symbol object itself
        '''
        return (Dwarf_Func_Symbol(sym, self._reader) 
                   for sym in self._reader.get_inline_calls(self._symbol_ref))


    def get_srcfile_and_lineno(self, pc):
        """
        If source file was found, returns (filename, line number) tuple.
        If source file wasn't found, returns None
        If there was an error, raises RuntimeError.
        """
        return self._reader.get_srcfile_and_lineno(self._symbol_ref, pc)

    @property
    def signature(self):
        """
        Return a dictionary containing details of the function's calling
        signature (currently just parameter names and types)
        """
        return [(sym.typename, name) for (name, sym) in self.params]

    @property
    def signature_string(self):
        """
        Return the function signature as a string
        
        ! Note: it is up to the caller to interpolate the function's name !
        """
        ret_type_obj = self.return_type
        if ret_type_obj:
            ret_type = ret_type_obj.struct_dict["type_name"]
        else:
            ret_type = "void"
        sig_dict = self.signature
        signature_string = "%s " % (ret_type) + "%s("
        if sig_dict:
            for type, arg in sig_dict:
                signature_string += "%s %s, " % (type, arg)
        else:
            signature_string += "void, "
        signature_string = signature_string[:-2] + ")"
        return signature_string

    @property
    def return_type(self):
        ret_symbol_ref = self._reader.get_return_type(self._symbol_ref)
        if ret_symbol_ref is None:
            # Function returns void
            return None
        return Dwarf_Type_Symbol.factory(ret_symbol_ref, self._reader)


    def get_offset_api(self, adjusted_address, adjusted_func_size):
        """
        If necessary return a wrapped version of self that accounts for a 
        difference between the ELF's and DWARF's view of the executable address
        space.  This allows us to obtain debugging information from an ELF that
        doesn't have patching preambles in the functions.
        
        Potential extension: Handle thumb state/maxim mode bit properly
        """
        if adjusted_func_size == 0:
            preamble_size = 0
        else:
            preamble_size = adjusted_func_size - (self.end_address - self.address)
        if (adjusted_address & ~1) != (self.address & ~1):
            return Dwarf_Offset_Func_Symbol(self, adjusted_address&~0x1,
                                            preamble_size=preamble_size)
        return self

class Dwarf_Offset_Func_Symbol(object):
    
    OVERRIDES = ("get_frame_info", "address", "end_address", "ranges")
    
    def __init__(self, wrapped_func, adjusted_address, preamble_size=8):
        """
        adjusted_address is the *adjusted* function's start address (in the 
        symbol table)
        
        preamble_size is the length of the manual preamble in the adjusted 
        function.
        
        """
        self._wrapped = wrapped_func
        self._addr_off = adjusted_address - self._wrapped.address
        self._pre_size = preamble_size

    def _adjusted_pc(self, wrapped_pc):
        """
        Adjust the PC from wrapped_func space to client (adjusted function)
        space
        """
        return wrapped_pc + self._addr_off + self._pre_size
    
    def _wrapped_pc(self, adjusted_pc):
        """
        Adjust the PC from adjusted function space to the wrapped function's
        space, mapping PCs in the adjusted function's preamble to the start of
        the wrapped function 
        """
        return  max(adjusted_pc - self._addr_off - self._pre_size, 
                    self._wrapped.address)
    
    def __getattr__(self, attr):
        
        if attr in self.OVERRIDES:
            raise TypeError("'%s' listed as an override, but not overridden!" % attr)
        
        return getattr(self._wrapped, attr)
    
    # Implement overrides for the Dwarf_Func_Symbol methods that relate to 
    # program counters
    def get_frame_info(self, pc, executing_pc, regs):
        return self._wrapped.get_frame_info(self._wrapped_pc(pc), 
                                            executing_pc, regs)
    
    @property
    def ranges(self):
        if self._wrapped.ranges is not None:
            return tuple((self._adjusted_pc(lo), self._adjusted_pc(hi)) for (lo,hi) in self._wrapped.ranges)
        return None
    
    def is_in_range(pc):
        if self.ranges is not None:
            for low, high in self.ranges:
                if high < low:
                    # high is relative to low
                    high += low
                if low <= pc and pc < high:
                    return True
        return False
    
    @property
    def address(self):
        return self._adjusted_pc(self._wrapped.address)

    @property
    def end_address(self):
        return self._adjusted_pc(self._wrapped.end_address)
    
    
class Dwarf_CU(Dwarf_Ranged_Symbol_Mixin):
    """
    Simple interface to the DWARF's compilation unit info.   This is a "friend"
    on Dwarf_Reader/Multi_Dwarf_Reader as it needs to get at their private
    members.
    """
    _allow_absent_range = True
    
    def __init__(self, name, symbol_ref_or_list, raw_reader, reader):
        self._name = name
        self._master_reader = reader
        # Variables that Dwarf_Ranged_Symbol_Mixin uses
        self._reader = raw_reader
        self._symbol_ref = symbol_ref_or_list
    
    @property
    def vars(self):
        try:
            return self._master_reader.vars[self._name]
        except KeyError:
            return scoped_symbol_dict({}, None)
    
    @property
    def types(self):
        try:
            self._types
        except AttributeError:
            def type_factory(scoped_name, symbol_ref):
                return Dwarf_Type_Symbol.factory(symbol_ref, self._reader)
            self._types = scoped_symbol_dict(self._reader.get_cu_type_dict(self._symbol_ref), type_factory)
        return self._types


    @property
    def enums(self):
        try:
            self._enums
        except AttributeError:
            def enum_factory(scoped_name, symbol_ref):
                return Dwarf_Enum_Symbol.factory(symbol_ref, self._reader)
            self._enums = scoped_symbol_dict(self._reader.get_cu_tag_dict(self._symbol_ref, 
                                                                DW_TAG["enumeration_type"], False, True), 
                                                                enum_factory)
        return self._enums

    @property
    def funcs(self):
        try:
            return self._master_reader.funcs[self._name]
        except KeyError:
            return scoped_symbol_dict({}, None)

    @property
    def local_variable_list(self):
        return (("::".join(name), var) for (name, var) in self.vars.items(filter=is_local))

    @property
    def local_function_list(self):
        return (("::".join(name), func) for (name, func) in self.funcs.items(filter=is_local))

    def get_variable(self, name):
        return self.vars.lookup_symbol(name)
    
    def get_function(self, name):
        return self.funcs.lookup_symbol(name)

    def get_type(self, name):
        """
        Get a type of the specified name, or if "name" involves a composition of 
        declarators, parse it and construct the appropriate type object from
        the relevant information.  However, it currently only knows how to
        construct pointers-to-X or arrays-of-X.  It can do arbitrary levels of
        pointers-to or arrays-of but it can't handle combinations (e.g. 
        "pointer-to array-of int", etc etc).
        """
        return self._master_reader.get_type_from_dwarf(name, 
                                                        symbol_dict=self.types)

    def get_enum(self, name):
        try:
            return self.enums.lookup_symbol(name)
        except self.enums.UnknownNameExcep:
            return self.get_enum("enum " + name)

                
class Dwarf_Stack_Frame(object):
    '''
    Class representing DWARF stack frame info for a given address offset
    within a given function
    
    '''

    def __init__(self, reader, func_or_name, pc_offset, executing_pc, regs):
       
        if isinstance(func_or_name, str):
            self._func_sym = reader.get_function(func_name)
        else:
            self._func_sym = func_or_name
        # Get the right c_reader for this function
        self._c_reader = self._func_sym._reader
        self._pc_offset = pc_offset
        self._executing_pc = executing_pc
        self._frame_data = self._c_reader.get_unwind_data(self._func_sym._symbol_ref,
                                                          self._pc_offset, regs)
        # _read_dwarf returns an empty dictionary if there's no unwind data
        # to be had
        if not self._frame_data:
            raise DwarfNoStackFrameInfo

    @property
    def rules(self):
        return self._frame_data

    @property
    def srcfile(self):
        try:
            self._srcfile
        except AttributeError:
            self._srcfile, self._lineno = self._srcfile_and_lineno()
        return self._srcfile

    @property
    def lineno(self):
        try:
            self._lineno
        except AttributeError:
            self._srcfile, self._lineno = self._srcfile_and_lineno()
        return self._lineno

    @property
    def params(self):
        '''
        Look up the DWARF symbols for the formal parameters of the
        function.  This returns a list of (name, Dwarf_Symbol) tuples
        '''
        return self._func_sym.params

    @property
    def locals(self):
        '''
        Look up the DWARF symbols for the local variables of the
        function.  This returns a list of (name, Dwarf_Symbol) tuples
        '''
        return self._func_sym.locals


    def local_var_loc(self, var):
        '''
        Get the list of location instructions to be used to get the value 
        of the given variable at the given PC, taking into account whether
        the instruction at this PC has started to be executed yet or not.
        '''
        return self._c_reader.get_local_loc(var._symbol_ref, 
                                            self._pc_offset + self._func_sym.address, 
                                            self._executing_pc)

    #Private methods

    def _srcfile_and_lineno(self):
        '''
        Return the source file and line number corresponding to the supplied
        program address
        Potential extension: This frame might actually cover a section of the function body that
        is an inline instance of another function.  So to get the right source
        location for the supposed call we need to adjust the PC offset to the
        point in the function at which the inlined instance containing this
        offset actually starts. 
        '''
        # The C layer returns None if the info can't be found; we return (None, None)
        # in this case because the callers expect a pair.
        return self._func_sym.get_srcfile_and_lineno(self._pc_offset) or (None, None)


class DictWithSourceInfo(object):
    """
    Class that transparently wraps a base_dict while also providing access to a
    dictionary of information about which of these are replacements and what the
    identity of the replacement source is, if supplied.  An entry in base_dict
    without a matching entry in replacement_info should be taken as not having 
    been replaced.  A DictWithSourceInfo passed no replacement_info dictionary
    will always return None for get_source_index.
    """
    def __init__(self, base_dict, replacement_info=None):
        
        self._base_dict = base_dict
        if replacement_info is None:
            replacement_info = {}
        self._replacement_info = replacement_info
        
    def __getattr__(self, attr):
        """
        Forward all attribute accesses to the base_dict, except for accesses to
        replacement_info
        """
        return getattr(self._base_dict, attr)
    
    def __getitem__(self, entry):
        return self._base_dict[entry]
    
    def __setitem__(self, entry, val):
        self._base_dict[entry] = val
    
    def __delitem__(self, entry):
        del self._base_dict[entry]

    def __contains__(self, key):
        return key in self._base_dict
    
    def __len__(self):
        return len(self._base_dict)

    def __iter__(self):
        return iter(self._base_dict.keys())
    
    def get_source_index(self, *indices):
        """
        Look up the replacement info to see if the symbol comes from the base
        source or a replacement source.  If there's no replacement source, 
        returns 0.  Obviously it is necessary to supply as many indices as there
        are levels in the dictionary.
        """
        try:
            cu_repl = self._replacement_info
            i = 0
            while i < len(indices):
                cu_repl = cu_repl[indices[i]]
                i += 1
            return cu_repl
        except KeyError:
            return 0

    def get_as_base_dict(self):
        """
        Retrieve the base_dict, but only if that's all this one contains
        """
        if self._replacement_info:
            raise ValueError("Attempting to convert a non-trivial DictWithSourceInfo back to a single base dict!")
        return self._base_dict    

def combine_dicts(cu_dict_list, name_filter=None):
    """
    Create a combined dictionary, overriding earlier symbols with later
    ones
    """
    def combine_recursively(scope_dict, later_dict, replace_dict, irepl):
        """
        Recurse on entries in later_dict that are themselves dictionaries as these
        represented nested scopes.
        Other entries are inserted into the master scope dict, with replacement
        info also recorded.
        """
        for scope_or_var_name, scope_or_var in later_dict.items():
            
            if isinstance(scope_or_var, dict):
                # We're dealing with a scope
                scope = scope_or_var
                scope_name = scope_or_var_name
                
                if scope_name in scope_dict:
                    # We're adding in a scope that already exists at this level,
                    # so to avoid adding to it and thereby modifying the original
                    # we shallow-copy so the thing we're adding to is a new dict.
                    scope_dict[scope_name] = scope_dict[scope_name].copy()

                
                combine_recursively(scope_dict.setdefault(scope_name,{}), scope,
                                    replace_dict.setdefault(scope_name,{}), irepl)
            else:
                # Mark any symbols that have appeared from
                # the newer dictionary with the dictionary's index
                var = scope_or_var
                var_name = scope_or_var_name
                if not name_filter or name_filter(var_name):
                    scope_dict[var_name] = var
                    replace_dict[var_name] = irepl

    # Spuriously combine the first dict with an empty dict in order to apply
    # the name filter to it and to produce a shallow copy of the base dict.
    cu_dict = {} 
    combine_recursively(cu_dict, cu_dict_list[0], {}, None)
    
    # Replacements are implicitly 0 (i.e. no replacement)
    replacements = {}

    if len(cu_dict_list) > 1:
        for i, later_cu_dict in enumerate(cu_dict_list[1:]):
            ireplace = i + 1
            combine_recursively(cu_dict, later_cu_dict, replacements, ireplace)
            
    return DictWithSourceInfo(cu_dict, replacements)
                     

 
# Filters for use in ScopedSymbolDict calls

def is_global(k, sym):
    return sym.is_global

def is_local(k, sym):
    return not sym.is_global


class Dwarf_Reader(object):    
    """
    Dwarf_Reader interface that manages symbols coming from multiple sources
    with duplicate names taken from the later of the sources.  Note: this
    doesn't play that nicely with the way we disambiguate duplicate symbol names
    within different compilation units in the same 
    """
                   
    class _DuplicatedLocalException(Exception):
        
        def __init__(self, msg, appears_in):
            Exception.__init__(self, msg)
            self.appears_in = appears_in

    class NotAGlobalException(ValueError):
        def __init__(self, name):
            ValueError.__init__(self, 
                        "'%s' is a variable but not a global one" % name)

    class Raw_Reader(object):
        """
        Simple wrapper around the raw C DWARF parser that stores the parent 
        Dwarf_Reader in case a symbol needs to access info about symbols other 
        than itself  
        """
        def __init__(self, c_reader, reader):
            self._c_reader = c_reader
            self.reader = reader
            
        def __getattr__(self, name):
            """
            Accesses to any attributes other than self.reader are just forwarded
            straight to the C layer
            """
            return getattr(self._c_reader, name)

    def __init__(self, elf_reader_object_or_paths, name_filters=None, ptr_size=None,
                 include_abstract_symbols=False):
        raw_readers = []
        accessors = []
        elf_files = []
        if isinstance(elf_reader_object_or_paths, Elf_Reader):
            elf_reader_or_paths = elf_reader_object_or_paths
            # We can construct with either a file path, in which case we
            # create an entirely new libelf instance in memory, or an existing
            # Elf_Reader, in which case we share the libelf instance
            # Elf_Reader containing a list of C-layer readers
            raw_readers = [
                    self.Raw_Reader(c_read_dwarf.Dwarf_Reader(rdr), 
                                    self) 
                    for rdr in elf_reader_or_paths._c_readers]
            elf_files = elf_reader_or_paths.elf_files 
        elif isinstance(elf_reader_object_or_paths, list):
            elf_readers_or_paths = elf_reader_object_or_paths
            for obj in elf_readers_or_paths:
                if isinstance(obj, str):
                    # Construct a new RawReader from an ELF file path
                    raw_readers.append(self.Raw_Reader(c_read_dwarf.Dwarf_Reader(obj), self))
                    elf_files.append(obj)
                elif isinstance(obj, Elf_Reader):
                    # Extract the C reader from the supplied ELF reader - there
                    # must be only one
                    elf_c_readers = obj._c_readers
                    if len(elf_c_readers) != 1:
                        raise ValueError("ElfReader supplied for ad-hoc "
                                         "Dwarf_Reader construction refers to %d "
                                         "(not 1) ELF files!" % len(elf_c_readers))
                    raw_readers.append(
                        self.Raw_Reader(c_read_dwarf.Dwarf_Reader(elf_c_readers[0]), self))
                    elf_files += obj.elf_files
                elif isinstance(obj, Dwarf_Reader):
                    accessors.append(obj._accessor)
        else:
            elf_path = elf_reader_object_or_paths
            raw_readers = [self.Raw_Reader(c_read_dwarf.Dwarf_Reader(elf_path),
                                           self)]
            elf_files.append(elf_path)
            
        if raw_readers:
            self._accessor = Multi_Reader_Dwarf_Accessor(raw_readers,
                                                         name_filters or {},
                                                         include_abstract_symbols,
                                                         elf_files)
        else:
            self._accessor = Combined_Dwarf_Accessor(accessors)

        self.ptr_size = ptr_size
            
    def set_verbosity(self, level):
        """
        Set verbosity level in the underlying C layer. Valid levels are:
         - 0 = silent
         - 1 = errors
         - 2 = warnings
         - 3 = info
        """
        self._accessor.set_verbosity(level)
        
        
    def get_variable(self, name):
        """
        Look up a variable of the given name.  The name need not be fully 
        scope-qualified but it should be unique.
        """
        return self.vars.lookup_symbol(name)

    def get_variable_all(self, name):
        """
        Return a dictionary containing all variable matches for the given name.
        This will have more than one entry if the given name appears in more 
        than one scope.  If there are no matches an empty dictionary is returned
        """
        return {n : self.vars[n] for n in self.vars.scoped_name_matches(name)}

    def get_global_var(self, name):
        """
        Get a global with the given name, which should be sufficiently 
        scope-qualified to be unique.  If there's no global by that name but
        there is a non-global, raises NotAGlobalException; otherwise raises 
        DwarfNoSymbol.
        """
        try:
            return self.vars.lookup_symbol(name, filter=is_global)
        except self.vars.UnknownNameExcep:
            # there's no *global* by that name...
            if self.vars.scoped_name_matches(name):
                # there are locals
                raise self.NotAGlobalException(name)
            # no locals
            raise # the original exception was fine

    @property
    def global_variable_list(self):
        """
        Return a name, value list of all the global variables
        """
        return self.vars.items(filter=is_global)

    @property
    def global_function_list(self):
        """
        Return a name, value list of all the global functions
        """
        return self.funcs.items(filter=is_global)

    def get_cu_variable(self, cu, name):
        """
        Look up the given variable in the given CU
         :p cu Full path to compilation unit
         :p name Variable name, sufficiently scope-qualified to be unique in that CU
        """
        return self.vars[cu].lookup_symbol(name)

    def get_cu_function(self, cu, name):
        """
        Look up the given function in the given CU
         :p cu Full path to compilation unit
         :p name Function name, sufficiently scope-qualified to be unique in that CU
        """
        return self.funcs[cu].lookup_symbol(name)

    def get_type_from_dwarf(self, name, alias=None, symbol_dict=None):
        """
        Get a type of the specified name, or if "name" involves a composition of 
        declarators, parse it and construct the appropriate type object from
        the relevant information.  However, it currently only knows how to
        construct pointers-to-X or arrays-of-X.  It can do arbitrary levels of
        pointers-to or arrays-of but it can't handle combinations (e.g. 
        "pointer-to array-of int", etc etc).
        
        Optionally return the type as a Dwarf_Aliased_Type_Symbol, to support
        creating ad hoc typedef'd types (e.g. when manually resolving opaque
        types)
        
        WARNING: this function disregards the fact that type declarations are
        implicitly scoped to the end of the compilation unit they are found in,
        meaning that different CUs can in principle use the same name for
        different types.  We just take the first definition of a type that has
        a matching name.
        """
        if symbol_dict is None:
            symbol_dict = self.types
        
        if isinstance(name, str):
            name = name.split("::")
        unq_name = name[-1]
        scope = tuple(name[:-1])
        
        if "*" in unq_name and "[" in unq_name:
            raise ValueError("Can't construct type details for mixed "
                             "array/pointer types")
        pointer_level = 0
        while unq_name.endswith("*"):
            pointer_level += 1
            unq_name = unq_name[:-1].rstrip()
        array_dims = []
        regexp=r"(.*)\[([0xa-fA-F\d]+)\]"
        while 1:
            match = re.match(regexp, unq_name)
            if match:
                unq_name = match.group(1)
                array_dims.append(int(match.group(2),0))
            else:
                break
        array_dims.reverse()
        
        name = scope + (unq_name,)
        type = None
        def is_definition(key, type_sym):
            return not type_sym.is_declaration
        try:
            type = symbol_dict.lookup_symbol(name,  filter=is_definition, require_unique=False)
        except symbol_dict.UnknownNameExcep:
            # There are no definitions by that name
            matching_declarations = symbol_dict.scoped_name_matches(name)
            if matching_declarations:
                # There are some declarations.  Can we get anywhere by looking
                # up the base_type_name?
                declaration_type_dict = symbol_dict[matching_declarations[0]].struct_dict
                if "base_type_name" in declaration_type_dict:
                    scoped_base_type_name = declaration_type_dict["base_type_name"].split("::") 
                    if scoped_base_type_name != name:
                        try:
                            type = symbol_dict.lookup_symbol(scoped_base_type_name,
                                                filter=lambda k, sym : not sym.is_declaration,
                                                report_name="::".join(name))
                        except symbol_dict.AmbiguousNameExcep as e:
                            type = symbol_dict[e.matches[0]]
                if type is None:
                    # Nothing doing with base_type_name
                    raise DwarfUndefinedType("Looking up definition of '%s' - "
                                         "found %d declarations but no definition!"
                                          % ("::".join(name), len(matching_declarations)))
            else:
                # Backwards compatibility: user may have passed an unqualified 
                # tagname, so try qualifying it (assume struct; unions are rare,
                # especially named ones).
                type = symbol_dict.lookup_symbol(scope + ("struct %s"%unq_name,),
                                                filter=is_definition,
                                                report_name="::".join(name))
                
        except symbol_dict.AmbiguousNameExcep as e:
            
            # Just pick the first one - we're going to assume that all types
            # looked up in this interface are "global" in the sense that
            # there's no other type with the same name in a different CU
            # that is materially different
            type = symbol_dict[e.matches[0]]
        
        
        while pointer_level > 0:
            type = type.pointer_to()
            pointer_level -= 1
        while array_dims:
            type = type.array_of(array_dims.pop())
        
        return type

    def get_type(self, name):
        """
        Wrapper around get_type_from_dwarf which exists to implement a cache,
        so as to avoid repeated DWARF look-ups for common types.
        See get_type_from_dwarf for semantics.
        """
        try:
            self._type_dict_cache
        except AttributeError:
            self._type_dict_cache = { }

        try:
            self._type_dict_cache[name]
        except KeyError:
            self._type_dict_cache[name] = self.get_type_from_dwarf(name)

        return self._type_dict_cache[name]

    def get_type_names(self):
        """
        Return a list of the fully-qualified names of all the types we know about
        """
        return [self.types.sep.join(k) for k in list(self.types.keys())]
            
    def get_cu_type(self, cu, name):
        """
        Look up the given type in the given CU
         :p cu Full path to compilation unit
         :p name Type name, sufficiently scope-qualified to be unique in that CU
        """
        return self.types[cu].lookup_symbol(name)

    def get_enum(self, name):
        """
        Look up the given enum in the given CU
         :p cu Full path to compilation unit
         :p name Enum name, sufficiently scope-qualified to be unique in that CU
        If the raw name doesn't match, tries prepending "enum " to it
        """
        try:
            # Enums tend to be #included in multiple CUs: we assume all the
            # instances we find will be the same thing 
            return self.enums.lookup_symbol(name, require_unique=False)
        except self.enums.UnknownNameExcep:
            return self.enums.lookup_symbol("enum " + name, report_name=name,
                                            require_unique=False)

    def get_function(self, name):
        """
        Look up the given function
         :p name Function name, sufficiently scope-qualified to resolve to a
         unique global or local
        """
        try:
            return self.funcs.lookup_symbol(name)
        except self.funcs.AmbiguousNameExcep as orig_excep:
            # Perhaps there's a global and some locals: we default to the global
            # in that case
            try:
                return self.funcs.lookup_symbol(name, filter=is_global)
            except self.funcs.UnknownNameExcep:
                # Nope; just ambiguous
                raise orig_excep

    def get_function_by_cu(self, name, cu):
        """
        Look up the given func in the given CU
         :p cu Full path to compilation unit, or None for a global
         :p name Function name, sufficiently scope-qualified to be unique in that CU
        """
        if cu is None: # global
            return self.get_function(name)
        else:
            return self.funcs[cu].lookup_symbol(name)


    def get_function_addr(self, name):
        """
        Look up the given function's address
         :p name Function name, sufficiently scope-qualified to resolve to a
         unique global or local
        """
        return self.get_function(name).address

    @property
    def function_list(self):
        """
        A list of all the functions available, returned as 
        (fully-scoped name, cu name (None for globals), function object) triples
        """
        for name, obj in list(self.funcs.items()):
            if obj.is_global:
                cu_name = None
            else:
                cu_name = name[0]
            yield "::".join(name[1:]), cu_name, obj
        

    def lookup_local(self, name):
        """
        Return all the name matches for non-global variables
        """
        return self.vars.scoped_name_matches(name, filter=is_local)

    def function_cus(self, name):
        """
        Return the list of CUs that a given function appears in, if any 
        """
        return [n[0] for n in self.vars.scoped_name_matches(name)]

    @property
    def funcs(self):
        """
        Master ScopedSymbolDict of functions
        """
        try:
            self._functions
        except AttributeError:
            def function_factory(scoped_name, symbol_ref):
                src = self._accessor.funcs.get_source_index(*scoped_name)
                return Dwarf_Func_Symbol(symbol_ref, self._accessor.get_raw_reader(src))
            self._functions = scoped_symbol_dict(self._accessor.funcs, function_factory)
        return self._functions
        
    
    @property
    def types(self):
        """
        Master ScopedSymbolDict of types
        """
        try:
            self._types
        except AttributeError:
            def type_factory(scoped_name, symbol_ref):
                src = self._accessor.types.get_source_index(*scoped_name)
                return Dwarf_Type_Symbol.factory(symbol_ref, self._accessor.get_raw_reader(src))
            self._types = scoped_symbol_dict(self._accessor.types, type_factory,
                                             combine_cus=True)
        return self._types

    @property
    def enums(self):
        """
        Master ScopedSymbolDict of enums
        """
        try:
            self._enums
        except AttributeError:
            def enum_factory(scoped_name, symbol_ref):
                src = self._accessor.enums.get_source_index(*scoped_name)
                return Dwarf_Enum_Symbol(symbol_ref, self._accessor.get_raw_reader(src))
            self._enums = scoped_symbol_dict(self._accessor.enums, enum_factory,
                                             combine_cus=True)  
        return self._enums

    @property
    def vars(self):
        """
        Master ScopedSymbolDict of vars
        """
        try:
            self._vars
        except AttributeError:
            def var_factory(scoped_name, symbol_ref):
                src = self._accessor.vars.get_source_index(*scoped_name)
                return Dwarf_Var_Symbol(symbol_ref, self._accessor.get_raw_reader(src))
            self._vars = scoped_symbol_dict(self._accessor.vars, var_factory)
        return self._vars

    @property
    def cus(self):
        """
        Master ScopedSymbolDict of compilation units
        """
        try:
            self._cus
        except AttributeError:
            def cu_factory(scoped_name, symbol_ref):
                scoped_name = "/".join(scoped_name)
                # The replacements dict will have the original, possibly
                # mixed, path separators, so this won't work properly
                src = self._accessor.cus.get_source_index(scoped_name)
                return Dwarf_CU(scoped_name, symbol_ref, self._accessor.get_raw_reader(src), self)
            self._cus = path_dict(self._accessor.cus, cu_factory)
        return self._cus 

    @property
    def econsts(self):
        # no need to cache at this level - the accessor already does so
        return self._accessor.econsts

    def get_global_cu(self, name):
        """
        Get the CU of the named global
        """
        return "/".join(self.vars.index[name][0][:-1])

    def get_global_function_cu(self, name):
        """
        Get the CU of the named global function
        """
        return "/".join(self.funcs.index[name][0][:-1])
    
    def is_external(self, sym, src=0):
        """
        Does the raw symbol in the given DWARF instance have the external 
        attribute set in the DWARF?
        :p sym Raw Dwarf_Symbol_Ref object
        :p src Index of DWARF instance
        """
        return self._raw_readers[src].symbol_is_external(sym)

    def get_frame_info(self, pc, reg_list):

        for irdr in range(100):
            try:
                rdr = self._accessor.get_raw_reader(irdr)
            except IndexError:
                break
            frame_info = rdr.get_unwind_data_at_pc(pc, reg_list)
            if frame_info is not None:
                return frame_info


def get_sym_matching_offset(sym_dict, offset):
    """
    Exhaustively search the given sym_dict for a symbol matching the given
    offset, and if found return a pair containing its key and the index of
    the reader it is associated with.
    """
    def search_scope(scope_dict):
        found = None
        for key, value in scope_dict.items():
            if isinstance(value, dict):
                found = search_scope(value)
                if found is not None:
                    return key + found
            else:
                if value.get_offset() == offset:
                    return key
    
    key = search_scope(sym_dict)
    if key is None:
        return None
    src = sym_dict.get_source_index(*key)
    return key, src
        

    
class Multi_Reader_Dwarf_Accessor(object):
    """
    Basic access to the categorised symbol dictionaries based on a set of 
    raw_readers, each of which corresponds to a different libdwarf session.
    """
    
    def __init__(self, readers, name_filters, include_abstract_symbols, elf_files):
        
        self._raw_readers = readers
        self._name_filters = name_filters
        self._include_abstract_symbols = include_abstract_symbols
        self.elf_files = elf_files
    
    def get_raw_reader(self, src):
        return self._raw_readers[src]
    
    def elf_for_reader(self, reader):
        for src, rdr in enumerate(self._raw_readers):
            if rdr is reader:
                return self.elf_files[src]
    
    @property
    def funcs(self):
        """
        Combined raw func dictionaries
        """
        try:
            self._functions
        except AttributeError:
            self._functions = combine_dicts(
              [reader.get_func_dict(self._include_abstract_symbols) 
                                for reader in self._raw_readers],
                name_filter=self._name_filters.get("funcs"))
        return self._functions
        
    
    @property
    def types(self):
        """
        Combined raw type dictionaries
        """
        try:
            self._types
        except AttributeError:
            self._types = combine_dicts(
              [reader.get_type_dict() for reader in self._raw_readers],
                name_filter=self._name_filters.get("types"))
        return self._types

    @property
    def enums(self):
        """
        Combined raw enum dictionaries
        """
        try:
            self._enums
        except AttributeError:
            self._enums = combine_dicts(
                [reader.get_tag_dict(DW_TAG["enumeration_type"], False, True, True) 
                                     for reader in self._raw_readers],
                                     name_filter=self._name_filters.get("enums"))
        return self._enums

    @property
    def vars(self):
        """
        Combined raw var dictionaries
        """
        try:
            self._vars
        except AttributeError:
            self._vars = combine_dicts(
                        [reader.get_tag_dict(DW_TAG["variable"], False, False, False)
                                  for reader in self._raw_readers],
                        name_filter=self._name_filters.get("vars"))
        return self._vars

    @property
    def cus(self):
        """
        Combined raw CU dictionaries
        """
        try:
            self._cus
        except AttributeError:
            self._cus = combine_dicts(
                [reader.get_cu_ref_dict() for reader in self._raw_readers],
                 name_filter=self._name_filters.get("cus"))
        return self._cus 

    @property
    def econsts(self):
        try:
            self._econsts
        except AttributeError:
            self._econsts = {}
            for reader in self._raw_readers:
                self._econsts.update(reader.get_enumerators_dict())
        return self._econsts

    def set_verbosity(self, level):
        for reader in self._raw_readers:
            ret = reader.set_verbosity(level)
        return ret



class Combined_Dwarf_Accessor(object):
    """
    Basic access to the categorised symbol dictionaries based on a set of 
    Accessors.  This is to allow the same libdwarf sessions to be shared between
    individual fw envs and combined fw envs.  
    
    The motivating use case is when we want to combine ROM and patch envs.   
    There is one accessor for the ROM and one for the patch, and there is a 
    Dwarf_Reader for each of those based on
    a Multifile_Dwarf_Accessor (though in fact there is only one ELF file for 
    each so the ability to merge at this level is redundant in this case).  Then
    to create a Dwarf_Reader for the combined environment we use this class to
    make the two Accessors act as one.
    """
    
    def __init__(self, accessor_list):
        
        self._accessors = accessor_list
        
    def get_raw_reader(self, src):
        return self._accessors[src].get_raw_reader(0)
        
    @property
    def elf_files(self):
        return sum((acc.elf_files() for acc in self._accessors), [])
        
    def elf_for_reader(self, reader):
        for acc in self._accessors:
            elf = acc.elf_for_reader(reader)
            if elf is not None:
                return elf

    @property
    def funcs(self):
        """
        Combined raw func dictionaries
        """
        try:
            self._functions
        except AttributeError:
            self._functions = combine_dicts(
                      [accessor.funcs.get_as_base_dict() for accessor in self._accessors])
        return self._functions
        
    
    @property
    def types(self):
        """
        Combined raw type dictionaries
        """
        try:
            self._types
        except AttributeError:
            self._types = combine_dicts(
                      [accessor.types.get_as_base_dict() for accessor in self._accessors])
        return self._types

    @property
    def enums(self):
        """
        Combined raw enum dictionaries
        """
        try:
            self._enums
        except AttributeError:
            self._enums = combine_dicts(
                      [accessor.enums.get_as_base_dict() for accessor in self._accessors])
        return self._enums

    @property
    def vars(self):
        """
        Combined raw var dictionaries
        """
        try:
            self._vars
        except AttributeError:
            self._vars = combine_dicts(
                      [accessor.vars.get_as_base_dict() for accessor in self._accessors])
        return self._vars

    @property
    def cus(self):
        """
        Combined raw CU dictionaries
        """
        try:
            self._cus
        except AttributeError:
            self._cus = combine_dicts(
                      [accessor.cus.get_as_base_dict() for accessor in self._accessors])
        return self._cus 

    @property
    def econsts(self):
        try:
            self._econsts
        except AttributeError:
            self._econsts = {}
            for accessor in self._accessors:
                self._econsts.update(accessor.econsts)
        return self._econsts


    def set_verbosity(self, level):
        for accessor in self._accessors:
            ret = accessor.set_verbosity
        return ret

