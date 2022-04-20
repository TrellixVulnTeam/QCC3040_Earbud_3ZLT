############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.index_or_slice import IndexOrSlice

class SubArray (object):
    """\
    Generic sub-array.
    """    
    def __init__(self, array, start, stop):
        self._array = array
        self._start = start
        self._stop = stop
    
    def __len__(self):
        return self._stop - self._start
    
    def __getitem__(self, index):
        
        if isinstance(index, int):        
            return self._array[index + self._start]
        
        elif isinstance(index, slice):
            i = IndexOrSlice(index, len(self))   # normalise
            return self._array[i.start + self._start : i.stop + self._start]
        
        else:
            raise TypeError("only integer or slice indices supported")    
        
    def __setitem__(self, index, value):
        
        if isinstance(index, int):        
            self._array[index + self._start] = value
        
        elif isinstance(index, slice):
            i = IndexOrSlice(index, len(self))   # normalise
            self._array[i.start + self._start : i.stop + self._start] = value
        
        else:
            raise TypeError("only integer or slice indices supported")    
        
