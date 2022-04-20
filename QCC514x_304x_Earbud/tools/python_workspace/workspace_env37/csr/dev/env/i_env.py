############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Device Debugging Framework Environment Interface

Isolates device models (HW & FW) from environment (xide, standalone).

Future:

-- Move index utilities out of here.

-- Move MemorySubregion out of here.
"""

from csr.wheels.bitsandbobs import PureVirtualError
try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int


class ICompilationUnit (object):
    """\
    Interface to compilation unit instance (CU)
    """
    
    @property
    def localvars(self):
        """\
        All local variables (IVariables) defined in this CU, indexed
        by symbol.
        """
        raise PureVirtualError(self)
    
    
# -----------------------------------------------------------------------------
# Firmware Variable Instance Interfaces
# -----------------------------------------------------------------------------

class IVariable(object):
    """\
    Firmware Variable Instance (Interface)
    
    Interface to structured and primitive variables ... as well as constant
    so-called "variables".
    
    Future:-
    - relate to type description.
    """

    @property 
    def mem(self):
        """\
        The IMemoryRegion occupied by this variable in Firmware's data 
        address space.
        
        Can be used for raw access.
        
        Prefer higher level access via .value property if available (and fast
        enough).
        """
        raise PureVirtualError(self) 

    # By default, reading an IVariable just returns the IVariable, and writing
    # it is impossible.  But we could override this behaviour for types that
    # map directly to Python types (i.e. ints).  See 
    # _Variable.USE_PROPERTIES_FOR_INTS.
    def get(self):
        return self
    set = None

class IInt (IVariable):

    def get_value(self):
        raise PureVirtualError(self) 

    def set_value(self, new_value):
        raise PureVirtualError(self) 

    value = property(lambda o: o.get_value(), lambda o,v: o.set_value(v))

class IEnum (IInt):

    def get_symbolic_value(self):
        raise PureVirtualError(self) 

    def set_symbolic_value(self, new_value):
        raise PureVirtualError(self) 

    symbolic_value = property(lambda o: o.get_symbolic_value(), lambda o,v: o.set_symbolic_value(v))
    
    def __eq__(self, other):
        if isinstance(other, str):
            return self.symbolic_value == other
        elif isinstance(other, int_type):
            return self.value == other
        elif isinstance(other, IEnum):
            return self.value == other.value
        else:
            raise TypeError
    
    def get(self):
        return self.symbolic_value
    
    def set(self):
        if isinstance(other, str):
            return self.symbolic_value == other
        elif isinstance(other, int_type):
            return self.value == other
        else:
            raise TypeError

class IStruct (IVariable):
    
    @property   
    def members(self):
        """\
        Dictionary of members (IVariables)
        """         
        raise PureVirtualError(self) 

    # Convenience (move implementation to a base class)
    
    def __getitem__(self, member_name):
        """\
        Access a member by name.
        
        s[member_name] == s.members[member_name]
        """
        return self.members[member_name].get()

    def __iter__(self):
        """\
        Iterate over members.

        for m in s: == for m in s.members
        """
        return self.members.__iter__()


class IArray (IVariable):
    
    @property   
    def elements(self):
        """\
        Array of elements (IVariables)
        
        N.B. The elements are descriptors for underlying firmware elements 
        _not_ values. You may be able to assign to their 
        .values (if simple and writable) but not to the array elements 
        themselves.
        """         
        raise PureVirtualError(self) 

    # Convenience (move implementation to a base class)
    
    def __getitem__(self, i):
        """\
        Access elements by index.
        
        So a[i] is shorthand for a.elements[i]
        """
        return self.elements[i].get()

    def __setitem__(self, i, v):
        
        el = self.elements[i]
        if el.set is not None:
            el.set(v)
        else:
            raise TypeError("Element type does not support assignment")
        
    def __iter__(self):
        """\
        Iterate over elements.

        for e in a: == for e in a.elements
        """
        return self.elements.__iter__()


# -----------------------------------------------------------------------------
#
# -----------------------------------------------------------------------------


class IMemoryRegion(object):
    """\
    Access to a memory region/subregion.
    """
    
    def __len__(self):
        
        raise PureVirtualError(self) 
            
    def __getitem__(self, index_or_slice):
        
        raise PureVirtualError(self) 

    def __setitem__(self, index_or_slice, value):
        
        raise PureVirtualError(self) 
    
    # Convenience (move implementation to a base class)
    
    def set(self, value):
        """\
        Set the entire region to the specified integer value.
        """
        self[:] = [value] * len(self)

    def clear(self):
        """\
        Clear the entire region to 0.
        """
        self.set(0)        


class MemorySubregion (IMemoryRegion):
    """
    A memory sub-region.
    
    Sub regions can nest.
    """
    
    def __init__(self, parent, offset, length):
                
        assert offset + length <= len(parent)
         
        self._parent = parent
        self._offset = offset
        self._length = length
            
    # IMemoryRegion compliance
    
    def __len__(self):
        
        return self._length
        
    def __getitem__(self, index):
                
        return self._parent[self._adjust_index(index)]
            
    def __setitem__(self, index, value):
        
        self._parent[self._adjust_index(index)] = value
    
    # Extensions (consider interface)
    
    @property
    def parent(self):
        """\
        Parent region.
        """
        return self._parent
        
    @property
    def offset(self):
        """\
        Offset w.r.t. parent region.
        """
        return self._offset
                
    # Private
    
    def _adjust_index(self, index):
        """\
        Offset index for application to the parent region.
        """
        return adjust_index(index, self._length, self._offset)

# -----------------------------------------------------------------------------
# Slice Helpers
# -----------------------------------------------------------------------------

def normalise_int_index(index, length):
    """\
    Normalise an integer index by converting any end-relative value to 
    an absolute one.
    
    There are a number of interfaces and utilities that don't work properly 
    with relative index values. This function returns absolute index value.
    
    Parameters:
    -- length: the size of the target container. This is required to normalise
    end-relative indices.
    """
    if index < 0:
        return length + index
    else:
        return index

def normalise_slice_index(index, length):
    """\
    Normalise a slice index by converting any implicit or 
    end-relative indices to absolute ones.
    
    There are a number of interfaces and utilities that don't work properly 
    with implicit and relative index values. This function returns a slice with 
    absolute start, stop and step values.
    
    Parameters:
    -- length: the size of the target container. This is required to normalise any
    end-relative indices.
    """
    start = index.start
    stop = index.stop
    step = index.step

    # Normalise the start
    if start is None: # means 0
        start = 0
    elif start < 0: # relative to end
        start = length + start
    
    # Normalise the stop
    if stop is None: # means end
        stop = length
    elif stop < 0: # relative to end
        stop = length + stop
                        
    # Normalise the step
    if step is None: # means 1
        step = 1
    
    return slice(start, stop, step)

def normalise_index(index, length):
    """\
    Normalise a slice or integer index by converting any implicit or 
    end-relative indices to absolute ones.
    
    There are a number of interfaces and utilities that don't work properly 
    with implicit and relative index values. This function returns an index
    with absolute integer value(s).
    
    The type of the index is preserved (int or slice).
    
    Parameters:
    -- length: the size of the target container. This is required to normalise any
    end-relative indices.
    """
    if isinstance(index, int):
        return normalise_int_index(index, length)
    elif isinstance(index, slice):
        return normalise_slice_index(index, length)
    else:
        raise TypeError("can only normalise int or slice indices")
    
def adjust_index(index, length, offset):
    """\
    Apply offset to integer or slice index.
    
    Parameters:
    -- length: the size of the target container. This is required to normalise 
    any implicit end index before applying offset.
    """
        
    # Normalise any magic values so can do the maths...
    index = normalise_index(index, length)
    
    if isinstance(index, int):
        return index + offset
    elif isinstance(index, slice):
        return slice(index.start + offset, index.stop + offset, index.step)                
    else:
        raise TypeError("can only adjust int or slice indices")

