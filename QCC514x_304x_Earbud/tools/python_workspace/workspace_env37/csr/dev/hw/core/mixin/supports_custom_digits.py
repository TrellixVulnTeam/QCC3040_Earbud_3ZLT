############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint, wprint
from csr.wheels.importer import import_source_module_from_path, import_source_file_as_module
from csr.dev.hw.core.meta.io_struct_io_map_info import find_matching_reg_files_in_tree,\
NonUniqueIostructNameFragment
import os
import sys
import importlib
import re

class SupportsCustomDigits(object):
    """
    Mixin for CoreInfo classes that support specifying custom digits via
    chip.emulator_build
    
    Subclasses that want to find io_structs from a CSR-style digital_results
    directory need to specify the relevant subsystem name as it appears in the
    subsystem subdirectory name in the digital_results file tree.  They do this
    via a class attribute DIGITS_SS_NAME.
    
    Subclasses that want to find uniquely named io_structs from within an 
    arbitrary file tree need to specify an iterable of regular expressions to
    search for in the names of io_struct files in the file tree.  They do this 
    via a class attribute IO_STRUCT_NAME_PATTERNS.  The values returned by the
    iterable should be strings rather than regular expression objects.  Normally
    the attribute would be a tuple. 
    
    There should be exactly one match produced; if there is any other number, 
    warnings will be printed and the custom_io_struct property will give None.    
    """
    def __init__(self, custom_digits=None, custom_digits_module=None):
        if sum(arg is not None for arg in (custom_digits, custom_digits_module)) > 1:
            raise TypeError("SupportsCustomDigits takes at most one of " 
            "'custom_digits' and 'custom_digits_module' as argument")
        if custom_digits:
            # We either have a path to the io_struct file or a path to a CSR-style
            # digital_results filetree or a (set of) root path(s) below which we should 
            # search for files matching a particular basename.
            if not isinstance(custom_digits, str):
                custom_digits_list = custom_digits
                if len(custom_digits) != 1:
                    multipath = True
                else:
                    multipath = False
                    custom_digits = custom_digits[0]
            else:
                multipath = False
                custom_digits_list = [custom_digits]
            
            if not multipath and os.path.isfile(custom_digits):
                # Case 1: we've been supplied an io_struct file directly
                self._custom_digits_path = os.path.dirname(custom_digits)
                self._custom_digits_module,_ = os.path.splitext(os.path.basename(custom_digits))
            elif not multipath and hasattr(self, "DIGITS_SS_NAME") and os.path.isdir(os.path.join(custom_digits, self.DIGITS_SS_NAME, "regs", "python")):
                # If it's a digital_results area use our
                # knowledge of how this is laid out to find the io_struct we want
                self._custom_digits_path = os.path.join(custom_digits,
                                                      self.DIGITS_SS_NAME, "regs", 
                                                      "python")
                self._custom_digits_module = "io_struct"
                io_struct_file = os.path.join(self._custom_digits_path, 
                                              self._custom_digits_module) + ".py"
                # Did we get it right?
                if not os.path.isfile(io_struct_file):
                    # If not, try to figure out whether the path was just wrong,
                    # or whether it was a valid path but there wasn't an io_struct
                    # below it where we were expecting.
                    if not os.path.exists(custom_digits):
                        wprint ("{} does not exist.  Falling back to checked-in "
                                "io_struct (if possible)".format(custom_digits))
                    else:
                        wprint ("No such io_struct file '%s'. Falling back to checked-in "
                               "io_struct" % io_struct_file)
                    # Either way we don't have a valid path to an io_struct.
                    self._custom_digits_path = None
            else:
                # Otherwise simply walk the file tree(s) looking for files ending with
                # io_struct.py which also contain the pattern(s) defined by the
                # mixed-in class
                self._custom_digits_module = None
                matching = []
                for custom_digits in custom_digits_list:
                    matching += find_matching_reg_files_in_tree(custom_digits, self.IO_STRUCT_NAME_PATTERNS,
                                                                 require_unique=False, extn="io_struct.py")
                if len(matching) > 1:
                    # Error - Multiple matches
                    wprint ("Multiple io_struct files matching pattern '{}'. Falling back to checked-in "
                            "io_struct (if possible)".format(self.IO_STRUCT_NAME_PATTERNS))
                    self._custom_digits_path = None
                elif len(matching) == 0:
                    # No matches
                    if all(not os.path.exists(custom_digits) for custom_digits in custom_digits_list):
                        wprint ("{} does/do not exist.  Falling back to checked-in "
                                "io_struct (if possible)".format(", ".join(custom_digits_list)))
                    else:
                        try:
                            self.FLAT_NAME_PATTERNS
                        except AttributeError:
                            wprint ("No io_struct file found matching pattern '{}'. Falling back to checked-in "
                                    "io_struct (if possible)".format(self.IO_STRUCT_NAME_PATTERNS))
                        else:
                            for custom_digits in custom_digits_list:
                                matching += find_matching_reg_files_in_tree(custom_digits, self.FLAT_NAME_PATTERNS,
                                                                            extn=".FLAT")
                            if len(matching) > 1:
                                # Error - Multiple matches
                                wprint ("Multiple FLAT files matching pattern '{}'".format(self.FLAT_NAME_PATTERNS))
                                self._custom_digits_path = None
                            elif len(matching) == 0:
                                self._custom_digits_path = None
                                wprint ("No FLAT files matching pattern '{}'".format(self.FLAT_NAME_PATTERNS))
                            else:
                                self._custom_digits_module = os.path.basename(matching[0])
                                self._custom_digits_path = os.path.dirname(matching[0])
                else:
                    # Found exactly one match
                    self._custom_digits_module = os.path.basename(matching[0])[:-3] # leave off the .py extension
                    self._custom_digits_path = os.path.dirname(matching[0])

            # We aren't loading via a package in this case
            self._custom_digits_package = None
        else:
            # We've been supplied a module name.  We assume this will be found
            # in an installed namespace package called qcom_iostructs.
            # (Also covers the case where custom_digits and custom_digits_module
            # are both None)
            self._custom_digits_path = None
            self._custom_digits_package = "qcom_iostructs"
            self._custom_digits_module = custom_digits_module
            

    @property
    def custom_io_struct(self):
        try:
            self._custom_io_struct
        except AttributeError:
            if self._custom_digits_path:
                # Use the path to the io_struct's containing directory and
                # its actual name (as a module) to load it.
                if not self._custom_digits_module.endswith(".FLAT"):
                    self._custom_io_struct = import_source_module_from_path(
                                                        self._custom_digits_path, 
                                                        self._custom_digits_module,
                                                        allow_replacement=True)
                else:
                    raise NotImplementedError("FLAT file support not implemented")
            elif self._custom_digits_module:
                try:
                    # Try to load from the qcom_iostructs package using the
                    # name we were given
                    self._custom_io_struct = importlib.import_module(self._custom_digits_module,
                                                                     package=self._custom_digits_package)
                except ImportError:
                    self._custom_io_struct = None
            else:
                self._custom_io_struct = None
        return self._custom_io_struct


class SupportsMultifileCustomDigits(object):
    """
    Mixin for the more complicated case where a given core's view of registers
    is described across more than one file  
    
    In this case the input is expected to be a list of multiple paths,
    from which we select the modules that the subclass is interested in using
    the supplied list of module names, and if necessary a function to map from
    the real file name on chip to the canonical module name
    
    """
    
    def __init__(self, custom_digits, custom_digits_modules):
        
        if custom_digits is None:
            # We need to import everything from the qcom_iostructs package
            self._custom_digits_modules = custom_digits_modules
            self._custom_digits_package = "qcom_iostructs"
        
        else:
            self._custom_digits_modules = sum(
                (find_matching_reg_files_in_tree(dir, 
                                                  self.IO_STRUCT_NAME_PATTERNS,
                                                  require_unique=False, extn="io_struct.py") 
                                              for dir in custom_digits), [])
            if not self._custom_digits_modules:
                if len(self.IO_STRUCT_NAME_PATTERNS) == 1:
                    wprint("No io_struct files matching the regexp '{}' found in {}"
                           .format(", ".join(self.IO_STRUCT_NAME_PATTERNS), ", ".join(custom_digits)))
                else:
                    wprint("No io_struct files matching any of the regexps {} found in {}"
                           .format(", ".join(self.IO_STRUCT_NAME_PATTERNS), ", ".join(custom_digits)))
            self._custom_digits_package = None
            
    @property
    def custom_io_structs(self):
        
        try:
            self._custom_io_structs
        except AttributeError:
        
            if self._custom_digits_package is not None:
                
                self._custom_io_structs = set(
                    importlib.import_module(cust_dig,
                                            package=self._custom_digits_package) 
                            for cust_dig in self._custom_digits_modules)

            else:
                self._custom_io_structs = set()
                
                for cust_dig in self._custom_digits_modules:
                    mod = import_source_file_as_module(cust_dig, allow_replacement=True)
                    if mod is None:
                        raise ImportError("Failed to import '{}'".format(cust_dig))
                    self._custom_io_structs.add(mod)
                
        return self._custom_io_structs
        