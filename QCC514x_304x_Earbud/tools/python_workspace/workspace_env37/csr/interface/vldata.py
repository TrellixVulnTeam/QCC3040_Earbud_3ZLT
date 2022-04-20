############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
'''
CSR Variable Length Data format (VLData) readers & writers.

__packed struct VLDATA
{
    uint   more : 1;         // Bit 7: More octets follow = 1
    uint   sign : 1;         // Bit 6: Sign bit
    uint   type : 1;         // Bit 5: Type of following octets      1 = octet string, 0 = integer or part of single octet data value (bits 6-0)
    uint   length : 5;       // Bit 4-0: Length of data[] or stringLength in octets or part of single octet data value (bits 6-0)
    uint8  data[];           // Multiple octet data value (big endian)
}

N.B. Unlike most hydra comms the data for integers is BigEndian.

N.B. This implementation sets the sign bit. (I've seen some that don't - 
specialise if we need to support that)
'''

from csr.wheels.int import U8Writer, BEIntWriter

# ----------------------------------------------------------------------------
# Exceptions
# ----------------------------------------------------------------------------

class VLIntTooBigError(Exception):
    """
    VLData Can only encode ints of 2^5 octets or less.
    """ 
    pass

# ----------------------------------------------------------------------------
# Model
# ----------------------------------------------------------------------------

# No explicit model needed at present - formatters take python ints & lists

# ----------------------------------------------------------------------------
# IO
# ----------------------------------------------------------------------------

class VLStringWriter():
    """
    VLData Octet-String Writer
    
    Example:-
    
        StringWriter(ostream).write([255, 127, 0])
    """
    
    def __init__(self, ostream):
        self._u8_writer = U8Writer(ostream)
    
    def write(self, *u8_strings):
        """
        Write one or more lists of octet values in CSR VLString format.
        """
        for s in u8_strings:
            self._write_one(s)
        
    def _write_one(self, u8_string):
        """
        Write a list of octet values in CSR VLString format.
        """        
        # Header = more follow, type=string, 1 octet stringLength => 0xA1
        # data[0] = stringLength (1 octet)
        # data[1]... = The octet string data itself
        #
        # Potential extension:: Support stringLength of more than 255
        #
        length = len(u8_string) 
        if length > 255:
            raise NotImplementedError()         
        header = 0xA1
        
        self._u8_writer.write(header, length, *u8_string)

class VLIntWriter():
    """
    VLData Integer Writer
    
    Example - Single use:-
    
        IntWriter(ostream).write(0xdeadbeef)
        
    Example - Repeated use:-
    
        writer = IntWriter(ostream)
                
        writer.write(0xdeadbeef)
        ...
        writer.write(0x10)
        ...
        writer.write(-1234)
    """
    
    def __init__(self, ostream):
        self._ostream = ostream
    
    def write(self, *ints):        
        """
        Write one or more integers in CSR VLInt format.
        """
        for i in ints:
            self._write_one(i)
            
    def _write_one(self, i):
        """
        Write an integer in CSR VLInt format.
        """
        from io import BytesIO
        
        ostream = self._ostream
        
        if  i >= -0x40 and i <= 0x3F:
            # Special case. 6 bit integers pack into one octet.
            i &= 0x7F    # loose bit 8 but keep bit 7 as sign bit
            U8Writer(ostream).write(i)
        else:            
            # Convert int to BE octet sequence.
            # Use temp. buffer so can measure it.
            #
            tmp_stream = BytesIO()
            BEIntWriter(tmp_stream).write(i)
            be_data = tmp_stream.getvalue()
            tmp_stream.close()
            
            length = len(be_data)
            if length > (5 << 1):
                raise VLIntTooBigError()
            
            # Header value
            #
            more = 1
            sign = 0 if i == abs(i) else 1
            header = (more << 7) | (sign << 6) | length
            
            # Write header & BE data to stream
            #
            U8Writer(ostream).write(header)
            ostream.write(be_data)
