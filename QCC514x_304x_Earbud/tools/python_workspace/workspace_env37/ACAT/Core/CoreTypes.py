############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Global Types.

This module is used to hold all the global types in ACAT.
"""
import logging
import numbers
import math

from ACAT.Core import Arch
from ACAT.Core.exceptions import (
    InvalidDebuginfoTypeError, InvalidDebuginfoEnumError,
    VariableNotPointerError, VariableMemberMissing
)

try:
    from future_builtins import hex
except ImportError:
    pass

##########################################################################
#                               Globals
##########################################################################
logger = logging.getLogger(__name__)

#############
# Data types
#############


class ConstSym(object):
    """A symbolic constant.

    e.g. $sbc.mem.ENC_SETTING_BITPOOL_FIELD or $_REGFILE_PC.

    Note:
        That register names are constants, and their values are the
        address of the register. (If the contents of register is also
        known then it should be wrapped as a DataSym instead.)

    Args:
        name
        value
    """

    def __init__(self, name, value):
        self.name = name
        self.value = value

    def __str__(self):
        return (
            'Name: ' + str(self.name) + '\n' + 'Value: ' + hex(self.value) +
            '\n'
        )

    def __repr__(self, *args, **kwargs):
        return str(self)


class DataSym(object):
    """A data symbol (register, dm_const entry, variable).

    Args:
        name
        address
        value
    """

    def __init__(self, name, address, value=None):
        self.name = name
        self.address = address  # Stored as an integer
        self.value = value  # Integer; won't necessarily know this.

    def __str__(self):
        # Special rule since you can't call hex() on None
        if self.address is not None:
            hex_addr = hex(self.address)
        else:
            hex_addr = 'None'

        return ('Name: ' + str(self.name) + '\n' + 'Address: ' +
                hex_addr + '\n' + 'Value: ' + self._value_to_hex() + '\n')

    def _value_to_hex(self):
        """Converts the value into hex and return it.

        The value of the Variable might be in different types. This method
        supports the conversion when the type is in Integer, list or tuple
        of Integers and None.

        When the value is a list of integers, each number will be
        converted into hex. Also, in case the value is None, the `None`
        string will be returned.

        Returns:
            str: A string representation of the Variable's value.
        """
        if isinstance(self.value, numbers.Integral):
            display_value = hex(self.value)

        elif isinstance(self.value, (list, tuple)):
            display_value = str([hex(val) for val in self.value])

        else:
            # Conversion to hex was unsuccessful.
            display_value = str(self.value)

        return display_value


class Variable(DataSym):
    """A debuginfo variable.

    Variables are more complicated than a standard data symbol, since they
    have a size and can be structures.

    'value' can be a single integer or (more likely) a list/tuple. For
    debuginfo variables the value represents the initial value which
    could be uninitialised.

    Args:
        name
        address
        size
        value
        var_type
        debuginfo
        members
        parent
    """
    integer_names = [
        "uint32",
        "uint24",
        "uint16",
        "uint8",
        "int",
        "unsigned",
        "unsigned int"
    ]

    def __init__(
            self,
            name,
            address,
            size,
            value=None,
            var_type=None,
            debuginfo=None,
            members=None,
            parent=None
    ):
        DataSym.__init__(self, name, address, value)
        self.size = size
        self.type = var_type  # type ID; meaningless to users

        # list of members (Variable objects).
        # These can be struct/union members, or array elements
        if members is None:
            self.members = []
        else:
            self.members = members
        self.parent = parent  # The Variable which owns this one (if any).
        # if non-zero, variable is an array of this length
        # Note: some types define an array of length 0...
        self.array_len = 0
        # e.g. struct foo, or uint16*
        self.type_name = ''
        # name of this struct/union member, without the parent part.
        self.base_name = ''

        # This will be set if the variable is part of a bitfield.
        self.size_bits = None  # Size in bits (self.size will always be '1').
        # Mask that must be ANDed with self.value to yield the correct bitfield
        self.val_mask = None
        # Right-shift that must be applied to self.value to yield the correct
        # bitfield
        self.val_rshift = None

        self.indent = ""  # Used when printing a variable out

        # Debuginfo is used to cast the structure fileds for better display.
        self.debuginfo = debuginfo

    def __repr__(self, *args, **kwargs):
        """String representation.

        The standard representation will be the same as the standard to
        string to make the interactive interpreter more user-friendly.
        """
        return self.var_to_str()

    def __str__(self):
        """The standard to string function."""
        return self.var_to_str()

    def _get_enum_name(self):
        """Returns the enum name based on the value."""
        # check if the debug info is set to get the enum names from
        # there.
        ret_string = ""
        if self.debuginfo is not None:
            enum_value_name = ""
            error_msg = ""
            try:
                # get the enum type name which is after the "enum " word
                # 1- is used as an indexer to avoid index error which
                # in this case is favourable
                enum_type_name = self.type_name.split("enum ")[-1]
                enum_value_name = self.debuginfo.get_enum(
                    enum_type_name,
                    self.value
                )
            except InvalidDebuginfoEnumError:  # The enum type is missing
                # This can happen if an enum type is typdefed. Something like:
                # typedef enum_type new_enum_type; try to dereference
                try:
                    # get the enum type
                    enum_type_id = self.debuginfo.get_type_info(
                        enum_type_name
                    )[1]
                    # get the referenced type from the enum type.
                    enum_type = self.debuginfo.types[enum_type_id]
                    enum_type = self.debuginfo.types[enum_type.ref_type_id]
                    enum_type_name = enum_type.name
                    # Finally, get the enum value name.
                    enum_value_name = self.debuginfo.get_enum(
                        enum_type.name,
                        self.value
                    )
                except (InvalidDebuginfoTypeError,
                        InvalidDebuginfoEnumError):
                    error_msg += (
                        "(enum type \"" + enum_type_name +
                        "\" not found for \"" + self.base_name + "\" member)"
                    )
                except KeyError:  # the enum is missing the values
                    error_msg += (
                        "(enum \"" + enum_type_name +
                        "\" has no value " + self._value_to_hex() + ")"
                    )
            except KeyError:  # the enum is missing the values
                error_msg += (
                    "(enum \"" + enum_type_name +
                    "\" has no value " + self._value_to_hex() + ")"
                )

            if enum_value_name == "":
                enum_value_name = self._value_to_hex() + " " + error_msg
            else:
                if len(enum_value_name) > 1:
                    enum_value_name = sorted(enum_value_name)

                    # There are multiple matches for the values. Display all
                    # of them one after the other.
                    temp_name = ""
                    for value_name in enum_value_name:
                        if temp_name == "":
                            temp_name += "( "
                        elif value_name == enum_value_name[-1]:
                            temp_name += " and "
                        else:
                            temp_name += ", "
                        temp_name += value_name
                    temp_name += " have value " + self._value_to_hex()
                    temp_name += " in " + enum_type_name + ")"
                    enum_value_name = self._value_to_hex() + " " + temp_name
                else:
                    # get_enum panics if there are no matches so we are
                    # sure that enum_value_name has at lest one element.
                    temp_name = enum_value_name[0]
                    enum_value_name = temp_name + " " + self._value_to_hex()
            # concatenate the return string.
            ret_string += self.base_name + ": " + enum_value_name + "\n"
        elif self.base_name != "":
            # Just display the base name and value.
            ret_string += self.base_name + ": " + self._value_to_hex() + "\n"
        else:
            # Display the value and name.
            ret_string += self.name + ": " + self._value_to_hex() + "\n"
        return ret_string

    def var_to_str(self, depth=0):
        """Converts a structure to a base_name: value string.

        Args:
            depth (int, optional)
        """
        depth_str = "  " * depth
        fv_str = ""

        if depth == 0:
            fv_str += "0x%08x " % self.address

        if self.members:
            if self.base_name == "":
                # Probably it was a pointer to something
                fv_str += (depth_str + self.name + ":\n")
            else:
                fv_str += (depth_str + self.base_name + ":\n")
            for member in self.members:
                fv_str += member.var_to_str(depth + 1)
        else:
            part_of_array = False
            if self.parent and \
                    isinstance(self.parent.array_len, int):
                # this member is an element of an array.
                part_of_array = True
                fv_str += (
                    depth_str + "[" + str(self.parent.members.index(self)) +
                    "]"
                )
                # no need to add additional depth string.
                depth_str = ""

            if self.type_name in self.integer_names:
                # display integers in hex
                if not part_of_array:
                    fv_str += (depth_str + self.base_name)
                fv_str += (": " + self._value_to_hex() + "\n")
            elif "bool" in self.type_name:
                # booleans are displayed as true (1) or false (0).
                fv_str += (depth_str + self.base_name + ": ")
                if self.value != 0:
                    fv_str += ("True\n")
                else:
                    fv_str += ("False\n")
            elif "enum " in self.type_name:
                fv_str += depth_str + self._get_enum_name()
            else:
                # This is probably a pointer to something.
                if not part_of_array:
                    if self.base_name != "":
                        fv_str += (depth_str + self.base_name)
                    else:
                        fv_str += (depth_str + self.name)

                try:
                    fv_str += (
                        ": ({0}) {1} \n".format(
                            self.debuginfo.get_type_name(self.type),
                            self._value_to_hex()
                        )
                    )
                except (InvalidDebuginfoTypeError, AttributeError):
                    # AttributeError: When ``self.debuginfo`` is None.
                    # InvalidDebuginfoTypeError: Type ID (``self.type``) is not
                    # corresponding to a valid type.
                    fv_str += (": " + self._value_to_hex() + "\n")

        return fv_str

    def set_debuginfo(self, debuginfo):
        """Sets the debug information for the variable.

        The debuginfo set will be used to better display the variable.

        Args:
            debuginfo: Debug information which will be used to get type
                and enum information.
        """
        self.debuginfo = debuginfo

    def __getitem__(self, key):
        if self.array_len > 0:
            item = self.members[key]
            # Politeness: if .value is an array of length 1, turn it into a
            # simple int.
            if item.value and \
                    not isinstance(item.value, numbers.Integral) and \
                    len(item.value) == 1:
                item.value = item.value[0]
            return self.members[key]

        return None

    def get_member(self, name):
        """Gets a variable member by name.

        Only valid if the variable is a struct or union.

        Note:
            Could have overridden the dot operator here, but wary of a
            clash with actual Variable members.

        Args:
            name(str): The name of the variable we want.
        """
        if not self.members:
            # No members to return!
            raise VariableMemberMissing("Variable has no member: " + name)

        if self.array_len is not None and self.array_len > 0:
            # This is an array; you can't access members by name
            raise VariableMemberMissing("Variable has no member: " + name)

        for member in self.members:
            if member.base_name == name:
                return member
        # No member found.
        raise VariableMemberMissing("Variable has no member: " + name)

def insert_spaces(input_string, depth_str):
    """Inserts spaces before each line for a given string."""
    return depth_str.join(input_string.splitlines(True))

class ChipdataVariable(object):
    """A chipdata variable.

    Variables are more complicated than a standard data symbol, since they
    have a size and can be structures.

    'value' can be a single integer or (more likely) a list/tuple.

    Args:
        debuginfo_var - Debuginfo variable
        chipdata - chipdata
    """
    def __init__(self, debuginfo_var, chipdata):


        if not isinstance(debuginfo_var, Variable):
            raise StandardError("debuginfo_var is " + str(type(debuginfo_var)))
        self._debuginfo_var = debuginfo_var
        self._chipdata = chipdata
        self._formatter_dict = None


    @property
    def deref(self):
        """Returns the de-referenced pointer to the variable.
        
        If the ChipdataVariable is a pointer it returns the dereferenced
        pointer to variable. Otherwise it raises VariableNotPointerError
        error.
        """
        pointed_to_type = self._chipdata.debuginfo.get_type_info(
            self._debuginfo_var.type,
            elf_id=self._debuginfo_var.debuginfo.elf_id
        )[2]


        # If the variale is a chipdata pointer cast it.
        if pointed_to_type:
            try:
                return self._chipdata.cast(
                    self._debuginfo_var.value, pointed_to_type,
                    elf_id=self._debuginfo_var.debuginfo.elf_id
                )
            except InvalidDebuginfoTypeError:
                # Unreferenced type might have zero length. Search for
                # the type based on the name which avoid zeor sized types.
                type_name = self._chipdata.debuginfo.get_type_name(
                    pointed_to_type,
                    elf_id=self._debuginfo_var.debuginfo.elf_id
                )
                pointed_to_type = self._chipdata.debuginfo.get_type_info(
                    type_name,
                    elf_id=self._debuginfo_var.debuginfo.elf_id
                )[1]
                return self._chipdata.cast(
                    self._debuginfo_var.value, pointed_to_type,
                    elf_id=self._debuginfo_var.debuginfo.elf_id
                )

        raise VariableNotPointerError(
            "Variable not a pointer."
        )

    def get_member(self, member):
        """Deprecated method will be removed in future!
        Gets a variable member by name.

        Only valid if the variable is a struct or union.

        Note:
            Could have overridden the dot operator here, but wary of a
            clash with actual Variable members.

        Args:
            name
        """
        logger.warning(
            "Use variable['%s'] instead if variable.get_member('%s')." % (member, member)
        )
        # get the member of the debuginfo variable. This can raise
        # VariableMemberMissing if member is missing.
        return_val = self._debuginfo_var.get_member(member)
        # Convert it to a ChipdataVariable
        return ChipdataVariable(return_val, self._chipdata)

    @property
    def value(self):
        """Returns value of the variable in the chip memory."""
        # Note the value of the debuginfo is replaced during the cast/get
        # var.
        return self._debuginfo_var.value

    @property
    def address(self):
        """Returns start address of the variable in the chip memory."""
        return self._debuginfo_var.address

    @property
    def size(self):
        """Returns size of the variable in memory."""
        return self._debuginfo_var.size

    def set_formatter_dict(self, formatter_dict):
        """Sets the formatter dictionary for the chipdata variable which will
        be uset to format the variable.

        Args:
            formatter_dict (dict): a dictionary of fuction which will be used
                to display the variable's members.
        """
        self._formatter_dict = formatter_dict

    def __repr__(self, *args, **kwargs):
        """String representation.

        The standard representation will be the same as the standard to
        string to make the interactive interpreter more user-friendly.
        """
        return self._chipvar_to_string()

    def __str__(self):
        """The standard to string function."""
        return self._chipvar_to_string()

    def _chipvar_to_string(self, depth=0, formatter_dict=None):
        """Converts a structure to a base_name: value string.

        Args:
            depth (int, optional)
        """
        debuginfo_var = self._debuginfo_var

        if formatter_dict is None:
            formatter_dict = self._formatter_dict
        if formatter_dict is None:
            return debuginfo_var.var_to_str(depth=depth)

        depth_str = "  " * depth
        fv_str = ""

        if depth == 0:
            fv_str += "0x%08x " % self.address

        if self._debuginfo_var.array_len:
            fv_str += (depth_str + debuginfo_var.base_name + ":\n")
            for index, element in enumerate(self):
                fv_str += (depth_str + "  [" + str(index) + "]")
                fv_str += element._chipvar_to_string(
                    depth=depth + 1,
                    formatter_dict=formatter_dict
                )
        else:
            debuginfo = debuginfo_var.debuginfo
            members = debuginfo.get_type_members(
                debuginfo_var.type, debuginfo.elf_id
            )
            has_formatter = (
                formatter_dict is not None and
                not isinstance(formatter_dict, dict)
            )

            if has_formatter:
                if debuginfo_var.base_name != "":
                    fv_str += (depth_str + debuginfo_var.base_name)
                fv_str += (": " + insert_spaces(formatter_dict(self), depth_str))
                return  fv_str

            if members:
                if debuginfo_var.base_name == "":
                    # Probably it was a pointer to something
                    fv_str += (depth_str + debuginfo_var.name + ":\n")
                else:
                    fv_str += (depth_str + debuginfo_var.base_name + ":\n")
                for member in members:
                    next_formatter_dict = formatter_dict.get(
                        member, None
                    )
                    chipdata_member = self[member]
                    fv_str += chipdata_member._chipvar_to_string(
                        depth=depth + 1,
                        formatter_dict=next_formatter_dict
                    )
            else:
                fv_str += debuginfo_var.var_to_str(depth=depth + 1)
                return fv_str


        return fv_str

    def __getattr__(self, attribute):
        """
        Facilitates accessing members of the variable with varible.member
        syntax. However the recommended syntax is variable['member'] because
        member names cannot be 'value' or 'address' with varible.member syntax.

        __getattr__ will be called only if the attribute hasn't been
        found by __getattribute__. This is desired to support accessing .value
        and .address
        """
        # get the member of the debuginfo variable. This can raise
        # VariableMemberMissing if member is missing.
        return_val = self._debuginfo_var.get_member(attribute)
        # Convert it to a ChipdataVariable
        return ChipdataVariable(return_val, self._chipdata)

    def __getitem__(self, key):
        """
        Facilitates accessing members of the variable with variable['member']
        syntax and accessing vector variables with variable[<member number>]s
        """
        if isinstance(key, numbers.Integral):
            # Get the array member from the debuginfo variable and
            # convert it to a ChipdataVariable.
            return ChipdataVariable(self._debuginfo_var[key], self._chipdata)

        # get the member of the debuginfo variable. This can raise
        # VariableMemberMissing if member is missing.
        return_val = self._debuginfo_var.get_member(key)
        # Convert it to a ChipdataVariable
        return ChipdataVariable(return_val, self._chipdata)

    def __len__(self):
        """
        Allows the use of len() on the object. Usefull when the variable is an
        array.
        """
        if self._debuginfo_var.array_len:
            return self._debuginfo_var.array_len
        return 0

    def __iter__(self):
        """
        Allows ChipdataVariable to be iterable to access array member
        """
        for i in self._debuginfo_var.members:
            yield ChipdataVariable(i, self._chipdata)

    def __dir__(self):
        """
        Allows autocompletion on chipdata variables.
        """
        # get the type functions and the default dict keys
        res = dir(type(self)) + list(self.__dict__.keys())

        # Add all the variable members name
        members = self._debuginfo_var.debuginfo.get_type_members(
            self._debuginfo_var.type
        )
        if members:
            res.extend(members)

        # Remove deref if the variable is not a pointer
        pointed_to_type = self._chipdata.debuginfo.get_type_info(
            self._debuginfo_var.type,
            elf_id=self._debuginfo_var.debuginfo.elf_id
        )[2]
        if not pointed_to_type:
            res.remove("deref")

        return res

class ChipVarHelper(object):
    """ Class which contains helper functions for formatting ChipVariable
    objects.
    """

    @staticmethod
    def value_format(formatter_function):
        """
        Wraps a function which can transform the value of a variable to
        something new.

        Args:
            formatter_function (function): The value of the variable will be
                formatted with this function.

        Returns:
            function: Variable formatter which converts the values ot str.
        """
        def ret_var_formatter(var):
            """
            Function which wraps formatter_function.
            """
            ret_str = "(" + hex(var.value) + ") "
            ret_str += str(formatter_function(var.value)) + "\n"
            return ret_str
        return ret_var_formatter

    @staticmethod
    def deref_with_formatter_dict(formatter_dict):
        """
        Wraps the variable dereference variable functionality with a given
        formatter.

        Args:
            formatter_dict (dict): Formatter dictionary.

        Returns:
            function: Variable formatter which dereferences a variable and
                applies the formatter dict to it.
        """
        def ret_deref(var):
            """
            Function which wraps the dereference.
            """
            if var.value != 0:
                deref_var = var.deref
                deref_var.set_formatter_dict(formatter_dict)
                return str(deref_var)

            return "NULL\n"

        return ret_deref

    @staticmethod
    def deref(var):
        """
        Provide a simplified deref.
        """
        return ChipVarHelper.deref_with_formatter_dict(None)(var)

    @staticmethod
    def parse_linked_list(list_ptr, next_field_name="next"):
        """
        This function generates a linked list iterator from a ChipdataVariable
        and the next filed name.

        Args:
            list_ptr (ChipdataVariable): Pointer or normal ChipdataVariable
                variable
            next_field_name (str): next field name in the structure.

        Returns:
            iterator: An iterator which goes trough the element of the list.
        """
        if not isinstance(list_ptr, ChipdataVariable):
            raise StandardError("ChipdataVariables are accepted")

        try:
            # This raises VariableMemberMissing if the list_ptr is a pointer.
            new_list_ptr = list_ptr[next_field_name]
            # The user supplied the first element as a variable. This can happen
            # if the head of the list is statically allocated. The head is still
            # part of the list so give it back to the user.
            # (Either that, or they supplied a pointer that happens to
            # have a member called next_field_name).
            yield list_ptr
            list_ptr = new_list_ptr
        except VariableMemberMissing:
            pass

        dm_word_width = 24
        if Arch.addr_per_word == 4:
            dm_word_width = 32

        while list_ptr.value != 0:
            # Check if the list terminated with 0xffffff(-1 in 24 bit =
            # 16777215 in 24bit) or 0xFFFFFFFF (32 bit). If so, raise and
            # error because this is not accepted anymore.
            if list_ptr.value == (math.pow(2, dm_word_width) - 1):
                raise Exception(
                    "List terminated with 0x%x" % list_ptr.value
                )

            element = list_ptr.deref
            yield element
            for field in next_field_name.split("."):
                field_var = element[field]
                element = field_var
            list_ptr = element

    @staticmethod
    def linked_list_with_formatter_dict(next_name, formatter_dict):
        """
        Wraps the variable parse_linked_list iteratior with a given
        formatter.

        Args:
            formatter_dict (dict): Formatter dictionary.
            next_field_name (str): next field name in the structure.

        Returns:
            function: Variable formatter which prases a linked_list from the
                variable and applies the formatter dict to all elements.
        """
        def ret_linked_list(next_name="next"):
            """
            Function which wraps the dereference with the formatter_dict.
            """
            def disp_linked_list(identifier):
                """
                Function which converts the list to str for display.
                """
                # Convert list to array
                to_array = list(
                    ChipVarHelper.parse_linked_list(identifier, next_name)
                )

                ret_str = ""
                if to_array:
                    # Get the tail index.
                    to_array_tail_index = len(to_array) - 1
                    # Go through the array
                    for index, element in enumerate(to_array):
                        # Display the element name. Head, next and tail
                        if index == 0:
                            if identifier is to_array[0]:
                                ret_str += "\nIs a static linked list head: "
                            else:
                                ret_str += "\nIs a linked list head: "
                        elif index == to_array_tail_index:
                            ret_str += "Tail: "
                        else:
                            ret_str += "Next: "
                        # display the element
                        element.set_formatter_dict(formatter_dict)
                        ret_str += (str(element))
                return ret_str
            return disp_linked_list
        return ret_linked_list(next_name)

    @staticmethod
    def linked_list(next_name="next"):
        """
        Provide a simplified linked_list_with_formatter_dict.
        """
        return ChipVarHelper.linked_list_with_formatter_dict(next_name, None)


class SourceInfo(object):
    """A bunch of information about a particular address in PM.

    Args:
        address
        module_name
        src_file
        line_number
    """

    def __init__(self, address, module_name, src_file, line_number):
        self.address = address
        self.module_name = module_name
        self.src_file = src_file
        self.line_number = line_number
        self.nearest_label = None  # a CodeLabel.

        # Note that nearest_label should be filled in on-demand, e.g. by calls
        # to DebugInfo.get_nearest_label(). It's too slow to calculate it
        # up-front for every PM RAM/ROM address.

    def __str__(self):
        if self.nearest_label is None:
            nearest_label_str = "Uncalculated"
        else:
            nearest_label_str = str(self.nearest_label)

        # Using str(x) here since it copes with the value being None
        return (
            'Code address: ' + hex(self.address) + '\n' + 'Module name: ' +
            str(self.module_name) + '\n' + 'Found in: ' + self.src_file +
            ', line ' + str(self.line_number) + '\n' + 'Nearest label is: ' +
            nearest_label_str
        )


class CodeLabel(object):
    """Information about a code label.

    Args
        name: Code label name.
        address: Code label address.
    """

    def __init__(self, name, address):
        self.name = name
        self.address = address

    def __str__(self):
        return self.name + ', address ' + hex(self.address) + '\n'
