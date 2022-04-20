############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import PureVirtualError
from abc import ABCMeta, abstractproperty, abstractmethod

# compatible with Python 2 and 3
ABC = ABCMeta('ABC', (object,), {'__slots__': ()})


class IRegisterFieldInfo (ABC):
    
    """\
    A common/generalised interface to hardware register and register field 
    meta-data as published by digits.
    
    Known uses:-
    - To access field state of a specific device (via Field object).
    - To display field values along with meta data (like bitz).
    
    Open:-
    - Can this interface be re-used/re-factored to represent firmware field 
    meta-data as well? - The patterns are very similar.
    """
    @abstractproperty
    def name(self):
        """\
        Name of the register or field as published by digits.
        
        N.B. In the case of fields this name is not necessarily unique.
        
        It is anticipated that applications (e.g. Core.fields[]) will construct 
        a unique index for fields that would otherwise not be unique. E.g. by 
        qualifying with parent (register) name "<parent.name>__<field.name>". 
        But this is a matter for the application and is not prescribed here.
        """
        raise PureVirtualError()
    
    @abstractproperty
    def description(self):
        """\
        Text description of the field.
        """
        raise PureVirtualError()

    @abstractproperty
    def parent(self):
        """\
        The containing register or field (IRegisterFieldInfo) or None if this 
        is a register.
        """
        raise PureVirtualError()
        
    @abstractproperty
    def children(self):
        """\
        The set of contained fields (IRegisterFieldInfos). Empty for leaf 
        registers and fields.
        """
        raise PureVirtualError()
        
    @abstractproperty
    def start_addr(self):
        """\
        Start word index (inclusive) of this field wrt. the respective core's 
        data address space. 
        """
        raise PureVirtualError()
    
    @abstractproperty
    def stop_addr(self):
        """\
        Stop word index (exclusive) of this field wrt. the respective core's 
        data address space. 
        """
        raise PureVirtualError()
    
    @abstractproperty
    def start_bit(self):
        """\
        Start bit index (inclusive) of this field wrt. start_addr.
        
        For Registers this will always be 0.
        """
        raise PureVirtualError()
    
    @abstractproperty
    def stop_bit(self):
        """\
        Stop bit index (exclusive) of this Field wrt. start_addr.
        """
        raise PureVirtualError()
        
    @abstractproperty
    def num_bits(self):
        """\
        Number of bits in this Field
        """
        raise PureVirtualError()
    
    @abstractproperty
    def mask(self):
        """\
        Bit mask value for this Field wrt. start_addr.
        
        N.B. This mask value may span multiple words.
        """
        raise PureVirtualError()
    
    @abstractproperty
    def does_span_words(self):
        """\
        Does this Field span multiple memory words?
        """
        raise PureVirtualError()
            
    @abstractproperty
    def is_writeable(self):
        """\
        Is this Field writeable.
        """
        raise PureVirtualError()
        
    @abstractproperty
    def reset_value(self):
        """\
        Reset/Initial value of this Field.
        """
        raise PureVirtualError()

    @property
    def enum_infos(self):
        """\
        The set of IEnumInfos applicable to this Field (possibly empty)
        """
        raise NotImplementedError
    
    @abstractmethod
    def enum_value_by_name(self, enum_name):
        """\
        Map of Enum name => value for this Field (possibly empty)
        
        Known uses:-
        - To support use of symbolic values in Hardware Proxy code 
        e.g. core.fields["XYZ"].symbolic_value = "ABC"
        e.g. if core.fields["XYZ"].symbolic_value == "ABC": ... 
        """
        raise NotImplementedError
    
    @abstractproperty
    def layout_info(self):
        '''
        ILayoutInfo object, which describes the addressable width, the data word
        width and the endianness of data
        '''

class SimpleRegisterFieldInfo(object):
    """
    Simple data type to represent register metadata
    """
    
    def __init__(self, name, description, parent,
                           children, start_addr, stop_addr,
                           start_bit,stop_bit, mask,
                           does_span_words, writeable,
                           reset_value, layout_info):
        self.name = name
        self.description = description
        self.parent = parent
        self.children = children
        self.start_addr = start_addr
        self.stop_addr = stop_addr
        self.start_bit = start_bit
        self.stop_bit = stop_bit
        self.mask = mask
        self.does_span_words = does_span_words
        self.is_writeable = writeable
        self.reset_value = reset_value
        self.layout_info = layout_info
        self.enum_infos = None
        
    def enum_value_by_name(self):
        return NotImplemented

# Make isinstance(X, IRegisterFieldInfo) pass when X is a SimpleRegisterFieldInfo
# instance.
IRegisterFieldInfo.register(SimpleRegisterFieldInfo)


class BaseRegisterFieldInfo(IRegisterFieldInfo):
    """
    Common base class implementing IRegisterFieldInfo's derivative methods in
    terms of the fundamental look-up methods which are implemented in the
    concrete instances (currently NoddyRegisterFieldInfo and 
    IoStructRegisterFieldInfo)
    """

    def __init__(self, field_sym, misc_values, layout_info):
        """\
        Construct _RegisterFieldInfo.
        
        Params:-
        - field_sym: Field symbol => tuple (see NoddyRegisterMapInfo)
        - misc_values: Misc symbol => value map
        """
        self._name = field_sym   
        self._misc_values = misc_values
        self._layout_info = layout_info

    @property
    def name(self):
        
        return self._name
    
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
    def stop_bit(self):
        
        # Derive
        try:
            self._stop_bit
        except AttributeError:
            self._stop_bit = self.start_bit + self.num_bits
            
        return self._stop_bit
        
    @property
    def mask(self):

        # Derive
        try:
            self._mask
        except AttributeError:
            self._mask = ((1 << self.num_bits) - 1) << self.start_bit
             
        return self._mask
    
    @property
    def does_span_words(self):
        
        # Derive
        try:
            self._does_span_words
        except AttributeError:
            self._does_span_words = self.stop_bit > self._layout_info.data_word_bits
            
        return self._does_span_words
            
    @property
    def enum_value_by_name(self, enum_name):
        
        # StopGap: Lookup in global misc values...
        return  self._misc_values[enum_name]

    @property
    def layout_info(self):
        return self._layout_info


