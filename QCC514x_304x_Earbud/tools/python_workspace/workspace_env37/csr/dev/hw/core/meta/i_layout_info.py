############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import PureVirtualError, build_le, build_be,\
flatten_le, flatten_be, bytes_to_dwords, dwords_to_bytes

class ILayoutInfo(object):
    """
    Represents basic information about the core data layout
    """
    
    LITTLE_ENDIAN = 0
    BIG_ENDIAN = 1
    
    @property
    def endianness(self):
        """
        Indicate the endianness of data - either LITTLE_ENDIAN or BIG_ENDIAN
        """
        raise PureVirtualError
    
    @property
    def addr_unit_bits(self):
        """
        Number of bits in an addressable unit.  8 for everyone but XAP (16) and
        K24 (24)
        """
        raise PureVirtualError

    @property
    def data_word_bits(self):
        """
        Number of bits in the data word.
        """
        raise PureVirtualError

    @property
    def addr_units_per_data_word(self):
        """
        Default implementation.  Subclasses can make this return a constant if
        they think it's worthwhile
        """
        return self.data_word_bits // self.addr_unit_bits

    @property
    def code_word_bits(self):
        """
        Number of bits in the code word.
        """
        return self.data_word_bits

    @property
    def addr_units_per_code_word(self):
        """
        Default implementation.  Subclasses can make this return a constant if
        they think it's worthwhile
        """
        return self.code_word_bits // self.addr_unit_bits

    def deserialise(self, addr_units, as_words=False):
        """
        Function to turn a list of addressable units (bytes) into an integer
        according to the platform's endianness
        """
        raise PureVirtualError
    
    def serialise(self, integer, num_units, wrapping=False, from_words=False):
        """
        Function to turn a supplied integer into a list of num_units 
        addressable units(bytes) according to the platform's endianness
        """
        raise PureVirtualError


    def adjust_stream(self, input_stream, input_width, output_width,
                       set_endianness=None):
        
        if input_width < output_width:
            
            if output_width % input_width != 0:
                raise ValueError("Output width must be multiple of input width "
                                 "but %d and %d supplied" % (output_width, input_width))
            
            iunits_per_ounit = output_width // input_width
            i = 0
            input_units = []
            is_le = (set_endianness if set_endianness is not None 
                     else self.endianness) == self.LITTLE_ENDIAN
            build_fn = build_le if is_le else build_be 
            for iunit in input_stream:
                input_units.append(iunit)
                i += 1
                if i % iunits_per_ounit == 0:
                    yield build_fn(input_units, input_width)
                    input_units = []
            if input_units:
                iprint("WARNING: %d octets left over!" % len(input_units)) 

        elif output_width < input_width:

            if input_width % output_width != 0:
                raise ValueError("Input width must be multiple of output width "
                                 "but %d and %d supplied" % (input_width, output_width))
                
            ounits_per_iunit = input_width//output_width
            is_le = (set_endianness if set_endianness is not None 
                     else self.endianness) == self.LITTLE_ENDIAN
            flatten_fn = flatten_le if is_le else flatten_be 
            for iunit in input_stream:
                for ounit in flatten_fn(iunit, num_words=ounits_per_iunit,
                                        word_width=output_width):
                    yield ounit

        else: # same width - nothing to do 
            # When we drop support for Python 2 we can replace this with a 
            # yield from.
            for unit in input_stream:
                yield unit


    def word_stream_from_octet_stream(self, octet_stream, set_endianness=None,
                                      octets_per_word=None):
        """
        Turn a sequence of octets into a corresponding sequence of words based
        on the word width.  By default the endianness of the octet stream is
        assumed to correspond to that of the platform but this can be overridden
        """
        
        if octets_per_word is None:
            octets_per_word = self.data_word_bits // 8
        return self.adjust_stream(octet_stream, 8, octets_per_word*8, 
                                  set_endianness=set_endianness)
        
    def octet_stream_from_word_stream(self, word_stream, set_endianness=None,
                                      octets_per_word=None):
        """
        Turn a sequence of octets into a corresponding sequence of words based
        on the word width.  By default the endianness of the octet stream is
        assumed to correspond to that of the platform but this can be overridden
        """
        if octets_per_word is None:
            octets_per_word = self.data_word_bits // 8
        return self.adjust_stream(word_stream, octets_per_word*8, 8,
                                  set_endianness=set_endianness)
            

    def addr_unit_stream_from_octet_stream(self, octet_stream, 
                                           set_endianness=None):
        """
        Turn a sequence of octets into a corresponding sequence of 
        addr_units based on the addr_unit width.  By default the endianness of
        the stream is assumed to correspond to that of the platform but this can
        be overridden
        """
        return self.adjust_stream(octet_stream, 8, self.addr_unit_bits,
                                  set_endianness=set_endianness)

 
class XapDataInfo(ILayoutInfo):
    
    @property
    def endianness(self):
        return self.BIG_ENDIAN
    
    @property
    def addr_unit_bits(self):
        return 16
    
    @property
    def data_word_bits(self):
        return 16
    
    def deserialise(self, addr_units):
        return build_be(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_be(integer, num_units, self.addr_unit_bits, wrapping)

    def from_words(self, words):
        return words
    
    def to_words(self, bytes):
        return bytes

class XapLEDataInfo(XapDataInfo):
    """
    Layout info for XAP MMU, which for some reason is little-endian
    """
    @property
    def endianness(self):
        return self.LITTLE_ENDIAN
    
    def deserialise(self, addr_units):
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    
class KalimbaDataInfo(ILayoutInfo):
    
    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units, as_words=False):
        if as_words:
            return self.deserialise_as_words(addr_units)
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)


class Kalimba24DataInfo(KalimbaDataInfo):

    @property
    def addr_unit_bits(self):
        return 24
    
    @property
    def data_word_bits(self):
        return 24

    def to_words(self, addr_units):
        return addr_units
    
    def from_words(self, words):
        return words

class Kalimba32DataInfo(KalimbaDataInfo):

    @property
    def addr_unit_bits(self):
        return 8
    
    @property
    def data_word_bits(self):
        return 32

    def to_words(self, addr_units):
        return bytes_to_dwords(addr_units)

    def from_words(self, words):
        return dwords_to_bytes(words)


class Generic32BitLEDataInfo(ILayoutInfo):

    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units, as_words=False):
        if as_words:
            return bytes_to_dwords(addr_units)
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    @property
    def addr_unit_bits(self):
        return 8
    
    @property
    def data_word_bits(self):
        return 32
    
    def to_words(self, addr_units):
        return bytes_to_dwords(addr_units)

    def from_words(self, words):
        return dwords_to_bytes(words)

class ArmCortexMDataInfo(Generic32BitLEDataInfo):
    pass

class XtensaDataInfo(Generic32BitLEDataInfo):
    pass
    


class X86DataInfo(ILayoutInfo):
    
    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units):
        return build_le(addr_units, self.addr_unit_bits)
    

    @property
    def addr_unit_bits(self):
        return 8
    
    @property
    def data_word_bits(self):
        return 32

class X86_64DataInfo(ILayoutInfo):
    
    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units):
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    @property
    def addr_unit_bits(self):
        return 8
    
    @property
    def data_word_bits(self):
        return 64


class ChipmateDataInfo(ILayoutInfo):

    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units, as_words=False):
        if as_words:
            return bytes_to_dwords(addr_units)
        return build_le(addr_units, self.addr_unit_bits)

    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    def to_words(self, addr_units):
        return bytes_to_dwords(addr_units)

    def from_words(self, words):
        return dwords_to_bytes(words)

    @property
    def addr_unit_bits(self):
        return 8

    @property
    def data_word_bits(self):
        return 16


class RISCVDataInfo(ILayoutInfo):
    
    @property
    def endianness(self):
        return self.LITTLE_ENDIAN

    def deserialise(self, addr_units, as_words=False):
        if as_words:
            return bytes_to_dwords(addr_units)
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    @property
    def addr_unit_bits(self):
        return 8
    
    @property
    def data_word_bits(self):
        return 32
    
    def to_words(self, addr_units):
        return bytes_to_dwords(addr_units)

    def from_words(self, words):
        return dwords_to_bytes(words)

