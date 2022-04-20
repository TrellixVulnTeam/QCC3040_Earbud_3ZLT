############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import IndexOrSlice
import sys
if sys.version_info > (3,):
    # Python 3
    int_type = int
else:
    # Python 2
    int_type = (int, long)


class MemoryPointer (object):
    """
    Reference to a location in a connected device's MemoryRegion (Immutable)

    Like C pointer - but low level - use with any MemoryRegion.
    
    Example use:-
    
        ptr = Pointer(data_space, 0x8000)
        ptr[0] = 0x1234
        x = ptr[8]
    """
    def __init__(self, mem_space, offset, length=None):
        
        self._mem = mem_space
        self._offset = offset
        self._length = length
    
    def __getitem__(self, index_or_slice):
        """\
        Read from referenced location + offset or a slice
        of a referenced location
        """
        if isinstance(index_or_slice, int_type):
            # Caller passed array so use it as is
            return self._mem[self._offset + index_or_slice]
        elif isinstance(index_or_slice, slice):
            start = IndexOrSlice(index_or_slice).start
            stop = IndexOrSlice(index_or_slice).stop
            if stop is not None:
                return self._mem[self._offset+start:self._offset+stop]
            return  self._mem[self._offset+start:]
        else:
            raise TypeError()


        
  
    def __setitem__(self, index_or_slice, value):
        """\
        Write to referenced location + index or a slice
        of a referenced location
        """

        if isinstance(index_or_slice, int_type):
            # Caller passed array so use it as is
            self._mem[self._offset + index_or_slice] = value
        elif isinstance(index_or_slice, slice):
            start = IndexOrSlice(index_or_slice).start
            stop = IndexOrSlice(index_or_slice).stop
            self._mem[self._offset+start:self._offset+stop] = value
        else:
            raise TypeError()

    def __len__(self):
        return self._length


    def read(self):
        
        return self[0]    
    
    def write(self, value):
        
        self[0] = value
