############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import re
import struct

class XUVStreamDecoder (object):
    """\
    Decodes Address-Value pairs from XUV stream. 
    
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
        self._av_re = re.compile('^@([0-9a-zA-Z]+) +([0-9a-zA-Z]+)')
        
    @property
    def address_value_pairs(self):
        """\
        Generates (address, value) integer tuples decoded from the stream.
        E.g. (0, 0) (1, 0) (2, 0xD00) (3, 0x92E0)
        """
        for line in self._xuv:
            m = self._av_re.match(line)
            if m:
                # This needs to cope with non-standard lines of the form
                #   @0x000000 1234
                # which old versions of XUVStreamEncoder emitted.
                addr = int(m.group(1), 16)
                val = int(m.group(2), 16)
                yield (addr, val)

    def _addr_from_line(self, line):
        return int(line.split()[0][1:], 16)
    
    def _value_from_line(self, line):
        return int(line.split()[1], 16)
    
    @property
    def value_block(self):
        '''
        Attempts to read the XUV file in a single block.
        '''
        xuv_lines = self._xuv.readlines()
        start = 0
        while not self._av_re.match(xuv_lines[start]):
            start += 1

        xuv_lines = xuv_lines[start:]

        start_addr = self._addr_from_line(xuv_lines[0])

        if len(xuv_lines) == self._addr_from_line(xuv_lines[-1]) + 1 - start_addr:
            #It's a single block contiguous block
            return ([int(line.split()[1], 16) for line in xuv_lines], start_addr)
        else:
            raise self.NotContiguous

    def chunks(self, max_chunk_size, alignment=None):
        '''
        Generator function that returns blocks of contiguous data of the
        requested max_chunk_size or smaller from the xuv file.
        If alignment is specified then the data chunks will not span
        a boundary of that alignment.
        '''
        data = []
        for (a,v) in self.address_value_pairs:
            
            #If we haven't started filling out this cmd yet, set the new
            #address
            if not data:
                address = a
            #If this address is contiguous with the last one, add the
            #value, and write the cmd if necessary
            if address + len(data) == a:
                data.append(v)
                #If doing register programming need to stop on a page boundary
                if (len(data) == max_chunk_size or
                        (alignment and (a & (alignment-1) == (alignment-1)))):
                    yield address,data                    
                    data = []

            #Otherwise, write what we already had and start a new cmd with 
            #the current values
            else:
                yield address,data
                address = a
                data = [v]
            
        #Return any remaining words
        if data:
            yield address,data
        return

    def byte_blocks(self):
        '''
        Generator function that returns multiple bytearrays of contiguous
        data from the xuv file. The return is a (word_address, bytearray)
        tuple.
        '''
        data = bytearray()
        for (a,v) in self.address_value_pairs:

            #If we haven't started filling out this cmd yet, set the new
            #address
            if not data:
                address = a
            #If this address is contiguous with the last one, add the
            #value, and write the cmd if necessary
            #N.B. len(data) gives a number of bytes, so we convert to words.
            if address + len(data)//2 == a:
                data += struct.pack("<H", v)

            #Otherwise, write what we already had and start a new cmd with
            #the current values
            else:
                yield address,data
                address = a
                data = struct.pack("<H", v)

        #Return any remaining words
        if data:
            yield address,data
        return

    def contiguous_byte_data(self):
        '''
        Returns the contents of the xuv file as a single contiguous
        bytearray with gaps padded with 0xff bytes. The return is a
        tuple of (byte_address, bytearray, padding_list) where the
        padding list is a list of tuples of (offset, length) to show
        where padding bytes were added.
        '''
        output_bytes = bytearray()
        start_byte_addr = None
        padding_list = []
        for word_block in self.byte_blocks():
            byte_len = len(word_block[1])
            byte_addr = word_block[0]*2
            if start_byte_addr is None:
                start_byte_addr = byte_addr
            else:
                padding_len = byte_addr - (start_byte_addr + len(output_bytes))
                if padding_len:
                    padding_list.append((len(output_bytes), padding_len))
                    output_bytes += bytearray([0xff] * padding_len)
            output_bytes += word_block[1]
        return (start_byte_addr, output_bytes, padding_list)


    class NotContiguous(ValueError):
        '''
        Exception indicating that the value_block property couldn't be returned
        since the addresses in the XUV file aren't contiguous
        '''
