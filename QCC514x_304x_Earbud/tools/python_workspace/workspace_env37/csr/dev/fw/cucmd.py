############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
CuCmd Firmware Component (Interface and Implementations)

CS-216183-SP : CuCmd spec
CS-208980-DD : Service records description

Uses:-
- Slt           To lookup device's cucmd field addresses.
- AddressSpace   To interact with the devices CuCmd firmware.
"""

import os
import re
from csr.wheels.bitsandbobs import dwords_to_words, \
                                   dwords_to_words_be, \
                                   words_to_dwords, timeout_clock
from csr.dev.hw.memory_pointer import MemoryPointer as Pointer
from csr.dev.model import interface
from csr.dev.fw.slt import SLTNotImplemented
import csr.interface.lib_util as util
from csr.dev.model.base_component import BaseComponent

try:
    long
except NameError:
    long = int


class CuCmdInterface(BaseComponent):
    """\
    CuCmd Interface (Virtual)

    Refs:-
    - CS-216183-SP
    """

    @property
    def version(self):
        """\
        Get CUCMD version.
        """
        raise NotImplementedError()

    def start_service(self, srh0, srh1=None, ies=None, timeout=3):
        """\
        Start a service, passing the service record header (srh).
        2nd srh word and ies words are optional.

        Returns the service_tag so we can stop this service.
        """
        raise NotImplementedError()

    def stop_service(self, service_tag, timeout=5):
        """\
        Stop the service that matches the stag passed
        """
        raise NotImplementedError()

    def set_mib(self, mibkey, value):
        """\
        Sets the MIB ID in mibkey to a value,
        whose properties will be determined by lower layers.
        """
        raise NotImplementedError()

    def set_mibs_from_file(self, fsroot):
        """
        @brief Set up the LPC MIBs for curator using CUCMD commands.

        Get the MIB values from the config file found in the given fsroot.
        Very bad things will go wrong if these don't work.
        """
        mibs = []
        mib_list = {}

        # pylint: disable=invalid-name
        # Get rid of comments
        p1 = re.compile('#.*$')
        # Normalise square braces; they must have adjoining spaces
        p2 = re.compile(r'(\[|\])')
        # Get rid of leading and trailing whitespace
        p3 = re.compile(r'(^\s+|\s+$)')
        # Normalise other spaces, and lose the equals sign
        p4 = re.compile(r'(\s*=\s*|\s+)')

        # Read through the *text* config file and dig out the ones we need.
        config_file_name = os.path.sep.join([fsroot, "subsys0_config1.htf"])
        config_file = open(config_file_name, "r")
        for line in config_file:
            line = p1.sub('', line)
            line = p2.sub(' \\1 ', line)
            line = p3.sub('', line)
            if line == '':
                continue
            # No messing with Watchdogs. They byte.
            if re.search('Watchdog', line):
                continue
            if re.search('=', line):
                line = p4.sub(' ', line)
                mib = line.split(' ')
                if mib[0] == 'file':
                    continue
                # config file has octet strings as hex
                # and integers as decimal
                if mib[1] == '[':
                    mib = [mib[0],
                           map(int, mib[2:-1],
                               [16 for dummy in range(len(mib) - 3)])]
                # Handle case insensitive bools (False, false, fAlSe etc) and
                # convert back to int.
                elif mib[1].lower() in {'false', 'true'}:
                    # eval is safe here because we've verified its values
                    # pylint: disable=eval-used
                    mib = [mib[0], int(eval(mib[1].title()))]
                else:
                    mib = [mib[0], int(mib[1], 0)]
                mib_list[mib[0]] = mib[1]
                mibs.append(mib[0])

        # Finished getting the MIBs, now send them to Curator with CUCMD
        for mib in mibs:
            self.set_mib(mib, mib_list[mib])

class CuCmd(CuCmdInterface): # pylint: disable=abstract-method
    """
    CuCmd Interface (Base)

    CS-216183-SP
    """

    # ------------------------------------------------------------------------
    # Constructors
    # ------------------------------------------------------------------------

    def __init__(self, version):
        self._version = version

    # ------------------------------------------------------------------------
    # Class Public
    # ------------------------------------------------------------------------

    @staticmethod
    def detect_version(slt, data):
        """
        Detect CuCmd protocol version via SLT.
        """
        version_addr = slt["cucmd_version"]
        return data[version_addr]

    @staticmethod
    def create(firmware, data, version=None):
        """
        Create CuCmd interface.

        If version if not specified it will be read via SLT.
        """
        if version is None:
            try:
                version = CuCmd.detect_version(firmware.slt, data)
            except TypeError:
                # Core dumps don't have a slt so create a dummy cucmd
                # object to keep the curator core object happy (but it
                # makes no sense on a core dump).
                version = 1
                slt = None

        if version == 0:
            return CuCmd_0(firmware, data)
        if version == 1:
            return CuCmd_1(firmware, data)
        raise NotImplementedError(
            "CuCmd version %d not supported." % version)

    # ------------------------------------------------------------------------
    # CuCmdInterface compliance
    # ------------------------------------------------------------------------

    @property
    def version(self):
        return self._version

    class TimeoutError(Exception):
        ''' Indicates an AppCmd has timed out
        '''

# To manufacture the CCP_SERVICE_RECORD_HEADER_*

CCP_SERVICE_CLASS_BLUETOOTH = 1
CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_HCI = 0
CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_TEST = 2
CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_INFORMATION = 5
CCP_SERVICE_PROVIDER_ANY = 31

# The full BT HCI Service with patches and config files loaded
CCP_SERVICE_RECORD_HEADER_BT_HCI = (
    (CCP_SERVICE_CLASS_BLUETOOTH << 10) +
    (CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_HCI << 5) +
    (CCP_SERVICE_PROVIDER_ANY))

# The test BT Service without patches nor config files loaded
CCP_SERVICE_RECORD_HEADER_BT = (
    (CCP_SERVICE_CLASS_BLUETOOTH << 10) +
    (CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_TEST << 5) +
    (CCP_SERVICE_PROVIDER_ANY))

CCP_SERVICE_RECORD_HEADER_BT_INFO = (
    (CCP_SERVICE_CLASS_BLUETOOTH << 10) +
    (CCP_SERVICE_CLASS_BLUETOOTH_INDEX_BLUETOOTH_INFORMATION << 5) +
    (CCP_SERVICE_PROVIDER_ANY))

CCP_SERVICE_CLASS_MISC = 0
CCP_SERVICE_CLASS_APPS_INDEX_APPS_STANDBY = 0
CCP_SERVICE_PROVIDER_APPS = 4 # apps

# The Apps Standby Service
CCP_SERVICE_RECORD_HEADER_APPS_STANDBY = (
    (CCP_SERVICE_CLASS_MISC << 10) +
    (CCP_SERVICE_CLASS_APPS_INDEX_APPS_STANDBY << 5) +
    (CCP_SERVICE_PROVIDER_APPS))

class CuCmdBaseError(RuntimeError):
    """
    A known CuCmd response code indicates an error occurred.
    self._response contains the response code
    (not the string, which is in the message and goes into self.args[0]).
    """
    def __init__(self, message, response):
        super(CuCmdBaseError, self).__init__(message)
        self.response = response

class CuCmdError(CuCmdBaseError):
    """
    A known CuCmd response code indicates an error occurred.
    self._response contains the response code translated to a string.
    """

class CuCmdUnknownError(CuCmdBaseError):
    """
    An unknown CuCmd response code indicates an unknown error occurred.
    self._response contains the response code,
    (not a string, because it is unknown).
    """

class CuCmdUnavailable(RuntimeError):
    """
    CuCmd interface not supported because no access to a valid SLT.
    """

class CuCmdCommon(CuCmd):
    """
    CuCmd Interface common to protocol versions 0 and 1.

    CS-216183-SP, Issue 3
    """

    # pylint: disable=too-many-instance-attributes
    # ------------------------------------------------------------------------
    # Constructors
    # ------------------------------------------------------------------------

    def __init__(self, firmware, data_space):
        """
        Construct CuCmd 0 interface.
        """
        CuCmd.__init__(self, 0)
        self.fw = firmware
        self._is_initialised = False
        self._data_space = data_space

    def _initialise_from_slt(self):
        ''' Read vital CuCmd field pointers derived from the device's SLT.
        We avoid doing that at instantiation of the class so we don't access
        the chip unnecessarily. That allows us to connect to a chip that has
        an invalid slt.
        '''
        if not self.fw.slt or isinstance(self.fw.slt, SLTNotImplemented):
            # We could get the addresses from the symbols but there isn't
            # much point as we can't use them for anything in a coredump.
            self._is_initialised = False
            raise CuCmdUnavailable(
                "Failed to read CuCmd field pointers as "
                "there is no access to the SLT.\n"
                "Curator may be attempting to run from a "
                "blank SQIF. Consider programming the SQIF "
                "or running Curator from ROM.")

        self.reread_slt(self.fw.slt, self._data_space)
        self._is_initialised = True

    @staticmethod
    def _get_ptr_from_slt(slt, field_name, data_space):
        """
        Derive CuCmd field pointer from field's slt entry.
        """
        # N.B. For some reason there is a double indirection in the cucmd
        # field address entries - this seems a pointless waste of space.
        # Maybe protocol version 1 should be used to get rid of this
        # indirection?
        #
        field_addr_ptr = Pointer(data_space, slt[field_name])
        field_addr = field_addr_ptr[0]
        field_ptr = Pointer(data_space, field_addr)
        return field_ptr


    # ------------------------------------------------------------------------
    # CuCmdInterface 're-initialise'
    # ------------------------------------------------------------------------
    def reread_slt(self, slt, data_space):
        """Cache vital CuCmd field pointers derived from the device's SLT."""

        self._command = self._get_ptr_from_slt(slt, "cucmd_cmd", data_space)
        self._response = self._get_ptr_from_slt(slt, "cucmd_rsp", data_space)
        self._parameters = self._get_ptr_from_slt(
            slt, "cucmd_parameters", data_space)
        self._results = self._get_ptr_from_slt(slt, "cucmd_results", data_space)

    def _on_reset(self):
        '''
        Resetting can change the version of firmware running on the Curator.

        If the Curator had been restarted in ROM, then the reset could cause it
        to restart from SQIF with a different SLT. Therefore re-read the SLT so
        that CuCMDs will continue to work.

        Ignore any failures, as if the SQIF is erased then CuCMDs won't be able
        to be used anyway.
        '''

        try:
            #Reread the pointers to the CuCMD arrays.
            self._initialise_from_slt()
        except Exception: # pylint: disable=broad-except
            #Could be SQIF is erased.
            pass
    # ------------------------------------------------------------------------
    # CuCmdInterface Compliance
    # ------------------------------------------------------------------------

    def start_service_(self, srh0, srh1=None, ies=None):
        """
        A generator which calls start_service, blocking until completion
        yielding interface.Code objects to report on progress and result.
        """
        yield interface.Code("Starting service")
        result, service_tag = self.start_service(srh0, srh1, ies)
        yield interface.Code("Start result %s, Service tag 0x%x" %
                             (result, service_tag))

    # CUCMD_START_SERVICE = 1
    def start_service(self, srh0, srh1=None, ies=None, timeout=3):
        """
        Instruct the device to start a service defined in a service record.

        Returns a tuple (length, service_tag)
            length - The total length of the service start response message,
                     including the fixed two-word header and any additional
                     information elements (IEs) (there are commonly no IEs,
                     meaning length=2). Any IE words in the response can be
                     retrieved by calling cucmd._read_result(n) for
                     n = 2,..,len
            service_tag - The tag associate with this service if
                          successfully started. Use this value to stop the
                          service using stop_service.

        :param shr0: The first word of the service record header
        :param shr1: The optional sectond word of the service record header
        :param ies: Any CCP IEs to send with the service start request
        :param timeout: The maximum time to wait for the response
        :return: A response tuple (length, service_tag) if the service starts
                 successfully
        :raises TimeoutError: If there is no response within timeout seconds
        :raises RuntimeError: If the requested service cannot be started
        """
        self.send_start_service_cmd(srh0, srh1, ies)
        return self.get_start_service_result(timeout)

    def send_start_service_cmd(self, srh0, srh1=None, ies=None):
        """Sends request to start service and returns immediately"""
        # Concat all the actual params.
        params = [srh0]
        # If ies but no srh1 then set srh1 to 0
        if ies:
            if srh1 is None:
                srh1 = 0
            params += [srh1] + ies
        else:
            if srh1 is not None:
                params += [srh1]
        # Compute total length (including the length param!) and prepend it.
        length = len(params) + 1
        params = [length] + params
        self._write_params(params)
        self.send_cmd_no_wait(self._cmd_code.START_SERVICE)

    def get_start_service_result(self, timeout=None):
        """Blocks until the cucmd completes, subject to timeout occurring"""
        self.wait_for_cmd_completion(timeout)
        result = self._read_result(0)
        service_tag = self._read_result(1)
        return result, service_tag

    # CUCMD_START_SERVICE = 1 with BT test service as the default.
    def start_bt_service(self):
        """Start the BT test service. This is the most commonly started service,
        and is used in the CUCMD service testing. Synonym start_bt_test_service.
        Note no patches nor config files are loaded, for which use
        start_bt_hci_service.

        See start_service for details of return tuple.

        :return: A response tuple (length, service_tag) if the service starts
                 successfully
        :raises TimeoutError: If there is no response within timeout seconds
        :raises RuntimeError: If the requested service cannot be started
        """
        return self.start_service(CCP_SERVICE_RECORD_HEADER_BT)

    # For naming clarity, the following is the preferred name to use
    start_bt_test_service = start_bt_service

    def start_bt_hci_service(self):
        """Start the BT HCI service: this service will load patches and config
        files, unlike start_bt_service which is just the test service.

        See start_service for details of return tuple.

        :return: A response tuple (length, service_tag) if the service starts
                 successfully
        :raises TimeoutError: If there is no response within timeout seconds
        :raises RuntimeError: If the requested service cannot be started
        """
        return self.start_service(CCP_SERVICE_RECORD_HEADER_BT_HCI)

    def start_bt_info_service(self):
        """Start the BT info service. This is not useful because we can't
        do anything with it, except for testing service start interleaving.

        See start_service for details of return tuple.

        :return: A response tuple (length, service_tag) if the service starts
                 successfully
        :raises TimeoutError: If there is no response within timeout seconds
        :raises RuntimeError: If the requested service cannot be started
        """
        return self.start_service(CCP_SERVICE_RECORD_HEADER_BT_INFO)

    def start_apps_standby_service(self):
        """Start the Apps Standby service. Used for testing.

        See start_service for details of return tuple.

        :return: A response tuple (length, service_tag) if the service starts
                 successfully
        :raises TimeoutError: If there is no response within timeout seconds
        :raises RuntimeError: If the requested service cannot be started
        """
        return self.start_service(CCP_SERVICE_RECORD_HEADER_APPS_STANDBY)

    # CUCMD_STOP_SERVICE = 2
    def stop_service(self, service_tag, timeout=5):
        """
        Attempt to stop a service.

        :param service_tag: The tag of the service to attempt to stop. This is
        the second value returned by service_start
        :param timeout: The maximum time to wait for the response

        :return: A dummy value (This remains to keep compatibility)

        :raises TimeoutError: There is no response within timeout seconds.
        :raises RuntimeError: The service cannot be stopped.
        """
        self._write_param(service_tag)
        self._send_cmd(self._cmd_code.STOP_SERVICE)
        self.wait_for_cmd_completion(timeout=timeout)
        result = self._read_result(0)
        return result

    # CUCMD_SET_MIB_KEY = 5
    # CUCMD_SET_MIB_OCTET_KEY = 6
    def set_mib(self, mibkey, value):
        '''
        Issues the appropriate MIB set command based on the type of the
        argument. If the argument evaluates to false, sends an empty array
        using the 'octet' type.
        '''
        if isinstance(mibkey, str):
            mibkey = self.fw.mib.container_nametopsid[mibkey]
        if isinstance(value, (int, long)):
            self._set_mib_integer_key(mibkey, util.decimal_to_vlint(value))
        elif not value:
            # Null setting clears the key
            self._set_mib_octet_key(mibkey, [])
        else:
            self._set_mib_octet_key(mibkey, value)

    # CUCMD_FORCE_DEEP_SLEEP = 8
    def force_deep_sleep(self):
        """Send a FORCE_DEEP_SLEEP"""
        self._send_cmd(self._cmd_code.FORCE_DEEP_SLEEP)

    # CUCMD_FORCE_DEEP_SLEEP_CONT = 9
    def force_deep_sleep_cont(self):
        """Send a FORCE_DEEP_SLEEP_CONT"""
        # No response is expected from this CUCMD
        self.send_cmd_no_wait(self._cmd_code.FORCE_DEEP_SLEEP_CONT)

    # CUCMD_CURATOR_STATE_REQ = 12
    def state_req(self):
        """Send a CURATOR_STATE_REQ"""
        self._send_cmd(self._cmd_code.CURATOR_STATE_REQ)
        return self._read_result(0)

    # CUCMD_MEASUREMENT_REQ = 13
    def measurement_req(self, type): # pylint: disable=redefined-builtin
        """Send a MEASUREMENT_REQ"""
        self._write_param(type)
        self._send_cmd(self._cmd_code.MEASUREMENT_REQ)
        return [self._read_result(0), self._read_result(1)]

    # CUCMD_TUNNEL_TOOLCMD_REQ = 14
    def tunnel_toolcmd_req(self, toolcmdid, payload, timeout=None):
        """Send a TUNNEL_TOOLCMD_REQ"""
        self._write_params([len(payload)+1, toolcmdid] + payload)
        self._send_cmd(self._cmd_code.TUNNEL_TOOLCMD_REQ, timeout)

        #Length of the result is encoded within, first read just the length
        #The length doesn't include itself, and doesn't want to be returned.
        reslen = self._read_result(0)+1
        return self._read_results(reslen)[1:]

    # CUCMD_GET_MIB_KEY = 18
    # CUCMD_GET_MIB_OCTET_STRING_KEY = 19
    def get_mib(self, mibkey):
        """
        A method to get the value of a Curator MIB key. This can be used
        for unisgned integers (8, 16 or 32 bits), signed integers and octet
        strings. "mibkey" can be the MIB name or the MIB id.
        """
        mibkey_id = 0
        mibkey_string = ""
        # We need the MIB keys id (for sending to Curator) and name (so we
        # can use the MIB dictionary
        if isinstance(mibkey, str):
            mibkey_id = self.fw.mib.container_nametopsid[mibkey]
            mibkey_string = mibkey
        elif isinstance(mibkey, int):
            mibkey_id = mibkey
            mibkey_string = self.fw.mib.container_psidtoname[mibkey]

        # Differentiate between integer MIBs and octet string MIBs, there is
        # a seperate CUCMD for each
        if self.fw.env.build_info.mibdb.mib_dict[mibkey_string].is_integer():
            mib_value = self._get_mib_integer(mibkey_id)
            # If the mib value is negative we must convert using 2's complement
            mib_type_string = self.fw.env.build_info.mibdb.mib_dict[
                mibkey_string].type_string()
            if re.match("int", mib_type_string) and (mib_value & 1 << 31):
                mib_value = mib_value - (1 << 32)
            return mib_value
        return self._get_mib_octet_string(mibkey_id)

    # ------------------------------------------------------------------------
    # Command codes
    # ------------------------------------------------------------------------

    class _cmd_code: # pylint: disable=too-few-public-methods, invalid-name
        START_SERVICE = 1
        STOP_SERVICE = 2
        SEND_SERVICE_AUX = 3
        GET_SERVICE_AUX = 4
        SET_MIB_KEY = 5
        SET_MIB_OCTET_KEY = 6
        START_HOSTIO_TEST = 7
        FORCE_DEEP_SLEEP = 8
        FORCE_DEEP_SLEEP_CONT = 9
        RESOURCE_REQ = 10
        RESOURCE_RELEASE_REQ = 11
        CURATOR_STATE_REQ = 12
        MEASUREMENT_REQ = 13
        TUNNEL_TOOLCMD_REQ = 14
        HOSTIO_DEBUGGER_INT = 15
        HOSTIO_DEBUGGER_START = 16
        CALL_FUNCTION = 17
        GET_MIB_KEY = 18
        GET_MIB_OCTET_STRING_KEY = 19

    _response_code = {
        0x4000: "SUCCESS",
        0x4001: "INVALID_PARAMETERS",
        0x4002: "INVALID_STATE",
        0x4003: "UNKNOWN_COMMAND",
        0x4004: "UNIMPLEMENTED",
        0x4100: "INVALID_PARAMETER_MASK",
        0x4FFF: "UNSPECIFIED"
    }

    # ------------------------------------------------------------------------
    # Command Protocol
    # ------------------------------------------------------------------------
    def _set_mib_integer_key(self, mib_id, mib_value):
        """Derived classes provide a means of setting a mib key to a value"""
        raise NotImplementedError()

    def _send_cmd(self, cmd_code, timeout=None, blocking=True):
        """
        Sends the given cucmd setting the toggle bit appropriately and
        waiting for the result

        CS-216183-SP, Issue 3, pp 5.2.2
        """
        self.send_cmd_no_wait(cmd_code)
        if blocking:
            self.wait_for_cmd_completion(timeout)

    def send_cmd_no_wait(self, cmd_code):
        """
        Sends the given cucmd setting the toggle bit appropriately but
        doesn't wait for the result. A subsequent call to
        wait_for_cmd_completion() should be made afterwards.

        CS-216183-SP, Issue 3, pp 5.2.2
        """

        self.start_response = self._read_response()
        if (self.start_response >> 15) == 0:
            cmd_code += 0x40

        self._write_cmd(cmd_code)

        self.cmd_start_time = timeout_clock()

    def wait_for_cmd_completion(self, timeout):
        """
        Second half of sending a command. This poll-waits for completion
        with an optional timeout.
        """
        while self._read_response() & 0x8000 == self.start_response & 0x8000:
            if timeout and timeout_clock() - self.cmd_start_time > timeout:
                raise self.TimeoutError("No CuCmd response within %d seconds"
                                        % timeout)

        response = self._read_response() & 0x7fff
        if response != 0x4000:
            if response in self._response_code.keys():
                raise CuCmdError(
                    "CuCmd Response:%s" % self._response_code[response],
                    self._response_code[response])
            raise CuCmdUnknownError(
                "Unknown CuCmd Response:0x%04X" % response,
                response)

    def _set_mib_octet_key_offset(self, mib_id, octet_offset, octet):
        self._write_params([mib_id, octet_offset, octet])
        self._send_cmd(self._cmd_code.SET_MIB_OCTET_KEY, timeout=2)

    def _set_mib_octet_key(self, mib_id, mib_value):
        """
        Setting the mib octets from highest index to lowest avoids
        the firmware re-allocating memory each time the mib length
        increases.
        """
        for offset in range(len(mib_value) - 1, -1, -1):
            self._set_mib_octet_key_offset(mib_id, offset, mib_value[offset])

    def _get_mib_integer(self, mibkey):
        """
        Get an integer MIB key, this can be signed or unsigned. If the value is
        signed Python will still read it as unsigned so you will need to convert
        it to signed. This supports 8, 16 and 32 bit values.
        """
        self._write_params([mibkey])
        self._send_cmd(self._cmd_code.GET_MIB_KEY, timeout=2)
        return self._read_result(0) | self._read_result(1) << 16

    def _get_mib_octet_string(self, mibkey):
        """
        Get a mib octet string, a list of values will be returned. This method
        uses the octet string length, which is the first word in the CUCMD
        response, to construct a list of the correct size.
        """
        value = []
        self._write_params([mibkey])
        self._send_cmd(self._cmd_code.GET_MIB_OCTET_STRING_KEY, timeout=2)
        # Octet string length is the first word
        length = self._read_result(0)
        # If the length is odd we must still read the last word
        if length % 2:
            length = length // 2 + 1
        else:
            length = length // 2
        # Contruct the list to be returned
        for i in range(length):
            value.append(self._read_result(i+1) >> 8)
            if (self._read_result(0) % 2) and (i == length - 1):
                continue
            value.append(self._read_result(i+1) & 0xff)
        return value

    # ------------------------------------------------------------------------
    # IO
    # ------------------------------------------------------------------------

    def _write_param(self, param, offset=0):
        """
        Write a single parameter word to device buffer.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        self._parameters[offset] = param

    def _write_params(self, params, offset=0):
        """
        Write multiple parameter words to device buffer.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        self._parameters[offset:offset+len(params)] = params


    def _write_cmd(self, cmd):
        """
        Write command code to device.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        self._command[0] = cmd

    def _read_response(self):
        """
        Read response code from device.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        return self._response[0]

    def _read_result(self, offset=0):
        """
        Read a single result word from device buffer.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        return self._results[offset]

    def _read_results(self, how_many):
        """
        Read multiple result words from device buffer.
        """
        if not self._is_initialised:
            self._initialise_from_slt()
        return self._results[0:how_many]

class CuCmd_0(CuCmdCommon): # pylint: disable=invalid-name
    """
    Implements version 0 of the protocol.
    CUCMD v0 expects MIB integer keys to be placed in the parameter area with
    the uint8 array comprising the VLINT placed into the words in LSO-first
    order, one octet per word.
    """
    def _set_mib_integer_key(self, mib_id, mib_value):
        self._write_params([mib_id, len(mib_value)] + mib_value)
        self._send_cmd(self._cmd_code.SET_MIB_KEY, timeout=2)

class CuCmd_1(CuCmdCommon): # pylint: disable=invalid-name
    """
    Implements version 1 of the protocol.
    CUCMD v1 expects MIB integer keys to be placed in the parameter area with
    the uint8 array comprising the VLINT packed into the words in LSO-first
    order.
    """
    def _set_mib_integer_key(self, mib_id, mib_value):
        mib_value.append(0)
        mib_value.reverse()
        packed_len = len(mib_value) // 2
        packed_mib_value = []
        for _ in range(packed_len):
            packed_mib_value.append(mib_value.pop() + (mib_value.pop()<<8))
        self._write_params([mib_id, packed_len] + packed_mib_value)
        self._send_cmd(self._cmd_code.SET_MIB_KEY, timeout=2)

    def call_total_arg_size(self, total_arg_size):
        """
        We don't know the total available scratch space so there's not much we can
        do with this information
        """
        pass

    def call_request_scratch_space(self, size):
        """
        We don't know the total available scratch space so we just tell the caller 
        they can't have any.
        """
        return None

    def call_function(self, func_name, args=None, timeout=1, blocking=True):
        """
        Issues CUCMD_CALL_FUNCTION for the given function and arguments,
        checking that the number of args is correct and that none of the
        function's parameters are more than 32 bits wide, as the underlying
        assembly code doesn't know how to set function calls like that up.
        Also, breaks down 32-bit arguments into 16-bit words in such a way that
        the assembly code doesn't need to know if they were 32-bit args or pairs
        of 16-bit args.
        """

        # pylint: disable=too-many-locals
        if args is None:
            args_ = []
        else:
            args_ = args

        fw_env = self.fw.env
        func_raw_addr = fw_env.functions[func_name]
        _, _, func_sym = fw_env.functions.get_function_of_pc(func_raw_addr)
        # Getting the address this way ensure that the appropriate high and low
        # bit mungery is performed (not that any is required for the XAP)
        func_addr = fw_env.functions.get_call_address(func_name)

        params = [p for p in func_sym.params]
        if len(params) != len(args_):
            raise TypeError("Can't call '%s': %d args supplied but takes %d" %
                            (func_name, len(args_), len(params)))
        args16 = []
        for arg, (name, param) in zip(args_, params):
            if param.byte_size > 4:
                raise TypeError("Can't call '%s': parameter %s too wide (%d "
                                "bytes)" % (func_name, name,
                                            param.struct_dict["byte_size"]))
            if param.byte_size == 4:
                # Have to break the parameter down into 16-bit words.  It seems
                # that if the first argument is 32 bits it has to be unpacked
                # little-endianly, but all subsequent 32 bit args have to be
                # unpacked big-endianly.  Don't ask me why.
                if not args16:
                    args16 += dwords_to_words([arg])
                else:
                    args16 += dwords_to_words_be([arg])
            else:
                args16.append(arg)

        # How much of the 32 bits of return value do we care about?
        ret_type = func_sym.return_type
        return_size = ret_type.struct_dict["byte_size"]//2 if ret_type is not None else None

        return self.call_to_address(func_addr, args16=args16, return_size=return_size,
                                    timeout=timeout,
                                    blocking=blocking)

    def call_to_address(self, func_addr, args16=None, return_size=None,
                        timeout=1, blocking=True):
        """
        Invoke a BSR to the given address having set the given arguments in 
        registers/stack as specified by the XAP calling convention.  If return_size
        is 1 or 2 (words), retrieve return value and (if 2 words) rebuild as a 
        uint32. 
        """
        self._write_params(
            [func_addr & 0xffff, func_addr >> 16, len(args16)] + args16)
        # All good; let's go
        self._send_cmd(self._cmd_code.CALL_FUNCTION, timeout=timeout,
                       blocking=blocking)

        if return_size == 2:
            return words_to_dwords(self._read_results(2))[0]
        if return_size == 1:
            return self._read_results(1)[0]
        return None
