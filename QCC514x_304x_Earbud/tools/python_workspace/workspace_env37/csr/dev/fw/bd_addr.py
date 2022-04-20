############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
from csr.wheels.global_streams import iprint
This module provides a class called BdAddr for encapsulating Bluetooth
addresses. See the documentation of the class for more information.
"""
from functools import total_ordering
from csr.wheels.global_streams import iprint

@total_ordering
class BdAddr(object):
    """
    This class encapsulates Bluetooth Addresses. This allows them to be
    created easily, compared for equality and displayed consistently (both
    in terms of formatting and ordering if you have a list of them).

    Create a Bluetooth address either from components:

       addr = BdAddr.from_nap_and_uaplap(0x0002, 0x5b00a5a5)
       addr = BdAddr.from_nap_uap_and_lap(0x0002, 0x5b, 0x00a5a5)

    or from a structure:

       addr = BdAddr.from_struct(struct)

    See the documentation of the from_struct() method for information on the
    requirements on the structure.

    Having got a type, it can be converted to a string for printing:

       iprint("Address: %s" % addr)

    This will give a fully zero-padded string with a leading 0x, such as
    "0x00025b00a5a5".

    If you're tight on space (perhaps in a table or a protocol tree) so every
    character counts, the property short_str() will give a shorter string, in
    this case by eliminating the leading "0x" to give "00025b00a5a5".

    Bluetooth addresses can be ordered. However, note that semantically,
    there's no sense in which one Blueooth address is lower than another. The
    only reason to compared addresses is for sorting before display so that
    users can find devices easily and so that lists of devices remain in the
    same order between different analyses (possibly on different coredumps).

    Properties are provided to extract the nap, uap, lap and combined uaplap
    parts of the address. Normally you should not need to use these (never use
    them for printing).
    """
    class BdAddrAttributeError(AttributeError):
        """
        The requirements for the structure passed to from_struct are:
          * It must have a field called nap.
          * It either has a field called uap_lap or it has two fields one
            called uap and one called lap.
        """
        def __str__(self):
            return self.args[0] + BdAddr.BdAddrAttributeError.__doc__

    def __init__(self, dummy=None, nap=None, uaplap=None):
        """
        Prefer the from... class methods for constructing objects
        """
        # Because I created this class and I still find myself typing
        # BdAddr(x), let's give a helpful error message.
        if dummy is not None or nap is None or uaplap is None:
            raise ValueError("Do not construct this object directly by saying "
                             "%s(...arguments...) instead use one of the "
                             "class methods starting 'from_' to specify what "
                             "data you are providing." %
                             self.__class__.__name__)
        if nap > 0xffff:
            iprint("NAP 0x%08x is out of range - truncating" % nap)
            nap &= 0x0000ffff
        if uaplap > 0xffffffff:
            iprint("UAPLAP 0x%08x is out of range - truncating" % uaplap)
            uaplap &= 0xffffffff

        self._nap = nap
        self._uaplap = uaplap

    @classmethod
    def from_nap_and_uaplap(cls, nap, uaplap):
        """
        Construct a Bluetooth Address object given a 16-bit NAP and a 32-bit
        combined UAP and LAP.

        If you have the UAP and LAP separately then used from_nap_uap_and_lap.

        If you have a structure then use from_struct.
        """
        return cls(nap=nap, uaplap=uaplap)

    @classmethod
    def from_nap_uap_and_lap(cls, nap, uap, lap):
        """
        Construct a Bluetooth Address object given a 16-bit NAP and a 8-bit UAP
        and 24-bit LAP.

        If you have the UAP and LAP already combined then use
        from_nap_and_uaplap.

        If you have a structure then use from_struct.
        """
        if uap > 0x000000ff:
            iprint("UAP 0x%08x is out of range - truncating" % uap)
            uap &= 0x000000ff
        if lap > 0x00ffffff:
            iprint("LAP 0x%08x is out of range - truncating" % lap)
            lap &= 0x00ffffff

        return cls(nap=nap, uaplap=(uap << 24) | lap)

    @classmethod
    def from_struct(cls, struct):
        """
        Construct a Bluetooth Address object given a structure with address
        fields.

        The requirements for the structure are:

          * It must have a field called nap.
          * It either has a field called uap_lap or it has two fields one
            called uap and one called lap.

        The structure may have other fields. These will be ignored. It may
        be desirable to extend this method in future to recognise other
        structures (for example, if some structures had a field name called
        "uaplap" instead of "uap_lap" then it would be natural to extend this
        method rather than make callers extract the fields themselves).

        If you have the values for the individual address parts then use
        from_nap_and_uaplap or from_nap_uap_and_lap
        """
        # It's sometimes useful to be able to use this method in a pipeline
        # where earlier stages have encountrered Nones. Pass these through
        # to the output.
        if struct is None:
            return None

        # Handle typed addresses
        try:
            struct = struct.addr
        except AttributeError:
            pass

        try:
            nap = struct.nap.value
            try:
                uaplap = struct.uap_lap.value
            except AttributeError:
                return cls.from_nap_uap_and_lap(nap, struct.uap.value,
                                                struct.lap.value)
        except AttributeError as exc:
            # exc.args[0] is Py2/3 eqivalent of exc.message
            raise BdAddr.BdAddrAttributeError(str(exc))
        return cls.from_nap_and_uaplap(nap, uaplap)

    @property
    def nap(self):
        """
        The NAP for this address.

        If you're displaying this to the user then use str() on this object
        to get a consistent display. If you want something a bit shorter
        then use the short_str property to get a consistent short string.
        """
        return self._nap

    @property
    def uaplap(self):
        """
        The combined UAP and LAP for this address.

        If you're displaying this to the user then use str() on this object
        to get a consistent display. If you want something a bit shorter
        then use the short_str property to get a consistent short string.
        """
        return self._uaplap

    @property
    def uap(self):
        """
        The UAP for this address.

        If you're displaying this to the user then use str() on this object
        to get a consistent display. If you want something a bit shorter
        then use the short_str property to get a consistent short string.
        """
        return self._uaplap >> 24

    @property
    def lap(self):
        """
        The LAP for this address.

        If you're displaying this to the user then use str() on this object
        to get a consistent display. If you want something a bit shorter
        then use the short_str property to get a consistent short string.
        """
        return self._uaplap & 0x00ffffff

    def __str__(self):
        return "0x%04x%08x" % (self.nap, self.uaplap)

    def __repr__(self):
        return "%s<%s>" % (self.__class__.__name__, self.__str__())

    @property
    def short_str(self):
        """
        Return a short string representation of this object. If you want a
        longer string use str() on the object.
        """
        return "%04x%08x" % (self.nap, self.uaplap)

    # From Python 3, the older __cmp__ method is deprecated. We provide
    # __eq__ and __lt__ and rely on functools.total_ordering to supply the
    # others.

    def __eq__(self, other):
        return (self.nap, self.uaplap) == (other.nap, other.uaplap)

    # Python 3 will delegate __ne__ to __eq__ but Python 2 won't

    def __ne__(self, other):
        return (self.nap, self.uaplap) != (other.nap, other.uaplap)

    def __lt__(self, other):
        return (self.nap, self.uaplap) < (other.nap, other.uaplap)
