############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

#
#Copyright (c) 1987, 1993
#   The Regents of the University of California.  All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions
#are met:
#1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#4. Neither the name of the University nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
#ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
#FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#SUCH DAMAGE.
#
#   @(#)sysexits.h    8.1 (Berkeley) 6/2/93
#

# pylint: disable=too-many-lines
"""
Lots of bits and bobs functions including ones for manipulating
bytes/words/dwords to/from dwords/words/bytes.
"""

# Try and be python version agnostic
try:
    # Python3
    from itertools import zip_longest as zip_longest # pylint: disable=useless-import-alias
    from collections import UserDict
    long = int # pylint: disable=redefined-builtin
except ImportError:
    # Python2
    # pylint: disable=ungrouped-imports
    from itertools import izip_longest as zip_longest
    from UserDict import UserDict
import sys
import os
from os.path import exists
import functools
import inspect
import platform
import string
import re
import subprocess as SP
import contextlib
import time
from datetime import datetime
import itertools

try:
    # pylint: disable=ungrouped-imports
    from collections.abc import Mapping, Container, ByteString, Iterable
except ImportError:
    # pylint: disable=ungrouped-imports
    from collections import Mapping, Container, Iterable
    ByteString = str
import copy
import math
import io
try:
    # py 2
    from StringIO import StringIO
except ImportError:
    # pylint: disable=ungrouped-imports
    from io import StringIO

from .global_streams import iprint
from . import global_streams as gstrm


try:
    import msvcrt
    WINDOWS_CRT = True
except ImportError:
    WINDOWS_CRT = False

if sys.version_info > (3,):
    input_func = input # pylint: disable=invalid-name
else:
    input_func = raw_input # pylint: disable=invalid-name, undefined-variable

class NameSpace(object): # pylint: disable=too-few-public-methods
    """\
    Object you can add arbitrary attributes to easily (object wont play)
    """

class StaticNameSpaceDict(object): # pylint: disable=too-few-public-methods
    '''
    Access a static dictionary as a namespace
    (gives command line TAB completion)
    '''
    def __init__(self, ref_dict, strip_leading=None):
        for reg in ref_dict.keys():
            if strip_leading and reg.startswith(strip_leading):
                reg_name = reg[len(strip_leading):]
            else:
                reg_name = reg
            setattr(type(self), reg_name, ref_dict[reg])

class TypeCheck(object): # pylint: disable=too-few-public-methods
    '''
    Asserts that object is of type or sub-type (transitive).

    Use this if:-
    - you don't want your duck-pond to be taken over by pterodactyls.
    '''
    def __init__(self, obj, obj_type):
        if isinstance(obj, LazyProxy):
            if not LazyProxy.isinstance(obj, obj_type):
                description = "'%s' is not a '%s'" % (str(obj), str(obj_type))
                raise TypeError(description)

        elif not isinstance(obj, obj_type):
            description = "'%s' is not a '%s'" % (str(obj), str(obj_type))
            raise TypeError(description)

class PureVirtualError(NotImplementedError):
    """
    Raised to indicate that a call has been made to a pure virtual method.

    Serves to document and enforce intent.

    Example:-

    > class MyAbstractBase:
    >    def method_1(self): raise PureVirtualError(self, "method_1")
    >    def method_2(self): raise PureVirtualError(self, "method_2")
    >    def method_3(self): default_implementation()

    You should seriously consider using abc.ABCMeta instead of this class.
    """
    def __init__(self, in_obj=None, method_name=None):

        NotImplementedError.__init__(self)
        self._in_obj = in_obj

        if method_name is None:
            try:
                method_name = inspect.getouterframes(
                    inspect.currentframe())[1][3]
            except TypeError:
                # Unfortunately inspect raises TypeError if there are Mocks
                # involved here because it ends up getting a Mock when it's
                # expecting a string, due to Mocks' auto-attribute creation
                # behaviour.
                method_name = "(unknown)"
        self._method_name = method_name

    def __str__(self):
        if self._method_name:
            method_name = self._method_name
        else:
            method_name = ''
        if self._in_obj:
            extra = ' in {}'.format(self._in_obj)
        else:
            extra = ''
        return "Pure Virtual Method {} has not been overridden{}".format(
            method_name, extra)


class exitcode(object): # pylint: disable=invalid-name,too-few-public-methods
    """
    Standard-ish Script Exit codes.
    """

    # From bash
    #
    OK = 0
    CATCHALL = 1 # overused = avoid
    # ...etc...

    # C exit codes (from sysexits.h)
    #
    USAGE = 64      # command line usage error
    CANTCREAT = 73  # can't create (user) output file
    # ...etc...
    # others are, e.g.:
    #define EX_USAGE    64    /* command line usage error */
    #define EX_DATAERR    65    /* data format error */
    #define EX_NOINPUT    66    /* cannot open input */
    #define EX_NOUSER    67    /* addressee unknown */
    #define EX_NOHOST    68    /* host name unknown */
    #define EX_UNAVAILABLE    69    /* service unavailable */
    #define EX_SOFTWARE    70    /* internal software error */
    #define EX_OSERR    71    /* system error (e.g., can't fork) */
    #define EX_OSFILE    72    /* critical OS file missing */
    #define EX_CANTCREAT    73    /* can't create (user) output file */
    #define EX_IOERR    74    /* input/output error */
    #define EX_TEMPFAIL    75    /* temp failure; user is invited to retry */
    #define EX_PROTOCOL    76    /* remote error in protocol */
    #define EX_NOPERM    77    /* permission denied */
    #define EX_CONFIG    78    /* configuration error */


class HexDisplayInt(int):
    '''
    Int subclass to display integers in hex format
    '''
    @classmethod
    def __new__(cls, *args, **kwargs):
        return int.__new__(*args, **kwargs)

    def __repr__(self):
        return "0x%x" % self


class HexDisplayLong(long):
    '''
    long subclass to display longs in hex format
    '''
    @classmethod
    def __new__(cls, *args, **kwargs):
        return long.__new__(*args, **kwargs)

    def __repr__(self):
        return "0x%x" % self

    __str__ = __repr__

def _display_int_hex(value):
    return value


def _display_ints_hex(value):
    '''
    Factory function that converts ints and longs into the appropriate hex-
    display versions
    '''
    if isinstance(value, int):
        value = HexDisplayInt(value)
    elif isinstance(value, long):
        value = HexDisplayLong(value)
    elif isinstance(value, list):
        value = [_display_ints_hex(v) for v in value]
    elif isinstance(value, tuple):
        value = tuple(_display_ints_hex(v) for v in value)
    elif isinstance(value, dict):
        value = dict((key, _display_ints_hex(v)) \
                                            for (key, v) in value.items())
    return value


def display_hex(func):
    '''
    Decorator which converts standalone integers and integers found inside
    lists and tuples in HexDisplayInts, which are identical to ints except
    that they are
    '''
    @functools.wraps(func)
    def _display_hex(*args, **kwargs):

        ret = func(*args, **kwargs)
        return _display_ints_hex(ret)

    return _display_hex


def display_hex_gen(func):
    '''
    Decorator which dynamically converts and re-yields iterable values as
    hex-display integers
    '''
    @functools.wraps(func)
    def _display_hex_gen(*args, **kwargs):

        for value in func(*args, **kwargs):
            yield _display_ints_hex(value)

    return _display_hex_gen


def set_hex_display():
    '''
    To change the way the interpreter prints interactively entered expressions,
    you can use this function to rebind sys.displayhook to a callable object;
    a decorator that sets the sys displayhook to hex temporarily so that
    function output appears in hex.  Only works for individual integers,
    sadly.  Also, if the value is assigned instead of printed, the displayhook
    remains modified until something else calls it.
    '''
    def _hex_ints(obj):
        if isinstance(obj, (int, long)):
            iprint(hex(obj))
        else:
            iprint(repr(obj))

    old_hook = sys.displayhook
    sys.displayhook = _hex_ints
    return old_hook

def restore_display(hook=sys.__displayhook__):
    """
    Restores sys.displayhook by default to the value at the start of execution
    of the program, or otherwise to the value passed in.
    See also set_hex_display() above.
    """
    sys.displayhook = hook

def convert_size_to_power_2(size):
    '''
    Convert size to lower power of 2
    '''
    if not size:
        return 0
    return 2**int(math.floor(math.log(size, 2)))

def build_be(words, word_width=16):
    """
    builds a number from the Big Endian sequence values in words,
    (i.e. beginning of list holds the Big End of the resulting number),
    treating each value as an entity with word_width bits. E.g.

    >>> hex(build_be([0x12, 0x34, 0x56, 0x78], word_width=8))
    '0x12345678'

    >>> hex(build_be([0x0102], word_width=16))
    '0x102'
    >>> hex(build_be([0x0102, 0x0304]))
    '0x1020304'
    >>> hex(build_be([0x0102, 0x0304, 0x0506, 0x0708]))
    '0x102030405060708L'
    >>> hex(build_be([0x0102, 0x0304, 0x0506, 0x0708, 0x090A]))
    '0x102030405060708090aL'

    >>> hex(build_be([0x010203, 0x040506], word_width=24))
    '0x10203040506L'

    >>> hex(build_be([0x01020304, 0x05060708], word_width=32))
    '0x102030405060708L'
    """
    value = 0
    for word in words:
        value <<= word_width
        value |= word
    return value

def build_le(words, word_width=16):
    """
    builds a number from the Little Endian sequence values in words,
    (i.e. beginning of list holds the Little End of the resulting number),
    treating each value as an entity with word_width bits. E.g.

    >>> hex(build_le([0x12, 0x34, 0x56, 0x78], word_width=8))
    '0x78563412'

    >>> hex(build_le([0x0102, 0x0304]))
    '0x3040102'
    >>> hex(build_le([0x0102, 0x0304, 0x0506, 0x0708]))
    '0x708050603040102L'

    >>> hex(build_le([0x010203, 0x040506], word_width = 24))
    '0x40506010203L'

    >>> hex(build_le([0x01020304, 0x05060708], word_width=32))
    '0x506070801020304L'
    """
    value = 0
    shift = 0
    for word in words:
        value |= (word << shift)
        shift += word_width
    return value

def flatten_be(value, num_words, word_width=16, wrapping=False):
    r"""
    Operates on the value, which is something convertible to an integer.
    Splits it into a number of words of the specified word_width.
    When wrapping is True, the input value is first masked to the number of
    bits required for the num_words of word_width.
    The Big End of the value comes at the beginning of the returned list.
    of words. E.g.

    >>> [hex(x) for x in flatten_be(0x12345678, 4, word_width=8)]
    ['0x12', '0x34', '0x56', '0x78']

    >>> [hex(x) for x in flatten_be(0x12345678, 2, word_width=16)]
    ['0x1234', '0x5678']

    >>> flatten_be(0x12345678, 1)
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
      File "......\pylib\csr\wheels\bitsandbobs.py", line 329, in flatten_be
      File "......\pylib\csr\wheels\bitsandbobs.py", line 351, in flatten_le
        >>> hex(build_le([0x0102, 0x0304]))
    ValueError: 0x12345678 too large to fit in 1 16-bit words!

    >>> [hex(x) for x in flatten_be(0x12345678, 1, word_width=32)]
    ['0x12345678L']

    >>> [hex(x) for x in flatten_be(0x1234, 1)]
    ['0x1234']

    # Demonstrate purpose of wrapping to mask over-sized input value first:
    >>> [hex(x) for x in flatten_be(0x1FEDCBA98, 2,word_width=16,wrapping=True)]
    ['0xfedcL', '0xba98L']
    >>> [hex(x) for x in flatten_be(0x1FEDCBA98, 2,word_width=16)]
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
      File "......\pylib\csr\wheels\bitsandbobs.py", line 415, in flatten_be
        be_words = flatten_le(value, num_words, word_width, wrapping)
      File "......\pylib\csr\wheels\bitsandbobs.py", line 475, in flatten_le
        value, num_words, word_width))
    ValueError: 0x1fedcba98 too large to fit in 2 16-bit words!
    """
    be_words = flatten_le(value, num_words, word_width, wrapping)
    be_words.reverse()
    return be_words

def flatten_le(value, num_words, word_width=16, wrapping=False):
    r"""
    Operates on the value, which is something convertible to an integer.
    Splits it into a number of words of the specified word_width.
    When wrapping is True, the input value is first masked to the number of
    bits required for the num_words of word_width.

    The Little End of the value comes at the beginning of the returned list of
    words. E.g.
    >>> [hex(x) for x in flatten_le(0x12345678, 4, word_width=8)]
    ['0x78', '0x56', '0x34', '0x12']

    >>> [hex(x) for x in flatten_le(0x12345678, 2, word_width=16)]
    ['0x5678', '0x1234']

    >>> flatten_le(0x12345678, 1)
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
      File "........\pylib\csr\wheels\bitsandbobs.py", line 351, in flatten_le
        >>> hex(build_le([0x010203, 0x040506], word_width = 24))
    ValueError: 0x12345678 too large to fit in 1 16-bit words!

    >>> [hex(x) for x in flatten_le(0x12345678, 1, word_width=32)]
    ['0x12345678L']

    >>> [hex(x) for x in flatten_le(0x1234, 1)]
    ['0x1234']

    # Demonstrate purpose of wrapping to mask over-sized input value first:
    >>> [hex(x) for x in flatten_le(0x1FEDCBA98, 2,word_width=16,wrapping=True)]
    ['0xba98L', '0xfedcL']

    >>> [hex(x) for x in flatten_le(0x1FEDCBA98, 2,word_width=16)]
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
      File "........\pylib\csr\wheels\bitsandbobs.py", line 351, in flatten_le
        >>> hex(build_le([0x0102, 0x0304]))
    ValueError: 0x1fedcba98 too large to fit in 2 16-bit words!
    """
    value = int(value)
    num_words = int(num_words)
    if wrapping:
        bit_mask = (1 << (num_words * word_width)) - 1
        value &= bit_mask

    try:
        words = [0]*num_words
        index = 0
        shifted_value = value
        while shifted_value:
            words[index] = (shifted_value & ((1 << word_width) -1))
            index += 1
            shifted_value >>= word_width
        return words
    except IndexError:
        raise ValueError("0x%x too large to fit in %d %d-bit words!" % (
            value, num_words, word_width))

def pack_unpack_data_be(data, from_width, to_width):
    """
    Packs/unpacks data in Big Endian arrangement.
    The source width (from_width) and destination width (to_width) are a number
    of bits; the relative size of these determines whether packing or unpacking
    occurs.
    """
    if to_width > from_width:
        ret_data = []
        num_to_pack = to_width // from_width
        for i in range(len(data) // num_to_pack):
            packed = 0
            for j in range(num_to_pack):
                packed |= (data[num_to_pack*i + j] <<
                           from_width * (num_to_pack - j - 1))
            ret_data.append(packed)

    elif to_width < from_width:
        to_mask = (1 << to_width) - 1
        ret_data = []
        num_to_unpack = from_width // to_width
        # iterating over range(len(data)) can be quicker,
        # but not with multiple indexing
        # so just iterate over the values instead
        for datum in data:
            unpacked = [0]*num_to_unpack
            for j in range(num_to_unpack):
                unpacked[j] = \
                    ((datum >> to_width*(num_to_unpack - j - 1)) & to_mask)
            ret_data += unpacked
    else:
        ret_data = data

    return ret_data

def pack_unpack_data_le(data, from_width, to_width):
    """
    Packs/unpacks data in Little Endian arrangement.
    The source width (from_width) and destination width (to_width) are a number
    of bits; the relative size of these determines whether packing or unpacking
    occurs.
    """
    if to_width > from_width:
        ret_data = []
        num_to_pack = to_width // from_width
        for i in range((len(data) + num_to_pack -1) // num_to_pack):
            packed = 0
            for j in range(num_to_pack):
                try:
                    packed |= (data[num_to_pack*i + j] << from_width * j)
                except IndexError:
                    # Any values we weren't supplied with are implicitly zero
                    pass
            ret_data.append(packed)

    elif to_width < from_width:
        to_mask = (1 << to_width) - 1
        ret_data = []
        num_to_unpack = from_width // to_width
        # iterating over range(len(data)) can be quicker,
        # but not with multiple indexing
        # so just iterate over the values instead
        for datum in data:
            unpacked = [0]*num_to_unpack
            for j in range(num_to_unpack):
                unpacked[j] = ((datum >> to_width*j) & to_mask)
            ret_data += unpacked
    else:
        ret_data = data

    return ret_data

def pack_unpack_data(data, from_width, to_width, from_le, to_le):
    """
    Packs/unpacks data using either pack_unpack_data_le or pack_unpack_data_be.
    The source width (from_width) and destination width (to_width) are a number
    of bits; the relative size of these determines whether packing or unpacking
    occurs.
    The boolean parameters from_le and to_le, determine whether Little Endian
    or Big Endian data format is assumed.
    """
    le = from_le if from_width < to_width else to_le
    func = pack_unpack_data_le if le else pack_unpack_data_be
    if func is pack_unpack_data_le:
        iprint("Selected to pack/unpack little-endian")
    elif func is pack_unpack_data_be:
        iprint("Selected to pack/unpack big-endian")
    else:
        iprint("Don't know what I'm doing")
    return func(data, from_width, to_width)

def bytes_to_dwords(bytes): # pylint: disable=redefined-builtin
    ''' Convert a list of bytes into a list of dword assuming little-endian
    packing
    '''
    return [a[0] | a[1]<<8 | a[2]<<16 | a[3]<<24
            for a in zip_longest(*[iter(bytes)]*4, fillvalue=0)]

def bytes_to_dwords_be(bytes): # pylint: disable=redefined-builtin
    ''' Convert a list of bytes into a list of dword assuming big-endian
    packing
    '''
    return [a[0]<<24 | a[1]<<16 | a[2]<<8 | a[3]
            for a in zip_longest(*[iter(bytes)]*4, fillvalue=0)]

def dwords_to_bytes(dwords):
    ''' Convert a list of dwords to a list of bytes assuming little-endian
    packing
    '''
    bytes = [] # pylint: disable=redefined-builtin
    for dword in dwords:
        bytes.append(dword &0xff)
        bytes.append((dword >> 8) &0xff)
        bytes.append((dword >> 16) &0xff)
        bytes.append((dword >> 24) &0xff)
    return bytes

def dwords_to_bytes_be(dwords):
    ''' Convert a list of dwords to a list of bytes assuming big-endian
    packing
    '''
    bytes = [] # pylint: disable=redefined-builtin
    for dword in dwords:
        bytes.append((dword >> 24) &0xff)
        bytes.append((dword >> 16) &0xff)
        bytes.append((dword >> 8) &0xff)
        bytes.append(dword &0xff)
    return bytes

def bytes_to_words(bytes): # pylint: disable=redefined-builtin
    ''' Convert a list of bytes into a list of words assuming little-endian
    packing
    '''
    return [a[0] | a[1]<<8 for a in zip_longest(*[iter(bytes)]*2, fillvalue=0)]

def bytes_to_words_be(bytes): # pylint: disable=redefined-builtin
    ''' Convert a list of bytes into a list of word assuming big-endian
    packing
    '''
    return [a[0]<<8 | a[1] for a in zip_longest(*[iter(bytes)]*2, fillvalue=0)]

def words_to_bytes(words):
    ''' Convert a list of words to a list of bytes assuming little-endian
    packing
    '''
    bytes = [] # pylint: disable=redefined-builtin
    for word in words:
        bytes.append(word &0xff)
        bytes.append((word >> 8) &0xff)
    return bytes

def words_to_bytes_be(words):
    ''' Convert a list of words to a list of bytes assuming big-endian
    packing
    '''
    bytes = [] # pylint: disable=redefined-builtin
    for word in words:
        bytes.append((word >> 8) &0xff)
        bytes.append(word &0xff)
    return bytes


def dwords_to_words(dwords):
    ''' Convert a list of dwords to a list of words assuming little-endian
    packing
    '''
    words = []
    for dword in dwords:
        words.append(dword &0xffff)
        words.append((dword >> 16) &0xffff)
    return words

def dwords_to_words_be(dwords):
    ''' Convert a list of dwords to a list of words assuming big-endian
    packing
    '''
    words = []
    for dword in dwords:
        words.append((dword >> 16) &0xffff)
        words.append(dword &0xffff)
    return words

def words_to_dwords(words):
    ''' Convert a list of words to a list of dwords assuming little-endian
    packing
    '''
    return [(words[x] | (words[x+1] << 16)) for x in range(0, len(words), 2)]

def words_to_dwords_be(words):
    ''' Convert a list of words to a list of dwords assuming big-endian
    packing
    '''
    return [((words[x] << 16 | words[x+1])) for x in range(0, len(words), 2)]

def to_signed(unsigned, bits=32):
    '''
    Converts unsigned value into signed by calculating two's complement.
    '''
    if unsigned >= (1 << (bits - 1)):
        unsigned -= (1 << bits)
    return unsigned


def from_signed(signed, bits=32, wrapping=False):
    '''
    Converts signed value into unsigned by calculating two's complement.
    '''
    if signed < 0:
        if signed < -(1 << (bits - 1)) and not wrapping:
            raise ValueError("0x%x too large to fit in %d bit signed integer!" %
                             (signed, bits))
        signed += (1 << bits)
    else:
        if signed > (1 << (bits - 1)) - 1 and not wrapping:
            raise ValueError("0x%x too large to fit in %d bit signed integer!" %
                             (signed, bits))

    return signed

def hex_array_str(data_bytes):
    ''' Return an array of bytes as a hex string '''
    return " ".join(["%02x"%a for a in data_bytes])

def hex_array_str_columns(data_bytes, num_columns=16, start_offset=0,
                          cursor=None, numbits=8):
    ''' Return a multi-line string representing the bytes as columns of
    hex numbers with an offset (address) at the start.
    Optional parameter 'start_offset' adds an offset to this address just
    for printing.
    Optional cursor puts an indicator next to that numbered entry taking into
    account the start_offset.
    Optional numbits sets the width of memory, defaulting to bytes - 8 bits
    '''
    output = ""
    as_ascii = ""
    printable = string.ascii_letters + string.punctuation + string.digits + ' '
    for offset, value in enumerate(data_bytes):
        as_ascii += (chr(value) if value < 256 and chr(value) in printable
                     else ".")
        if offset % num_columns == 0:
            output += "%04x: " % (offset + start_offset)
        output += ">" if offset + start_offset == cursor else " "
        output += "%0*x" % (numbits // 4, value)
        if offset % num_columns == (num_columns - 1):
            output += "  " + as_ascii + os.linesep
            as_ascii = ""
    return output

def show_mem( # pylint: disable=too-many-arguments
        mem, start_offset=0, length=256, num_columns=16, cursor=None,
        numbits=None):
    ''' Show memory as a table of bytes / words (based on the memory's
    word width).
    The width in bits can be specified as a parameter if the word width is
    not known. This will not coalesce bytes into words
    if the memory is byte addressable.
    e.g. show_mem(apps.pm) or show_mem(apps.dm, 0x1000, 64)
    '''
    # Deal with memory that addressed more than 8 bits i.e. XAP processor (16),
    # Kalimba DSP (16,32)
    if numbits is None:
        try:
            numbits = mem.word_bits
        except AttributeError:
            numbits = 8

    iprint(hex_array_str_columns(
        mem[start_offset:start_offset+length],
        start_offset=start_offset,
        num_columns=num_columns,
        cursor=cursor,
        numbits=numbits))

def detect_keypress(char_ordinal):
    """
    Useful on Windows platforms to detect a CTRL-Z keyboard hit.
    (Not a useful keyboard press to use on linux because it will mess with
    your process).
    """
    if WINDOWS_CRT and msvcrt.kbhit():
        if ord(msvcrt.getch()) == char_ordinal:
            return True
    return False

def detect_ctrl_z():
    """
    Turns a CTRL-Z keyboard hit into a KeyboardInterrupt exception, for use
    in circumstances where this does not happen automatically, e.g. in a
    pooling loop, like the one in polling_loop.py.
    """
    if detect_keypress(26):
        raise KeyboardInterrupt


def byte_reverse_dword(dword):
    '''
    Return a byte-reversed version of the input dword
    >>> hex(byte_reverse_dword(0x01020304))
    '0x4030201'
    '''
    return (((dword & 0xff) << 24) |
            ((dword & 0xff00) << 8) |
            ((dword >> 8) & 0xff00) |
            ((dword >> 24) & 0xff))

def create_reverse_lookup(dict_in):
    ''' Given a dictionary, create a reverse lookup version.
    '''
    return {v:k for k, v in dict_in.items()}

def chunks(list_of_values, number_of_values_in_each_chunk):
    ''' Takes a list of values and returns an iterator of a list with them
    grouped into chunks of the specified size.
    '''
    return zip(*[iter(list_of_values)]*number_of_values_in_each_chunk)


def unique_subclass(base_type, id_hint=None):
    """
    Return a unique subclass of base_type by constructing a new type name by
    incrementing an index until no matching name is found in base_type's
    subclasses.
    id_hint can be used to avoid a linear search from 0.  If id_hint is
    supplied, this function returns a tuple consisting of the new type and the
    id used for it; otherwise it just returns the new type.
    """
    # Using id as part of name for subclass id as is like the builtin meaning
    # pylint: disable=redefined-builtin,invalid-name
    id = id_hint if id_hint is not None else 0
    base_name = base_type.__name__
    while ("%s_sub_%d" % (base_name, id) in
           [cl.__name__ for cl in base_type.__subclasses__()]):
        id += 1
    if id_hint is not None:
        return type("%s_sub_%d" % (base_name, id), (base_type,), {}), id
    return type("%s_sub_%d" % (base_name, id), (base_type,), {})

def print_bits_set(word):
    """
    Print which bits of the supplied word are set
    """
    if not word:
        gstrm.iout.write("None")
    else:
        gstrm.iout.write(" ".join(["%d" %b for b in bits_set(word)]))
    gstrm.iout.write("\n")

def bits_set(word):
    """
    Return a list of which bits in a word are set
    """
    if word < 0:
        raise RuntimeError("Can't count bits in a negative number (%d)" % word)
    i = 0
    bits = []
    while word:
        if word & 1:
            bits.append(i)
        word >>= 1
        i += 1
    return bits

class AnsiColourCodes(object): # pylint: disable=too-few-public-methods
    """
    Provides a means of stripping Ansi Colour codes from some text via the
    instance method strip_codes(text).
    """
    def __init__(self):
        dim_prefix = "dark"
        bright_prefix = "bright"
        self.base_colours = (
            ("black", 0), ("red", 1), ("green", 2), ("yellow", 3),
            ("blue", 4), ("magenta", 5), ("cyan", 6), ("white", 7))
        self.base_colour_codes = {
            name:'\x1b[3%dm' % num for name, num in self.base_colours}
        self.reset_code = '\x1b[0m'
        self.style_code = {"bright"    : '\x1b[1m',
                           "normal"    : '\x1b[22m',
                           "dim"       : '\x1b[2m'}

        self.code = {name:self.base_colour_codes[name]
                     for name in self.base_colour_codes}
        self.code.update(
            {bright_prefix + name:
             self.style_code["bright"] + self.base_colour_codes[name]
             for name in self.base_colour_codes})
        self.code.update(
            {dim_prefix + name:
             self.style_code["dim"] + self.base_colour_codes[name]
             for name in self.base_colour_codes})
        self.names = list(self.code.keys())
        # pylint: disable=anomalous-backslash-in-string
        self.colour_code_regex = re.compile("\x1b\[\\d+m")

    def strip_codes(self, text):
        """
        Remove any colour codes that might be present in the given piece of text
        """
        return self.colour_code_regex.sub("", text)


def add_colours(highlights, text, reset_colour=AnsiColourCodes().reset_code):
    """Returns a string with the colours added based on the provided highlights.
    - highlights: a (preferably Ordered)dict with
          {regexp_string: colourcode_string}.
    - text: the string to apply colours to.
    - reset_colour: the colour code to use as a default colour/reset.
    Note that this function maintains any original colours in the string.
    However bear in mind that the highlights are applied on top of each other,
    in the provided order."""

    # Create list with colour codes at the position matching that of the string
    colour_inserts = [None] * (len(text) + 1)
    # Go over all the regexps looking for matches and updating the list of
    # colour codes
    for pattern in highlights.keys():
        # The colour code for this pattern
        colour = highlights[pattern]
        # Find all matches for this pattern
        matches = re.finditer(pattern, text)
        # match is also a global function in this module
        for match in matches: # pylint: disable=redefined-outer-name
            # By default, revert to the provided reset_colour
            endcolour = reset_colour
            # Check if there are any colour codes in the span of the
            # matched text
            endcolours = [ccode for ccode in colour_inserts[
                match.start():match.end() + 1] if ccode is not None]
            if endcolours:
                # Remove (i.e. set to None) all the colours present in the span
                # of the matched text
                colour_inserts = (colour_inserts[:match.start()] +
                                  [None] * (match.end() - match.start()) +
                                  colour_inserts[match.end():])
                # Keep the last (rightmost) colour code for reverting to
                endcolour = endcolours[-1]
            # There aren't any colours in the span of the matched text
            else:
                # Check for colours before the start of the matched text
                endcolours = [ccode for ccode in colour_inserts[:match.start()]
                              if ccode is not None]
                if endcolours:
                    # Keep the last (rightmost) colour code for reverting to
                    endcolour = endcolours[-1]
            # Add the starting colour for this pattern to the list
            colour_inserts[match.start()] = colour
            # Add the end colour to revert to
            colour_inserts[match.end()] = endcolour

    # Always revert back to the reset colour when the text ends
    colour_inserts[-1] = reset_colour
    # Insert the colours into the string from right to left
    colour_inserts.reverse()
    text_coloured = list(text)
    for count, colour in enumerate(colour_inserts):
        if colour:
            text_coloured.insert(len(text) - count, colour)

    return "".join(text_coloured)

try:
    import colorama as _colorama
    # test of import hence:
    # pylint: disable=unused-import
    from colorama.ansitowin32 import AnsiToWin32 as ColoramaAnsiToWin32
except ImportError:
    _colorama = False
    class ColoramaAnsiToWin32(object): # pylint: disable=too-few-public-methods
        """A dummy stream wrapper class that just returns the stream again"""

        # Arbitrary arguments are passed to wrapped, so tell pylint.
        # pylint: disable=unused-argument
        def __init__(self, wrapped, *args, **kwargs):
            self.stream = wrapped

_COLORAMA_INITIALISED = False
@contextlib.contextmanager
def colorama_enabled():
    """
    Context guard for the colorama module.  When this is enabled readline
    doesn't work, so we ensure it is only enabled specifically when it is
    needed, and is reliably turned off.
    """
    # pylint: disable=global-statement,invalid-name
    global _colorama, _COLORAMA_INITIALISED
    if _colorama:
        if _COLORAMA_INITIALISED:
            _colorama.reinit()
        else:
            _colorama.init()
            _COLORAMA_INITIALISED = True
    yield
    if _colorama:
        _colorama.deinit()

def enable_colorama(func):
    """Enables colorama for the duration of execution function func"""
    @functools.wraps(func)
    def colorama_enabled_func(*args, **kwargs):
        """Execute func, using passed arguments, with colorama enabled"""
        with colorama_enabled():
            return func(*args, **kwargs)
    return colorama_enabled_func

def flatten_list(lst):
    """
    Return a flattened version of the multi-level list passed
    """
    flat_list = []
    for item in lst:
        if isinstance(item, Iterable):
            flat_list.extend(flatten_list(item))
        else:
            flat_list.append(item)
    return flat_list

def json_to_dict(file_path):
    '''
    Simple function to parse a JSON file and
    return a dictionary object.

    Note: this does not support comments in the
    JSON file.
    '''
    import json

    with open(file_path, 'rt') as json_file:
        return json.load(json_file)

class FrozenNamespace(object):
    """
    Insert the contents of the given dictionary as attributes of a plain vanilla
    object, freezing it afterwards so that none of the attributes can be removed
    and no further attributes can be created
    """
    def __init__(self, dict_, delay_finalisation=False):

        self._freeze = False
        for name, var in dict_.items():

            setattr(self, name, var)
        if not delay_finalisation:
            self.freeze()

    def freeze(self):
        """marks attribute list as frozen"""
        self._freeze = True

    @property
    def _is_frozen(self):
        try:
            return self._freeze
        except AttributeError:
            return False

    def __setattr__(self, name, val):
        """
        Add an attribute only if we're in the unfrozen window at construction
        time
        """
        if not self._is_frozen:
            super(FrozenNamespace, self).__setattr__(name, val)
        else:
            raise ValueError("FrozenNamespace: can't set attribute!")

    def __delattr__(self, name):

        if not self._is_frozen:
            super(FrozenNamespace, self).__delattr__(name)
        else:
            raise ValueError("FrozenNamepsace: can't delete attribute!")

def construct_lazy_proxy(lp): # pylint: disable=invalid-name
    """
    Helper function to do the dirty work of reaching inside the LazyProxy
    wrapper and invoking object construction.  If that fails with any of the
    exceptions supplied to the LazyProxy wrapper, returns None.
    Otherwise returns the constructed instance (i.e. the LazyProxy wrapper is
    thrown away).
    Non-LazyProxy objects can be passed to this function safely; they are
    returned untouched.
    """
    # Tell pylint we really want to do this as said above
    # pylint: disable=protected-access
    try:
        constructor = lp._LazyProxy__create_on_demand
    except AttributeError:
        # It's not a LazyProxy at all
        return lp
    else:
        return constructor(quiet=True)

def discard_lazy_proxy(lp): # pylint: disable=invalid-name
    """
    Helper function to do the dirty work of reaching inside the LazyProxy
    wrapper and invoking object memory discarding.
    """
    # Tell pylint we really want to do this as said in construct_lazy_proxy
    # pylint: disable=protected-access
    try:
        destructor = lp._LazyProxy__discard
    except AttributeError:
        # It's not a LazyProxy at all
        pass
    else:
        destructor()


def retrieve_lazy_proxy_setup(lp): # pylint: disable=invalid-name
    """
    Helper function to do the dirty work of reaching inside the LazyProxy
    wrapper and returning all the arguments passed to the LazyProxy's
    constructor.  This is to enable a similar LazyProxy to be constructed, e.g.
    when building a clone firmware environment in a multi-device session.
    """
    # Tell pylint we really want to do this as above
    # pylint: disable=protected-access
    try:
        return {"name" : lp._LazyProxy__name,
                "factory" : lp._LazyProxy__factory,
                "cls" : lp._LazyProxy__cls,
                "cons_args" : lp._LazyProxy__cons_args,
                "cons_kw_args" : lp._LazyProxy__cons_kw_args,
                "cons_excep_list" : lp._LazyProxy__cons_excep_list,
                "hook_list" : lp._LazyProxy__hook_list,
                "attrs_none_if_none" : lp._LazyProxy__attrs_none_if_none}
    except AttributeError:
        raise TypeError("Not a LazyProxy")

class LazyProxy(object): # pylint: disable=too-many-instance-attributes
    """
    Keep an object on standby so it isn't created before it's needed.
    """
    def __init__( # pylint: disable=too-many-arguments
            self, name, cls_or_factory_type_pair,
            cons_args, cons_kw_args, cons_excep_list=None, hook_list=None,
            attrs_none_if_none=False):
        """
        Set up details needed to construct the wrapped object when required.
         - either a class or a pair (factory callable, type for "isinstance")
         plus positional and keyword args for the class constructor/factory
         - a list of exceptions the construction process might throw
         - a list of hook callables to invoke on successful construction
        """
        self.__name = name
        try:
            self.__factory, self.__cls = cls_or_factory_type_pair
        except TypeError:
            self.__factory = self.__cls = cls_or_factory_type_pair
        self.__cons_args = cons_args
        self.__cons_kw_args = cons_kw_args
        self.__cons_excep_list = (tuple(cons_excep_list)
                                  if cons_excep_list else ())
        self.__hook_list = hook_list if hook_list else []
        self.__attrs_none_if_none = attrs_none_if_none

    def __create_on_demand(self, quiet=False):
        """
        Run the creation call, if it hasn't already been run.  If this fails
        with one of the supplied exceptions, simply set the instance variable
        to None.  Return the instance variable (whether successfully created or
        set to None) so the helper function "construct_lazy_proxy" can return
        it directly.
        """
        try:
            self.__the_instance
        except AttributeError:
            try:
                self.__the_instance = self.__factory(*self.__cons_args,
                                                     **self.__cons_kw_args)
            except self.__cons_excep_list as exc:
                # If constructing __the_instance fails, forget we ever tried
                # - it might work next time
                if not quiet:
                    iprint("Error while creating instance: %s" % exc)
                    raise
                return False
            else:
                for hook in self.__hook_list:
                    hook()
        return self.__the_instance

    def __discard(self):
        try:
            del self.__the_instance
        except AttributeError:
            pass

    def __getattribute__(self, attr):
        """
        Pass attribute accesses through to the underlying object, creating it if
        required.  Trap the ones that are actually destined for the LazyProxy
        itself
        """
        if attr.startswith("_LazyProxy__"):
            return super(LazyProxy, self).__getattribute__(attr)
        if self.__create_on_demand() is not False:
            if self.__the_instance is None and self.__attrs_none_if_none:
                # Special behaviour: if the instance evaluates to None, rather
                # than trigger an AttributeError here every time, act as if
                # all attributes exist but are themselves None.
                return None
            return getattr(self.__the_instance, attr)
        raise AttributeError("Couldn't create instance")

    def __setattr__(self, attr, value):
        if attr.startswith("_LazyProxy__"):
            return super(LazyProxy, self).__setattr__(attr, value)
        if self.__create_on_demand():
            return setattr(self.__the_instance, attr, value)
        raise AttributeError("Couldn't create instance")

    @staticmethod
    def isinstance(obj, obj_type):
        """
        return whether obj is of a class that is a subclass of type obj_type
        """
        # Tell pylint we want to do this as said in __init__
        # pylint: disable=protected-access
        return issubclass(obj.__cls, obj_type)

class LazyAttribEvaluator(object): # pylint: disable=too-few-public-methods
    """
    Given a wrapper class that is intended to present an underlying
    dictionary-like object as an object with attributes corresponding to the
    keys, this mixin allows that to be done such that the keys/attributes are
    all visible in the overlying object up-front, but the objects they refer
    to are not constructed until required.

    This is useful for things like <core>.fw.gbl, which is designed to make it
    convenient to browse the global variables in a core via readline tab-
    completion.  However we don't want to construct all the variables up-front
    because that's expensive.

    Important Note: any class that inherits LazyAttribEvaluator *should only
    have a single instance* because LazyAttribEvaluator achieves its objective
    by creating properties, which are class, not object, attributes.  Use the
    unique_subclass class factory to conveniently create a unique class for the
    object at construction time, e.g.

    Old:
       my_obj = MyClass(args)
    New:
       my_obj = unique_subclass(MyClass)(args

    """
    def __init__(self, attribs=None, strip_prefix=None):
        if attribs:
            self.populate(attribs, strip_prefix=strip_prefix)

    def populate(self, attribs, name_remapping=None, strip_prefix=None,
                 verbose=False): # pylint: disable=unused-argument
        """
        Populates the underlying dictionary mentioned in class docstring
        with the keys from attribs.
        An attribute name can have a prefix of strip_prefix stripped when
        strip_prefix is set.
        An attribute name can be remapped using dictionary name_remapping,
        if that has a mapping key of a tuple (k,) for a given attribute k.
        verbose, whilst not presently used, could be used to output helpful
        debug information.
        """
        if name_remapping is None:
            name_remapping = {}
        def getter_factory(attrib):
            """for making a property getter function for attribute attrib"""
            def getter(obj): # pylint: disable=unused-argument
                """
                looks up attribute name attrib from dict supplied to populate.
                obj is an object placeholder.
                """
                return attribs[attrib]
            return getter
        for k in list(attribs.keys()):
            kname = name_remapping.get((k,), k)
            if strip_prefix and kname.startswith(strip_prefix):
                kname = kname[len(strip_prefix):]
            kname = kname.replace("::", "__")
            kname = kname.replace(" ", "__")
            setattr(type(self), kname, property(getter_factory(k)))


def path_is_windows_remote_mount(pth):
    """
    Attempt to determine using standard system utilities whether the given path
    is mounted across a network or not.  The pth must be a str (not a bytes or
    a unicode)

    Warning: on Linux this just unconditionally says no.
    """
    
    rpth = os.path.realpath(pth)

    if "Windows" in platform.system():

        # Remove long path prefix ("\\?\LONG_PATH")
        if rpth.startswith(r"\\?\\"):
            rpth = rpth[4:]

        # Assume it's a network path if it starts with "\\"
        if rpth.startswith(r"\\"):
            return True

        try:
            rpth_label = rpth.split(r":", 1)[0].upper()
        except IndexError:
            # There was no ":" in the path - don't know how to parse it, so give
            # up
            return False

        try:
            uppercase_letters = string.uppercase
        except AttributeError:
            # Python 3
            uppercase_letters = string.ascii_uppercase

        if len(rpth_label) != 1 or rpth_label not in uppercase_letters:
            # There was a ":" in the path, but not indicating a drive letter, so
            # give up
            return False

        # Otherwise run "net use" and parse the output to see if the drive label
        # is one of the ones that are mentioned
        try:
            net_use_proc = SP.Popen(["net", "use"], stdout=SP.PIPE)
        except OSError:
            iprint("Failure attempting to detect network paths "
                   "('net use' could not be executed)")
            return False
        stdout, _ = net_use_proc.communicate()
        if net_use_proc.returncode != 0:
            iprint("Failure attempting to detect network paths "
                   "('net use' failed)")
            return False
        # Make a list of the drive letters listed by this command, throwing away
        # all the other cruft, and see whether the drive of the supplied path
        # is in the list
        # Note: we work with bytes here because Popen.communicate returns bytes
        # and we don't necessarily know what encoding it will have.  However,
        # we do know that Windows drive letters will be ASCII (don't we?).
        return rpth_label.encode() in [l.split()[0].rstrip(b":") 
                                        for l in stdout.split(b"\n") 
                                              if re.match(b"^\s+[A-Z]:\s+", l)]
        
    # On other platforms we believe remote-ness doesn't matter so much.
    return False

class BitArray(object):
    """
    Fixed-length array of bits providing index or slice access with support
    for strides, and reasonably efficient checks on whether the array contains
    any ones or any zeros.

    The main aim is memory efficiency.  The bits are stored in a bytearray with
    any trailing bits set to 0.

    Values returned are integers/lists of integers.  Values set must be
    integers/indexable sequences of integers.

    Contains an additional convenience function that can efficiently set long
    contiguous ranges of values to 1.  This is useful for the BitArray's
    original purpose, to support marking bytes in the coredump caches as
    "loaded".
    """

    def __init__(self, length, val=0):
        self._len = length
        if val == 1:
            val = 0xff
        elif val != 0:
            raise ValueError(
                "BitArray: can only initialise a BitArray to 0 or 1!")
        # pylint: disable=invalid-name
        self._b = bytearray(val for _ in range((length+7)//8))
        self._trailing_bits = (0x100 - (1 << (self._len & 7))) & 0xff
        # Zero the trailing bits so that the "contains" calculation will work
        if val != 0:
            self._b[-1] = (~self._trailing_bits)&0xff

    def __len__(self):
        return self._len

    def __contains__(self, val):
        """
        Determines whether the BitArray contains any zeros or ones by comparing
        the sum of the bytes in the underlying bytearray with the expected total
        if it contains all zeros (0, of course) or if it contains all ones (a
        slightly more complex calculation)
        """
        if val not in (0, 1):
            raise ValueError("BitArray: can't test for contained value other "
                             "than 0 or 1!")
        total = sum(self._b)
        # Just need the total to be greater than 0 to prove there's a one
        # in there
        # OTOH there's a zero in there if the total is less than an array
        # full of 0xff less any unused upper bits in the last entry.
        return (total > 0 if val == 1 else
                total < (0xff*((self._len+7)//8) - self._trailing_bits))


    def __getitem__(self, i):
        if isinstance(i, (int, long)):
            if i < 0:
                i += self._len
            if 0 > i >= self._len:
                raise IndexError("BitArray: Index %d out of range [0,%d]" %
                                 (i, self._len))
            return (self._b[i>>3] >> (i & 7)) & 1
        start = 0 if i.start is None else i.start
        stop = self._len if i.stop is None else i.stop
        step = 1 if i.step is None else i.step
        return [self[idx] for idx in range(start, stop, step)]

    def __setitem__(self, i, item):
        if isinstance(i, (int, long)):
            val = self._b[i>>3]
            val &= ~(1<<(i & 7))
            val |= ((item & 1) << (i & 7))
            self._b[i>>3] = val
        else:
            start = 0 if i.start is None else i.start
            stop = self._len if i.stop is None else i.stop
            step = 1 if i.step is None else i.step
            if isinstance(item, (int, long)):
                for idx in range(start, stop, step):
                    self[idx] = item
            else:
                for idx, val in zip(list(range(start, stop, step)), item):
                    self[idx] = val

    def set(self, start, stop):
        """
        Set all the bits in the given range to 1
        """
        start_byte = start//8
        end_byte = (stop+8)//8 # Set up end_byte as if we were being consistent
        # and using an empty trailing byte mask for the case of an aligned stop.
        # However, we don't actually attempt to set this mask because the
        # trailing byte might not be there.
        if end_byte - start_byte > 1:
            self._b[start_byte+1:end_byte-1] = [0xff]*(end_byte-start_byte-2)
            # Set the start byte
            self._b[start_byte] |= 0x100 - (1<<(start&7))
            # Set the end byte only if it's a non-trivial one
            if stop % 8 != 0:
                self._b[end_byte-1] |= (1<<(stop&7))-1
        else:
            self._b[start_byte] |= ((1 << (stop-start)) - 1) << (start&7)

    def expand(self, new_len, value=0):
        """
        Expand to a new (greater) length, leaving the existing bits alone and
        setting the new bits to the given value
        """
        if new_len <= self._len:
            return
        
        # If we need more bytes in the byte array, then create a new one and
        # initialise it with the old one's contents.
        if (new_len + 7)//8 > len(self._b):
            # We need a new bytearray of the right length containing the original
            # values
            addtnl_bytes = (new_len + 7)//8 - len(self._b)
            self._b = bytearray(itertools.chain(self._b, [0]*addtnl_bytes))

        if value:
            self.set(self._len,new_len)
        
        
        

@contextlib.contextmanager
def null_context():
    """
    A utility for use in a 'with' expression containing a conditional, e.g.
      with x if condition else null_context():
          something
    such that the former does something useful but the fallback does nothing.
    """
    yield


class CloneFactory(object): # pylint: disable=too-few-public-methods
    """
    Very simple generic clone factory: holds all the details needed
    to build a particular object and simply creates a fresh (deep) copy for
    each call on the CloneFactory instance.
    """
    def __init__(self, cls, *args, **kwargs):
        self._cls = cls
        self._args = args
        self._kwargs = kwargs

    def __call__(self):
        """
        Create a new instance of the clone object
        """
        return self._cls(*self._args, **self._kwargs)


def is_executable(filename):
    """
    Does this file exist as given or is it available on the path, and if so is
    it executable?  (On Windows "executable" means exists with an extension in
    the PATHEXT env var; filename can be given without any extension and
    the "executable" extensions will be tried in turn for a match.)
    """
    is_windows = platform.system() == "Windows"
    if is_windows:
        def is_exe(pth): # pylint: disable=unused-argument
            """returns True for any Windows file pth"""
            return True
    else:
        def is_exe(pth):
            """returns True when linux file pth has executable access mode"""
            return os.access(pth, os.X_OK)

    def path_gen(filename):
        """
        Generates a list of paths, starting with filename itself,
        then each folder in PATH prepending filename, with on Windows only
        all possible filename extensions listed in PATHEXT env var.
        """
        sep = os.pathsep
        if is_windows:
            path_ext_list = os.getenv("PATHEXT").split(sep)
        else:
            path_ext_list = []
        yield filename
        path_cmpts = os.getenv("PATH").split(sep)
        for pth in path_cmpts:
            yield os.path.join(pth, filename)
            for ext in path_ext_list:
                yield os.path.join(pth, filename+ext)

    for pth in path_gen(filename):
        if os.path.isfile(pth) and is_exe(pth):
            return True
    return False

def path_split(pth):
    """
    Split the given path on both possible separators
    """
    # This expression splits on backslash first, then splits each resulting
    # component on forward slash adding the list of subcomponents onto a
    # master list as it goes
    return sum((pth2.split("/") for pth2 in pth.split("\\")), [])

def path_join(pth_list, system=None):
    """
    Joins a path from a list in pth_list; doesn't rely on current OS
    unless system is None, instead use can supply one of "Linux" or "Windows".
    So doesn't use current value of os.path.sep.
    """
    if not system:
        system = platform.system()
    if system == "Linux":
        separator = "/"
    elif system == "Windows":
        separator = "\\"
    else:
        raise ValueError(
            "Unknown system %s. Expected Linux or Windows." % system)
    pth = separator.join(pth_list)
    return pth

def unique_basenames(path_list,
                     verbose=False):
    """
    Given a list of paths, return a dictionary mapping the original paths
    to the shortest unique "extended basename", which is returned as a list of
    trailing path components from the original path that is no longer than
    necessary to be unique.  The caller can process this list in any way they
    wish to get an actual string.

    Paths can contain forward slashes or backslashes or both as separators.

    If new_sep is None, the extended basenames are constructed with the default
    system path separator (where they need a separator at all).
    """
    # Following code is a bit convoluted, hence
    # pylint: disable=too-many-branches, too-many-locals
    basenames = {}
    for pth in path_list:
        if pth:
            basenames.setdefault(path_split(pth)[-1], set()).add(pth)


    unique = {}
    for name, pths in list(basenames.items()):
        if len(pths) == 1:
            unique[pths.pop()] = (name,)
        else:
            if verbose:
                iprint("'%s' occurs in %d paths" % (name, len(pths)))
            pths = list(pths)
            # add enough of the path to get unique names
            pth_cmpts = [path_split(p) for p in pths]
            test_cmpt = -1
            unique_path_frags = {}
            while True:
                # Get the next test components for the paths that are still
                # in the running
                pth_test_cmpts = [(i, p[test_cmpt]
                                   if -1*test_cmpt <= len(p) else "")
                                  for (i, p) in enumerate(pth_cmpts)
                                  if i not in unique_path_frags]
                if verbose:
                    iprint("  (With cmpt index = %d there are now %d "
                           "pth_test_cmpts in the running)" %
                           (test_cmpt, len(pth_test_cmpts)))
                if not pth_test_cmpts:
                    # None left to process
                    break
                count_occurrences = {}
                # If there's any path component in the list that is now unique,
                # the corresponding basename has been sufficiently resolved,
                # so add its index to unique_path_frags
                for index, cmpt in pth_test_cmpts:
                    count_occurrences.setdefault(cmpt, []).append(index)
                for index_list in count_occurrences.values():
                    if len(index_list) == 1:
                        ind = index_list[0]
                        cmpts = pth_cmpts[ind][test_cmpt:]
                        unique_path_frags[ind] = cmpts
                test_cmpt -= 1

            for i, pth in enumerate(pths):
                if verbose:
                    iprint("unique basename list of '%s' is '%s'" %
                           (pth, unique_path_frags[i]))
                unique[pth] = tuple(unique_path_frags[i])

    return unique

def get_first_real_attr(obj, *attributes):
    """Check each supplied attribute in turn to see if it exists for the
       passed obj. Multiple attribute names can be passed and they may
       include "." separators.

       The first existing attribute will be accessed and returned.

       This function is expected to be used in cases where variables are
       known to have been renamed or moved between different versions of
       a chip.

       If none of the suggested attributes exist, an AttributeError
       Exception is raised. This is likely to be the same Exception
       as raised if the obj does not exist, but may depend upon exact
       usage (missing objects might cause KeyError, IndexError for instance).

       There may be possible usage where the alternative objects require
       Keys or Indexes rather than just attributes. Variants of this
       function may then be possible - accepting tuples of (obj,attr)
       or understanding indexing.
       """
    for attribute_name in attributes:
        current_obj = obj
        for attribute in attribute_name.split("."):
            try:
                current_obj = getattr(current_obj, attribute)
            except AttributeError:
                break
        else:
            return current_obj
    raise AttributeError("None of the supplied attributes exists")

def us_as_human_readable_str(time_in_us):
    """Return a display string for a time in micro-seconds.

       The value is considered and displayed as us, ms, seconds
       or minutes as deemed most appropriate.

       No guarantee is made regarding truncation or rounding for the
       human readable value.
       """

    display_time = time_in_us * 1.0
    unit = "us"

    # Over 10ms, report as ms value
    if abs(display_time) > (10 * 1000):
        unit = "ms"
        display_time = display_time // 1000

    # Over 2 seconds, report as decimal seconds (display only
    # has 2 decimal places)
    if abs(display_time) > 2000 and unit == "ms":
        unit = "s"
        display_time = display_time // 1000

    if abs(display_time) > 119 and unit == "s":
        unit = "mins"
        display_time = display_time // 60

    return "%.2f %s"%(display_time, unit)

def wait_for_keypress(quiet_mode=None):
    '''
    Wait for user to press a key before proceeding.
    Does not wait if parameter quiet_mode is '1' or True
    which defaults to the value of environment variable PYDBG_QUIET_MODE,
    (so does wait if it is not defined).
    Exits if a KeyboardInterrupt (Ctrl-C) is detected.
    '''
    if quiet_mode is None:
        quiet_mode = os.environ.get('PYDBG_QUIET_MODE', None)
    if quiet_mode:
        return
    try:
        input_func("Press return to continue...")
    except KeyboardInterrupt:
        iprint("\nExiting the program... ")
        sys.exit()

def as_string_limited_length(mem, start, max_len):
    """
    Interpret up to max_len bytes in mem starting at start as a string.  A
    null character (zero byte) encountered before max_len terminates the string.
    """
    from csr.dev.hw.memory_pointer import MemoryPointer as Pointer
    return CLang.get_string(Pointer(mem, start, length=max_len))


def match(name, patterns=None):
    """
    Method checking if one of the patterns matches name variable.

    @param name: mandatory parameter, string which is matched against patterns
    @param patterns: a regular expression pattern used to filter out register
                     names and bitfields.
    @return: Bool flag indicating pattern match
    """

    for pattern in patterns:
        re_flags = re.IGNORECASE
        re_pattern = re.compile(pattern, re_flags)
        re_result = re_pattern.search(name)

        if re_result:
            return True

    return False


def get_bits_string(info):
    """
    Returns a string representing, in the form of an index or slice expression,
    a bit or range of bits between info.start_bit and info.stop_bit, e.g.
    '[2]', or '[0:3]'.
    """
    if info.start_bit + 1 == info.stop_bit:
        bits = "[%d]" % info.start_bit
    else:
        bits = "[%d:%d]" % (info.start_bit, info.stop_bit - 1)
    return bits


class CLang(object):
    """\
    C language/code helpers
    """

    @staticmethod
    def get_string(mem):
        """
        Read null terminated ascii string from start of mem and return python
        string.
        """
        text = ''
        for item in mem:
            if not item:
                break
            text += CLang.printable_char(item)
        return text

    @staticmethod
    def printable_char(x, non_print='.'): # pylint: disable=invalid-name
        """Return ascii char - or dot"""
        return chr(x) if 0x20 <= x < 0x7f else non_print

class SliceableDict(UserDict): # pylint: disable=too-many-ancestors
    """
    Dictionary that can be used with array-like slicing operations.  Keys need
    to be integral, naturally.   We don't try that hard to prevent non-integral
    keys being used - the only check is in __setitem__.
    """
    def __getitem__(self, key_or_key_slice):

        if isinstance(key_or_key_slice, slice):
            start = key_or_key_slice.start
            stop = key_or_key_slice.stop
            if None in (start, stop):
                raise ValueError("Must supply explicit slice start and stop "
                                 "for a SliceableDict")
            step = key_or_key_slice.step or 1
            return [self[v] for v in range(start, stop, step)]
        return UserDict.__getitem__(self, key_or_key_slice)

    def __setitem__(self, key_or_key_slice, value):

        if isinstance(key_or_key_slice, slice):
            start = key_or_key_slice.start
            stop = key_or_key_slice.stop
            if None in (start, stop):
                raise ValueError("Must supply explicit slice start and stop "
                                 "for a SliceableDict")
            step = key_or_key_slice.step or 1
            for i, idx in enumerate(range(start, stop, step)):
                self[idx] = value[i]
            return None
        if isinstance(key_or_key_slice, (int, long)):
            return UserDict.__setitem__(self, key_or_key_slice, value)
        raise TypeError("Can only use integral keys in SliceableDict!")

@contextlib.contextmanager
def open_or_stdout(filename, mode="w"):
    """Open a file handle to filename, or stdout if filename is set to '-'."""
    handle = open(filename, mode) if filename != "-" else gstrm.iout
    try:
        yield handle
    finally:
        if handle is not gstrm.iout:
            handle.close()

def strargs(*args):
    'returns stringified, space-separated args left stripped'
    return " ".join(["{}".format(x) for x in args]).lstrip()

def noncontainer_types(obj):
    """
    Report all the types stored in the given container that aren't built-in
    container types (Sequences or Mappings)
    """
    if isinstance(obj, Mapping):
        return sum((noncontainer_types(o) for o in obj.values()), [])
    if isinstance(obj, Container) and not isinstance(obj, (str, ByteString)):
        return sum((noncontainer_types(o) for o in obj), [])

    return [type(obj)]


def full_dict_copy(d): # pylint: disable=invalid-name
    """
    Returns a deep copy of d if it is a dictionary otherwise a shallow copy.
    """
    if isinstance(d, Mapping):
        return {k:full_dict_copy(v) for (k, v) in d.items()}
    return copy.copy(d)


@contextlib.contextmanager
def redirected_stdout():
    """
    Provides a context manager (use as part of a 'with' statement) which
    for the context block replaces just stdout with a memory file.
    """
    real_stdout = sys.stdout
    # We need to select the appropriate type of memory file according to whether
    # sys.stdout is set up to expect str or bytes (in Python 3 terms) /
    # unicode or str (in Python 2 terms).  Note that "real" stdout expects
    # str in both cases, but (because this is Python strings we're talking
    # about), these are different types of object in 2 and 3.  Sigh.
    sys.stdout = (StringIO() if isinstance(real_stdout, io.TextIOBase) else
                  io.BytesIO())
    gstrm.replace_stream(prev=real_stdout, new=sys.stdout)
    try:
        yield sys.stdout
    finally:
        gstrm.replace_stream(prev=sys.stdout, new=real_stdout)
        sys.stdout = real_stdout

@contextlib.contextmanager
def redirected_stdout_stderr():
    """
    Provides a context manager (use as part of a 'with' statement) which
    for the context block replaces both stdout/stderr with memory files.
    """
    real_stdout = sys.stdout
    real_stderr = sys.stderr
    gstrm.replace_stream(prev=real_stdout,
                         new=sys.stdout)
    gstrm.replace_stream(prev=real_stderr,
                         new=sys.stderr)
    # We need to select the appropriate type of memory file according to whether
    # sys.stdout is set up to expect str or bytes (in Python 3 terms) /
    # unicode or str (in Python 2 terms).  Note that "real" stdout expects
    # str in both cases, but (because this is Python strings that we're talking
    # about), these are different types of object in 2 and 3.  Sigh.
    sys.stdout = StringIO() if isinstance(
        real_stdout, io.TextIOBase) else io.BytesIO()
    sys.stderr = StringIO() if isinstance(
        real_stderr, io.TextIOBase) else io.BytesIO()
    try:
        yield sys.stdout, sys.stderr
    finally:
        gstrm.replace_stream(prev=sys.stdout, new=real_stdout)
        gstrm.replace_stream(prev=sys.stderr, new=real_stderr)
        sys.stdout = real_stdout
        sys.stderr = real_stderr

def ansi_c_date_to_iso(ansi_c_date):
    '''
    Convert ansi_c_date to ISO format from ANSI C format.
    '''
    from locale import setlocale, LC_ALL
    currentlocale = setlocale(LC_ALL, 'C')
    try:
        as_datetime = datetime.strptime(ansi_c_date, "%b %d %Y")
        return as_datetime.strftime("%Y-%m-%d")
    finally:
        setlocale(LC_ALL, currentlocale)

def get_earlier_date(baseline, time_unit, num_units):
    """
    Simple function that can take a whole number of years, months, weeks or days
    off a given date, taking into account variable lengths of months.

    If the time to subtract is given in years or months, the returned date
    ignores the hours, minutes and seconds of the baseline date, whereas if
    it is given in weeks or days, it only ignores the seconds.
    """

    def get_last_day_of_month(year, month):
        """
        returns the integer value for the last day of the month in the year,
        e.g. 31, 30, 29, 28.
        """
        datetime(year, month, 1) # check year and month are valid
        last_day = 31
        while True:
            try:
                datetime(year, month, last_day)
            except ValueError:
                last_day -= 1
            else:
                break
        return last_day

    if time_unit == "year":
        earlier = datetime(baseline.year-num_units,
                           baseline.month, baseline.day)
    elif time_unit == "month":
        earlier_month = ((baseline.month - num_units)-1) % 12 + 1
        earlier_year = baseline.year + (baseline.month - 1 - num_units) // 12
        earlier_day = baseline.day
        while True:
            try:
                earlier = datetime(earlier_year, earlier_month, earlier_day)
            except ValueError:
                earlier_day -= 1
            else:
                break
    elif time_unit == "week":
        earlier = get_earlier_date(baseline, "day", 7*num_units)

    elif time_unit == "day":
        earlier_year = baseline.year
        earlier_month = baseline.month
        earlier_day = baseline.day
        while num_units >= earlier_day:
            # Go back to the previous month
            num_units -= earlier_day
            earlier_month = (earlier_month - 2) % 12 + 1
            earlier_year -= earlier_month//12
            earlier_day = get_last_day_of_month(earlier_year, earlier_month)
        earlier_day -= num_units
        earlier = datetime(earlier_year, earlier_month, earlier_day,
                           baseline.hour, baseline.minute)

    return earlier

def autolazy(getter_func):
    """
    Given an attribute with the name provided by getter_func, provide
    automatically for there being another attribute of the same name but
    starting with an underscore.
    """
    instance_name = "_" + getter_func.__name__

    @functools.wraps(getter_func)
    def lazy_getter_func(slf):
        """
        Try and get the attribute 'instance_name' from object 'slf'
        and if it is not set then
        set it via the function getter_func of 'slf'.
        """
        try:
            getattr(slf, instance_name)
        except AttributeError:
            setattr(slf, instance_name, getter_func(slf))
        return getattr(slf, instance_name)

    return lazy_getter_func

# pylint: disable=invalid-name
if sys.version_info < (3,):
    # Python3 module time does not have clock
    # pylint: disable=no-member
    timeout_clock = time.clock
    performance_clock = time.clock
else:
    timeout_clock = time.process_time
    performance_clock = time.perf_counter

def hex_bytes(data=None, length=None):
    """
    Return a string of a list of hex byte values
    as read from data.
    Wraps line every 16 octets.
    """
    if data is None:
        return ''
    if length is None:
        length = len(data)
    result = ""
    for i in range(0, length, 16):
        result += (' '.join(['{:0>2x}'.format(x)
                             for x in data[i:i+16]]) + '\n')
    return result

def format_int_as_dec_hex(value, width=0, hex_first=False):
    """
    return a string containing "decimal (0x(hex))" of value
    with value formatted right-aligned into optional width
    which defaults to 0 causing no alignment.

    When hex_first is True, the output order is swapped thus:
    "0x(hex) (decimal)".
    """

    if value is None:
        return 'None'
    if hex_first:
        return '0x{0:0{1}x} ({0:{1}})'.format(int(value), width)
    return '{0:{1}} (0x{0:0{1}x})'.format(int(value), width)

class FilelikeQtWindow:
    
    def __init__(self):
        try:
            import qtpy
        except ImportError:
            raise RuntimeError("{} needs qtpy and pyqt5 installed!".format(self.__class__.__name__))
        import atexit
        atexit.register(self.close)
        
        
    def open(self, mode="w"):
        
        from qtpy.QtWidgets import QPlainTextEdit, QApplication
        if mode != "w":
            raise ValueError("FilelikeQtWindow can only be opened for writing!")
        
        self._app = QApplication([])
        self._output = QPlainTextEdit()
        self._output.setReadOnly(True)
        self._output.show()
        self._text = []
        
    def close(self):
        self._app.exit()
        
    def write(self, text):
        
        self._output.appendPlainText(text)
        self._app.processEvents()
        
        
def open_bytes_compressed(filename):
    
    if not exists(filename):
        if exists(filename+".xz"):
            import lzma
            return lzma.open(filename+".xz")
        if exists(filename+".bz2"):
            import bz2
            try:
                bz2.open
            except AttributeError:
                # Py2 doesn't have bz2.open
                return bz2.BZ2File(filename+".bz2")
            else:
                return bz2.open(filename+".bz2")
        if exists(filename+".gz"):
            import gzip
            return gzip.open(filename+".gz")
    return open(filename, "rb")

class UnimportableObjectProxy:
    """
    Proxy for objects (e.g. modules) that we can't import because we're in a
    limited Pydbg distribution.  This object simply delays raising the
    ImportError until the object is accessed via an attribute.  This avoids
    spurious import errors where an import is unconditional but the imported
    object is never actually used.
    """
    def __init__(self, exc):
        self._exc_to_raise = exc

    def __getattr__(self, attr):
        raise self._exc_to_raise
