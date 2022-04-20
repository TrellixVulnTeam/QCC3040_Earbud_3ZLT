############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
'''
Provides classes for representing the firmware Symbol Lookup Table (SLT).

OldRawSLT -> RawSLT

FlashheartExtendedSLT ->}
    FlashheartBaseSLT ->}->SLTOldBase}
                         AudioBaseSLT}
                          AppsBaseSLT}
           BTZeagleSLT ->  ARMBaseSLT}
AudioStubBaseSLT -> HydraStubBaseSLT }
      CuratorBaseSLT -> }  XAPBaseSLT}-> SLTBase }-> IFwVerReporter -> Base\
      BTBaseSLT .... -> }                                              Component
                      CuratorFakeSLT }-> FakeSLT }
                    FlashheartFakeSLT}
      BTZeagleFakeSLT ->    BTFakeSLT}
                          AppsFakeSLT}
                         AudioFakeSLT}
CuratorSLTNotImplemented   } -> SLTNotImplemented }
FlashheartSLTNotImplemented}
        BTSLTNotImplemented}
      AppsSLTNotImplemented}
     AudioSLTNotImplemented}

'''
#pylint: disable=too-many-lines
import sys
import re
from collections import OrderedDict
from csr.dev.hw.memory_pointer import MemoryPointer as Pointer
from csr.wheels.bitsandbobs import PureVirtualError, \
    words_to_dwords_be, display_hex, \
    words_to_bytes_be, as_string_limited_length as base_asll, CLang
from csr.wheels.version import Version
from csr.dev.model import interface
from csr.dev.model.base_component import BaseComponent
from csr.dev.model.interface import Table
from csr.dev.hw.address_space import AddressSpace
from csr.dev.fw.meta.elf_code_reader import NotInLoadableElf

if sys.version_info > (3,): # python 3 backwards compatibilities
    long = int #pylint: disable=redefined-builtin,invalid-name

def as_string_limited_length(memory, addr, max_len, likely_max_len=None):
    """
    Interpret up to max_len bytes in memory starting at address as a string.  A
    null character (zero byte) encountered before max_len terminates the string.
    """
    try:
        memory.address_range_prefetched
    except AttributeError:
        return base_asll(memory, addr, max_len)
    else:
        if likely_max_len is None:
            likely_max_len = max_len
        with memory.address_range_prefetched(addr, addr+likely_max_len):
            return base_asll(memory, addr, max_len)


class RawSLT(object):
    #pylint: disable=too-many-instance-attributes,too-few-public-methods
    """
    Raw CSR Embedded Symbol Lookup Table (SLT) Interface.

    Accesses raw values in a 'SLT' based on numeric _keys_. (NLUT?)

    Depends only on access to the address space containing the SLT.

    Used by SltBase to access the raw slt table.

    Example:-

       slt = Slt(xap_dataspace)
       cucmd_cmd_ver_addr = slt[0x0009]
       cucmd_cmd_ver = xap_dataspace[cucmd_cmd_ver_addr]

    Does not:-
    - map symbolic keys to key numbers.
    - attempt any interpretation, typing...
    Such things should be layered on top if needed. Not in here please.

    Raises:-
        BadFingerprint - if it looks bad!

    See also:-
    - SymSltBase

    Alt:- - Could have made this a base class for main Slt but the raw
    indexing logic is quite fiddly enough and is rather naturally an [index].
    """
    # ------------------------------------------------------------------------
    # Exceptions
    # ------------------------------------------------------------------------

    class BadFingerprint(RuntimeError):
        'Exception for reporting an unexpected value for the SLT fingerprint'
        def __init__(self, fingerprint, expected_fingerprint):
            self.fingerprint = fingerprint
            self.expected_fingerprint = expected_fingerprint

            super(RawSLT.BadFingerprint, self).__init__(
                "Found 0x%04x, Expected 0x%04x" %
                (fingerprint, expected_fingerprint))

    class BadKey(IndexError):
        'Exception for reporting a bad value for a SLT index lookup key'

    # ------------------------------------------------------------------------
    # 'Structors
    # ------------------------------------------------------------------------

    def __init__(self, core, fingerprint_addr, reference_fingerprint,
                 table_in_data, **kwargs):
        """\
        Construct Interface to the SLT in specified data-space (*)
        kwargs may contain:
            'address_space_offset', default 0
        """
        self._fingerprint_addr = fingerprint_addr
        self._reference_fingerprint = reference_fingerprint
        # Cache pointer to table
        self._table_addr = None
        self._table_in_data = table_in_data
        self._address_space_offset = kwargs.get('address_space_offset', 0)
        # Cache pointer to core
        self._core = core
        self._l_info = self._core.info.layout_info
        self._word_in_chars = (self._l_info.data_word_bits //
                               self._l_info.addr_unit_bits)

        self._init_data_spaces()
        self._reset_table()

    def _init_data_spaces(self):
        self._slt_space = self._core.program_space
        self._slt_data_space = (self._core.data if self._table_in_data
                                else self._slt_space)

    def _check_fingerprint(self):
        fingerprint = self._read_value(self._slt_space, self._fingerprint_addr)
        if fingerprint != self._reference_fingerprint:
            raise self.BadFingerprint(fingerprint,
                                      self._reference_fingerprint)

    def _reset_table(self):
        # Check finger-print
        # Needs to read a full word in each case
        self._check_fingerprint()

        table_addr_addr = self._fingerprint_addr + self._word_in_chars
        table_addr = self._read_value(self._slt_space, table_addr_addr)
        if table_addr != self._table_addr:
            self._table_addr = table_addr
            self._table = Pointer(self._slt_data_space, table_addr)

    def _read_value_bytes(self, memory, offset, num_bytes):
        try:
            return memory[self._address_space_offset + offset:
                          self._address_space_offset + offset + num_bytes]
        except IndexError:
            # Return as many bytes as we can from this memory area
            # This is to support reading the build id string whose
            # length is unknown.
            return memory[self._address_space_offset + offset:]

    def _read_value(self, memory, offset):
        return self._l_info.deserialise(
            self._read_value_bytes(memory, offset, self._word_in_chars))

    def _read_values(self, memory, offset, num_words):
        try:
            memory.address_range_prefetched
        except AttributeError:
            return [self._read_value(memory, offset + i * self._word_in_chars)
                    for i in range(num_words)]
        else:
            with memory.address_range_prefetched(
                    [(offset, offset + num_words*self._word_in_chars)]):
                return [self._read_value(
                    memory, offset + i * self._word_in_chars)
                        for i in range(num_words)]


    # ------------------------------------------------------------------------
    # Required
    # ------------------------------------------------------------------------
    @property
    def slt_data_space(self):
        'accessor to RawSLT data space - intended for use by SLTBase'
        return self._slt_data_space

    # ------------------------------------------------------------------------
    # Public
    # ------------------------------------------------------------------------
    # Following are only intended for use by SLTBase, but python doesn't
    # have concept of friend-access unlike C++
    read_value_bytes = _read_value_bytes
    read_value = _read_value
    read_values = _read_values

    def __getitem__(self, key):
        """\
        Returns the value (usually a data-space address) indexed by the
        key_num.

        Raises:-
        - IndexError - If key_num is illegal.
        - KeyError - If key_num is not found in table.

        N.B. This interface returns the raw value. It does not attempt any
        interpretation.
        """

        # Check whether the table has moved (as a result of new firmware being
        # flashed)
        self._reset_table()

        if not isinstance(key, int):
            raise self.BadKey("key must be integer.")
        if key == self._END_OF_TABLE_KEY:
            raise self.BadKey("Illegal key value: %d." % key)

        # Each entry is two words in table [0] = key, [1] = value
        # Note that words may not be address units
        # (they are for xap, but not for apps)
        cur_offset = 0
        cur_key = self._read_value(self._table, cur_offset)

        while cur_key != self._END_OF_TABLE_KEY:
            if cur_key == key:
                cur_offset += self._word_in_chars
                return self._read_value(self._table, cur_offset)
            # Next
            cur_offset += 2 * self._word_in_chars
            cur_key = self._read_value(self._table, cur_offset)

        raise KeyError("key %d not found." % key)

    # ------------------------------------------------------------------------
    # Private
    # ------------------------------------------------------------------------

    _END_OF_TABLE_KEY = 0

class OldRawSLT(RawSLT):
    #pylint: disable=too-few-public-methods
    """
    Support for old format of Raw CSR Embedded Symbol Lookup Table (SLT)
    Interface.

    Accesses raw values in a 'SLT' based on offset.

    Does not:-
    - map symbolic keys to key numbers.
    - attempt any interpretation, or typing...

    Such things should be layered on top if needed. Not in here please.

    Raises:-
        BadFingerprint - if it looks bad!
    """

    # ------------------------------------------------------------------------
    # Exceptions
    # ------------------------------------------------------------------------

    class BadKey(IndexError):
        'Exception for reporting a bad value for a SLT index lookup key'
    # ------------------------------------------------------------------------
    # 'Structors
    # ------------------------------------------------------------------------

    def __init__(self, core, fingerprint_addr, reference_fingerprint,
                 address_space_offset=0):
        """\
        Construct Interface to the SLT. This is currently always in data-space.
        """
        RawSLT.__init__(self, core, fingerprint_addr, reference_fingerprint,
                        True, address_space_offset=address_space_offset)

    def _init_data_spaces(self):
        # Maintain two data spaces for compatibility with RawSLT
        self._slt_space = self._core.data
        self._slt_data_space = self._slt_space

    def _reset_table(self):
        self._check_fingerprint()

        # Treat the fingerprint as entry 0 of the table
        # This matches the firmware view
        table_addr = self._fingerprint_addr
        self._table_addr = table_addr
        self._table = Pointer(self._slt_data_space, table_addr)

    # ------------------------------------------------------------------------
    # Public
    # ------------------------------------------------------------------------

    def __getitem__(self, word_index):
        """\
        Returns the value (usually a data-space address) at the requested
        word_index.

        Raises:-
        - BadKey - If equested index is illegal format.

        N.B. This interface returns the raw value. It does not attempt any
        interpretation.
        """

        # Account for table having moved (as a result of new firmware being
        # flashed)
        self._reset_table()

        if not isinstance(word_index, int):
            raise self.BadKey("SLT index must be integer.")

        # Note that words may not be address units
        # (they are for xap, but not for apps)
        return self._read_value(self._table, word_index * self._word_in_chars)

class SymbolLookupError(KeyError):
    'A Symbolic Lookup Table entry is not found'

class IFwVerReporter(BaseComponent):
    '''
    Basic SLT reporting interface.  Abstracts access to firmware version info
    away from SLTs, because it's also available in coredumps.  Subclasses
    must provide the build_id_number and build_id_string methods.
    '''
    MAX_BUILD_ID_LEN = 1000  # Default len to which build_id_strings are clipped
    LIKELY_MAX_BUILD_ID_LEN = 100 # Most build IDs are within this bound.
    # We use this to avoid an excessive prefetch.

    def fw_ver(self):
        'Returns firmware version as interface object'
        # You should not have any dependence on the env in fw_ver
        # because it needs to work when there is no fw env available.
        # Conditionalising on the availability of the fw env is tricky.
        try:
            return interface.Code(self.build_id_string)
        except AddressSpace.NoAccess:
            # The self.build_id_string memory is inaccessible
            if hasattr(self.core, "dump_build_string"):
                # The equivalent is available from the loaded core dump
                return interface.Code(self.core.dump_build_string)
            return interface.Code("Memory not accessible")

    def _generate_report_body_elements(self):
        report = []
        report.append(interface.Code('build id number: %d' % \
                                                        self.build_id_number))
        report.append(interface.Code('build id string: %s' % \
                                                        self.build_id_string))
        return report

    @property
    def build_id_number(self):
        'A derived class should return the build id number as integer'
        raise PureVirtualError

    @property
    def build_id_string(self):
        'A derived class should return the build id number as string'
        raise PureVirtualError

    @property
    def core(self):
        'Derived classes returns the core object for this component'
        raise PureVirtualError(self)

class BTZeagleFwVersion(IFwVerReporter):
    """
    Provides suitable means for supporting access to the firmware version
    information.

    """

    # All the reporting functions from derived classes are now in this class
    # pylint: disable=too-many-public-methods

    # Fixed address in memory of the PFAL_DBG_CoreDumpInfo_2 variable
    # (on some builds).
    DBG_CORE_DUMP_INFO_ADDRESS = 0xFEE0 # 0x8EE0 on QCC514X_QCC304X
    BUILD_VERSION_OFFSET = 16
    BUILD_LABEL_OFFSET = 20
    # Magic numbers used in QC BT build system to mean build is unreleased
    UNRELEASED_BUILD_ID = 2000
    UNRELEASED_PATCH_BUILD_ID = 0xFFFF
    # Length limits on what we fetch/print
    MAX_BUILD_ID_LEN = 128 # part of VersionString which is 64 more
    MAX_TIME_LEN = 5 # only want HH:MM

    def __init__(self, core):
        super(BTZeagleFwVersion, self).__init__()
        self._core = core
        self._init_version_reporter()

    # ------------------------------------------------------------------------
    # IFwVerReporter compliance
    # ------------------------------------------------------------------------
    @property
    def build_id_string(self):
        """
        Synonyms self.build_label self.build_id_string.
        This is the build label string uniquely defines builds.

        See also patch_id_string.
        """

        # Read as if a coredump - this is only set I think when bin dump taken
        addr = self['dump_build_string']
        # This is a pointer to the variable
        addr = self._core.dataw[addr]
        return as_string_limited_length(
            self.core.data, addr,
            self.MAX_BUILD_ID_LEN,
            likely_max_len=self.MAX_BUILD_ID_LEN)

    build_label = build_id_string # synonym for Zeagle Users

    @property
    def build_id_number(self):
        """
        A number that should uniquely define a customer-release build.
        
        See also patch_id_number.
        """

        # Read as if a coredump - this is only set I think in the firmware
        # variable when a ram dump is taken. So may not work on a live DUT.
        return self._core.dataw[self['dump_build_id']] & 0xFFFF

    @property
    def core(self):
        'Derived classes returns the core object for this component'
        return self._core


    #+-------------------------------------------------------------------------
    # Protected/overrides
    #+-------------------------------------------------------------------------

    @property
    def title(self):
        return "FW version info from core dump"

    def _init_version_reporter(self):
        """
        Class specific initialisation.

        Locate the firmware version id_number, id string.
        A derived class can use a SLT, but this implementation assumes there
        is no SLT and instead that it can use a hard-wired memory location.

        Does a basic check that the variables at the expected location
        have plausible values.
        """

        addr = self['dump_build_string']
        # We do not expect it to be 0, otherwise it could be anywhere.
        if addr == 0:
            raise ValueError('dump_build_string address is 0')
        # Some things in _field_names we don't want to bother reporting
        # as they won't be found for objects of this class
        # (only derived ones)

        self._field_names = OrderedDict(BTZeagleFwVersion._field_names)
        for name in ('build_ram_version_string', 'build_date', 'build_time',
                     'product_id'):
            del self._field_names[name]

    def _find_a_label(self):

        try:
            label = self.patch_id_string
            if label:
                return label
        except (SymbolLookupError, AddressSpace.NoAccess):
            pass
        try:
            label = self.build_id_string
            if label: # there are scenarios where it is an empty string
                return label
        except (AttributeError,
                SymbolLookupError, AddressSpace.NoAccess) as exc:
            try:
                label = self._core.dump_build_string
            except AttributeError:
                raise exc # original
        else:
            try:
                label = self._core.dump_build_string
            except AttributeError:
                raise SymbolLookupError
        return label

    def __getitem__(self, symbolic_key):
        # Note this code must not use the elf to lookup variables because
        # this routine can be used in the process of locating the elf.
        if symbolic_key == 'dump_build_string':
            return self.DBG_CORE_DUMP_INFO_ADDRESS + self.BUILD_LABEL_OFFSET
        if symbolic_key == 'dump_build_id':
            return self.DBG_CORE_DUMP_INFO_ADDRESS + self.BUILD_VERSION_OFFSET
        raise KeyError

    @property
    def _old_slt_build_id_number(self):
        """
        """

        if self.build_id_string in [
                'CI_BTFM.HW_AUP.1.0-00005.1-QCACHROM-25',  # EMU TO1
                'CI_BTFM.HW_AUP.1.0-00002.1-QCACHROM-29']: # ROM TO1
            return 0
        # Otherwise it's an unreleased SQIF build which used to wrongly
        # self-identify with II of 0.
        return self.UNRELEASED_BUILD_ID

    @staticmethod
    def _parse_label(label):
        """
        Parse a build label and return its significant parts in a tuple
        (build_asic, rom_release_number, patch_release_number (if applicable),
        build_number,
        optional build date, optional changelist).
        Optional/inapplicable items are returned as None.

        Note none of the above provides Product Id, Chip ver, Build ver
        which are in the release label not the build label.
        """

        if not label:
            raise ValueError('No label')
        matches = re.match(
            r'(?:CI_)?BTF[MW]\.(\w+)\.(\d+.\d+)'
            r'((?:\.)\d+)?' # patch release number
            r'-(\d+)(?:.\d+)-(\w+)-(\d+)',
            label)
        if matches:
            return (adjust_build_asic(matches.group('build_asic')),
                    matches.group('rom_no'),
                    matches.group('patch_no'), # optional
                    matches.group('branch_build_no'), None, None)

        # non-Jenkins builds - internal CI releases
        # we don't expect to test more of these and the label format
        # has a meaning ambiguous between branch build number
        # and job number/buildspin.
            return (matches.group(1), matches.group(2),
                    matches.group(3), # optional
                    matches.group(4), None, None)
        matches = re.match(
            r'NONPROD-'
            r'(?P<build_asic>\w+)' # build_asic
            r'\.(?P<rom_no>\d+.\d+)' # rom release number
            r'(?P<patch_no>(?:\.)\d+)?' # optional patch release number
            r'-(?P<branch_build_no>\d+)' # jenkins job == branch build number
            r'-(?P<date>\d{8})' # date
            r'@(?P<change>\d{1,8})', # CL
            label)
        if matches:
            #(build_asic, rom_release_number,
            # patch_release_number, build_number,
            # builddate, changelist)
            return (adjust_build_asic(matches.group('build_asic')),
                    matches.group('rom_no'),
                    matches.group('patch_no'),
                    matches.group('branch_build_no'),
                    matches.group('date'), matches.group('change'))

        matches = re.match(
            r'BTF[WM]\.'
            r'(\w+)\.(\d+.\d+)'
            r'((?:\.)\d+)?' # patch release number
            r'-([-a-zA-Z0-9_.]+)'
            r'-(\d+)-(\d{8})@(\d{1,8})',
            label)
        if matches:
            #(build_asic, rom_release_number,
            # patch_release_number, build_number,
            # builddate, changelist)
            return (matches.group(4), matches.group(2),
                    matches.group(3), matches.group(5),
                    matches.group(6), matches.group(7))

        raise SymbolLookupError(
            'Cannot parse build label {}'.format(label))

    #+-------------------------------------------------------------------------
    # Extensions
    #+-------------------------------------------------------------------------

    @property
    def full_build_id_string(self):
        """
        Provides formatted build_label including date and time
        in style of hydra (other) subsystems.

        However cannot augment with date/time info without access to env
        so this implementation is same as build_id_string.
        """

        id_string = self.build_id_string
        if not id_string:
            id_string = '(empty build label)'
        return id_string

    @property
    def build_ram_version_string(self):
        """
        Synonyms self.release_label self.build_ram_version_string.
        Contents of the RAM variable that holds the Zeagle Shark VersionString
        """

        raise SymbolLookupError

    release_label = build_ram_version_string # synonym for Zeagle users

    @property
    def build_date(self):
        """
        Return the date of build of the fw in ISO format
        """

        raise SymbolLookupError

    @property
    def build_time(self):
        """
        Return the time of build of the fw.
        """

        raise SymbolLookupError

    @property
    def patch_id_number(self):
        """
        Patches do have a build number (or developer magic number) at a
        fixed location.

        See also build_id_number.
        """

        raise SymbolLookupError

    @property
    def patch_id_string(self):
        """
        Patch build string is available in a coredump at fixed location.

        See also build_id_string.
        """

        raise SymbolLookupError

    @property
    def product_id(self):
        """
        A number that should uniquely define the product_id.
        """

        raise SymbolLookupError

    @property
    def chip_ver(self):
        """
        Returns the chip_ver.
        """

        # Read as if a coredump
        return int((self._core.dataw[self['dump_build_id']] & 0xFFFF0000) >> 16)

    @property
    def rom_release_number(self):
        """
        Returns the rom_release_number. 
        Returned as a 16-bit number, major (N) in top byte, minor (M) in bottom.
        """

        major, minor = self._parse_label(
            label=self._find_a_label())[1].split('.')
        return ((int(major) & 0xFF) << 8) + (int(minor) & 0xFF)

    @property
    def patch_release_number(self):
        """
        Returns the patch_release_number. 
        Do not confuse with the patch_id_number.

        """

        number = self._parse_label(label=self._find_a_label())[2]
        return None if number is None else int(number[1:])

    @property
    def build_asic(self):
        """
        The build ASIC descriptor string as scraped from the build_id_string.
        """

        return self._parse_label(label=self._find_a_label())[0]

    @property
    def is_patched(self):
        """
        Whether this firmware is patched based on information to hand in
        this object.
        """
        return self.patch_release_number is not None

    #+------------------------------------------------------------------------
    # Reporting routines
    #+------------------------------------------------------------------------
    WARN_NON_PRINTABLE = (
        'Firmware version contains unexpected characters: '
        '(are you using symbols from the wrong build type?)')
    MSG_FIELDS_UNAVAILABLE = (
        'Some fields are not found until fw env loaded. '
        'Try bt.fw.env.load=True.')

    @classmethod
    def decorate_dotted_number(cls, value):
        """
        Annotates the ROM release number.
        """
        return "{}.{}".format(
            (value & 0xFF00)>> 8, value & 0x00FF)

    @classmethod
    def decorate_build_id_number(cls, value):
        'annotates the value'
        result = '{0} (0x{0:x})'.format(value)
        if value == cls.UNRELEASED_BUILD_ID:
            return result + ' (UNRELEASED - non-unique)'
        return result

    @classmethod
    def decorate_build_id_string(cls, value):
        'annotates the value'
        if value == '':
            return '(empty build label)'
        return value
    @classmethod
    def decorate_patch_id_number(cls, value):
        """annotates the value"""

        if not value:
            return 'None (no patch loaded)'
        result = '{0} (0x{0:x})'.format(value)
        # QCC514X_QCC304X TO1 patch branch also misidentified builds with
        # UNRELEASED_BUILD_ID + 1.
        # TO1.1 uses UNRELEASED_PATCH_BUILD_ID.
        if value in [cls.UNRELEASED_BUILD_ID,
                     cls.UNRELEASED_BUILD_ID+1,
                     cls.UNRELEASED_PATCH_BUILD_ID]:
            return result + ' (Likely UNRELEASED - non-unique)'
        if value > 0:
            return result
        return None

    @classmethod
    def decorate_patch_id_string(cls, value):
        'annotates the value'
        if value == '':
            return '(n/a)'
        return value

    @classmethod
    def decorate_ram_version_string(cls, value):
        'annotates the value'
        if value == '':
            return '(uninitialised)'
        return value

    @classmethod
    def decorate_chip_ver(cls, value):
        'displays the value as hex, as done in release label'
        return '{:04x} (hex)'.format(value)

    @classmethod
    def decorate_as_is(cls, value):
        'does not annotate the value'
        return value

    @staticmethod
    def check_initialised(data):
        return data

    def check_build_version(self):
        # pylint: disable=no-self-use
        return []

    def check_anomalies(self):
        """
        Checks state of SLT for anomalies and returns a list of interface
        Warnings/Errors if there are some, else empty list.
        """

        report = []
        report.extend(self.check_build_version())
        if hasattr(self, 'uninitialised'):
            report.append(interface.Warning(self.WARN_NON_PRINTABLE))
        if hasattr(self, 'missing_entries'):
            report.append(interface.Text(self.MSG_FIELDS_UNAVAILABLE))
        return report

    # Ones which are not accessible via fixed address variables
    # are commented as 'not self' but derived classes may provide
    # see method _init_version_reporter
    _field_names = OrderedDict([
        # The typically most important items first
        ('build_id_number', decorate_build_id_number),
        ('patch_id_number', decorate_patch_id_number),
        ('build_date', decorate_as_is), # not self
        ('build_time', decorate_as_is), # not self
        ('build_id_string', decorate_build_id_string),
        ('patch_id_string', decorate_patch_id_string),
        ('build_ram_version_string', decorate_ram_version_string), # not self
        ('ROM_release_number', decorate_dotted_number),
        ('chip_ver', decorate_chip_ver),
        ('product_id', decorate_as_is), # not self
        ('build_ASIC', decorate_as_is),
        ])

    def __str__(self):
        """
        E.g. as output by the print function/stmt: print(bt.fw.slt)
        """
        return self.full_build_id_string

    def __repr__(self):
        """
        E.g. as output in the REPL, e.g. >>> bt.fw.slt
        """
        content = ''

        for field, _ in self._field_names.items():
            try:
                value = getattr(self, field.lower())
                content += '{}{}: {}'.format(
                    ', ' if content else '', field,
                    # output ints/longs as hex
                    hex(value) if isinstance(value, (int, long)) else value)
            except (SymbolLookupError, ValueError, AddressSpace.NoAccess):
                # just miss them out, after all we're only trying to show
                # what we can, without falling in a heap.
                pass
        return '{}<{}>'.format(self.__class__.__name__, content)

    def __call__(self, **kwargs):
        # kwargs is handy for debugging with trace=True so it is possible
        # with BTZeagleFirmware.version being a property not a method
        # to call bt.fw.version(trace=True)
        return self.generate_report(**kwargs) # or maybe fw_ver()

    def _format_fw_ver(self, core):
        """
        Does the heavy-lifting of formatting the fw_ver output
        """
        try:
            ver_string = self.full_build_id_string
        except AddressSpace.NoAccess:
            # The self.build_id_string memory is inaccessible
            if hasattr(self.core, "dump_build_string"):
                # The equivalent is available from the loaded core dump
                ver_string = self.core.dump_build_string
            else:
                ver_string = "Memory not accessible"

        # And add some more things too
        try:
            build_ram_version = self.check_initialised(
                self.build_ram_version_string)
            ver_string += '\n\n' + (build_ram_version if build_ram_version
                                    else '(not started)')
        except (SymbolLookupError, ValueError):
            # First Zeagle SLT did not support build_ram_version_string
            # or it is uninitialised
            pass

        try:
            ver_string += ('\n' + '(running from {})'.format(
                'ROM' if core.running_from_rom else 'SQIF'))
        except (AttributeError, NotImplementedError):
            pass

        ver_string += self.patch_info().text
        return ver_string

    def patch_info(self):
        """
        return a string representing all the patch version:
        return patch_id_number and the patch_id string.
        """

        # patch_id_number support!
        ver_string = ''
        # Assumption: patch number 0 means same as None: no patch.
        try:
            if self.patch_id_number:
                ver_string += '\nPatch Build: %s (%d) ' % \
                              (hex(self.patch_id_number),
                               self.patch_id_number)
            else:
                ver_string += '\nPatch Build: None '
        except SymbolLookupError:
            # First Zeagle SLT did not support patch_id_number.
            pass

        try:
            if self.patch_id_number:
                patch_id_string = self.check_initialised(self.patch_id_string)
                # could be None when same as ROM.
                ver_string += 'Patch Build ID: %s' % (
                    patch_id_string if patch_id_string else 'n/a')
        except ValueError:
            # not initialised or wrong symbols
            ver_string += 'Patch Build ID: (garbage)'
        except SymbolLookupError:
            # First Zeagle SLT did not support patch_id_string
            ver_string += '\nPatch version not directly available. '\
                          'Call bt.patch_ver() or bt.fw.patch.report()'\
                          '(loads ELF if necessary)'
        return interface.Code(ver_string)

    def fw_ver(self):
        return interface.Code(self._format_fw_ver(self.core))

    def _generate_report_body_elements(self):
        """
        returns a list of interface objects (anomalies) and an interface.Code
        object containing BT fw version string
        """
        #pylint:disable=too-many-locals

        report = interface.Group("BT Firmware version")
        # First gather the data for the report
        report_fields = {}
        for field, decorator_fn in self._field_names.items():
            value = None
            try:
                value = getattr(self, field.lower())
                # decorator_fn is a classmethod object, so call its __func__
                if not isinstance(value, (int, long, type(None))):
                    report_fields[field] = decorator_fn.__func__(
                        self, self.check_initialised(value))
                else:
                    report_fields[field] = decorator_fn.__func__(self, value)
            except AddressSpace.NoAccess:
                report_fields[field] = 'Memory not accessible'
            except SymbolLookupError:
                report_fields[field] = '(not found)'
                self.missing_entries = True
            except RuntimeError as exc:
                report.append(interface.Error(str(exc)))
            except ValueError:
                report_fields[field] = '{} (garbage)'.format(value)
                self.uninitialised = True

        # Now report it
        report.extend(self.check_anomalies())

        report.append(interface.Text('(running from {})'.format(
            'ROM' if self.core.running_from_rom else 'SQIF')))

        for field, _ in self._field_names.items():
            report.append(
                interface.Code('%s: %s' % (
                    field.replace('_', ' '), report_fields[field])))

        return [report]


class SLTBase(IFwVerReporter):
    """\
    CSR Embedded Symbol Lookup Table (SLT) (Abstract Base)

    Retrieves values from a SLT based on textual symbols (The S in Slt!).

    Uses:
    - RawSLT to access the table

    Example:-

       slt = CuratorFirmware.create_slt(data_space)
       cucmd_cmd_ver_addr = slt["CuCmd_Version"]
       cucmd_cmd_ver = xap_dataspace[cucmd_cmd_ver_addr]

    By default, just reports the build ID and string; if more information
    should appear in the report, _generate_report_body_elements should be
    overridden
    """


    def __init__(self, core_or_elf_code, address_space_offset=0):

        self._core = core_or_elf_code
        self._raw_slt = RawSLT(core_or_elf_code, self._fingerprint_addr,
                               self._reference_fingerprint,
                               self.slt_table_is_in_data,
                               address_space_offset=address_space_offset)

#    def __str__(self):
#        text = ''
#        for key, info in self.entry_info.items():
#            index = info[0]
#            if len(info) > 1:
#                type_ = info[1]
#            else:
#                type_ = 'unknown'
#            text += ('%-16s : %4d : %-16s = %04x\n' %
#                     (key, index, type_, self[key]))
#        return text

    @property
    def core(self):
        return self._core

    @property
    def title(self):
        return 'SLT'

    @property
    def _fingerprint_addr(self):
        raise PureVirtualError

    @property
    def _reference_fingerprint(self):
        raise PureVirtualError(self)

    @property
    def _slt_data(self):
        return self._raw_slt.slt_data_space

    @property
    def is_hydra_generic_build(self):
        'returns whether this build is a generic hydra one'
        return False

    def _read_value_bytes(self, offset, num_bytes, data_space=False):
        mem_space = self.core.data if data_space else self._slt_data
        return self._raw_slt.read_value_bytes(mem_space, offset, num_bytes)

    def _read_value(self, offset, data_space=False):
        mem_space = self.core.data if data_space else self._slt_data
        return self._raw_slt.read_value(mem_space, offset)

    def _read_values(self, offset, num_words, data_space=False):
        mem_space = self.core.data if data_space else self._slt_data
        return self._raw_slt.read_values(mem_space, offset, num_words)

    # Extensions

    def __getitem__(self, symbolic_key):

        """Lookup SLT value by symbolic name"""
        try:
            info = self._entry_info[symbolic_key]
        except KeyError as exc:
            raise SymbolLookupError(
                'Symbol lookup table _entry_info for "{}" undefined: {}'.format(
                    symbolic_key, str(exc)), exc)
        numeric_key = info[0]
        try:
            val = self._raw_slt[numeric_key]
        except KeyError as exc:
            raise SymbolLookupError(
                'Symbol lookup table entry "{}", (key {}), missing: {}'.format(
                    symbolic_key, numeric_key, str(exc)), exc)

        # Potential extension: interpret fancy/indirect entry types here
        # based on type info.

        return val

    # Protected / Required
    @property
    def slt_table_is_in_data(self):
        'Accessor to whether the SLT is in the data space instead of code space'
        raise NotImplementedError

    @property
    def _entry_info(self):
        """\
        Dictionary of entry info by symbol.

        E.g:-
        {
            "CuCmd_Cmd"         : (0x0005, "uint8",     "Command", "W"),
            "CuCmd_Rsp"         : (0x0006, "uint16",    "Response", "R"),
            "CuCmd_Parameters"  : (0x0007, "variable",
                                   "Union of command parameters", "W"),
            "CuCmd_Results"     : (0x0008, "variable",
                                   "Union of command results", "R"),
            "CuCmd_Version"     : (0x0009, "uint16",    "Protocol version", "R")
        }
        """
        raise PureVirtualError(self)

class XAPBaseSLT(SLTBase):
    """\
    Interface to XAP baseline SLT.

    This is here for the Curator and BT SLTs to inherit from
    """
    def __init__(self, core):

        SLTBase.__init__(self, core)

    # Extensions

    @property
    def build_id_string(self):
        # Address in PROGRAM SPACE
        addr = self['build_id_string']
        try:
            return as_string_limited_length(
                self.core.data, addr,
                self.MAX_BUILD_ID_LEN,
                likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)
        except RuntimeError:
            # Generic window probably not set up correctly, presumably because
            # the BT firmware hasn't actually been started.  On TRB we can get
            # there directly anyway, so try that.
            return as_string_limited_length(
                self.core.program_space, addr,
                self.MAX_BUILD_ID_LEN,
                likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

    @property
    def build_id_number(self):
        return NotImplemented

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        return 0x80

    @property
    def _reference_fingerprint(self):
        return 0xd397

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space instead of code space'
        return False

class ARMBaseSLT(SLTBase):
    """\
    Interface to ARM baseline SLT.

    It assumes 32 Fingerprint and 32 pointers.
    """
    def __init__(self, core):
        SLTBase.__init__(self, core)

    # Extensions

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        '32 bit fingerprint address'
        return 0x80

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return False

class CuratorBaseSLT(XAPBaseSLT):
    """\
    Interface to Curator baseline SLT.

    The keys here are guaranteed to be valid for future Curator firmware
    versions and can thus be used without knowing the installed firmware
    version. Thats the point of the SLT.

    If/when new fields are added to the Curator SLT they should be modelled by
    specialisation of this class and exposed by the firmware-version-specific
    Firmware.slt interface. This interface will always be published to clients
    that don't know/care what firmware version is installed via the static
    Firmware.create_baseline_slt().

    Future: Consider parsing the SLT header/xml for a generic implementation
    of firmware version-specific SLTs if/when needed.
    """

    @staticmethod
    def generate(core):
        'factory method that generates the python representation of fw SLT.'
        try:
            return CuratorBaseSLT(core)
        except RawSLT.BadFingerprint:
            # If the SLT fingerprint check failed, this may be a Hydra generic
            # build, try that next.
            try:
                return HydraStubBaseSLT(core)
            except (AddressSpace.NoAccess, RawSLT.BadFingerprint):
                return None

    # Extensions

    @property
    def build_id_number(self):
        # Address in PROG SPACE
        words = self._read_values(self['build_id_number'], 2)
        return words_to_dwords_be(words)[0]

    @property
    def patch_id_number(self):
        'returns the patch id number as integer'
        # Address in Data SPACE
        words = self._read_values(self['patch_id_number'], 2, data_space=True)
        return words_to_dwords_be(words)[0]

    # Protected / SLTBase compliance

    def fw_ver(self):
        ver_string = self.build_id_string
        if self.patch_id_number > 0:
            ver_string += '\nPatch Build ID: %s (%d)' % \
                           (hex(self.patch_id_number), self.patch_id_number)
        return interface.Code(ver_string)

    _entry_info = {
        "build_id_number"   : (0x0001, "ptr_to_const_uint32"),
        "build_id_string"   : (0x0002, "ptr_to_string"),
        "panic_data"        : (0x0003, "uint8"),
        "cucmd_cmd"         : (0x0005, "uint8", "Command", "W"),
        "cucmd_rsp"         : (0x0006, "uint16", "Response", "R"),
        "cucmd_parameters"  : (0x0007, "variable",
                               "Union of command parameters", "W"),
        "cucmd_results"     : (0x0008, "variable",
                               "Union of command results", "R"),
        "cucmd_version"     : (0x0009, "uint16", "Protocol version", "R"),
        "patch_id_number"   : (0x000A, "ptr_to_const_uint32"),
    }

class BTBaseSLT(XAPBaseSLT):
    """\
    Interface to BT baseline SLT.

    The keys here are guaranteed to be valid for future BT firmware
    versions and can thus be used without knowing the installed firmware
    version. Thats the point of the SLT.

    If/when new fields are added to the BT SLT they should be modelled by
    specialisation of this class and exposed by the firmware-version-specific
    Firmware.slt interface. This interface will always be published to clients
    that don't know/care what firmware version is installed via the static
    Firmware.create_baseline_slt().

    Future: Consider parsing the SLT header/xml for a generic implementation
    of firmware version-specific SLTs if/when needed.

    Future: Consider merging with the Curator one into a XAPSLTBase class
    """

    @staticmethod
    def generate(core):
        'factory method that generates the python representation of fw SLT.'
        try:
            return BTBaseSLT(core)
        except RawSLT.BadFingerprint:
            # If the SLT fingerprint check failed, this may be a Hydra generic
            # build, try that next.
            try:
                return HydraStubBaseSLT(core)
            except (AddressSpace.NoAccess, RawSLT.BadFingerprint):
                return None

    @property
    def patch_id_number(self):
        'returns the patch id number as integer'
        # Address in Data SPACE
        try:
            word = self._read_value(self['patch_id_number'], data_space=True)
        except KeyError:
            word = 0

        return word

    def fw_ver(self):
        ver_string = self.build_id_string
        if self.patch_id_number > 0:
            ver_string += '\nPatch Build ID: %s (%d)' % \
                           (hex(self.patch_id_number), self.patch_id_number)
        return interface.Code(ver_string)

    def _generate_report_body_elements(self):
        report = []
        report.append(interface.Code(
            'build id string: %s' % self.build_id_string))
        if self.patch_id_number > 0:
            report.append(interface.Code(
                'patch id number: %d' % self.patch_id_number))
        return report

    _entry_info = {
        "patch_id_number"   : (0x002d, "ptr_to_const_uint16"),
        "build_id_string"   : (0x000d, "ptr_to_string")
    }

class BTZeagleSLT(ARMBaseSLT, BTZeagleFwVersion):
    'This is for Bluetooth Zeagle IP only, based on ARM'

    # Override this value lest any code route uses it, though it shouldn't
    # because we have a SLT.
    DBG_CORE_DUMP_INFO_ADDRESS = 0x8EE0 # 0xFEE0 on non-hydra

    MAX_BUILD_ID_LEN = BTZeagleFwVersion.MAX_BUILD_ID_LEN
    MAX_RAM_VERSION_STRING_LEN = 64 + MAX_BUILD_ID_LEN
    MAX_DATE_LEN = 16
    MAX_PATCH_ID_LEN = MAX_BUILD_ID_LEN

    # Extensions
    _entry_info = {
        "build_id_number" : (8, "ptr_to_const_uint32"),
        "build_id_string" : (13, "ptr_to_string"), # string_full; not 7

        "patch_id_number" : (45, "ptr_to_const_uint32"),
        "patch_id_string" : (46, "ptr_to_string"),

        "build_ram_version_string" : (47, "ptr_to_string"),
        "build_date" : (48, "ptr_to_string"),
        "build_time" : (49, "ptr_to_string"),
    }

    def _init_version_reporter(self):
        """
        Locate the firmware version id_number, id string.
        This implementation does nothing.
        """

    @staticmethod
    def generate(core):
        'factory method that generates the python representation of fw SLT.'
        try:
            return BTZeagleSLT(core)
        except RawSLT.BadFingerprint:
            # If the SLT fingerprint check failed, this may be a Hydra generic
            # build, try that next.
            try:
                return HydraM0StubSLT(core)
            except (AddressSpace.NoAccess, RawSLT.BadFingerprint):
                return None

    @staticmethod
    def check_initialised(data):
        '''
        Checks data content represents normal ascii data and if not
        raises ValueError.
        Returns data.
        '''
        if data is None:
            return None
        if re.search(r'([^\w\d:./@= \-]+)', data):
            raise ValueError("(data is non-printable - is BT HCI started?)")
        return data

    @property
    def build_id_number(self):
        try:
            self['build_ram_version_string']
        except SymbolLookupError:
            return self._old_slt_build_id_number
        return int(self._read_value(self['build_id_number']) & 0x0000FFFF)

    @property
    def build_id_string(self):
        # Address in PROGRAM SPACE, so always initialised
        addr = self['build_id_string']
        try:
            return as_string_limited_length(
                self.core.data, addr,
                self.MAX_BUILD_ID_LEN,
                likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)
        except RuntimeError:
            # Generic window probably not set up correctly, presumably because
            # the BT firmware hasn't actually been started.  On TRB we can get
            # there directly anyway, so try that.
            return as_string_limited_length(
                self.core.program_space, addr,
                self.MAX_BUILD_ID_LEN,
                likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

    build_label = build_id_string # synonym for Zeagle users

    @property
    def chip_ver(self):
        # read out of half of the build_id_number
        # (for old AUP TO1 this will be 0 but should be 0x11)
        return int((self._read_value(self['build_id_number']) &
                    0xFFFF0000) >> 16)

    @property
    def patch_id_string(self):
        '''
        The patch-modified (ram) build label, from RAM SPACE;
        so may be either uninitialised, or not yet modified from the ROM
        build_id_string if patch installation has not yet happened in which
        latter case it returns None.
        '''
        # pointer (in PFAL_DBG_CoreDumpInfo_2.uBuildLabel)
        # to patch-modified (ram) build label (gBuildLabel in the patch)
        try:
            addr = self._read_value(self['patch_id_string'], data_space=True)
        except NotInLoadableElf:
            # We're looking at the SLT in the ELF file, not on chip, and since
            # the patch ID string is a RAM variable it's not available in the
            # ELF loadable sections
            return None
        # return None if value is same as rom build_id_string
        # because is unpatched
        patch_id_string = as_string_limited_length(
            self.core.data, addr, self.MAX_PATCH_ID_LEN)
        if patch_id_string == self.build_id_string:
            return None
        return patch_id_string

    @property
    def patch_id_number(self):
        '''
        16 bit patch id number which is the LMP Subversion number for
        the patch once it has been installed.
        Returns None if not yet installed.
        '''
        # return None if value is same as rom build_id_number
        # because is unpatched
        try:
            patch_id_number = int(
                self._read_value(self['patch_id_number'], data_space=True)
                & 0x0000FFFF)
        except NotInLoadableElf:
            # We're looking at the SLT in the ELF file, not on chip, and since
            # the patch ID number is a RAM variable it's not available in the
            # ELF loadable sections
            return None
        if patch_id_number == self.build_id_number:
            return None
        return patch_id_number

    @staticmethod
    def build_date_to_iso(ansi_c_date):
        '''
        Convert ansi_c_date to ISO format from ANSI C format
        so that similar to the other hydra subsystems.
        '''
        from csr.wheels.bitsandbobs import ansi_c_date_to_iso
        return ansi_c_date_to_iso(ansi_c_date)

    @property
    def build_date(self):
        'return SLT entry for the build date'
        # Address in PROGRAM SPACE
        addr = self['build_date']
        try:
            return self.build_date_to_iso(
                as_string_limited_length(self.core.data, addr,
                                         self.MAX_DATE_LEN))
        except RuntimeError:
            # Generic window probably not set up correctly, presumably because
            # the BT firmware hasn't actually been started.  On TRB we can get
            # there directly anyway, so try that.
            return self.build_date_to_iso(
                as_string_limited_length(self.core.program_space, addr,
                                         self.MAX_DATE_LEN))

    @property
    def build_time(self):
        """return SLT entry for the build time"""

        # Address in PROGRAM SPACE
        addr = self['build_time']
        try:
            return as_string_limited_length(
                self.core.data, addr, self.MAX_TIME_LEN)[:self.MAX_TIME_LEN]
        except RuntimeError:
            # Generic window probably not set up correctly, presumably because
            # the BT firmware hasn't actually been started.  On TRB we can get
            # there directly anyway, so try that.
            return as_string_limited_length(
                self.core.program_space, addr,
                self.MAX_TIME_LEN)[:self.MAX_TIME_LEN]

    @property
    def build_ram_version_string(self):
        '''
        Synonyms self.release_label self.build_ram_version_string.
        return SLT entry for the VersionString set once BT HCI Service started
        or modified when a patch is applied.

        This may be an empty string in RAM if not yet initialised.
        '''

        addr = self['build_ram_version_string']
        return as_string_limited_length(self.core.data, addr,
                                        self.MAX_RAM_VERSION_STRING_LEN)

    release_label = build_ram_version_string # synonym for Zeagle users

    @property
    def product_id(self):
        """
        Returns the product_id.

        Might throw AddressSpace.NoAccess if ROM code no available.
        Throws SymbolLookupError if variable not available on this firmware.

        Generally represented as a decimal integer.
        """

        # Scrape from build_ram_version_string which may
        # not have been found/initialised
        try:
            return int(self.build_ram_version_string.
                       split('.')[0].split(' ')[1])
        except IndexError:
            raise SymbolLookupError

    #+-------------------------------------------------------------------------
    #+ ARMBaseSLT Compliance
    #+-------------------------------------------------------------------------

    @property
    def _fingerprint_addr(self):
        '32 bit fingerprint address'
        return 0x180000c0

    @property
    def _reference_fingerprint(self):
        '32 bit fingerprint'
        return 0x42545353 # 'BTSS' from bt_fingerprint.h

    #+-------------------------------------------------------------------------
    #+ Reporting routines
    #+-------------------------------------------------------------------------

    @property
    def full_build_id_string(self):
        """
        Provides formatted build_label including date and time
        in style of other hydra subsystems
        """
        try:
            build_date = self.build_date
        except SymbolLookupError:
            # First tapeout didn't have SLT entries for build date/time
            return self.build_id_string
        return "%s %s %s" %(build_date, self.build_time, self.build_id_string)

    def _format_fw_ver(self, core):
        '''
        Does the heavy-lifting of formatting the fw_ver output
        '''
        # You should not have any dependence on the env in fw_ver
        # because it needs to work when there is no fw env available.
        # Conditionalising on the availability of the fw env is tricky.

        if not core.is_clocked():
            return (
                "(fw_ver is not available: BTSS is not clocked."
                " Try starting the BT HCI Service.)")
        return BTZeagleFwVersion._format_fw_ver(self, core)

    def patch_info(self):
        """
        return a string representing all the patch version:
        return patch_id_number and the patch_id string.
        This is used in fw_ver() so should not access the env.
        """

        # patch_id_number support!
        ver_string = ''
        # Assumption: patch number 0 means same as None: no patch.
        try:
            if self.patch_id_number:
                ver_string += '\nPatch Build: %s (%d) ' % \
                              (hex(self.patch_id_number),
                               self.patch_id_number)
            else:
                ver_string += '\nPatch Build: None '
        except SymbolLookupError:
            # First Zeagle SLT did not support patch_id_number.
            pass

        try:
            if self.patch_id_number:
                patch_id_string = self.check_initialised(self.patch_id_string)
                # could be None when same as ROM.
                ver_string += 'Patch Build ID: %s' % (
                    patch_id_string if patch_id_string else 'n/a')
        except ValueError:
            # not initialised or wrong symbols
            ver_string += 'Patch Build ID: (garbage)'
        except SymbolLookupError:
            # First Zeagle SLT did not support patch_id_string
            # Message similar to that in DefaultFirmware.fw_ver().
            ver_string += '\nPatch version not directly available '\
                          'via symbol lookup table. '\
                          'Call bt.patch_ver() or bt.fw.patch.report()'\
                          '(loads ELF if necessary)'
        return interface.Code(ver_string)

    def fw_ver(self):
        # You should not have any dependence on the env in fw_ver
        # because it needs to work when there is no fw env available.
        # Conditionalising on the availability of the fw env is tricky.
        #
        # This function is provided as a (redirection from a) core-command,
        # (i.e. bt.fw_ver()), so it must return an interface object,
        # which the CoreCommand Manager renders.

        return interface.Code(self._format_fw_ver(self.core))

    def check_anomalies(self):
        """
        Checks state of SLT for anomalies and returns a list of interface
        Warnings/Errors if there are some, else empty list.
        """

        report = []
        report.extend(self.check_build_version())
        if hasattr(self, 'uninitialised'):

            report.append(interface.Warning(
                'Firmware version contains unexpected characters: '
                '(have you started BT HCI Service or '
                'are you using symbols from the wrong build type?)'))
        if hasattr(self, 'missing_entries'):
            report.append(interface.Error(
                'Some symbol lookup table entries missing - '
                'this may be the case for pre AUP TO1.1 builds'))
        return report

class AppsBaseSLT(SLTBase):
    """\
    Interface to Apps baseline SLT.

    The keys here are guaranteed to be valid for future Apps firmware
    versions and can thus be used without knowing the installed firmware
    version. Thats the point of the SLT.

    If/when new fields are added to the Apps SLT they should be modelled by
    specialisation of this class and exposed by the firmware-version-specific
    Firmware.slt interface. This interface will always be published to clients
    that don't know/care what firmware version is installed via the static
    Firmware.create_baseline_slt().

    Future: Consider parsing the SLT header/xml for a generic implementation
    of firmware version-specific SLTs if/when needed.
    """

    @classmethod
    def generate(cls, core):
        'factory method that generates the python representation of fw SLT.'
        try:
            return cls(core)
        except (AddressSpace.NoAccess, RawSLT.BadFingerprint):
            # If the SLT fingerprint check failed, this may be a Hydra generic
            # build, try that next.
            return HydraStubBaseSLT(core)

    # Extensions

    @property
    def build_id_number(self):
        # Address in PROGRAM SPACE
        return self._read_value(self['build_id_number'])

    @property
    def build_id_string(self):
        # Address in PROGRAM SPACE
        addr = self['build_id_string']
        return as_string_limited_length(
            self._core.dm, addr, self.MAX_BUILD_ID_LEN,
            likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

    @property
    def bsif_prim_log_size(self):
        'return the value (which is size of buffer) directly from after the ID'
        return self['bsif_prim_log_size']

    @property
    def bsif_prim_log(self):
        '''
        return the pointer value directly
        (which is to the bsif_prim_log buffer)
        '''
        return self['bsif_prim_log']

    @property
    def bsif_prim_log_pos(self):
        '''
        return the pointer value directly
        (which is to the bsif_prim_log position)
        '''
        return self['bsif_prim_log_pos']

    @property
    def bsif_prim_log_start(self):
        '''
        return the pointer value directly
        (which is to the bsif_prim_log start)
        '''
        return self['bsif_prim_log_start']

    @property
    def trap_version(self):
        'return the version number of the trap interface'
        return Version(self._read_values(self['trap_version'], 3))

    @property
    @display_hex
    def trapset_bitmap(self):
        'returns the trapset bitmap'
        bitmap_len_dwords = self._read_value(self['trapset_bitmap_length'])
        bm_list = self._read_values(self['trapset_bitmap'], bitmap_len_dwords)
        bitmap = 0
        for i, bm_entry in enumerate(bm_list):
            bitmap |= bm_entry << (32 * i)
        return bitmap

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        return 0x80

    @property
    def _reference_fingerprint(self):
        return 0x41707073

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return True

    def _generate_report_body_elements(self):
        report = super(AppsBaseSLT, self)._generate_report_body_elements()

        try:
            report.append(interface.Code('trap API version: %s' % \
                                                        self.trap_version))
        except KeyError:
            pass
        try:
            report.append(interface.Code('trapset bitmap  : %s' % \
                                                        self.trapset_bitmap))
        except KeyError:
            pass

        return report

    # Note that apps uses a packed id string (id 3)
    _entry_info = {
        "build_id_number"       : (0x0001, "ptr_to_const_uint32"),
        "build_id_string"       : (0x0003, "ptr_to_string"),
        "bsif_prim_log_size"    : (0x0007, "uint32"),
        "bsif_prim_log"         : (0x0008, "pointer to uint8[]"),
        "bsif_prim_log_start"   : (0x0009, "pointer to uint16"),
        "bsif_prim_log_pos"     : (0x000A, "pointer to uint16"),
        "trap_version"          : (11, "ptr_to_const_uint32"),
        "trapset_bitmap"        : (12, "ptr_to_const_uint32"),
        "trapset_bitmap_length" : (13, "ptr_to_const_uint32"),
    }

class Apps1SLT(AppsBaseSLT):
    """Interface for P1 specific SLT
    """
    def _get_slt_app_start_offset(self):
        """ The FW SLT section contains a fingerprint value and a pointer
            to the SLT table. The Application's SLT starts right after that.

        Returns:
            int -- The offset in program memory where App's SLT section starts
        """
        return (self._fingerprint_addr +
                ((self._reference_fingerprint.bit_length() + 1) /
                 self.core.info.layout_info.addr_unit_bits) +
                self.core.info.layout_info.addr_units_per_data_word)

    def libs_ver(self):
        'returns table of library versions'
        libs = LibsVersions(self.core, self.core.info.layout_info,
                            self._get_slt_app_start_offset())
        return libs.as_table()

class LibsVersions(object):
    'Class encapsulating information about versions of libraries in apps1 SS'
    #pylint: disable=too-few-public-methods
    def __init__(self, core, layout_info, slt_app_start):
        self.slt_app_start = slt_app_start
        self.layout_info = layout_info
        self._core = core
        self.word_in_chars = self.layout_info.addr_units_per_data_word

    def _find_libs(self):
        """ Generator that returns one library entry at a time and stops when
           it finds the special marker in memory or it has reached the end
           of the program memory

        Returns:
            tuple[str, int] -- A tuple containing the name and version
                               of each entry:
                               E.g. ("my_library", 123456)
        """
        end_of_libs_marker = int('0x17bec0de', 16)

        # Current library count is a little less than 150 so 200 max count
        # seems a good compromise between speed and future proofing for the case
        # where the marker can't be found. I.e. older app builds.
        max_number_of_libraries = 200

        version_in_chars = self.word_in_chars
        name_pointer_in_chars = self.word_in_chars
        lib_in_chars = version_in_chars + name_pointer_in_chars

        end_of_libs_addr = self.slt_app_start
        for addr in range(0, lib_in_chars * max_number_of_libraries,
                          lib_in_chars):
            if end_of_libs_marker == self._core.progw[addr]:
                end_of_libs_addr = addr

        # The generator will resume execution from the yield statement inside
        # the for loop below on each iteration so the search loop above will
        # only be executed once, the first time the generator runs.
        for addr in range(self.slt_app_start, end_of_libs_addr, lib_in_chars):
            version = self._core.progw[addr]

            name_ptr = self._core.progw[addr + version_in_chars]
            name = as_string_limited_length(self._core.data, name_ptr, 64)

            yield (name, version)

    def as_table(self):
        'return interface.Table object containing library version info'
        libs_table = interface.Table(headings=["Lib Name", "Version"])

        for lib in self._find_libs():
            libs_table.add_row(lib)

        return libs_table

class AudioBaseSLT(SLTBase):
    """\
    http://wiki/Hydra_audio_subsystem/SLT
    """
    # Extensions

    @staticmethod
    def generate(core):
        'factory method that generates the python representation of fw SLT.'
        try:
            return AudioBaseSLT(core)
        except RawSLT.BadFingerprint:
            # If the SLT fingerprint check failed, this may be a Hydra generic
            # build, try that next.
            try:
                return AudioStubBaseSLT(core)
            except (AddressSpace.NoAccess, RawSLT.BadFingerprint):
                return None

    @property
    def build_id_number(self):
        # Address in DATA SPACE - limited to 16 bits
        return self._read_value(
            self['build_id_number'], data_space=True) & 0xFFFF

    @property
    def build_id_string(self):
        # Address in DATA SPACE
        addr = self['build_id_string']
        return as_string_limited_length(
            self.core.dm, addr,
            self.MAX_BUILD_ID_LEN,
            likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

    @property
    def patch_id_number(self):
        'returns the build id number as integer'
        # Address in DATA SPACE
        value = self._read_value(self['patch_id_number'], data_space=True)
        if value >= 0x10000:
            value = 0
        return value

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        return 0x20

    @property
    def _reference_fingerprint(self):
        return 0x007C40FB

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return True

    def fw_ver(self):
        ver_string = self.build_id_string
        try:
            if self.patch_id_number > 0:
                ver_string += '\nPatch Build ID: %s (%d)' % \
                               (hex(self.patch_id_number), self.patch_id_number)
        except KeyError:
            # Not available in this firmware
            pass
        return interface.Code(ver_string)

    # Note that apps uses a packed id string (id 3)
    _entry_info = {
        "build_id_number"       : (0x0001, "ptr_to_const_uint32"),
        "build_id_string"       : (0x0003, "ptr_to_string"),
        "patch_id_number"       : (0x000b, "patch_id_number")
    }

class SLTOldBase(SLTBase):
    """\
    Base for an old format CSR Embedded Symbol Lookup Table (SLT)

    Retrieves values from a SLT based on textual symbols (The S in Slt!).

    Uses:
    - OldRawSLT to access the table

    By default, just reports the build ID and string; if more information
    should appear in the report, _generate_report_body_elements should be
    overridden
    """
    #No need to super call nor access abstract slt_table_is_in_data
    #pylint: disable=super-init-not-called, abstract-method
    def __init__(self, core, address_space_offset=0):
        self._core = core
        self._raw_slt = OldRawSLT(core, self._fingerprint_addr,
                                  self._reference_fingerprint,
                                  address_space_offset=address_space_offset)

    def _read_value_bytes(self, offset, num_bytes, data_space=True):
        return SLTBase._read_value_bytes(self, offset, num_bytes, data_space)

    def _read_value(self, offset, data_space=True):
        return SLTBase._read_value(self, offset, data_space)

    def _read_values(self, offset, num_words, data_space=True):
        return SLTBase._read_values(self, offset, num_words, data_space=True)

    # Protected / Required
    @property
    def _reference_fingerprint(self):
        raise PureVirtualError(self)

    @property
    def _entry_info(self):
        """\
        Dictionary of entry info by symbol.
        First entry in the tuple is a (word) offset in the table.
        Extension tables are NOT YET DEALT WITH

        E.g:-
        {
            "build_id_ptr"  : (0x0001, "uint8", "Pointer to build ID"),
            "slt_hostptr"   : (0x0002, "uint16",
                               "Pointer to host details extension"),
            "slt_extended"  : (0x0003, "variable", "extended slt"),
        }
        """
        raise PureVirtualError(self)

class FlashheartBaseSLT(SLTOldBase):
    """\
    Interface to Flashheart baseline SLT (as of A05).

    The keys here should be valid for future Flashheart firmware
    versions and can thus be used without knowing the installed firmware
    version. Thats the point of the SLT.

    If/when new fields are added to the SLT they should be modelled by
    specialisation of this class and exposed by the firmware-version-specific
    Firmware.slt interface. This interface will always be published to clients
    that don't know/care what firmware version is installed via the static
    Firmware.create_baseline_slt().
    """

    @property
    def subcomponents(self):
        return {"ext" : "_slt_ext"}

    def _generate_report_body_elements(self):
        slut_table = Table()
        slut_table.add_row(["Application library version ",
                            "0x%04X @ address 0x%04X"
                            %(self.build_id_number, self["build_id_ptr"])])
        slut_table.add_row(["Pointer to host download control structure ",
                            "0x%04X @ address 0x%04X"
                            %(self._core.data[self["slt_hostptr"]],
                              self["slt_hostptr"])])

        # Perform basic checks on the extended SLT
        esl_status = "found at address 0x%X"%self["slt_extended"]
        if self.ext is None:
            esl_status = "Not "+esl_status
        slut_table.add_row(["Extended SLT", esl_status])

        return [slut_table]


    # Extensions

    @property
    def build_id_number(self):
        'returns the build id number as integer'
        # Address in DATA SPACE
        return self._read_value(self['build_id_ptr'])

    @property
    def patch_id_number(self):
        'returns the patch id number as integer'
        # Maintained this API, although the value is not what we think
        # because a) history; b) can be of use in future
        return self.build_id_number

    @property
    def build_id_string(self):
        build_id = self.build_id_number
        return "Flashheart build %d"%build_id

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        return 0xEFFC

    @property
    def _reference_fingerprint(self):
        return 0x7ab1

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return True

    def fw_ver(self):
        ver_string = self.build_id_string
        # Potential extension:: Get extra info from ext slt
        return interface.Code(ver_string)

    _entry_info = {
        "fingerprint"   : (0x0000, "uint16"),
        "build_id_ptr"  : (0x0001, "uint16", "Pointer to build ID"),
        "slt_hostptr"   : (0x0002, "uint16",
                           "Pointer to host details extension"),
        "slt_extended"  : (0x0003, "variable", "extended slt"),
    }

    @property
    def ext(self):
        'returns the extended SLT'
        try:
            self._slt_ext
        except AttributeError:
            if self["slt_extended"] != 0:
                try:
                    self._slt_ext = FlashheartExtendedSLT(self.core,
                                                          self["slt_extended"])
                except RawSLT.BadFingerprint:
                    self._slt_ext = None
            else:
                self._slf_ext = None
        return self._slt_ext

    def __getattr__(self, attr):
        if attr != "_slt_ext":
            try:
                return getattr(self.ext, attr)
            except AttributeError:
                pass
        raise AttributeError("'%s' object has no attribute '%s'" %
                             (type(self).__name__, attr))

class UnexpectedFlashheartROMVersion(Exception):
    """
    Read an unexpected ROM version
    """
    def __init__(self, version):
        super(UnexpectedFlashheartROMVersion, self).__init__(
            "Unexpected Flashheart ROM build ID %04x" % version)

class FlashheartExtendedSLT(SLTOldBase):
    """
    The Flashheart extended SLT is pointed to by an entry in the base SLT.  It
    contains some extra details such as the ROM name and build ID and build
    strings for the application and ROM, plus a few others.
    """

    def __init__(self, core, fprint_addr):
        #pylint: disable=super-init-not-called
        self._core = core
        self._fprint_addr = fprint_addr
        self._raw_slt = OldRawSLT(core, self._fingerprint_addr,
                                  self._reference_fingerprint)

    @property
    def title(self):
        return "Extended SLT"

    def _generate_report_body_elements(self):
        # Dump the extended SLT contents

        table = Table()

        # We access the slut extended manually rather than via symbols
        # (the whole point of slut)
        table.add_row(["Rom Revision Label", self.rom_name])
        table.add_row(["Expected ROM ID", "0x%X"% self.expected_rom])
        table.add_row(["Build name (Application)", self.app_version_string])
        table.add_row(["Build name (ROM)", self.rom_version_string])
        table.add_row(["Interface Control Block for UCI Debug SPI transport",
                       "@ 0x%04x"% self["spi_icb"]])
        table.add_row(["Interrupt register for INT_SOURCE_DBG_USER_EVENT",
                       "@ 0x%04x"% self["dbg_user_event"]])
        extended = ("not found" if self["extension"] in (0, 0xffff) else
                    "found @ 0x%04x" % self["extension"])
        table.add_row(["Extended SLT extension...", extended])

        return [[table, False]]

    @property
    def _fingerprint_addr(self):
        return self._fprint_addr

    @property
    def _reference_fingerprint(self):
        return 0x7ab3

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return True

    _entry_info = {
        "fingerprint"            : (0x0000, "uint16"),
        "rom_name_0"             : (0x0001, "uint16"),
        "rom_name_1"             : (0x0002, "uint16"),
        "expected_rom"           : (0x0003, "uint16"),
        "app_version_string"     : (0x0004, "uint16"),
        "rom_version_string"     : (0x0005, "uint16"),
        "spi_icb"                : (0x0006, "uint16"),
        "dbg_user_event"         : (0x0007, "uint16"),
        "extension"              : (0x0008, "uint16")
        }

    @property
    @display_hex
    def expected_rom(self):
        """
        Look up the expected_rom key, correcting 0x4a0 to 0x58e, cos apparently
        you have to.  Values other than these and 0x68f are unexpected
        """
        slt_value = self["expected_rom"]
        if slt_value == 0x4a0:
            return 0x58e
        if slt_value in (0x58e, 0x68f):
            return slt_value
        raise UnexpectedFlashheartROMVersion(slt_value)

    @property
    def rom_name(self):
        'returns name of rom as found in SLT'
        name_chars = words_to_bytes_be([self["rom_name_0"], self["rom_name_1"]])
        return CLang.get_string(name_chars)

    @property
    def app_version_string(self):
        """This is a pointer to a pointer"""
        addr = self["app_version_string"]
        # de-ref
        try:
            addr = self._core.data[addr]
        except AddressSpace.NoAccess:
            return "No memory @ 0x%x"%addr

        return as_string_limited_length(
            self._core.data, addr,
            self.MAX_BUILD_ID_LEN,
            likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

    @property
    def rom_version_string(self):
        'returns the rom version identified by SLT as a string'
        addr = self["rom_version_string"]
        return as_string_limited_length(
            self._core.data, addr,
            self.MAX_BUILD_ID_LEN,
            likely_max_len=self.LIKELY_MAX_BUILD_ID_LEN)

class HydraStubBaseSLT(SLTBase):
    """\
    Interface to Hydra generic SLT's.
    """
    # Extensions

    @property
    def build_id_number(self):
        # Address in PROGRAM SPACE
        return self._read_value(self['build_id_number'])

    @property
    def build_id_string(self):
        # Address in PROGRAM SPACE
        addr = self['build_id_string']
        data = self._read_value_bytes(addr, 100)
        return CLang.get_string(data)

    # Protected / SLTBase compliance

    @property
    def _fingerprint_addr(self):
        return 0x80

    @property
    def _reference_fingerprint(self):
        return 0xE235

    @property
    def slt_table_is_in_data(self):
        'returns True if the SLT is in the data space and not code space'
        return True

    @property
    def is_hydra_generic_build(self):
        'returns whether this build is a generic hydra one'
        return True

    # Hydra stubs do not support packed strings, build_id_string is unpacked.
    _entry_info = {
        "build_id_number"       : (0x0001, "ptr_to_const_uint32"),
        "build_id_string"       : (0x0002, "ptr_to_string"),
        "panic_data"            : (0x0003, "uint32"),
        "rom_id_number"         : (0x0004, "ptr_to_const_uint32"),
        "service_record_length" : (0x0005, "uint32"),
        "service_records"       : (0x0006, "ptr_to_const_uint16"),
        "sub_sys_chip_version"  : (0x0007, "ptr_to_const_uint16"),
    }

class AudioStubBaseSLT(HydraStubBaseSLT):
    """\
    This is identical to the other stub SLTs with the exception that the
    fingerprint is at 0x20
    """

    @property
    def _fingerprint_addr(self):
        return 0x20

class HydraM0StubSLT(HydraStubBaseSLT):
    """
    This is identical to the other stub SLTs with the exception that the
    fingerprint is a 32 bit fingerprint address at 0xc0
    """
    @property
    def _fingerprint_addr(self):
        # 32 bit fingerprint address
        return 0x180000c0

class SLTNotImplemented(IFwVerReporter):
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """

    @property
    def title(self):
        return "SLT not available"

    @property
    def build_id_number(self):
        'Raises because the SLT build id number is not implemented'
        raise NotImplementedError

    @property
    def build_id_string(self):
        raise NotImplementedError

    @property
    def patch_id_number(self):
        'Raises because the SLT patch id number is not implemented'
        raise NotImplementedError

    def _generate_report_body_elements(self):
        report = []
        report.append(interface.Code('No access to SLT'))
        return report


class CuratorSLTNotImplemented(SLTNotImplemented):
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """
    #pylint: disable=abstract-method
    @property
    def build_id_string(self):
        return "SLT is not available."

class CuratorSLTBlank(SLTNotImplemented):
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """
    #pylint: disable=abstract-method
    @property
    def build_id_string(self):
        return "SLT is not available.\nCurator may be attempting to run " \
               "from a blank SQIF. Consider programming the SQIF or " \
               "running Curator from ROM."


class FlashheartSLTNotImplemented(SLTNotImplemented):
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """
    #pylint: disable=abstract-method
    ext = None

class BTSLTNotImplemented(SLTNotImplemented): #pylint: disable=abstract-method
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """

class AppsSLTNotImplemented(SLTNotImplemented): #pylint: disable=abstract-method
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """


class AudioSLTNotImplemented(SLTNotImplemented): #pylint: disable=abstract-method
    """
    This class exists to indicate cleanly to callers of high-level functions
    that reference the SLT that SLT access isn't available.  It overrides the
    SLTReportMixin's methods with appropriate errors/messages.
    """

class FakeSLTAttributeError(AttributeError):
    """
    Specialisation of AttributeError to distinguish errors arising from the use
    of FakeSLT instances instead of real ones
    """

class FakeSLT(IFwVerReporter):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

    def __init__(self, fw_id, fw_str):
        self._fw_id = fw_id
        self._fw_str = fw_str

    @property
    def title(self):
        return "FW version info from core dump"

    @property
    def build_id_number(self):
        return self._fw_id

    @property
    def build_id_string(self):
        return self._fw_str

    def __getattr__(self, attr):
        raise FakeSLTAttributeError("AttributeError: '{}' object has "
                      "no attribute '{}'".format(self.__class__.__name__,attr))

class CuratorFakeSLT(FakeSLT):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

class FlashheartFakeSLT(FakeSLT):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

class BTFakeSLT(FakeSLT):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

class AppsFakeSLT(FakeSLT):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

class AudioFakeSLT(FakeSLT):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.
    """

class BTZeagleFakeSLT(BTFakeSLT, BTZeagleFwVersion):
    """
    Give access to the most interesting information in the SLT - the firmware
    version - by other means, in order to implement the IFwVerReporter
    interface.

    A class supporting either form of Zeagle SLT.
    Original one had unreliable build_id_number in RAM.
    New ones have ROM-based build_id_number and extra SLT entries.

    Has synonyms self.build_label and self.build_id_string.
    """

    fw_ver = BTZeagleFwVersion.fw_ver

    def __init__(self, fw_id, fw_str, core):
        # Extract the top 16-bits which are the Chipset Version (defined in
        # firmware build environment variable CHIP_VER) and exclude from
        # the fw build id number.
        self._chip_ver = int(fw_id >> 16)
        fw_id = int(fw_id & 0x0000FFFF)

        # super class sets self._fw_id, (accessor self.build_id_number) and
        # self._fw_str (accessor self.build_id_string)
        super(BTZeagleFakeSLT, self).__init__(fw_id, fw_str)

        self._core = core
        chip = core.subsystem.chip


    @property
    def core(self):
        'Derived classes returns the core object for this component'
        return self._core

    @property
    def build_id_number(self):
        '''
        Replace default implementation with one that adjusts the return value.
        '''
        return self._fw_id

    build_label = FakeSLT.build_id_string # synonym for Zeagle Users

    @property
    def chip_ver(self):
        """
        A number that should uniquely define the chip version.

        This is not the same as the rom_release_number or the product id
        or the build ver.
        """
        return self._chip_ver
