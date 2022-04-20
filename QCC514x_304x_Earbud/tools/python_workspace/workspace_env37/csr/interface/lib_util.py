############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
#
# lib_util.py
# Utility functions for use in python test scripts

import itertools
from csr.wheels.bitsandbobs import bytes_to_dwords, bytes_to_words, \
                                                dwords_to_bytes, words_to_bytes

VLDATA_MORE_BIT = 7
VLDATA_MORE_BIT_MASK = 1 << VLDATA_MORE_BIT
VLDATA_SIGN_BIT = 6
VLDATA_SIGN_BIT_MASK = 1 << VLDATA_SIGN_BIT
VLDATA_TYPE_BIT = 5
VLDATA_TYPE_BIT_MASK = 1 << VLDATA_TYPE_BIT
VLDATA_OCTET_STRING_HDR_BITS = VLDATA_MORE_BIT_MASK | VLDATA_TYPE_BIT_MASK
VLDATA_OCTET_STRING_LEN_FIELD_LEN_MASK = 0x1f
VLDATA_SINGLE_OCTET_INT_MASK = 0x7f
OCTET_MASK = 0xff

VLINT_MORE_BIT = 7
VLINT_MORE_BIT_MASK = 1 << VLINT_MORE_BIT
VLINT_SIGN_BIT = 6
VLINT_SIGN_BIT_MASK = 1 << VLINT_SIGN_BIT

def twos_complement(num, size, to_unsigned=False):
    """
    @brief Converts a number in two's complement format into the appropriate
    python integer. It can deal with positive and negative numbers
    
    @param num Number to convert.

    @param size How many bits the number uses.
    """
    if to_unsigned:
        if num < 0:
            num += (1 << size)
    else:
        if num & (1 << (size - 1)): # if negative
            return num - (1 << size)
    return num

def hex_to_string(hex_numbers):
    outstring = ""
    for hex_value in hex_numbers:
        outstring += "%c" % hex_value
    return outstring

def make_octet_array(digitstring):
    '''
    Turns a string of digits into an array of octets in big-endian order
    '''
    strlen = len(digitstring)
    if strlen % 2 != 0:
        digitstring = "0" + digitstring
    num_octets = (strlen + 1) // 2
    return [int(digitstring[2*i:2*i+2],16) for i in range(num_octets)]

def vldata_unpack(vlint):
    '''
    @brief Returns a signed integer number or a byte array decoded from a 
    given vlint represented as a list of bytes. The not defined vlint will be
    returned as None.
    
    @param vlint Vlint represented as a list of bytes.
    '''
    if not vlint:
        return None
    hdr = vlint[0]
    vlint = vlint[1:]
    hdr_more = (hdr & VLDATA_MORE_BIT_MASK) != 0
    hdr_positive = (hdr & VLDATA_SIGN_BIT_MASK) == 0
    if hdr_more:
        hdr_integer = (hdr & VLDATA_TYPE_BIT_MASK) == 0
        hdr_length = hdr & VLDATA_OCTET_STRING_LEN_FIELD_LEN_MASK
        if hdr_length == 0:
            return None
        if hdr_integer:
            val = l8_to_uint(vlint, hdr_length)
            if hdr_positive:
                return val
            else:
                return twos_complement(val, hdr_length * 8)
        else:
            os_len = l8_to_uint(vlint, hdr_length)
            os = vlint[hdr_length:(hdr_length + os_len)]
            if os_len > len(os):
                raise IndexError
            return os
    else:
        val = hdr & VLDATA_SINGLE_OCTET_INT_MASK
        if hdr_positive:
            return val
        else:
            return twos_complement(val, VLDATA_SIGN_BIT + 1)

def vldata_pack(data_in):
    '''
    @brief Converts given data into the corresponding VLDATA. Data can be a
    signed integer or a byte array. None will be converted to the not defined
    vlint which is sometimes used for indicating special actions.
    
    @param data_in Data to be converted.
    '''
    if data_in == None:
        hdr_more = True
        hdr_positive = True
        hdr_integer = True
        hdr_length = 0
        strlen = []
        data = []
    elif isinstance(data_in,list):
        # Octet string encoding
        hdr_more = True
        hdr_positive = True
        hdr_integer = False
        strlen = uint_to_l8(len(data_in))
        hdr_length = len(strlen)
        data = data_in
    else:
        if (data_in >= -64) and (data_in <= 63):
            # Single byte encoding
            hdr_more = False
            hdr_positive = (data_in & VLDATA_SIGN_BIT_MASK) == 0
            hdr_integer = (data_in & VLDATA_TYPE_BIT_MASK) == 0
            hdr_length = data_in & VLDATA_OCTET_STRING_LEN_FIELD_LEN_MASK
            data = []
        else:
            # Multiple byte encoding
            hdr_more = True
            hdr_positive = data_in >= 0
            hdr_integer = True
            if hdr_positive:
                data = uint_to_l8(data_in)
            else:
                data = int_to_l8(data_in)
            hdr_length = len(data)
        strlen = []
    hdr = [(((1 if hdr_more else 0) << VLDATA_MORE_BIT) |
            ((1 if not hdr_positive else 0) << VLDATA_SIGN_BIT) |
            ((1 if not hdr_integer else 0) << VLDATA_TYPE_BIT)) +
           hdr_length]
    return hdr + strlen + data

def vlint_unpack(vlint):
    '''
    @brief Returns a signed integer number decoded from a given vlint represented as a list of bytes.

    @param vlint Vlint represented as a list of bytes.
    '''
    if not vlint:
        return None

    if (vlint[0] & VLINT_MORE_BIT_MASK) != 0:
        # Multi byte VLINT
        length = vlint[0] & 0x7F
        if (length == 0) or (len(vlint) < length):
            return None
        value = 0 if (vlint[1] & 0x80) == 0 else -1
        for i in range(0, length):
            value <<= 8
            value = value | vlint[1 + i]
        return value
    else:
        # Single byte VLINT
        if (vlint[0] & VLINT_SIGN_BIT_MASK) != 0:
            result = -1
            result = (result & ~0x7F) | (vlint[0] & 0x7F) # extend int7 to int8
            return result
        else:
            return vlint[0] & 0x7F

def vlint_pack(data_in):
    '''
    @brief Converts given data into the corresponding vlint.

    @param data_in Data to be converted.
    '''

    if data_in is None:
        return [0x80]

    if (data_in >= -64) and (data_in <= 63):
        return [data_in & (VLINT_MORE_BIT_MASK - 1)]
    else:
        ret = int_to_l8(data_in)
        if (data_in > 0) and (ret[0] & 0x80) != 0:
            ret.insert(0, 0)
        ret.insert(0, len(ret) | VLINT_MORE_BIT_MASK)
        return ret

def vldata_to_decimal(hex_numbers):
    '''
    Converts a string of octets interpreted as a VLDATA into the corresponding
    integer value
    '''
    return vldata_unpack(hex_numbers)

def decimal_to_vldata(decimal):
    """ Converts a normal integer into an array of integers (which should be
    interpreted as individual octets)
    """
    return vldata_pack(decimal)

def vlint_to_decimal(hex_numbers):
    '''
    Converts a string of octets interpreted as a VLINT into the corresponding
    integer value
    '''
    return vlint_unpack(hex_numbers)

def decimal_to_vlint(decimal):
    """ Converts a normal integer into an array of integers (which should be
    interpreted as individual octets)
    """
    return vlint_pack(decimal)

def octet_array_to_octet_string(value):
    '''
    Converts an octet array into an octet string by calculating the header
    and length octets.

    The elements in the list "value" must be valid octets, i.e. integers less
    than 256 and non-negative, but this is assumed rather than checked.

    ** NB For efficiency does not return a list object but a generator for
    the list (i.e. something you can do "for elem in <generator>" with). **
    '''
    return itertools.chain(vldata_pack(value))

def octet_string_to_octet_array(octets):
    '''
    Decodes the supplied octet string into a plain octet array.
    Note: the argument is an octet array encoded in the vldata format, not a
    Python string.
    '''
    return vldata_unpack(octets)

def l8_to_uint(l8, size = None):
    """
    @brief Helper function which returns an unsigned integer from a list of
    bytes. We are assuming the list uses big endian ordering. Initially this is
    meant to be used in the vlint decoder and the vlnit format uses big endian.
    
    @param l8 List of 8 bit integers.
    
    @param size Size of the expected integer in bytes. If the list is longer or
    shorter the final value will be truncated or padded.
    """
    ret = 0
    if size == None:
        size = len(l8)
    for i in range(size):
        ret = (ret << 8) + l8[i]
    return ret

def uint_to_l8(val, size = None):
    """
    @brief Helper function which returns a list of 8 bit integers from a
    single unsigned integer value.
    
    @param val Unsigned integer value to convert.
    
    @param size Size of the expected byte list. If the value is larger or
    smaller the final list will be truncated or padded.
    """
    ret = []
    if size == None:
        count = val
    else:
        count = (1 << (size*8)) - 1
    while count:
        ret.append(val & 0xff)
        val = val >> 8
        count = count >> 8
    ret.reverse()
    return ret

def int_to_l8(val, size = None):
    """
    @brief Helper function which returns a list of 8 bit integers from a
    single signed integer value.
    
    @param val Signed integer value to convert.
    
    @param size Size of the expected byte list. If the value is larger or
    smaller the final list will be truncated or padded.
    """
    if val >= 0:
        ret = uint_to_l8(val, size)
    else:
        ret = []
        bit = 0x100
        for i in uint_to_l8(~(val << 1), size):
            # Write the 8-th bit
            i = ((~i) & (~0x100)) | bit
            # Save bit 0 as the 8-th bit of the next byte
            bit = (i & 0x01) << 8
            # Get the processed octet
            i = (i >> 1) & 0xff
            ret.append(i)
    return ret

def unit_to_l8(addr_unit_bits, list_unit):
    """
    @brief Helper function which returns an 8 bit integer list from an 
    addressable unit sized integer list.
    
    @param addr_unit_bits
    Size of the addressable unit in bits.
    
    @param list_unit
    List of addressable unit sized integers.
    
    Checks how many octets the addressable unit takes and converts the list
    appropriately.
    """
    if addr_unit_bits // 8 == 1:
        ret = list_unit
    elif addr_unit_bits // 8 == 2:
        ret = words_to_bytes(list_unit)
    elif addr_unit_bits // 8 == 3:
        # not supported yet
        ret =  None
    elif addr_unit_bits // 8 == 4:
        ret = dwords_to_bytes(list_unit)
    else:
        ret = None
    return ret
    
def l8_to_unit(addr_unit_bits, l8):
    """
    @brief Helper function which returns an addressable unit sized integer
    list from an 8 bit integer list. 
    
    @param addr_unit_bits
    Size of the addressable unit in bits.
    
    @param l8
    List of 8 bit integers.
    
    Checks how many octets the addressable unit takes and converts the list
    appropriately.
    """
    if addr_unit_bits // 8 == 1:
        ret = l8
    elif addr_unit_bits // 8 == 2:
        ret = bytes_to_words(l8)
    elif addr_unit_bits // 8 == 3:
        # not supported yet
        ret =  None
    elif addr_unit_bits // 8 == 4:
        ret = bytes_to_dwords(l8)
    else:
        ret = None
    return ret

def bmsgel_unpack(bmsgel, n):
    '''
    Unpacks a bmsgel into a dict.
    
    n is the number of expected bytes out of this bmsgel although the real
    number of bytes stored may be smaller. The rest of them should be found
    in the next bmsgels. n is usually the index of the bmsg that owns this
    bmsgel.
    '''
    ret = dict()
    ret["next"] = bmsgel["next"].value
    ret["buflen"] = bmsgel["buflen"].value
    buf = bmsgel["buf"]
    
    # Save the buffer length and rewrite it to the real length
    num_elements_orig = buf._info._struct["num_elements"]
    buf._info._struct["num_elements"] = ret["buflen"] * 2
    
    ret["buf"] = []
    for i in range(min(n, ret["buflen"] * 2)):
        ret["buf"].append((buf[i // 2].value >> ((i % 2) * 8)) & 0xff)
    
    # Restore the buffer length
    buf._info._struct["num_elements"] = num_elements_orig
    
    return ret
    
def bmsg_unpack(bmsg):
    '''
    Unpacks a bmsg into a dict.
    '''
    ret = dict()
    ret["index"] = bmsg["index"].value
    ret["outdex"] = bmsg["outdex"].value
    if bmsg["list"].value != 0:
        bmsgel = bmsg["list"].deref
    else:
        #exception
        return None
    ret["list"] = []
    total = 0
    while ret["index"] > total:
        if bmsgel.address == 0:
            #exception
            return None
        ret["list"].append(bmsgel_unpack(bmsgel, ret["index"] - total))
        total = total + len(ret["list"][-1]["buf"])
        if bmsgel["next"].value == 0:
            break
        bmsgel = bmsgel["next"].deref
    return ret

def bmsg_get_data(bmsg_dict):
    '''
    Returns the data encoded by a bmsg as an 8 bit integer list
    '''
    ret = []
    data_len = bmsg_dict["index"]
    for bmsgel_dict in bmsg_dict["list"]:
        if data_len <= 0:
            #exception
            return None
        buflen = bmsgel_dict["buflen"] * 2
        if buflen <= data_len:
            ret = ret + bmsgel_dict["buf"]
            data_len = data_len - buflen
        else:
            ret = ret + bmsgel_dict["buf"][:data_len]
            data_len = 0
    return ret


