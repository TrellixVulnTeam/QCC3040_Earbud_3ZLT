############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import struct

class XUVStreamEncoder (object):
    """\
    Encodes Address-Value pairs to XUV stream. 
    
    Stream format:-
    
    // ROM START ADDRESS 0
    @000000   0000
    @000001   0000
    @000002   0D00
    @000003   92E0
    
    Refs:-
    - http://wiki/Xap_program_image_formats
    """
    def __init__(self, xuv_stream):
        
        self._xuv = xuv_stream
        
    def address_value_pair(self, address, value):
        """\
        Writes (address, value) pair to the stream.
        """
        tmp = "@%06x   %04x\n" % (address,value)
        self._xuv.write(tmp.upper())

    def write_byte_array(self, word_address, byte_data):
        '''
        Write an array of bytes to the xuv file with the parameter
        word_address being the starting address.
        '''
        for i in range(0, len(byte_data), 2):
            self.address_value_pair(word_address, struct.unpack("<H", byte_data[i:i+2])[0])
            word_address += 1
