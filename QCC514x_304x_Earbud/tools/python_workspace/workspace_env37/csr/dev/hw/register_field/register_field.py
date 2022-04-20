############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .meta.i_register_field_info import IRegisterFieldInfo
from csr.wheels.global_streams import iprint
from csr.wheels import TypeCheck, display_hex, PureVirtualError, unique_subclass, \
get_bits_string, gstrm, NameSpace
from csr.dev.adaptor.text_adaptor import StringTextAdaptor, TextAdaptor
from csr.dev.model.interface import Group, Table, Warning
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.memory_pointer import MemoryPointer
import logging
import sys
from functools import cmp_to_key

class IllegalRegisterRead(RuntimeError):
    """
    Attempt to read a write-only register
    """

class IllegalRegisterWrite(RuntimeError):
    """
    Attempt to write a read-only register
    """

class IllegalRegisterRMW(RuntimeError):
    """
    Attempt to write a true bitfield of a write-only register, which
    can't be done because an RMW is required
    """

_bits_class_index = 1 # global for helping quickly generate unique subclass in which to
# insert bits properties.

if sys.version_info > (3,):
    # Python 3
    int_type = int
    def cmp(a, b):
        return (a > b) - (a < b)
else:
    # Python 2
    int_type = (int, long)

def bitz_engine(reg, report=False, desc_width=None, value=None):
    """
    Construct a report of the given register's current bit values
    """
    if value is None:
        value = reg.read()
    
    columns = ["field","bits","value"]
    if desc_width is not None:
        columns.append("description")
    t = Table(columns)

    # For registers with children, breakdown the register's value into
    # subfield values
    def cmp_func(a1_a2, b1_b2):
        ret_val = cmp(a1_a2[1].start_bit, b1_b2[1].start_bit)
        # If the start bits are the same then one subfield is a superset of the others. Report this first.
        if ret_val == 0:
            ret_val = -cmp(a1_a2[1].stop_bit, b1_b2[1].stop_bit)
        return ret_val

    if reg.info.children:
        children_info = list(reg.info.children.items())
        children_info = sorted(children_info, key=cmp_to_key(cmp_func))
        for child_name, child_info in children_info:
            if value is False:
                child_val = " - "
            else:
                child_val = "0x%x" % ((value & child_info.mask) >> child_info.start_bit)
            row = [child_name, get_bits_string(child_info), child_val]
            if desc_width is not None:
                row.append(child_info.description[0:desc_width])
            t.add_row(row)
    # For registers without children or subfields, print the value as-is
    else:
        row = [reg.info.name, get_bits_string(reg.info), 
                       "0x%x" % value if value is not False else " - "]
        if desc_width is not None:
            row.append(reg.info.description[0:desc_width])
        t.add_row(row)
        
    g = Group(reg.info.name)
    if value is False:
        g.append(Warning("Write-only register"))
    g.append(t)
    if report:
        return g
    TextAdaptor(g, gstrm.iout)

class BadRegisterSubfield(TypeError):
    """
    Exception indicating that someone attempted to write to a subfield that
    doesn't actually exist.
    """

class BitField (object):
    """\
    Bit Resolution Field (Abstract Base)
    
    Implements methods to access bit-resolution memory fields.
    
    BitFields may span memory words.
    
    Limitations:- 
    
    - There is scope in here for optimisation to avoid unnecessary RMWs of sub-
    word fields that have a whole word to themselves and large fields that span
    some words but start or end in the middle of others. (consider overlap with
    multi-field updates,cache & transactions)
    """
    def __str__(self):
        
        return "BitField @0x%04x[%d:%d]" % (self.start_addr, self.start_bit, self.stop_bit)
        
    @property
    def is_word_filling(self):
        """\
        Does this field completely fill the data word(s) it occupies?
        
        If so can avoid RMW when updating. 
        
        Limitations:-
        
        - This does not spot small fields that are alone in a word(s) of their
        own.
        """
        word_bits = self.layout_info.data_word_bits
        return ((self.start_bit % word_bits == 0)
                and (self.stop_bit % word_bits == 0))

    @property
    def is_partial_width(self):
        return (self.info.parent is not None and
                self.info.num_bits < self.info.parent.num_bits)

    @property
    def _requires_carry_safe_read(self):
        # A read-only word-filling multi-word register should be read carefully
        # in case it is a counter that could wrap during the read process.
        return not self.is_writeable and self.is_word_filling and self.stop_bit - self.start_bit > self.layout_info.data_word_bits

    def _raw_read(self):

        # Read all words spanned by this field.
        #
        if not self.is_readable:
            raise IllegalRegisterRead("Attempt to read write-only field 0x{:x}".format(self.start_addr))
        return self.mem[self.start_addr : self.stop_addr]

    def _raw_write(self, data, bank_select_already_set=False):
        # bank_select is not a thing for generic BitFields, just RegisterFields
        self.mem[self.start_addr:self.stop_addr] = data

    def _value_from_addr_units(self, addr_units, bad_value_check=True):
        # Unmarshal to integer.
        #
        raw_value = self.layout_info.deserialise(addr_units)
        
        if bad_value_check:
            try:
                bad_ret_val = self._bad_read_reg.read() 
            except AttributeError:
                pass
            except AddressSpace.ReadFailure:
                # Most likely case of ReadFailure for these registers is if access is restricted
                # which is done in hardware, thus we should ignore.
                pass
            else:
                # If the bad read reg contains 0, assume it shouldn't be taken 
                # seriously (one way this can happen is if its cache has not been 
                # loaded yet in coredump or sim modes) 
                if bad_ret_val != 0 and raw_value == bad_ret_val:
                    iprint("\n***WARNING: parent register read returned the value of %s (0x%x) - "
                        "may be a bad read***\n" % (self._bad_read_reg.info.name,
                                                    bad_ret_val))

        # Apply field mask & shift
        #
        value = raw_value & self.mask
        return value >> self.start_bit

    @display_hex
    def read(self):
        """\
        Read this Field's current value
        """
        addr_units = self._raw_read()
        
        if self._requires_carry_safe_read:
            # Read again to see if there was a change in any of the words.  If
            # there was more than one then we just saw a carry 
            old_words = addr_units
            new_words = self.mem[self.start_addr : self.stop_addr]
            while sum(ow != nw for (ow, nw) in zip(old_words, new_words)) > 1:
                old_words = new_words
                new_words = self.mem[self.start_addr : self.stop_addr]
            addr_units = new_words
        
        return self._value_from_addr_units(addr_units)
    
    def write(self, value, truncate=False, load=False):
        """\
        Write value to this Field
        
        Params:-        
        - truncate : If set true then a value that is too big will be
        truncated silently. Use with care.
        - load : If set then a value will be written to a read-only register.
        Only use this when loading defaults.
        
        Raises:-         
        - ValueError : If value won't fit in field and truncate is not
        specified.
        
        Future:-         
        - The logic here is getting a bit scrappy - but does not even optimise
        for small lonely fields yet.
        """
        if self._core and not load:
            self._core.fields.logger.trace_verbose("Setting: %s <= 0x%x" % 
                                                        (self.info.name, value))
        
        if not self.is_writeable and not load:
            raise IllegalRegisterWrite("Attempt to write read-only field %x" % self.start_addr)
        
        # Shift the value
        shifted_value = value << self.start_bit
        
        # Optionally truncate/mask, else check it fits
        if truncate:
            shifted_value = shifted_value & self.mask
        else:
            if shifted_value != shifted_value & self.mask:
                raise ValueError("value %x too large for field" % value)

        # Calculate whole word value to be written
        #
        addr_bits = self.layout_info.addr_unit_bits

        try:
            need_rmw = self.is_partial_width
        except AttributeError:
            # AdHocBitfield does not have the info attribute
            # so we need to fall back to using is_word_filling
            need_rmw = not self.is_word_filling
        if need_rmw:
            # Assume RMW is necessary
            #
            # Read all words spanned by this field, unmarshal to BE integer and
            # mask new value in.
            #
            try:
                addr_units = self._raw_read()
            except IllegalRegisterRead:
                raise IllegalRegisterRMW("Can't perform RMW to write to bitfield as "
                                        "register is write-only.  Use capture/flush instead.")

            w_value = self.layout_info.deserialise(addr_units)
            w_value = (w_value & ~self.mask) | shifted_value
        else:
            # No need to RMW if the field fills all the words it spans
            #
            # There are other cases not optimised - this is the
            # low hanging fruit.
            #
            w_value = shifted_value

        # Marshal the new value with appropriate endianness
        #
        num_addrs = self.stop_addr - self.start_addr 
        data = self.layout_info.serialise(w_value, num_addrs)
        
        # Write it
        #
        if self._core and not load:
            name = (self.info.parent.name if self.info.parent is not None 
                    else self.info.name)
            self._core.fields.logger.trace("Writing: %s <= 0x%x" % (name, 
                                                                    w_value))

        self._raw_write(data, bank_select_already_set=need_rmw)

        """
        If verify_write is set and the register is readable, 
        check data was written correctly to memory.
        Raise en error if the verification fails.
        If the register is not readable assume write was ok.
        """
        if (self._core is not None and
                self._core.fields.verify_write
                and self.is_readable):
            # Read the data that was written and
            # check it equals the expected value
            if value != self.read():
                raise RegisterWriteVerificationFailure
 
    @property
    def layout_info(self):
        """
        Uniform access to layout_info
        """
        raise PureVirtualError


class RegisterWriteVerificationFailure(RuntimeError):
    """ Error that is raised if the physical connection is compromised """


class AdHocBitField (BitField):
    """
    """
    def __init__(self, mem, layout_info, start_addr, start_bit, num_bits, 
                 is_writeable=True):
        self._core = None
        self._core_or_mem = mem
        self._core = None
        self._layout_info = layout_info
        self._start_addr = start_addr
        self._start_bit = start_bit
        self._num_bits = num_bits
        self._is_writeable = is_writeable
    
    @property
    def mem(self):
        return self._core_or_mem
    
    @property
    def is_writeable(self):
        """\
        Is this Field writeable?
        """
        return self._is_writeable

    def is_readable(self):
        return True

    @property
    def start_addr(self):
        """\
        Start word index of this Field wrt. core data-space.
        """
        return self._start_addr
    
    @property
    def stop_addr(self):
        
        # Derive
        try:
            self._stop_addr
        except AttributeError:
            # Get the logical stop address
            self._stop_addr = self.start_addr + ((self.stop_bit - 1) // self._layout_info.addr_unit_bits) + 1
            # Align the logical stop address to the next data word boundary
            dsize = self.layout_info.data_word_bits // self.layout_info.addr_unit_bits
            self._stop_addr = dsize * ((self._stop_addr + dsize - 1) // dsize)
            
        return self._stop_addr
    
    @property
    def start_bit(self):
        """\
        Start bit index of this Field wrt. start_addr.
        """
        return self._start_bit
    
    @property
    def stop_bit(self):
        """\
        Stop bit index of this Field wrt. start_addr.
        """
        return self._start_bit + self._num_bits
    
    @property
    def num_bits(self):
        """\
        Number of bits in this Field
        """
        return self._num_bits
    
    @property
    def mask(self):
        """\
        Bit mask value for this Field wrt. start_addr.
        
        N.B. This mask value may span multiple words.
        """
        # Derive
        try:
            self._mask
        except AttributeError:
            self._mask = ((1 << self.num_bits) - 1) << self.start_bit
        return self._mask
    
    @property
    def does_span_words(self):
        """
        Indicates whether this register spans multiple *data words*, not whether
        it spans multiple *addresses*.
        """ 
        # Derive
        try:
            self._does_span_words
        except AttributeError:
            self._does_span_words = \
                            self.stop_bit > self._layout_info.data_word_bits
            
        return self._does_span_words
                

    @property
    def layout_info(self):
        return self._layout_info
                
class RegisterField (BitField):
    """\
    Device register or register field descriptor.
    
    Encapsulates logic to access (read/write) a specific register field given
    the field's meta data and containing memory block.
    
    Presents a unified interface to whole registers and sub-fields.
    
    Note:-
    - Information about relation of fields to registers can be inferred via the 
    meta-data (RegisterField.info.parent & .children). If useful to do so, 
    then this could easily be reflected in this Field interface where .parent 
    and .children would refer to instance rather than meta-data objects.
    """

    next_ind = 0
    
    # Potential extension:: Multi-word field write
        
    def __init__(self, field_info, core_or_mem, bad_read_reg=None):
        """\
        Construct a RegisterField.
        
        Params:-
        - field_info: Field meta-data (see hw/meta/IRegisterFieldInfo) 
        - mem: The memory space containing this register field instance.
        """
        TypeCheck(field_info, IRegisterFieldInfo)

        self._flag_new_attributes = False
        
        self._info = field_info
        self._bad_read_reg = bad_read_reg
        self._core_or_mem = core_or_mem
        if hasattr(core_or_mem, "data"):
            self._core = core_or_mem
            self._mem = core_or_mem.data
            
        else:
            self._core = None
            self._mem = core_or_mem

        # Add the children as members
        self.set_child_properties(type(self))

        try:
            bank_select_info, bank_select_val = field_info.__bank__
        except AttributeError:
            pass # no bank select
        else:
            self.__bank__ = (RegisterField(bank_select_info, core_or_mem), bank_select_val)

        self._flag_new_attributes = True

    def _getter_factory(self, child_info, core_or_mem):
        """
        Factory for creating a getter function for bitfield properties. The getter
        returns a RegisterField object representing the bitfield, which is
        owned by the closure.
        """
        reg = RegisterField(child_info, core_or_mem, self._bad_read_reg)
        def getter(managed_self):
            return reg
        return getter
    def _setter_factory(self, child_info, core_or_mem):
        """
        Factory for creating a setter function for bitfield properties. The setter
        writes to a RegisterField object representing the bitfield, which is
        owned by the closure.
        """
        reg = RegisterField(child_info, core_or_mem, self._bad_read_reg)
        def setter(managed_self, value):
            reg.write(value)
        return setter
        
    def set_child_properties(self, cls):
        """
        Add the children as properties on the supplied class (could be type(self)
        or some other container for bitfield objects)
        Return the names of duplicated attributes 
        """
        global _bits_class_index
        duplicates = set()
        
        # Factory functions returning the getter and setter functions required
        # to construct the property
        if self.info.children:
            for child, child_info in self.info.children.items():
                if hasattr(cls, child):
                    duplicates.add(child)
                else:
                    setattr(cls, child, property(self._getter_factory(child_info, self._core_or_mem),
                                                 self._setter_factory(child_info, self._core_or_mem)))
        if self.info.num_bits > 1 and cls is type(self):
            def getter(managed_self):
                global _bits_class_index
                try:
                    managed_self._bits
                except AttributeError:
                    bits_class, _bits_class_index = unique_subclass(NameSpace, id_hint=_bits_class_index)
                    for bit_pos, bit_info in enumerate(managed_self.info.bits):
                        setattr(bits_class, "BIT{}".format(bit_pos), property(managed_self._getter_factory(bit_info, managed_self._core_or_mem),
                                                                            managed_self._setter_factory(bit_info, managed_self._core_or_mem)))
                    managed_self._bits = bits_class()
                return managed_self._bits
            cls.bits = property(getter)

        return duplicates

    def __setattr__(self, name, value):
        """
        Disallow setting of attributes post-construction in an attempt to catch
        mistyped accesses to subfields when trying to write values through them,
        as this will silently create a new integer attribute.
        """
        try:
            new_attributes_banned = self._flag_new_attributes
        except AttributeError:
            # Setting before construction by a subclass - this is OK
            new_attributes_banned = False

        if (name not in ("_flag_new_attributes","_bits") and new_attributes_banned
                                                    and not hasattr(self, name)):
            raise AttributeError("'%s' is not a subfield of %s" % 
                                                            (name, self.info.name))
        else:
            super(RegisterField, self).__setattr__(name, value)

    @property
    def mem(self):
        return self._mem

    @property
    def info(self):
        """\
        Access this field's meta-data (IRegisterFieldInfo)
        """
        return self._info
    
    @property
    def layout_info(self):
        """
        Access the basic memory layout meta-data
        """
        return self._layout_info
    
    @property
    def is_writeable(self):
        """\
        Is this Field writeable?
        """
        return self._info.is_writeable
    
    @property
    def is_readable(self):
        """\
        Is this Field readable?
        """
        return self._info.is_readable

    @property
    def start_addr(self):
        """\
        Start word index of this Field wrt. core data-space.
        """
        return self._info.start_addr
    
    @property
    def stop_addr(self):
        """\
        Start word index of this Field wrt. core data-space.
        """
        return self._info.stop_addr
    
    @property
    def start_bit(self):
        """\
        Start bit index of this Field wrt. start_addr.
        """
        return self._info.start_bit
    
    @property
    def stop_bit(self):
        """\
        Start bit index of this Field wrt. start_addr.
        """
        return self._info.stop_bit
    
    @property
    def num_bits(self):
        """\
        Number of bits in this Field
        """
        return self._info.num_bits
    
    @property
    def mask(self):
        """\
        Bit mask value for this Field wrt. start_addr.
        
        N.B. This mask value may span multiple words.
        """
        return self._info.mask
    
    @property
    def does_span_words(self):
        
        return self._info.does_span_words

    @property
    def layout_info(self):
        return self._info.layout_info

    # Display the RegisterField by running bitz on it
    def __repr__(self):
        try:
            value = self.read()
        except IllegalRegisterRead:
            value = False
        if self.info.children:
            rpt = bitz_engine(self, report=True, value=value)
            return StringTextAdaptor(rpt)
        return "0x%x" % value if value is not False else "-- write-only register --"

    def __str__(self):
        return repr(self)

    # Make the RegisterField act as much like an integer as possible
    
    # Comparison
    #-----------
    
    def __eq__(self, val):
        return self.read() == val
    
    def __ne__(self, val):
        return not (self == val)
    
    def __lt__(self, val):
        return self.read() < val

    def __gt__(self, val):
        return self.read() > val

    def __le__(self, val):
        return self.read() <= val

    def __ge__(self, val):
        return self.read() >= val
    
    # Defining this allows RegisterFields to be used as
    # ints in the context of dictionary or set keys.
    def __hash__(self):
        return hash(self.read())

    # Arithmetic
    # ------------
    def __add__(self, val):
        return self.read() + val
    __radd__ = __add__ 
    
    def __sub__(self, val):
        return self.read() - val
    def __rsub__(self, val):
        return val - self.read()
    
    def __mul__(self, val):
        return self.read() * val
    __rmul__ = __mul__
    
    def __mod__(self, val):
        return self.read() % val

    # Bitwise
    #-------------------
    def __invert__(self):
        return ~ self.read()
    
    def __and__(self, val):
        return self.read() & val
    __rand__ = __and__
    
    def __or__(self, val):
        return self.read() | val
    __ror__ = __or__

    def __lshift__(self, val):
        return self.read() << val

    def __rshift__(self, val):
        return self.read() >> val
    
    
    # Explicit conversion
    #--------------------
    
    def __int__(self):
        return int(self.read())

    def _raw_read(self):
        try:
            bank_select_reg, bank_select_val = self.__bank__
        except AttributeError:
            pass
        else:
            bank_select_reg.write(bank_select_val)
        
        return BitField._raw_read(self)

    def _raw_write(self, data, bank_select_already_set=False):
        if not bank_select_already_set:
            try:
                bank_select_reg, bank_select_val = self.__bank__
            except AttributeError:
                pass
            else:
                bank_select_reg.write(bank_select_val)
        return BitField._raw_write(self, data)

    @display_hex
    def read(self):
        """\
        Read this Register's current value
        
        """
        try:
            self.mem.cached_register_names
        except AttributeError:
            result = BitField.read(self)
        else:
            self.mem.cached_register_names.append(self.info.name)
            result = BitField.read(self)
            self.mem.cached_register_names.pop()
        
        return result

    def write(self, value, truncate=False, load=False):
        """\
        Write value to this Register
        
        Params:-        
        - truncate : If set true then a value that is too big will be
        truncated silently. Use with care.
        - load : If set then a value will be written to a read-only register.
        Only use this when loading defaults.
        
        Raises:-         
        - ValueError : If value won't fit in field and truncate is not
        specified.
        
        Future:-         
        - The logic here is getting a bit scrappy - but does not even optimise
        for small lonely fields yet.
        """
        try:
            self.mem.cached_register_names
        except AttributeError:
            BitField.write(self, value, truncate=truncate,load=load)
        else:
            self.mem.cached_register_names.append(self.info.name)
            BitField.write(self, value, truncate=truncate,load=load)
            self.mem.cached_register_names.pop()

    def set_default(self):
        """
        Write this register's default value
        """
        self.write(self._info.reset_value, load=True)
        
        
class RegisterArray(object):
    
    def __init__(self, array_info, core):
        
        self._array_info = array_info
        self._core = core
        self.set_child_properties(type(self))
        self._elements = [None]*len(self)

        self.start_addr = self._array_info.start_addr

    @property
    def info(self):
        return self._array_info

    def set_child_properties(self, cls):
        """
        Add the children as properties on the supplied class (could be type(self)
        or some other container for bitfield objects)
        Return the names of duplicated attributes 
        """
        duplicates = set()
        
        # Factory functions returning the getter function required
        # to construct the property
        def _getter_factory(child_info, core):
            regarray = RegisterArray(child_info, core)
            def getter(managed_self):
                return regarray
            return getter

        if self.info.children:
            for child, child_info in self.info.children.items():
                if hasattr(cls, child):
                    duplicates.add(child)
                else:
                    setattr(cls, child, property(_getter_factory(child_info, self._core))) 

        return duplicates
        

    def _get_element(self, index):
        if self._elements[index] is None:
            element_info = self._array_info.element_info(index)
            register_field_type = RegisterField if element_info.parent is not None else ParentRegisterField
            subtype, type_ind = unique_subclass(register_field_type, 
                                            id_hint=register_field_type.next_ind)
            register_field_type.next_ind = type_ind + 1
            self._elements[index] = subtype(element_info, self._core)
        return self._elements[index]
        
    def __getitem__(self, index_or_slice):
        """
        Look up the given entry/ies in the register array
        """
        if isinstance(index_or_slice, int_type):
            # Build and return a RegisterField for the given address
            index = index_or_slice
            if index < 0 or index >= self._array_info.num_elements:
                raise IndexError("Index %d is out of range" % index)
            return self._get_element(index)
        else:
            sl = index_or_slice
            start = 0 if sl.start is None else sl.start
            stop = self._array_info.num_elements if sl.stop is None else sl.stop
            step = 1 if sl.step is None else sl.step
            return [self[i] for i in range(start,stop,step)]
        

    def __setitem__(self, index_or_slice, value):
        
        if isinstance(index_or_slice, int_type):
            # Build and return a RegisterField for the given address
            index = index_or_slice
            if index < 0 or index >= self._array_info.num_elements:
                raise IndexError("Index %d is out of range" % index)
            self._get_element(index).write(value)
        else:
            sl = index_or_slice
            start = 0 if sl.start is None else sl.start
            stop = self._array_info.num_elements if sl.stop is None else sl.stop
            step = 1 if sl.step is None else sl.step
            for ivalue, ireg in enumerate(range(start, stop, step)):
                self[ireg] = value[ivalue]

    def __len__(self):
        return self._array_info.num_elements
    
    def __iter__(self):
        class _iter(object):
            def __init__(iter_self):
                iter_self.ind = 0
            def __next__(iter_self):
                ind = iter_self.ind
                iter_self.ind += 1
                try:
                    return self._get_element(ind)
                except IndexError:
                    raise StopIteration
            
            next = __next__  # Backwards compatibility for Python 2
            
        return _iter()
    
    def __repr__(self):
        return "\n".join(repr(r) for r in self)


class ParentRegisterField(RegisterField):
    """
    RegisterField with additional functionality that is only meaningful for
    a parent containing children
    """
    _subclass_id = 0

    def capture(self, value=None):
        """
        Capture the register value, or a custom value, into a clone of this
        RegisterField.  Writes to the returned object, including via its
        bitfields etc, will be cached locally.  They can be written out
        to the chip by calling the flush method.
        """
        if value is None:
            raw_bytes = self._raw_read()
        else:
            raw_bytes = self.layout_info.serialise(value, 
                               self.stop_addr - self.start_addr) 
        mem = MemoryPointer(raw_bytes, -self.start_addr)
        captured_register_field_subclass, self._subclass_id = unique_subclass(
                                        CapturedRegisterField, self._subclass_id)
        return captured_register_field_subclass(self._info, self._core, mem, 
                                        bad_read_reg=self._bad_read_reg)

    def writebits(self, base_value=None, **bitfield_settings):
        """
        Write a register by or-ing in the values for the supplied set of bitfields
        into the given base value.
        """
        captured = self.capture(value=base_value)

        for bitfield, value in bitfield_settings.items():
            setattr(captured, bitfield, value)

        captured.flush()    


class CapturedRegisterField(RegisterField):
    """
    RegisterField variant that is produced by capturing, i.e. refers to a local cache of
    memory instead of chip memory.  This has the same interface as a RegisterField except
    that it can also be flushed, meaning its cached value is written to the chip.
    """
    def __init__(self, info, core, cache_mem, bad_read_reg=None):
        self._cache_mem = cache_mem
        RegisterField.__init__(self, info, core, bad_read_reg=bad_read_reg)

    def _getter_factory(self, child_info, core_or_mem):
        """
        Factory for creating a getter function for bitfield properties. The getter
        returns a CapturedRegisterField object representing the bitfield, which is
        owned by the closure.
        """
        reg = CapturedRegisterField(child_info, core_or_mem, self._cache_mem,
                                    bad_read_reg=self._bad_read_reg)
        def getter(managed_self):
            return reg
        return getter
    def _setter_factory(self, child_info, core_or_mem):
        """
        Factory for creating a setter function for bitfield properties. The setter
        writes to a CapturedRegisterField object representing the bitfield, which is
        owned by the closure.
        """
        reg = CapturedRegisterField(child_info, core_or_mem, self._cache_mem, 
                                    bad_read_reg=self._bad_read_reg)
        def setter(managed_self, value):
            reg.write(value)
        return setter
        
    def read(self):
        """
        Read the captured register's cached value
        """
        # We don't apply readability checks here because it is legitimate to read the
        # cached value of a write-only register, e.g. to check that the bitfields have
        # been set up correctly on a value that is being prepared for writing.
        # The readability will have already been tested if necessary at the point of capture.
        return self._value_from_addr_units(self._cache_mem[self.start_addr:self.stop_addr],
                                           bad_value_check=False)

    def _raw_read(self):

        # Read all words spanned by this field.
        #
        return self._cache_mem[self.start_addr : self.stop_addr]

    def _raw_write(self, data, bank_select_already_set=False):
        """
        Write the supplied byte data to cache mem, unless cache_mem has been suspended,
        in which case behave like a non-captured register, i.e. write out to the chip.
        """
        if self._cache_mem is None:
            super(CapturedRegisterField, self)._raw_write(data, 
                                bank_select_already_set=bank_select_already_set)
        else:
            self._cache_mem[self.start_addr:self.stop_addr] = data

    def flush(self):
        """
        Writes a captured register object's value out to the chip.
        """
        value = self.read()
        # Temporarily suspend self._cache_mem so that we don't just
        # write to it again
        cache_mem, self._cache_mem = self._cache_mem, None
        try:
            self.write(value)
        finally:
            self._cache_mem = cache_mem

