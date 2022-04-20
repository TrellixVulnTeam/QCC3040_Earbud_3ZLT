############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Device Debugging Framework Standalone Firmware Environment (Adaptor)
"""
import re
import os

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import path_split, path_join, PureVirtualError
from csr.dev.hw.address_space import PassiveAccessCache
from csr.dev.hw.core.base_core import BaseCore
from csr.dev.fw.meta.i_firmware_build_info import IFirmwareBuildInfo
from csr.dev.framework.meta.elf_firmware_info import varinfo_factory
from .env_helpers import  var_factory, GlobalFirmwarePresenter,  var_address, var_info, _Structure
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.fw.meta.elf_code_reader import NotInLoadableElf
import sys
from csr.dev.fw.slt import RawSLT
from csr.dev.fw.struct_semantics import StructSemanticsDict


try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int


def _decode_struct_request(request):
    
    #Preliminary step: if a request starts with a "*", put the rest in 
    #parentheses, because that's what it means, and it's easier to parse that
    #way
    if request[0] == "*":
        request = request[0] + "(" + request[1:] + ")"
    
    #Now, process anything in parentheses recursively, replacing it in the
    #string wtih its escaped index.
    
    revised_request =""
    paren_num = 0
    parens = []
    i = 0
    while i < len(request):
        #Recurse on the contents of parentheses
        if request[i] == "(":
            if i == len(request) - 1:
                #Can't have an opening parenthesis as the last character!
                raise ValueError("Mis-matched parentheses")
            start_paren = i
            i += 1
            #Walk through to the matching closing parenthesis
            inner_count = 1
            while inner_count > 0:
                if i == len(request):
                    raise ValueError("Mis-matched parentheses")
                if request[i] == ")":
                    inner_count -= 1
                elif request[i] == "(":
                    inner_count += 1
                i += 1
            end_paren = i
            
            paren_str = request[start_paren+1 : end_paren-1]
            #Recursively decode the contents of the string inside the parentheses
            parens.append(_decode_struct_request(paren_str))
            revised_request += "$%d" % paren_num
            paren_num += 1
            i = end_paren
            
        #Just copy everything else into the revised request as is. 
        else:
            revised_request += request[i]
    
        i += 1
    
    request = revised_request   
    
    #Split on member accesses
    target_list = request.split(".")
    
    #Now process the funnies...
    
    #First, pull dereferences out of names, i.e. turn "*a" into ["a", "*"]
    revised_target_list = []
    for target in target_list:
        revised_target_list.append(target.replace("*",""))
        if target.startswith("*"):
            revised_target_list.append("*")
    
    target_list = revised_target_list
    revised_target_list = []
    
    #Next turn 'a->b' into ['a', '*', 'b']
    for target in target_list:
        match = re.match("(.+)->(.+)", target)
        if match:
            revised_target_list +=[match.group(1),"*",
                                   match.group(2)]
        else:
            #Nothing to see here
            revised_target_list.append(target)
            
    #Finally, parse array indices
    target_list = revised_target_list
    revised_target_list = []

    for target in target_list:
        #We can handle single indices or slices (single stride)
        match = re.match("(.+)\[([0-9:]+)\]", target)
        if match:
            #Append the array name
            revised_target_list.append(match.group(1))
            #Construct a slice if necessary
            slice_match = re.match("(.+):(.+)", match.group(2))
            if slice_match:
                revised_target_list.append(slice(int(slice_match.group(1)),
                                                 int(slice_match.group(2))))
            #Otherwise just append the single index
            else:
                revised_target_list.append(int(match.group(2)))
        else:
            revised_target_list.append(target)
                
    target_list = revised_target_list

    #Now insert the parenthesis lists in place of their escaped indices
    i = 0
    revised_target_list = []
    while i < len(target_list):
        try:
            paren_match = re.match("\$([0-9]+)",target_list[i])
            if paren_match:
                revised_target_list = revised_target_list[0:i] + \
                                        parens[int(paren_match.group(1))]
            else:
                revised_target_list.append(target_list[i])
        except TypeError:
            #Target_list[i] is probably an integer or slice if this is thrown
            revised_target_list.append(target_list[i])
        i += 1
        
    return revised_target_list



        
class StandaloneFirmwareEnvironment (GlobalFirmwarePresenter):
    """\
    FirmwareEnvironment implemented by direct access to discrete meta-data and
    data.
    """    
    # The implementation approach is to keep the instance and meta-data
    # artifacts cleanly separated and carry a reference from variable instances
    # to their meta-data.
    #   
    # Contrasts with the interface and xide adaptor implementation where meta
    # data & data access are inextricably intertwined.
        
    def __init__(self, fw_build_info, core_data_space, layout_info,
                 program_space=None):
        """
        """
        self._build_info = fw_build_info
        self._layout_info = layout_info
        
        self.compressed_var_display = True

        if isinstance(core_data_space, BaseCore):
            self._data = core_data_space.data
            self._core = core_data_space
        else:
            self._data = core_data_space
            self._core = None

        if (self._core is not None and
                os.getenv("PYDBG_DISABLE_FW_VER_CHECKS") is None and
                self._core.access_cache_type != PassiveAccessCache):
            try:
                elf_build_id_str = self._build_info.elf_slt.build_id_string
                elf_build_number = self._build_info.elf_slt.build_id_number
                fw_build_id_str = self._core.fw.build_string
                fw_build_number = self._core.fw.build_number
                build_id_numbers_implemented = fw_build_number is not None
                build_id_strings_implemented = fw_build_id_str is not None
            except (AttributeError, NotImplementedError, KeyError, NotInLoadableElf):
                # either build_info doesn't have an elf_slt or there's no 
                # build_id_string in it
                build_id_numbers_implemented = False
                build_id_strings_implemented = False

            # We try build id strings first but fall back to build id numbers to catch debug event systems.
            if build_id_strings_implemented:
                if elf_build_id_str != fw_build_id_str:
                    raise ValueError("Provided ELF file build ID: '{}' "
                                     "does not match loaded build ID: '{}'"
                                     .format(elf_build_id_str,
                                             fw_build_id_str))
            elif build_id_numbers_implemented and elf_build_number != fw_build_number:
                raise ValueError("Provided ELF file build number: '{}' "
                                 "does not match loaded build number: '{}'"
                                 .format(elf_build_number,
                                         fw_build_number))

        # Quartz App firmware has the DWARF info unhelpfully spread across two
        # files.  Other build_info objects will give the same object for the
        # two.
        sym_tab_dwarf = self._build_info.dwarf_sym
        if sym_tab_dwarf is self._build_info.dwarf_full:
            sym_tab_dwarf = None
        
        asm_sym = self._build_info.get_asm_sym

        GlobalFirmwarePresenter.__init__(self, self._build_info.dwarf_full,
                                         self._build_info.elf_reader,
                                         var_factory(self, self._data,
                                                     layout_info),
                                         layout_info,
                                         self._build_info.toolchain,
                                         sym_tab_dwarf=sym_tab_dwarf,
                                         asm_sym=asm_sym)

        # Run the program space load if necessary: this is the first point at
        # which we definitely have access to the required info
        if program_space is not None:
            iprint("Loading program space (%s)" % program_space.name)
            fw_build_info.load_program_cache(program_space)


    @property
    def _info(self):
        try:
            self._info_
        except AttributeError:
            self._info_ = GlobalFirmwarePresenter(self.dwarf, self.elf,
                                                  varinfo_factory(self._layout_info),
                                                  self._layout_info,
                                                  self._toolchain)
        return self._info_

    @property
    def elf_info(self):
        return self._info
    @property
    def info(self):
        return self._info
    
    @property
    def dwarf(self):
        return self._build_info.dwarf_full
    
    @property
    def elf(self):
        return self._build_info.elf_reader

    @property
    def host_interface_info(self):
        return self._info.host_interface_info
                    
    # Extensions
    
    @property
    def build_info(self):
        """\
        Interface to firmware build information (IFirmwareBuildInfo)
        """
        return self._build_info
    
    @property
    def layout_info(self):
        """
        For convenience, give access to the supplied layout_info object
        """
        return self._layout_info
    
    @property
    def struct_semantics(self):
        """
        A dictionary-like object that allows look-up of a structure type name,
        returning (if available) a dictionary of additional semantics for the
        structure, including information like:
         - the current valid field of a union field
         - whether a pointer field is just a pointer, or an array, and what its
         length is
         - whether to interpret a pointer or array as a string, and how to
         decode it
        """
        try:
            self._struct_semantics
        except AttributeError:
            self._struct_semantics = StructSemanticsDict(
                self,
                self._core, 
                self._core.fw.custom_struct_semantics_type if self._core else None)
        return self._struct_semantics



    def get_data_symbol_value(self, key):        
        raise NotImplementedError()
        raise KeyError("Global Symbol '%s' unknown" % key)            

    @property
    def data(self):
        return self._data
    
    def struct(self, name_or_address, type=None, module=None):
        """
        Print a representation of the given variable, specified either by name
        or 
        
        Note: this is really only maintained for backwards compatibility as 
        variable objects are self-displaying, and looking them up by address
        is easy via the env.vars or env.globalvars containers.  One thing this
        does do is let you point at memory not containing a real variable and
        represent it as a given type, but you can also do that via env.cast. 
        
        :param name_or_address: Name or address of variable (can be an address
         anywhere in the memory footprint of a real variable, so long as it is
         entirely in memory rather than wholly or partly in a register, or an
         arbitrary address, but in the latter case an explicit type is required)
        :param type, optional: Name of the type of the variable stored at the
         given address.  Only required when the variable isn't one the debugging
         symbols know about, e.g. because it has been malloc'd.
        :param module, unused: maintained for backwards compatibility, but no
         longer used.
        
        """
        var = None

        
        if isinstance(name_or_address, str):
            name = name_or_address

            #First, split on member reference.
            target_list = _decode_struct_request(name)
            var = self.vars[target_list[0]]
        
        elif isinstance(name_or_address, int_type):
            
            if type is None:
                addr, name, var = self.vars.get_var_at_address(name_or_address)
                if not var:
                    raise TypeError("struct: Must supply a type if first argument "
                                    "is an address that doesn't match a "
                                    "statically allocated variable!")
            else:
            
                var = self.cast(name_or_address, type, module)
                name = "<unnamed>"
                target_list = [name]
            
        else:
            raise TypeError("struct: must supply either a variable name or a "
                            "data space address as first argument")
        
        if var:
            string_list = var.display(name, " |", target_list, [],
                                      last=True)
            for ostr in string_list:
                iprint(ostr) 
        else:
            iprint("'%s' not found" % name) 


    def find_field(self, type=None, value=None):
        """
        Look through all structures to find any with field whose type matches
        type and/or whose value matches value
        
        At least one of type and value must be supplied
        
        :param type, optional: Name of field type to search for
        :param value, optional: (Integral) value of field to search for
        :return Recursive dictionary whose keys are structure of member names 
         and whose values are either further dictionaries of the same kind, or
         True to indicate that the member itself matches.
        """

        def check_var(var, type, value):
            
            fields = {}
            # Loop over members.  If any is  Structure, recurse
            for mname in var.members.keys():
                mbr = var.members[mname]
                
                try:
                    mvalue = mbr.value
                    value_matches = (value is None or mvalue == value)
                    mtype = mbr.typename
                    type_matches =  (type is None or type == mtype)
                    
                    if value_matches and type_matches:
                        fields[mname] = True
                except AttributeError:
                    # Arrays and Structures
                    if isinstance(mbr, _Structure):
                        subfields = check_var(mbr, type, value)
                        if subfields is not None:
                            fields[mname] = subfields
            if fields:
                return fields   

        all_fields = {}
        
        for varname in self.vars.keys():
            
            if isinstance(self.vars[varname], _Structure):
                
                fields = check_var(self.vars[varname], type, value)
                if fields is not None:
                    
                    all_fields[varname] = fields

        return all_fields            

    class _SizeTreeEntry(object):
        """
        Element object in size tree.
        Each element captures the code and data size of either one or a
        collection of symbols. The size tree forms a tree where leaf nodes
        represent symbols and non-leaf nodes represent a collection of symbols
        identified by CU path.
        """
        def __init__(self, code_size=0, data_size=0):
            """
            Constructor for size tree element.
            
            :param code_size: To be used for leaf nodes. Indicates the code size
            of a symbol.
            :param data_size: To be used for leaf nodes. Indicates the data size
            of a symbol.
            """
            self.next = dict()
            self._code_size = code_size
            self._data_size = data_size
        def __str__(self):
            return "'code_size':%d, 'data_size':%d , %s" % (self.code_size(), self.data_size(), list(self.next))
        def __repr__(self):
            return self.__str__()
        def code_size(self):
            """
            Calculates recursively the total code size of a node and it's
            children.
            
            :returns: Total code size of sub-tree.
            """
            return sum([k.code_size() for k in self.next.values()] + [self._code_size])
        def data_size(self):
            """
            Calculates recursively the total data size of a node and it's
            children.
            
            :returns: Total data size of sub-tree.
            """
            return sum([k.data_size() for k in self.next.values()] + [self._data_size])

    @property
    def _size_tree(self):
        """
        Lazily constructs the size tree. The size tree forms a tree where leaf
        nodes represent symbols and non-leaf nodes represent a collection of
        symbols identified by CU path.
        
        :returns: The size tree.
        """
        try:
            self.__size_tree
        except AttributeError:
            self.__size_tree = self._SizeTreeEntry()
            # vars_skipped keeps track of how many variables could not be parsed
            vars_skipped = 0
            for cu_key in self.cus:
                try:
                    cu = self.cus[cu_key]
                except KeyError:
                    # some CUs cannot be obtained; this should be fixed in the future
                    continue
                # starting from the root, parse the tree adding the path for the current CU if not already in
                tree = self.__size_tree
                for lvl_name in cu_key:
                    if lvl_name not in tree.next:
                        tree.next[lvl_name] = self._SizeTreeEntry()
                    tree = tree.next[lvl_name]
                # for the current CU create all leaf nodes that capture the function sizes
                for func in cu.funcs:
                    func_addr = cu.funcs[func]
                    tmp = self.functions.get_function_of_pc(func_addr)[2]
                    func_size = tmp.size
                    func_name = tmp.name
                    tree.next[func_name] = self._SizeTreeEntry(code_size=func_size, data_size=0)
                # for the current CU create all leaf nodes that capture the variable sizes
                for var in cu.vars:
                    try:
                        cu.vars[var]._info
                    except RuntimeError:
                        # sometimes there are errors parsig vatiables; this should be fixed in the future
                        vars_skipped += 1
                        continue
                    var_size = cu.vars[var]._info.size
                    var_name = var[0]
                    tree.next[var_name] = self._SizeTreeEntry(code_size=0, data_size=var_size)
            # let the user know how many variables could not be parsed
            if vars_skipped:
                iprint("Error while parsing %d variables" % vars_skipped)
        return self.__size_tree

    def size_report(self, filter=[], report=False):
        """
        Reports the code and data sizes of the sub-tree indicated by the
        optional filter. Filtering is done by CU path. When no filtering is used
        the report covers the whole ELF file.
        
        :param filter: This should be a path in string or list form. Only
        leading sub-paths are supported with no trailing slashes or backslashes.
        :returns: Code and data sizes report along with first level of details.
        """
        # First, convert a string filter to a list and fiddle single
        # slash/backslash paths to look like the CU path representation
        if type(filter) == str:
            filter = path_split(filter)
        if len(filter) == 2 and filter[0] == "" and filter[1] == "":
            filter = filter[:-1]
        # Find the parent path and sub-tree root node.
        parent = []
        parent_cu_tree_level = self._size_tree.next
        while True:
            if parent == filter:
                break
            found = False
            for cu_tree_level_name in parent_cu_tree_level:
                if cu_tree_level_name == filter[len(parent)]:
                    parent.append(cu_tree_level_name)
                    found = True
                    break
            if not found:
                raise ValueError("Filter does not match")
            parent_cu_tree_level = parent_cu_tree_level[parent[-1]].next
        if len(parent):
            if len(parent) == 1 and parent[0] == "":
                # fiddle single slash/backslash paths to look like the CU path representation
                parent.append("")
            parent = path_join(parent)
        else:
            parent = '-'
        # We now have the sub-tree we need, get the first level of detail
        children = []
        child_cu_tree_level = parent_cu_tree_level
        for child_cu_tree_level_name in child_cu_tree_level:
            child = {'name': [child_cu_tree_level_name]}
            level = child_cu_tree_level[child_cu_tree_level_name]
            while True:
                # If an element of the first level is of length 1 (has only one
                # child node), go deeper
                if len(level.next) == 1:
                    name = list(level.next.keys())[0]
                    child['name'].append(name)
                    level = level.next[name]
                else:
                    break
            child['code_size'] = level.code_size()
            child['data_size'] = level.data_size()
            children.append(child)
        # Sort the children to produce a predictable order (makes it easy to compare reports)
        children = sorted(children, key=lambda t: "".join(t["name"]))
        # Construct two tables; the first shows the first level of detail and the second calculates a total
        headings = [parent, 'Code Size', 'Data Size']
        total_headings = ['Total', 'Code Size', 'Data Size']
        total_row = [parent, 0, 0]
        detail_table = interface.Table(headings)
        for child in children:
            if len(child['name']):
                if len(child['name']) == 1 and child['name'][0] == "":
                    # fiddle single slash/backslash paths to look like the CU path representation
                    child['name'].append("")
                child['name'] = path_join(child['name'])
            row = [
             child['name'], child['code_size'], child['data_size']]
            detail_table.add_row(row)
            total_row[1] += child['code_size']
            total_row[2] += child['data_size']
        total_table = interface.Table(total_headings)
        total_table.add_row(total_row)
        group = interface.Group("Size Report")
        group.append(detail_table)
        group.append(total_table)
        if report is True:
            return group
        TextAdaptor(group, gstrm.iout)

class StandaloneGlobalSymbols(object):
    """
    Implements the (currently implicit) globalSymbols interface based on the
    standalone firmware environment class.
    
    Potential extension: Move inside the StandaloneFirmwareEnv class?
    """
    
    def __init__(self, fw_env):
        
        self._fw_env = fw_env
        
        #We construct a cache of xIDE-friendly symbols on demand (see _list() 
        #below), because it's a slow process.
        
    def searchSymbols(self, section, regex):
        """
        
        """
        if section != "DATA":
            raise ValueError("Non-data symbols not supported yet")
        
        regexc = re.compile(regex)
        
        return [sympair for sympair in self._list if regexc.search(sympair[0])]
                
    
    def symbolsBeforeAddress(self, address):
        pass
        
        

    @property
    def _list(self):
        try:
            self.__list
        except AttributeError:
            iprint("Caching global symbols for xIDE-style search...")
            self.__list = self._create_symbol_list()
        return self.__list

    def _create_symbol_list(self):
        globvars = self._fw_env.globalvars
        cus = self._fw_env.cus

        globals = []
        locals = []
        
        for key in self._fw_env.vars.keys():
            var = self._fw_env.vars[key]
            from csr.dwarf.read_dwarf import Dwarf_Var_Symbol
            if isinstance(var, Dwarf_Var_Symbol):
                iprint(key)
            address = var_address(var)
            var_name = "::".join(key[1:])
            try:
                varinfo = var_info(var)
            except AttributeError:
                varinfo = var
            if var_info(var).is_external:
                globals.append((var_name,address))
            else:
                cu_name = cus.minimal_unique_subkey(key[0]).split(".")[0]
                locals.append((".".join((cu_name,var_name)), address))
                
        return globals + locals + list(self._fw_env.abs.items())
