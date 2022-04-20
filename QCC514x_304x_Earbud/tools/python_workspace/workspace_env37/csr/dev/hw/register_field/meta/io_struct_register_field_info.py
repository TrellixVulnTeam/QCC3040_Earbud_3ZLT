# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
from .i_register_field_info import BaseRegisterFieldInfo
from collections import namedtuple
 
class IoStructRegisterFieldInfo (BaseRegisterFieldInfo):
    """\
    Implementation of IRegisterFieldInfo by reference to autogenerated register
    info objects created in the digital register generation process.  The 
    module the digits supply contains reading and writing support, but we are
    only interested in the metadata, which is adapted to our existing interface
    via this class.
    
    Note: in this context "word" means "data word", not "addressable unit".
    """
    class ResetAttributeError(AttributeError):
        'reset attribute is not supported'

    def __init__(self, field_sym, io_struct_dict_or_record, misc_values, layout_info,
                 parent=None, index=None, children=True, elt_spacing=None):
        """\
        Construct _RegisterFieldInfo.
        
        Params:-
        - field_sym: Name of a register or register field
        - io_struct_dict: Dictionary of underlying objects (c_reg or c_bits)
        - layout_info: Layout of data in this context
        """
        BaseRegisterFieldInfo.__init__(self, field_sym, misc_values, layout_info)
        try:
            self._record = io_struct_dict_or_record[field_sym]
        except (TypeError, AttributeError):
            # No subscript operator: it's a record, not a dict
            self._record = io_struct_dict_or_record
        else:
            try:
                bank_reg_name, bank_reg_val = self._record.__bank__
            except AttributeError:
                try:
                    self.__bank__ = parent.__bank__ # No banking info for this reg - borrow parent's if any
                except AttributeError:
                    pass
            else:
                bank_reg_info = IoStructRegisterFieldInfo(bank_reg_name, io_struct_dict_or_record,
                                                     misc_values, layout_info)
                self.__bank__ = (bank_reg_info, bank_reg_val)
        self._parent = parent
        self._index = index
        self.elt_spacing = elt_spacing
        if self.elt_spacing:
            self._addr_offset = 0 if index is None else index * self.elt_spacing
        else:
            self._addr_offset = 0 if index is None else index * layout_info.data_word_bits//layout_info.addr_unit_bits
        if not children:
            self._children = False
        
    # IRegisterFieldInfo compliance
    
    @property
    def name(self):
        if self.parent:
            name = self.parent.name+"."+self._name
        else:
            name = self._name
        if self._index is not None:
            name += "[%d]" % self._index
        return name
    
    @property
    def description(self):
        
        # Lookup
        return self._record.text

    @property
    def parent(self):
        
        return self._parent
        
    @property
    def children(self):
        
        try:
            self._children
        except AttributeError:
            if not hasattr(self._record, "parent"):
                # Keep a copy of all the register's children, if this is a full
                # register
                self._children = {}
                for mbr_name, mbr in self._record.__dict__.items():
                    try:
                        mbr.lsb, mbr.msb
                    except AttributeError:
                        # Not a c_bits
                        pass
                    else:
                        self._children[mbr_name] = IoStructRegisterFieldInfo(
                                         mbr_name, self._record.__dict__, 
                                         self._misc_values, self._layout_info,
                                         parent=self, elt_spacing=self.elt_spacing)
            else:
                self._children = False
        return self._children
    
    @property
    def bits(self):
        """Create objects representing the individual bits in the register/
        bitfield so these can be easily manipulated independently of how the
        register subfields are defined"""
        try:
            self._bits
        except AttributeError:
            c_bits_sub = namedtuple("c_bits_sub", "lsb msb mask width parent")
            def create_single_bit(bit_pos):
                return c_bits_sub(bit_pos, bit_pos, 1<<bit_pos, 1,
                                  self._record)
            self._bits = [IoStructRegisterFieldInfo("BIT{}".format(bit),
                                                    create_single_bit(bit+self.start_bit),
                                                    self._misc_values, self._layout_info,
                                                    parent=self, children=False) 
                                for bit in range(self.num_bits)]
        return self._bits
    @property
    def start_addr(self):
        
        # Lookup
        try:
            return self._record.addr + self._addr_offset
        except AttributeError:
            return self._parent.start_addr + self._addr_offset

    @property
    def stop_addr(self):
        try:
            self._stop_addr
        except AttributeError:
            try:
                # Get the logical stop address
                self._parent.start_addr
            except AttributeError:
                # This isn't a subfield so the basic BitField stop_addr logic works
                return super(IoStructRegisterFieldInfo, self).stop_addr
            else:
                # This is a subfield.  We need to get the parent's stop address
                # But it may be a subfield of a subfield (BITn of a bitfield) so
                # we need to be sure to get the actual register's num_bits so we
                # compute stop_addr correctly.
                try:
                    num_bits = self._parent._parent.num_bits
                except AttributeError:
                    num_bits = self._parent.num_bits
                self._stop_addr = (self._parent.start_addr + 
                                   (num_bits - 1) //
                                        self._layout_info.addr_unit_bits + 1 + 
                                    self._addr_offset)
                # Align the logical stop address to the next data word boundary
                dsize = self.layout_info.data_word_bits // self.layout_info.addr_unit_bits
                self._stop_addr = dsize * ((self._stop_addr + dsize - 1) // dsize)
        return self._stop_addr
    
    @property
    def start_bit(self):
        
        try:
            return self._record.lsb
        except AttributeError:
            # It's a register, so it starts at bit 0 by definition
            return 0
    
    @property
    def num_bits(self):
        
        # Lookup
        try:
            return self._record.width
        except AttributeError:
            return self._record.msb - self._record.lsb + 1
    
    @property
    def mask(self):

        # Derive
        try:
            self._mask
        except AttributeError:
            try:
                # Bitfields have masks provided...
                self._mask = self._record.mask
            except AttributeError:
                # ...registers don't
                self._mask = (1 << self.num_bits) - 1
             
        return self._mask
    
    @property
    def is_writeable(self):
        
        # Lookup
        try:
            return self._record.w
        except AttributeError:
            return self.parent.is_writeable

    @property
    def is_readable(self):

        # Lookup
        try:
            return self._record.r
        except AttributeError:
            return self.parent.is_readable
        
    @property
    def reset_value(self):
        
        # Lookup
        try:
            return self._record.reset
        except AttributeError as exc:
            raise self.ResetAttributeError(exc)

    @property
    def enum_infos(self):
        
        raise NotImplementedError("Enums Not implemented in DB")

    @property
    def module_name(self):

        # Lookup
        try:
            return self._record.mod_name
        except AttributeError:
            # Bitfields do not have module, but registers do.
            return self._record.parent.mod_name

    

class IoStructRegisterArrayInfo(object):
    
    def __init__(self, sym, sym_records, misc_io_values, layout_info,
                 parent_array=None):
        """
        We can either construct this with an actual array symbol to look up in a
        list of sym_records, or with the array record supplied as the parent
        and the supplied symbol being the child field name
        """
        self._sym = sym
        self._misc_io_values = misc_io_values
        self._layout_info = layout_info
        if parent_array is None:
            self._array_record = sym_records[sym]
            self._base_elt_record = self._array_record.element
            # The c_reg used for the Element has an address of None, so override this
            # here to the base address of the array
            self._base_elt_record.addr = self._array_record.addr
        else:
            # Need three things, really: the array record, and then the parent
            # and child field info records.  The two field records are turned 
            # into IoStructRegisterFieldInfos inside element_info.
            self._array_record = parent_array._array_record
            self._base_elt_record = sym_records[sym]
        
        self._parent_array = parent_array

    # Implement parts of the IoStructRegisterFieldInfo interface that are needed
    # so this can be used as the parent of a bitfield. 
        
    @property
    def name(self):
        return self._sym

    @property
    def start_addr(self):
        # This is the start address of the *array*
        return self._array_record.addr

    @property
    def num_bits(self):
        # this is the number of bits in an *element*
        return self._base_elt_record.width
    
    def element_info(self, index):
        
        return IoStructRegisterFieldInfo(self._sym, self._base_elt_record,
                                         self._misc_io_values, self._layout_info,
                                         index=index, parent=self._parent_array,
                                         elt_spacing=self.elt_spacing)

    @property
    def num_elements(self):
        return self._array_record.num_elements

    @property
    def elt_spacing(self):
        try:
            return self._array_record.elt_spacing
        except AttributeError:
            return None
    
    @property
    def children(self):
        
        try:
            self._children
        except AttributeError:
            if not hasattr(self._base_elt_record, "parent"):
                # Keep a copy of all the register's children, if this is a full
                # register
                self._children = {}
                for mbr_name, mbr in self._base_elt_record.__dict__.items():
                    if (hasattr(mbr, "parent") and hasattr(mbr, "lsb") and 
                        mbr.parent is self._base_elt_record):
                        # This constructor call needs fixing up
                        self._children[mbr_name] = IoStructRegisterArrayInfo(
                                         mbr_name, self._base_elt_record.__dict__, 
                                         self._misc_io_values, self._layout_info,
                                         parent_array=self)
            else:
                self._children = False
        return self._children

    @property
    def is_writeable(self):
        return self._base_elt_record.w

    @property
    def is_readable(self):
        return self._base_elt_record.r