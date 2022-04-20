############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from ...register_field.register_field import RegisterField, ParentRegisterField, \
    RegisterArray, IllegalRegisterRead
from csr.wheels.bitsandbobs import display_hex, unique_subclass, match, get_bits_string
from csr.dev.model.base_component import BaseComponent
from csr.dev.hw.address_space import AddressSpace
from csr.dev.model.interface import Group, Table
from csr.dev.adaptor.csv_adaptor import CSVAdaptor
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels import gstrm
import re, copy, sys
import csv
import logging
from functools import cmp_to_key

if sys.version_info >= (3, 0):
    def cmp(a, b):
        return (a > b) - (a < b)


class IIOMapInfo (object):
    """\
    IO map meta-data interface.
    
    Potential extension:: index of fields etc should be behind this interface not
    built on it!
    
    Potential extension:: Move into IOMap package
    """
       
    @property 
    def misc_io_values(self):
        """\
        Access dictionary of miscellaneous io map values.
        
        This should only be used to lookup symbolic io values that do not have
        anywhere better to live (e.g. field enums values should be accessed via field
        object returned from lookup_field_info)

        Future:-
        - Expect symbols to be removed from this set as and when better homes become  
        available (i.e. they are no longer miscellaneous!)
        
        Known Uses:-
        - To pick up symbolic field values for use with AdhocBitFields (which are 
        a bit of a hack in lieue of missing meta-data - or a way to patch/extend
        it during import)
        """
        raise PureVirtualError()

    def lookup_field_info(self, field_sym):
        """\
        Lookup register/field meta-data interface (IRegisterFieldInfo)
        """
        raise PureVirtualError()
    
    def filter_io_map_info(self, sym_reg_ex):
        """
        Create a new IIoMapInfo containing all the symbols in this one that
        match the reg ex
        """
        raise PureVirtualError
    
    @property
    def field_records(self):
        """
        Return a reference to the underlying records
        """
        raise PureVirtualError
    
    @property
    def virtual_field_records(self):
        """
        Return a reference to the underlying records
        """
        raise PureVirtualError
    
class BaseIOMapInfo(IIOMapInfo):
    """
    Common implementation details for NoddyIOMapInfo and IoStructIOMapInfo
    """    
    def __init__(self, misc_io_values, layout_info):
        
        self._misc_values = misc_io_values
        self._layout_info = layout_info
    
    @property 
    def misc_io_values(self):
        
        return self._misc_values
    
    def lookup_field_info(self, field_sym):
        
        # Create on the fly - its just a wrapper.
        return self.RegisterFieldInfoType(field_sym, self._field_records, 
                                          self.misc_io_values,
                                          self._layout_info)

    def lookup_array_info(self, array_sym):
        
        # Create on the fly - its just a wrapper.
        return self.RegisterArrayInfoType(array_sym, self._array_records, 
                                          self.misc_io_values,
                                          self._layout_info)

        
    def filter_io_map_info(self, sym_reg_ex,
                           addr_xfm = None,
                           name_xfm = None):
        """
        Return a new instance of this class containing all fields with symbol 
        containing sym_reg_ex.  If specified, a transformation can be applied 
        to each address as well.
        """
        field_records = dict([(symbol, self._field_records[symbol]) \
                                for symbol in list(self._field_records.keys()) \
                                    if re.match(sym_reg_ex, symbol)])
        
        if addr_xfm:
            for key in list(field_records.keys()):
                # field_regs[key] is of class c_reg from io_struct.
                # Just copy the whole object and then transform the address. 
                new_c_reg = copy.copy(field_records[key])
                new_c_reg.addr = addr_xfm(new_c_reg.addr)
                field_records[key] = new_c_reg
                
        if name_xfm:
            field_records = dict([(name_xfm(key),value) \
                                     for (key, value) in list(field_records.items())])
                                    
        misc_values = dict([(symbol, self._misc_values) \
                                for symbol in list(self._misc_values.keys()) \
                                    if re.match(sym_reg_ex, symbol)])

        virt_field_records = dict([(symbol, self._virtual_field_records[symbol]) \
                                for symbol in list(self._virtual_field_records.keys()) \
                                    if re.match(sym_reg_ex, symbol)])
        
        # IoStructIOMapInfo(io_struct, misc_io_values, layout_info,
        #                    field_records, virt_field_records)
        return self.__class__(None, misc_values, self._layout_info,
                              field_records, virt_field_records)
        
    @property
    def field_records(self):
        return self._field_records
        
    @property
    def array_records(self):
        return self._array_records
        
    @property
    def virtual_field_records(self):
        return self._virtual_field_records

        
# Helper classes
    
class FieldValueDict (BaseComponent):
    """\
    Dictionary of register field values by symbolic name.        
    Known Uses:-
    - Core.fields
    """
    def __init__(self, field_ref_dict, array_dict):
        """\
        Params:-
        - field_ref_dict: A FieldRefDict used to lookup field references 
        by name.
        """
        self._flag_new_attributes = False
        
        self._field_ref_dict = field_ref_dict
        self._array_dict = array_dict
        self.verify_write = False

        def _getter_factory(reg_name):
            def get(managed_self):
                return self._field_ref_dict[reg_name]
            return get
        def _setter_factory(reg_name):
            def set(managed_self, value):
                self._field_ref_dict[reg_name].write(value)
            return set

        for reg in field_ref_dict.keys():
            
            setattr(type(self), reg, property(fget=_getter_factory(reg),
                                              fset=_setter_factory(reg)))

        def _array_getter_factory(regarray_name):
            def get(managed_self):
                return self._array_dict[regarray_name]
            return get

        for regarray in array_dict.keys():
            
            setattr(type(self), regarray, property(fget=_array_getter_factory(regarray)))

        self._flag_new_attributes = True

    def __setattr__(self, name, value):
        # Allow BaseComponent.logger to create a _logger attribute
        if (name not in ("_flag_new_attributes", "_logger") and
            self._flag_new_attributes and not hasattr(self, name)):
            raise AttributeError("'%s' is not a register name" % name)
        else:
            super(FieldValueDict, self).__setattr__(name, value)

    @display_hex
    def __getitem__(self, field_name):
        
        # Use temporary reference for access
        return self._field_ref_dict[field_name].read()

    def __setitem__(self, field_name, value):
        
        # Use temporary reference for access
        self._field_ref_dict[field_name].write(value)

    def set_defaults(self, set_zero_values=False):
        """
        Loop over the registers writing non-zero reset values (or optionally,
        all of them) into the registers.
        If the register's address isn't in the addressing model, silently
        ignore it and carry on.
        """
        for reg in self._field_ref_dict.keys():
            try:
                reg = self._field_ref_dict[reg] 
                if reg.info.reset_value is not None and (reg.info.reset_value != 0 or set_zero_values):
                    reg.set_default()
            except AddressSpace.NoAccess:
                pass

    def dump(self, csv_path="reg_dump.csv", filter=None, ignore_read_errors=None):

        """
        Register dumping method. Iterates over all register and reads theirs values, then
        extracts bitfield values from the acquired register value.
        The filtering is not applied on bitfield names.

        :param csv_path: path to the CSV file which will be written out
        :param filter: a regular expression used to limit number of dumped registers.
           In order to exclude certain register from the dump process use
           such regular expression:
              dut.bt.reg_dump(filter="^(?!HZ_WATCHDOG_|SQIF_)")
           Method will dump all registers except those having HZ_WATCHDOG_ or
           SQIF_ at the beginning of the register name.
           In order to dump only certain registers use such filter:
              dut.bt.reg_dump(filter="CHIPMATE_WRITE_")
           It will dump only registers having CHIPMATE_WRITE_ at the beginning of
           the register name.
        :param ignore_read_errors: Keep going even if read transaction errors 
          are encountered.  Defaults to True if no filter is supplied, else False.
        :return: register information at the moment of creating the dump returned as Group() object
        """
    
        if ignore_read_errors is None:
            ignore_read_errors = (filter is None)

        def bitz_engine(reg, desc_width=None, value=None):
            """
            Construct a report of the given register's current bit values, returned as a table.
            """
            if value is None:
                value = reg.read()

            perms = ""
            if reg.is_readable:
                perms += "R"
            if reg.is_writeable:
                perms += "W"

            t = Table()
            # For registers with children, breakdown the register's value into subfield values
            def cmp_func(a1_a2, b1_b2):
                return cmp(a1_a2[1].start_bit, b1_b2[1].start_bit)
            if reg.info.children:
                children_info = list(reg.info.children.items())
                children_info = sorted(children_info,
                                       key=cmp_to_key(cmp_func))
                for child_name, child_info in children_info:
                    if value is NotImplemented:
                        value_fmt = "!!! Read failed !!!"
                    elif value is False:
                        value_fmt = "!!! Write only !!!"
                    else:
                        value_fmt = "0x%x" % ((value & child_info.mask) >> child_info.start_bit)
                    row = [reg.info.name, child_name, get_bits_string(child_info),
                           hex(reg.info.start_addr),
                           perms,
                           value_fmt]
                    if desc_width is not None:
                        row.append(child_info.description[0:desc_width])
                    t.add_row(row)
            else:
                if value is NotImplemented:
                    value_fmt = "!!! Read failed !!!"
                elif value is False:
                    value_fmt = "!!! Write only !!!"
                else:
                    value_fmt = "0x%x" % value
                # get register information
                row = [reg.info.name, "-", get_bits_string(reg.info),
                       hex(reg.info.start_addr),
                       perms,
                       value_fmt]
                if desc_width is not None:
                    row.append(reg.info.description[0:desc_width])
                t.add_row(row)

            return t

        reg_dump = Group()
        for reg_name in self._field_ref_dict.keys():

            # Skip the register if it was matched by one of the patterns
            if filter and match(reg_name, [filter]) is False:
                continue

            try:
                reg_obj = self._field_ref_dict[reg_name]
                if not reg_obj.is_readable:
                    continue
                reg_dump.append(bitz_engine(reg=reg_obj))
            except AddressSpace.NoAccess as e:
                if ignore_read_errors:
                    reg_dump.append(bitz_engine(reg=reg_obj, value=NotImplemented))
            except IllegalRegisterRead:
                if ignore_read_errors:
                    reg_dump.append(bitz_engine(reg=reg_obj, value=False))
            except AddressSpace.ReadFailure as e:
                if not ignore_read_errors:
                    raise AddressSpace.ReadFailure("Error while reading %s "
                                                   "register. "
                                                   "Solution might be "
                                                   "filtering it out from the "
                                                   "register dump. %s" %
                                                   (reg_name, e))
                reg_dump.append(bitz_engine(reg=reg_obj, value=NotImplemented))

        for array_reg_name in self._array_dict.keys():

            # Skip the register if it was matched by one of the patterns
            if filter and match(array_reg_name, [filter]) is False:
                continue
            
            array_reg_obj = self._array_dict[array_reg_name]            
            for array_index in range(array_reg_obj._array_info.num_elements):
                try:
                    reg_dump.append(bitz_engine(reg=array_reg_obj[array_index]))
                except AddressSpace.NoAccess as e:
                    if ignore_read_errors:
                        reg_dump.append(bitz_engine(reg=array_reg_obj[array_index], 
                                                    value=NotImplemented))
                except IllegalRegisterRead:
                    if ignore_read_errors:
                        reg_dump.append(bitz_engine(reg=array_reg_obj[array_index], value=False))
                except AddressSpace.ReadFailure as e:
                    if not ignore_read_errors:
                        raise AddressSpace.ReadFailure("Error while reading %s "
                                                       "register. "
                                                       "Solution might be "
                                                       "filtering it out from the "
                                                       "register dump. %s\[%d\]" %
                                                       (array_reg_name, array_index, e))
                    reg_dump.append(bitz_engine(reg=array_reg_obj[array_index], 
                                                value=NotImplemented))

        if csv_path:
            CSVAdaptor(reg_dump, csv_path)
        else:
            TextAdaptor(reg_dump, gstrm.iout)

        return reg_dump

    def restore(self, csv_path="reg_dump.csv", filter=None):

        """
        @brief: Method restoring register and bitfields values from a CSV file.
                The CSV file must be compatible with the one created by the dump method.

        @param csv_path: path to the CSV file containing values to be restored
        @param filter: a regular expression used to limit the number of restored registers.
                       The filtering is not applied on bitfield names.

                       In order to exclude certain register from being restored use
                       such regular expression:
                          dut.bt.reg_dump(filter="^(?!HZ_WATCHDOG_|SQIF_)")
                       Method will restore all registers except those having HZ_WATCHDOG_ or
                       SQIF_ at the beginning of the register name.

                       In order to only restore certain registers use such filter:
                          dut.bt.reg_dump(filter="CHIPMATE_WRITE_")
                       It will restore only registers having CHIPMATE_WRITE_ at the beginning
                       of the register name.

        @return: N/A
        """

        with open(csv_path, 'rb') as csv_file:
            csv_reader = csv.reader(csv_file, delimiter=',')

            for row in csv_reader:
                reg_name = row[0]
                field_name = row[1]
                value = int(row[5], base=16)

                # skip the register if user has decided so by setting filtering
                if filter and match(reg_name, [filter]) is False:
                    continue

                try:
                    reg_obj = self._field_ref_dict[reg_name]

                    if reg_obj.is_writeable:

                        if reg_obj.info.children:

                            if field_name == "-":
                                raise Exception("Trying to restore bitfield but the dump "
                                                    "doesn't contain data for it")

                            # restore a bitfield
                            bitfield_obj = getattr(reg_obj, field_name)
                            bitfield_obj.write(value)
                        else:

                            if field_name != "-":
                                raise Exception("Trying to restore register but the data "
                                                    "is for a bitfield")

                            # restore a register
                            reg_obj.write(value)

                except AddressSpace.NoAccess:
                    # ignore NoAccess errors, however raise any other
                    pass


class BitfieldValueDict(BaseComponent):
    """
    Container of bitfields referenced directly rather than as attributes of the 
    parent register.  Any duplicated names are discarded at construction.
    """
    def __init__(self, field_ref_dict, array_ref_dict):
        
        self._flag_new_attributes = False
        
        duplicates = set()
        for reg in field_ref_dict.keys():
            reg_dupls = field_ref_dict[reg].set_child_properties(type(self))
            duplicates.update(reg_dupls)
        for array in list(array_ref_dict.keys()):
            array_dupls = array_ref_dict[array].set_child_properties(type(self))
            duplicates.update(array_dupls)
        # Remove any subfield names that appear in multiple places
        for name in duplicates:
            delattr(type(self), name)
            
        self._flag_new_attributes = True

    def __getattr__(self, name):
        """
        Simulate hal getter and setter function syntax
        """
        if name.startswith("hal_get"):
            # Magic up a getter function
            field_name = name[8:].upper()
            return getattr(self, field_name).read
        elif name.startswith("hal_set"):
            # Magic up a setter function
            field_name = name[8:].upper()
            return getattr(self, field_name).write
        else:
            # Let the base class deal with it
            return getattr(super(BitfieldValueDict, self), name)
        
    def __setattr__(self, name, value):
        # Allow BaseComponent.logger to create a _logger attribute
        if (name not in ("_flag_new_attributes","_logger") and
            self._flag_new_attributes and not hasattr(self, name)):
            raise AttributeError("'%s' is not a bitfield name (or is an "
                                 "ambiguous one)" % name)
        else:
            super(BitfieldValueDict, self).__setattr__(name, value)
        

class FieldRefDict (object):
    """\
    Dictionary of register field references by symbolic name.
    Known Uses:-
    - Core.field_refs
    - FieldValueDict
    
    Potential extension:: Move to CoreInfo
    """
    
    def __init__(self, io_map_info, core_or_data_space,
                 bad_ret_val_name=None):

        """\
        Params:-
        - info: Core meta-data (ICoreInfo) including Map of misc 
        symbols => values. This is passed in
        lieue of proper Enum value meta-data - allows _any_ symbolic value
        to be used as a field value (convenience > safety).
        """
        self._io_map_info = io_map_info
        self._data = core_or_data_space
        self._bad_ret_val_name = bad_ret_val_name

    @property
    def _bad_ret_val_reg(self):
        try:
            self.__bad_ret_val_reg
        except AttributeError:
            if self._bad_ret_val_name is not None:
                self.__bad_ret_val_reg = self[self._bad_ret_val_name]
            else:
                self.__bad_ret_val_reg = None
        return self.__bad_ret_val_reg

    def __getitem__(self, field_sym):

        # Create field meta data interface on the fly 
        # (its just a wrapper on raw tuple(s))
        #
        # OPTIM: cache if we ever have lots of instances of the same device
        # to deal with.
        #
        field_info = self._io_map_info.lookup_field_info(field_sym)
        register_field_type = RegisterField if field_info.parent is not None else ParentRegisterField            
        subtype, type_ind = unique_subclass(register_field_type, 
                                            id_hint=register_field_type.next_ind)
        register_field_type.next_ind = type_ind + 1
        if field_sym != self._bad_ret_val_name:
            bad_read_reg = self._bad_ret_val_reg
        else:
            bad_read_reg = None
        return subtype(field_info, self._data, bad_read_reg=bad_read_reg)

    def keys(self):
        """
        Implement the standard dictionary keys method by forwarding to the
        underlying real dictionary.
        """
        return self._io_map_info.field_records.keys()


class FieldArrayRefDict(object):
    
    next_ind = 0
    
    def __init__(self, io_map_info, core_or_data_space,
                 bad_ret_val_name=None):

        """\
        Params:-
        - info: Core meta-data (ICoreInfo) including Map of misc 
        symbols => values. This is passed in
        lieue of proper Enum value meta-data - allows _any_ symbolic value
        to be used as a field value (convenience > safety).
        """
        self._io_map_info = io_map_info
        self._data = core_or_data_space

    def __getitem__(self, array_sym):
        array_info = self._io_map_info.lookup_array_info(array_sym)
        subtype, type_ind = unique_subclass(RegisterArray, 
                                            id_hint=FieldArrayRefDict.next_ind)
        FieldArrayRefDict.next_ind = type_ind + 1
        return subtype(array_info, self._data)

    def keys(self):
        return list(self._io_map_info.array_records.keys())


def get_fields_obj(io_map_info, data):
    """
    Construct a "fields" type object based on a raw data space object and an io_map_info
    """

    field_refs = FieldRefDict(io_map_info, data)
    field_array_refs = FieldArrayRefDict(io_map_info, data)
    return unique_subclass(FieldValueDict)(field_refs, field_array_refs)


