# Copyright (c) 2016-2020 Qualcomm Technologies International, Ltd.
#   %%version
from .i_io_map_info import BaseIOMapInfo
import re
try:
    from collections.abc import Iterable
except ImportError:
    from collections import Iterable
import os
import re

from ...register_field.meta.io_struct_register_field_info import IoStructRegisterFieldInfo,\
IoStructRegisterArrayInfo

def find_reg_files_in_tree(root, extn="io_struct.py"):
    """
    Retrieve a list of all the io_struct files below a given directory root
    (also works with a single file)
    """
    if os.path.isfile(root) and root.endswith(extn):
        return [root]
    
    io_structs = []
    for pth, dirs, files in os.walk(root):
        
        io_structs += [os.path.join(pth, f) for f in files if f.endswith(extn)]
        
    return io_structs

class NonUniqueIostructNameFragment(ValueError):
    pass

def find_matching_reg_files_in_tree(root, name_map, require_unique=True, extn="io_struct.py"):
    
    io_structs = find_reg_files_in_tree(root, extn=extn)
    
    def find_matches(name_pat):
        matches = []
        for io_struct in io_structs:
            io_struct_name = os.path.basename(io_struct)
            if re.search(name_pat, io_struct_name):
                if matches and require_unique:
                    raise NonUniqueIostructNameFragment("Multiple matches found for '{}'".format(name_pat))
                matches.append(io_struct)
        return matches
    
    try:
        name_map.items
    except AttributeError:
        # It's just a plain iterable, so we don't need to indicate which match
        # came from which item
        matches = []
        for name_pat in name_map:
            matches += find_matches(name_pat)
    else:
        # It's a mapping, so store the matches against the key
        matches = {}
        for key, name_pat in name_map.items():
            matches[key] = find_matches(name_pat)
    return matches
    
    
    


class RegistersUnavailable(AttributeError):
    """\
    Register field definitions unavailable in pylib environment.
    An io_struct file is needed.
    """

class c_virtual_reg(object):
    """
    Virtual register class. We need to get digits to add this to their python deliverable.
    """
    pass

class IoStructIOMapInfo (BaseIOMapInfo):
    """\
    Implementation of IIOMapInfo using the Digits' structured register info stuff

    See __init__() for details.

    """
    RegisterFieldInfoType = IoStructRegisterFieldInfo
    RegisterArrayInfoType = IoStructRegisterArrayInfo

    def __init__(self, io_struct, misc_io_values, layout_info,
                 field_records = None, virtual_field_records = None):
        """\
        Params:-
        - io_struct: Module containing register and register field objects

        - misc_values:  Dictionary of misc symbols values.
        Includes field enums.
        ExcludesField lsb, msb & mask

        symbol => value
        """
        if io_struct is not None and not isinstance(io_struct, Iterable):
            io_struct_iter = [io_struct]
        else:
            io_struct_iter = io_struct

        if misc_io_values is None:
            # Emulate the dictionary that is scraped out of io_map.asm by the
            # Curator and Apps register import scripts if necessaryx
            misc_io_values = self._MiscIoValueDict(io_struct_iter)
        BaseIOMapInfo.__init__(self, misc_io_values, layout_info)

        if io_struct_iter is not None:
            self._field_records, self._array_records, self._virtual_field_records = \
                                           self._build_io_struct_dict(io_struct_iter)

        if field_records is not None:
            self._field_records = field_records
        if virtual_field_records is not None:
            self._virtual_field_records = virtual_field_records

    def _build_io_struct_dict(self, io_struct_iter):
        """
        Insert registers and register fields into the same dictionary.  There's
        no reason to treat them as belonging to separate namespaces since they're
        all carefully given unique names.
        """
        reg_field_dict = {}
        reg_array_dict = {}
        virtual_reg_field_dict = {}
        pad_idx = set()

        for io_struct in io_struct_iter:
            # Loop through all the register objects in the io_struct module
            for name in dir(io_struct):
                entry = getattr(io_struct, name)
                if isinstance(entry, io_struct.c_reg) and entry.addr is not None:
                    # Insert the register by name
                    reg_field_dict[name] = entry
    
                    # Some versions of io_struct don't contain parent info
                    # in the subfields, so we'll insert it here
                    for subname, subfield in entry.__dict__.items():
                        if hasattr(subfield, "lsb"):
                            if not hasattr(subfield, "parent"):
                                subfield.parent = entry
    
                elif hasattr(io_struct, "c_regarray") and isinstance(entry, io_struct.c_regarray):
                    reg_array_dict[name] = entry
    
                elif isinstance(entry, io_struct.c_enum):
                    if entry.reg not in virtual_reg_field_dict:
                        virtual_reg_field_dict[entry.reg] = c_virtual_reg()
                    if "_PAD_IDX" in name:
                    # Add another entry if we have processed this pad name before
                        if entry.value not in pad_idx:
                            pad_idx.add(entry.value)
                        else:
                            virtual_reg_field_dict[name] = entry
                    setattr(virtual_reg_field_dict[entry.reg], name, entry)
        return reg_field_dict, reg_array_dict, virtual_reg_field_dict

    class _MiscIoValueDict(object):
        """
        Emulate a dictionary of misc IO values by lumping all the objects in
        the io_struct module that have a "value" attribute, and all the 
        subfields of each c_reg object with a "value" attribute, into a dictionary.
        If names are duplicated the latest one wins.
        """
        def __init__(self, io_struct_iter):
            self._cache = {}
            for io_struct in io_struct_iter:
                if io_struct is None:
                    raise RegistersUnavailable
                for name in dir(io_struct):
                    entry = getattr(io_struct, name)
                    if isinstance(entry, io_struct.c_reg):
                        for subname, subfield in entry.__dict__.items():
                            try:
                                self._cache[subname] = subfield.value
                                # legacy register enum references have _ENUM on the
                                # end
                                if isinstance(subfield, io_struct.c_enum):
                                    self._cache[subname+"_ENUM"] = subfield.value
                            except AttributeError:
                                pass
                    else:
                        try:
                            self._cache[name] = entry.value
                            if isinstance(entry, io_struct.c_enum):
                                self._cache[name+"_ENUM"] = entry.value
                        except AttributeError:
                            pass
                    

        def __getitem__(self, value_name):
            """
            Look up the given enum/value name in the enum/value dictionary
            """
            # Remove any leading "$"
            value_name_str = value_name.replace("$","")
            if value_name_str in self._cache:
                return self._cache[value_name_str]
            raise KeyError("No register enum/value '%s' found" % value_name)

        def items(self):
            return iter(self._cache.items())

        def keys(self):
            for k, v in self.items():
                yield k

        def values(self):
            for k, v in self.items():
                yield v
