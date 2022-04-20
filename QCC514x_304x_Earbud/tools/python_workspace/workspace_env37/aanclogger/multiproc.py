############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Module to provide support for running ACAT in multiprocessing mode.

This allows a stereo class to be created that can access fields from both
devices.

"""

import argparse
from multiprocessing.managers import BaseManager, NamespaceProxy
import operator

from ACAT.Core.CoreUtils import qformat_factory
from ACAT.Analysis.Opmgr import Capability, Operator
from aanclogger.connect import CONN_LEFT, CONN_RIGHT
from aanclogger.graph import OPERATOR_NAME, OPERATOR_CAP_ID, OPERATOR_CAP_IDX
from aanclogger.graph import HANDLE_NAME, HANDLE_OPERATOR, HANDLE_ATTR, \
    HANDLE_CONVERSION, HANDLE_LOG_FMT, HANDLE_MISSING
from aanclogger.graph import CONVERSION_BITMASK, CONVERSION_OFFSET, \
    CONVERSION_QFMT, CONVERSION_SCALE, CONVERSION_SIGNED
from aanclogger.graph import NoOperatorsException

class MyManager(BaseManager):
    """Custom manager to multi-process ACAT instances."""


class AANCProxy(NamespaceProxy):
    """Proxy for AANC."""
    _exposed_ = ('__getattribute__', 'read', 'do_analysis', 'refresh',
                 'reload', 'log_fmt', 'valid')

    def read(self, attr):
        """read values bypassing the need to pickle intermediate objects."""
        callmethod = object.__getattribute__(self, '_callmethod')
        return callmethod('read', (attr,))

    def log_fmt(self, attr):
        """Return the log format for a given attribute."""
        callmethod = object.__getattribute__(self, '_callmethod')
        return callmethod('log_fmt', (attr,))

    def do_analysis(self):
        """Run an ACAT analysis."""
        callmethod = object.__getattribute__(self, '_callmethod')
        return callmethod('do_analysis')

    def reload(self, operators, handles):
        """Reload object data."""
        callmethod = object.__getattribute__(self, '_callmethod')
        return callmethod('reload', (operators, handles,))

    def valid(self):
        """Return object validity."""
        callmethod = object.__getattribute__(self, '_callmethod')
        return callmethod('valid')

    def __getattribute__(self, attr):
        if '.' in attr:
            return self.read(attr)

        return super(AANCProxy, self).__getattribute__(attr)


class ACATChipVar(): # pylint: disable=too-many-instance-attributes
    """Represent a chip variable in ACAT.

    Args:
        handle (ACAT.Opmgr.Operator): ACAT operator object
        attr (str): Attribute to read
        conversion (dict): Conversion dictionary
        missing (int): Value to return if data is missing
        log_fmt (str): Format for logging data

    This stores the address of the variable & format to read it in to allow
    for refresh commands this side of the proxy.
    """
    def __init__(self, handle, attr, conversion, missing, log_fmt=""): # pylint: disable=too-many-arguments
        self._handle = handle
        self.attr = attr

        self.conv = argparse.Namespace()
        self.conv.signed = conversion[CONVERSION_SIGNED]
        self.conv.qfmt = conversion[CONVERSION_QFMT]
        self.conv.scale = conversion[CONVERSION_SCALE]
        self.conv.offset = conversion[CONVERSION_OFFSET]
        self.conv.bitmask = conversion[CONVERSION_BITMASK]

        self.missing = missing
        self.log_fmt = log_fmt

        # Make sure bitmask is an integer - could be passed as a hex string
        if self.conv.bitmask:
            self.conv.bitmask = int(self.conv.bitmask, 16)

        # Scale can be passed as a string, e.g. '2**-16'. Convert to a number
        # via eval.
        if isinstance(self.conv.scale, str):
            self.conv.scale = eval(self.conv.scale) # pylint: disable=eval-used

        # Generate Q format conversion factory
        if self.conv.qfmt:
            fmtvals = [int(val) for val in self.conv.qfmt.split('.')]
            self.conv.qfmt = qformat_factory(*fmtvals)

        # If the handle is a string (e.g. 'constant') then no chip data is read
        if isinstance(self._handle, str):
            return

        # Read the chip data and get the address and size, along with a handle
        # to read data later
        var_handle = self.get_var_handle()
        self.address = var_handle.address
        self.size = var_handle.size
        if self.size not in [1, 2, 4]:
            raise ValueError("Unhandled variable size: %d (%s)" % (
                self.size, self.attr))
        self._chipdata = var_handle._chipdata

    def get_var_handle(self):
        """Get a handle to the ACAT ChipdataVariable."""
        attrs = self.attr.split('.')[::-1]
        handle = self._handle

        while attrs:
            cur_attr = attrs.pop()
            try:
                handle = handle.__getattr__(cur_attr)
            except AttributeError:
                handle = handle.__getattribute__(cur_attr)

        return handle

    def read(self):
        """Read the attribute."""
        # Return the constant value if just a constant
        if isinstance(self._handle, str):
            return self.attr

        # Read a raw word & convert to bytes. This is needed if we need to
        # access a sub-chunk.
        try:
            data = self._chipdata.get_data(self.address)
            data_bytes = data.to_bytes(4, 'little')
        except TypeError:
            # Kalimba access error
            return self.missing

        data_bytes = data_bytes[self.address % 4:]

        # Reinterpret the byte string
        data = int.from_bytes(data_bytes, 'little', signed=self.conv.signed)
        if self.size == 2:
            data = data & 0xFFFF
        elif self.size == 1:
            data = data & 0xF

        # Apply any conversions: Q format is standalone
        if self.conv.qfmt:
            return self.conv.qfmt(data)

        if self.conv.bitmask:
            data = data & self.conv.bitmask

        return (data * self.conv.scale) + self.conv.offset

class AANCLauncher():
    """Launch an AANC class.

    Args:
        params (list): List of parameters to launch ACAT with.

    """
    def __init__(self, params):
        self._params = params
        self._operators = {}
        self.handles = {}
        self._opmgr = None
        self._ids = []
        self._session = None

    def load_session(self):
        """Load the ACAT session."""
        # ACAT import has to be within the class to be encapsulated inside
        # the subprocess.
        print("Loading Session")
        try:
            import ACAT # pylint: disable=import-outside-toplevel
            ACAT.parse_args(self._params)
            self._session = ACAT.load_session()
        except TypeError:
            print("Load session failed: audio processor booted?")
            self._session = None
        # Kalimba exception caught by text rather than type
        except Exception: # pylint: disable=broad-except
            print("Load session failed: could not connect")
            self._session = None

    def initialize_operators(self, operators):
        """Initialize operators.

        Args:
            operators (list(dict)): List of operator definitions.
        """
        print("Initializing Operators")
        if not operators:
            raise ValueError("No operators to initialize")

        if self._session is None:
            raise NoOperatorsException("Session not initialized")

        self._opmgr = operator.attrgetter("p0.opmgr")(self._session)
        # Get a list of operator entries and corresponding capability IDs
        oplist = self._opmgr.get_oplist('entry')
        if not oplist:
            raise NoOperatorsException("No operators found in the graph.")

        capids = [entry.cap_data.deref.id.value for entry in oplist]
        # Create a handle for each object specified in the graph
        for obj in operators:
            tgtid = obj[OPERATOR_CAP_ID]
            # Find operator IDs that match the required capability ID
            idxs = [idx for idx, capid in enumerate(capids) if capid == tgtid]
            if not idxs:
                raise NoOperatorsException(
                    "No operators found for capability %d" % tgtid)
            try:
                # Select the capability based on the index
                opentry = oplist[idxs[obj[OPERATOR_CAP_IDX]]]
                elfid = self._opmgr.debuginfo.table.get_elf_id_from_address(
                    opentry.cap_data.deref['handler_table'].value)
                cap = Capability(opentry.cap_data.deref, self._opmgr, elfid)
                opr = Operator(opentry, self._opmgr, cap)
                self._operators[obj[OPERATOR_NAME]] = opr
            except IndexError as err:
                raise IndexError("Operator index %d not found for capability "
                                 "%d (%d operators found)" % (
                                     obj[OPERATOR_CAP_IDX]), tgtid, len(idxs)
                                ) from err

    def initialize_handles(self, handles):
        """Initialize handles.

        Args:
            handles (list(dict)): List of handle definitions.
        """
        print("Initializing Handles")
        self._ids = []
        for handle in handles:
            op_name = handle[HANDLE_OPERATOR]

            # If the op_name isn't a real operator and is just a constant
            # then don't dereference to a handle to the operator
            if op_name.lower() == '(constant)':
                opr = op_name
            else:
                if op_name not in self._operators.keys():
                    raise ValueError("Couldn't find operator %s in existing "
                                     "handles (%s)"% (op_name,
                                                      self._operators.keys()))
                opr = self._operators[op_name]
                self._ids.append(opr.id)
            # Store the handle
            self.handles[handle[HANDLE_NAME]] = ACATChipVar(
                opr, handle[HANDLE_ATTR], handle[HANDLE_CONVERSION],
                handle[HANDLE_MISSING], handle[HANDLE_LOG_FMT])

    def reload(self, objects, handles):
        """Reload chip data."""
        if self._session is None:
            self.load_session()

        # Re-check whether loading the session worked.
        if self._session is None:
            return

        try:
            self.initialize_operators(objects)
            self.initialize_handles(handles)
        except TypeError:
            # argument of type 'KalaccessError' is not iterable is thrown
            # if the device is reset/audio processor not started
            print("Reload failed: audio processor booted?")
            self._session = None
            return

    def valid(self):
        """Check for operator validity before reading data."""
        if self._opmgr is None:
            return False

        try:
            oprs = self._opmgr.get_oplist()
        except TypeError:
            # argument of type 'KalaccessError' is not iterable is thrown
            # if the device is reset/audio processor not started
            print("Validity check failed: audio processor booted?")
            self._session = None
            return False

        if not oprs:
            return False

        for opid in self._ids:
            if opid not in oprs:
                return False

        return True

    def read(self, attr):
        """Read an attribute value."""
        return self.handles[attr].read()

    def log_fmt(self, attr):
        """Return the format value for an attribute."""
        return self.handles[attr].log_fmt

    def do_analysis(self):
        """Do the ACAT analysis."""
        # ACAT import has to be within the class to be encapsulated inside
        # the subprocess.
        import ACAT # pylint: disable=import-outside-toplevel
        ACAT.do_analysis(self._session)

MyManager.register('AANC', AANCLauncher, AANCProxy)


class StereoConnection(): # pylint: disable=too-few-public-methods
    """Represent a stereo connection to two AANC devices.

    Args:
        left (AANCProxy): Proxy to the left connection
        right (AANCProxy, optional): Proxy to the right connection. Defaults to
            None.
    """
    def __init__(self, left, right):
        self.left = left
        self.right = right
        self.left_operators = []
        self.right_operators = []
        self.left_attrs = []
        self.right_attrs = []

    def reload(self, operators, handles):
        """Initialize operators and handles.

        Args:
            operators (list(dict)): List of operator definitions.
            handles (list(dict)): List of handle definitions.
        """
        self.left_operators = operators[CONN_LEFT]
        left_names = [oper[OPERATOR_NAME] for oper in self.left_operators]
        left_handles = [handle for handle in handles if
                        handle[HANDLE_OPERATOR] in left_names or
                        handle[HANDLE_OPERATOR].lower() == '(constant)']
        self.left_attrs = [handle[HANDLE_NAME] for handle in left_handles]

        self.right_operators = operators[CONN_RIGHT]
        right_names = [oper[OPERATOR_NAME] for oper in self.right_operators]
        right_handles = [handle for handle in handles if
                         handle[HANDLE_OPERATOR] in right_names]
        self.right_attrs = [handle[HANDLE_NAME] for handle in right_handles]

        if self.left is not None:
            self.left.reload(self.left_operators, left_handles)
        if self.right is not None:
            self.right.reload(self.right_operators, right_handles)

    def read(self, attr):
        """Read a value."""
        if attr in self.left_attrs:
            return self.left.read(attr)
        if attr in self.right_attrs:
            return self.right.read(attr)
        raise ValueError("Unknown attribute: %s" % attr)

    def log_fmt(self, attr):
        """Return the log format for a given attribute."""
        if attr in self.left_attrs:
            return self.left.log_fmt(attr)
        if attr in self.right_attrs:
            return self.right.log_fmt(attr)
        raise ValueError("Unknown attribute: %s" % attr)

    @property
    def valid(self):
        """Return the validity of the session."""
        if self.left is not None and not self.left.valid():
            return False
        if self.right is not None and not self.right.valid():
            return False
        return True

class SingleConnection(): # pylint: disable=too-few-public-methods
    """Represent a single connection to an AANC device.

    The principle here is to expose the same `left` member as a
    `StereoConnection` to allow graphs connecting to a single device to work
    with either mode.

    Args:
        params (list): List of parameters to launch ACAT with.

    """
    def __init__(self, params):
        self.left = AANCLauncher(params)
        self.left_attrs = []

    def reload(self, operators, handles):
        """Initialize operators and handles.

        Args:
            operators (list(dict)): List of operator definitions.
            handles (list(dict)): List of handle definitions.
        """
        self.left.initialize_operators(operators)
        self.left.initialize_handles(handles)
        self.left_attrs = [handle[HANDLE_NAME] for handle in handles]

    def read(self, attr):
        """Read a value."""
        if attr in self.left_attrs:
            return self.left.read(attr)
        raise ValueError("Unknown attribute: %s" % attr)

    def log_fmt(self, attr):
        """Return the log format for a given attribute."""
        if attr in self.left_attrs:
            return self.left.log_fmt(attr)
        raise ValueError("Unknown attribute: %s" % attr)

    @property
    def valid(self):
        """Return the validity of the session."""
        return self.left.valid()
