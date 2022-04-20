#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""User defined integer types

Any of those types should follow
eval(object.__repr__()) == object

Where object is an instance of any of these classes

Examples

eval(IntHexU16(16512).__repr__()) == IntHexU16(16512)
IntHexU16(16512) == 16512
"""

ENABLE = True


# pylint: disable=too-few-public-methods

class IntHexU8(int):
    """Integer unsigned 8 bit representation class

    This is a subclass of int that represents integers in 0x%02x format by default
    In other respects it should behave exactly the same as an integer

    Args:
        uppercase (bool): Represent letters in uppercase
    """

    def __new__(cls, *args, **kwargs):
        if ENABLE:
            uppercase = kwargs.pop('uppercase', False)
            self = super(IntHexU8, cls).__new__(cls, *args, **kwargs)
            self.rformat = '0x%02' + ('x' if not uppercase else 'X')
            return self
        return int(*args)

    def __repr__(self):
        return self.rformat % (self)  # pylint: disable=no-member

    __str__ = __repr__


class IntHexU16(int):
    """Integer unsigned 16 bit representation class

    This is a subclass of int that represents integers in 0x%04x format by default
    In other respects it should behave exactly the same as an integer

    Args:
        uppercase (bool): Represent letters in uppercase
    """

    def __new__(cls, *args, **kwargs):
        if ENABLE:
            uppercase = kwargs.pop('uppercase', False)
            self = super(IntHexU16, cls).__new__(cls, *args, **kwargs)
            self.rformat = '0x%04' + ('x' if not uppercase else 'X')
            return self
        return int(*args)

    def __repr__(self):
        return self.rformat % (self)  # pylint: disable=no-member

    __str__ = __repr__


class IntHexU32(int):
    """Integer unsigned 32 bit representation class

    This is a subclass of int that represents integers in 0x%08x format by default
    In other respects it should behave exactly the same as an integer

    Args:
        uppercase (bool): Represent letters in uppercase
    """

    def __new__(cls, *args, **kwargs):
        if ENABLE:
            uppercase = kwargs.pop('uppercase', False)
            self = super(IntHexU32, cls).__new__(cls, *args, **kwargs)
            self.rformat = '0x%08' + ('x' if not uppercase else 'X')
            return self
        return int(*args)

    def __repr__(self):
        return self.rformat % (self)  # pylint: disable=no-member

    __str__ = __repr__


class IntHexU64(int):
    """Integer unsigned 64 bit representation class

    This is a subclass of int that represents integers in 0x%016x format by default
    In other respects it should behave exactly the same as an integer

    Args:
        uppercase (bool): Represent letters in uppercase
    """

    def __new__(cls, *args, **kwargs):
        if ENABLE:
            uppercase = kwargs.pop('uppercase', False)
            self = super(IntHexU64, cls).__new__(cls, *args, **kwargs)
            self.rformat = '0x%016' + ('x' if not uppercase else 'X')
            return self
        return int(*args)

    def __repr__(self):
        return self.rformat % (self)  # pylint: disable=no-member

    __str__ = __repr__


class IntHex:
    """Integer factory representation class

    This is a subclass of int that represents integers in hex format format by default
    In other respects it should behave exactly the same as an integer

    Args:
        size (int): Number of bytes (1, 2, 4 or 8)
        uppercase (bool): Represent letters in uppercase
    """

    def __new__(cls, *args, **kwargs):
        size = kwargs.pop('size', 1)
        if size == 1:
            return IntHexU8(*args, **kwargs)
        if size == 2:
            return IntHexU16(*args, **kwargs)
        if size == 4:
            return IntHexU32(*args, **kwargs)
        if size == 8:
            return IntHexU64(*args, **kwargs)
        raise ValueError('unable to create IntHex with size:%s' % (size))
