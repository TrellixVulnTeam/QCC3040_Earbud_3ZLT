############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2011 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
'''
Integer Utilities
'''

class Int:
    '''
    Integer Utilities
    '''
        
    # Constants
    min_u8 = 0
    max_u8 = 0xFF        
    min_u16 = 0
    max_u16 = 0xFFFF        
    min_u32 = 0
    max_u32 = 0xFFFFFFFF

    @staticmethod
    def is_in_u8_range(i):
        return (i >= Int.min_u8) and (i <= Int.max_u8)
    
    @staticmethod
    def is_in_u16_range(i):
        return (i >= Int.min_u16) and (i <= Int.max_u16)
    
    @staticmethod
    def is_in_u32_range(i):
        return (i >= Int.min_u32) and (i <= Int.max_u32)

# ----------------------------------------------------------------------------
# Formatters
# ----------------------------------------------------------------------------

class U8Writer(object):
    """
    Writes U8 integers to an octet stream.
    
    Example:-
    
        U8Writer(ostream).write(123, 45, 0x12)
    
    If you prefer to have "ostream.write(chr(123),chr(45),chr(0x12))" all over 
    your code then go ahead.
    """
    def __init__(self, ostream):
        self._ostream = ostream
    
    def write(self, *u8s):
        for u8 in u8s:
            self._ostream.write(chr(u8))

    
class BEU16Writer(object):
    """
    Writes U16 integers as BigEndian octet stream.
    
    Example: One-off use:-
    
        BEU16Writer(ostream).write(0x1234, 0xFFFF, 0x12)
        
    Example: Interleaved use:-
    
        beu16_writer = BEU16Writer(ostream)
        ...
        beu16_writer.write(0x1234)
        ...
        beu16_writer.write(0xFFFF)
        ...
        beu16_writer.write(0x12)
        
    If you prefer to have "ostream.write(struct.pack('>H', 0x1234))" all over 
    your code then go ahead.
    """
    def __init__(self, ostream):
        self._u8writer = U8Writer(ostream)
    
    def write(self, *u16s):
        for u16 in u16s:
            self._write_one(u16)
            
    def _write_one(self, u16):
        assert(Int.is_in_u16_range(u16))
        self._u8writer.write((u16 >> 8) & 0xFF, u16 & 0xFF)


class LEU16Writer(object):
    """
    Writes U16 integers as LittleEndian octet stream.
    
    Example:-
    
        LEU16Writer(ostream).write(0x1234, 0xFFFF, 0x12)
        
    If you prefer to have "ostream.write(struct.pack('<H', 0x1234))" all over 
    your code then go ahead.
    """
    def __init__(self, ostream):
        self._u8writer = U8Writer(ostream)
    
    def write(self, *u16s):
        for u16 in u16s:
            self._write_one(u16)
            
    def _write_one(self, u16):
        assert(Int.is_in_u16_range(u16))
        self._u8writer.write(u16 & 0xFF, (u16 >> 8) & 0xFF)


class BEIntWriter(object):
    """
    Writes native integers to shortest possible BigEndian sequence of octets.
    
    "0" is represented by a single octet.
    Handles -ve integers.
    
    Known Uses:-
    - vldata
    """
    
    def __init__(self, ostream):
        self._u8_writer = U8Writer(ostream)
    
    def write(self, *ints):
        for i in ints:
            self._write_one(i)
            
    def _write_one(self, i):       
        if (i >= -0x80 and i <= 0xff):
            # Can represent in one octet: Write it.
            self._u8_writer.write(i & 0xff)
        else:
            # Too big for one octet: Pick off LSB and recurse.
            self._write_one(i >> 8)
            self._u8_writer.write(i & 0xff) 
