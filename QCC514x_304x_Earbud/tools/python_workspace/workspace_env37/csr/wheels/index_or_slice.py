############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .bitsandbobs import TypeCheck
import sys
if sys.version_info > (3,):
    # Python 3
    int_type = (int,)
else:
    # Python 2
    int_type = (int, long)


class IndexOrSlice (object):
    """\
    Helper/wrapper object for dealing with/normalising arguments 
    that can be either integral indices or slices.
    
    Python slice syntax encourages implicit (==None) and end-relative (-ve)
    start & stop values and optional step values that play havoc with
    simple maths. This helper is intended to normalise the various special
    cases. E.g. so that integer maths can be performed on them.
    
    Note:-
    - This class normalise the maths but clients may still need to be aware that
    functions passing a slice typically expect an array to be returned/passed 
    whereas those passing an integer expect a single value.
    
    FAQ:-
    - Will this ever be extended to handle non integral indices? - No.
    
    Future:-
    - handle optional length and implicit, end-relative indices.
    """
    def __init__(self, index_or_slice, array_length=None):
        """\
        Construct IndexOrSlice helper object from integral index or slice. 
        """
        self._socket = index_or_slice
        self._len = array_length

    @property
    def start(self):
        """\
        Inclusive Start index whether index or slice.
        """
        index = self._socket
        try:
            return int(index)
        except TypeError: 
            # not convertible to int - assume a slice
            if index.start is None:
                return 0
            else:
                return int(index.start)
    
    @property
    def stop(self):
        """\
        Exclusive stop index whether index or slice.
        """
        index = self._socket
        try:
            return int(index) + 1
        except TypeError:
            # not convertible to int - assume a slice
            if index.stop is None:
                return self._len
            return int(index.stop)
    
    @property
    def step(self):
        """\
        Step
        """
        index = self._socket
        try:
            int(index)
        except TypeError:
            if index.step is None:
                return 1
            return int(index.step)
        else:
            return 1
    
    @property
    def length(self):
        """\
        Length of indexed span. 
        1 if simple index. N.B. Will be 1 for a 1 element slice as well.
        """
        return self.stop - self.start
    
    def does_span(self, index_or_slice):
        """\
        Does this IndexOrSlice fully span the specified index or slice? 
        """
        other = IndexOrSlice(index_or_slice) # wrap the other slice as well
        return self.start <= other.start and other.stop <= self.stop
