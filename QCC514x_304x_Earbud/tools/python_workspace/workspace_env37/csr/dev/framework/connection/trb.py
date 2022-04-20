############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
# pylint: disable=invalid-name, no-self-use, redefined-builtin, import-error

import os, re, string, subprocess, time, sys, platform
from csr.wheels.global_streams import iprint, wprint
from csr.dev.hw.address_space import ReadRequest, WriteRequest, AddressSpace
from csr.dev.hw.port_connection import MasterPort, AccessPath, PortConnection,\
ReadFailure,                     WriteFailure, \
ReadFailureSubsystemInaccessible, WriteFailureSubsystemInaccessible,\
ReadFailureDUTInaccessible,       WriteFailureDUTInaccessible,\
ReadFailureLinkInvalid,          WriteFailureLinkInvalid
from csr.dev.hw.chip_version import ChipVersion
from csr.wheels.bitsandbobs import TypeCheck, PureVirtualError, timeout_clock
from csr.dev.hw.debug_bus_mux import TrbAccessRequest
from csr.dev.hw.address_space import AccessAlignmentError
from csr.wheels.bitsandbobs import bytes_to_dwords_be, bytes_to_words_be
from csr.dev.hw.chip.mixins.has_reset_transaction import HasResetTransaction
from csr.wheels.bitsandbobs import dwords_to_bytes_be, words_to_bytes_be
from csr.transport.trbtrans import Trb, TrbErrorLinkReset,\
TrbErrorTBusAccessFailed, TrbErrorBridgeLinkIsDown, TrbErrorInvalidStream

# On Windows load trbtrans.dll from the architecture specific directory.
if platform.system() == "Windows":
    _LIB_PATH = "win%d" % (32 if sys.maxsize == 2**31-1 else 64)
    _OVERRIDE_LIB_PATH = os.path.join(_LIB_PATH, "trbtrans.dll")
else:
    _OVERRIDE_LIB_PATH = None

def map_exception(exc, read_not_write):
    return {
        TrbErrorTBusAccessFailed : ReadFailureSubsystemInaccessible if read_not_write 
                                    else WriteFailureSubsystemInaccessible,
        TrbErrorBridgeLinkIsDown : ReadFailureDUTInaccessible if read_not_write 
                                    else WriteFailureDUTInaccessible,
        TrbErrorInvalidStream : ReadFailureLinkInvalid if read_not_write
                                else WriteFailureLinkInvalid
                }.get(exc.__class__, ReadFailure if read_not_write else WriteFailure)


class PydbgTrb(Trb):
    '''
    A Wrapper for the Trb class.

    Within pydbg this class should be used instead of the Trb class. It has pydbg specific
    extensions, such as retrying after a reset and loading architecture specific dynamic libraries.
    '''

    def __init__(self):
        Trb.__init__(self, override_lib_path=_OVERRIDE_LIB_PATH)
        
    def clone(self):
        """
        Create a new PydbgTrb instance identical to the existing one in terms of
        the dongle it is connected to (if any).  From Pydbg's point of view 
        they are interchangeable; the point of doing this is to get thread safety. 
        """
        new_trb = PydbgTrb()
        try:
            dongle = self.get_dongle_details()
        except TrbErrorInvalidStream:
            # the cloned instance hasn't been opened yet, so don't open this one
            pass
        else:
            new_trb.open(dongle.driver, dongle.id)
            
        return new_trb 

    def sequence(self, actions):
        '''
        Override Trb's sequence method to handle the exception that is
        thrown on the first access after a reset.
        '''
        try:
            Trb.sequence(self, actions)
        except TrbErrorLinkReset:
            iprint("Trb link reset: retrying access")
            Trb.sequence(self, actions)

def _max_aligned_width(range, length):
    """
    Get the highest access width that is consistent with the address alignment
    of the slice
    """
    if range.start % 4 == 0 and length % 4 == 0:
        return 4
    if range.start % 2 == 0 and length % 2 == 0:
        return 2
    return 1

def _reverse(data, width):
    if width == 1:
        return

    for i in range(len(data) // width):
        data[i*width:(i+1)*width] = reversed(data[i*width:(i+1)*width])

# Taken from CS-129670-SP, 3.8
TBUS_STATUS_CODES = {0 : "No error",
                     1 : "Subsystem power off",
                     2 : "Subsystem asleep",
                     3 : "Routing error",
                     11: "Access protection",
                     12: "No memory here",
                     13: "Wrong length",
                     14: "Not writable",
                     15: "Bad alignment"}


def trb_aligned_read_helper(subsys, rq, reader, reverse=False, max_trans=None,
                            verbose=False, dongle_id=None):
    """
    Helper function that handles the generic logic associated with converting
    a potentially unaligned access request to the right alignment before
    submitting it to a suitable trb-like transport interface.
    
    :param subsys bus ID of target subsystem
    :param rq The original request
    :param reader Callable to be invoked with the modified request.  Should have
    the same signature and semantics as trbtrans.read
    :param reverse Flag indicating the bytes returned by the reader call need 
    to be reversed (in a per-transaction-width sense)
    :param max_trans Largest number of transactions that can be submitted in one
    call to reader.  None => no limit
    :param verbose Flag indicating whether to comment on what's being submitted
    :param dongle_id Identifier for the dongle, for use in the verbose output:
    must be convertible to string
    """
    aligned_rq = rq.aligned_read
    range = aligned_rq.region
    block = rq.block_id
    access_width = aligned_rq.access_width

    if verbose:
        range = rq.region
        try:
            if dongle_id is not None:
                iprint("TRB read request for [0x%08x,0x%08x)[0x%x] arrived with "
                   "access_width %d on dongle %s" \
                   % (range.start, range.stop, block, rq.access_width, str(dongle_id)))
            else:
                iprint("TRB read request for [0x%08x,0x%08x)[0x%x] arrived with "
                   "access_width %d" \
                   % (range.start, range.stop, block, rq.access_width))
        except AccessAlignmentError:
            iprint("TRB read request for [0x%08x,0x%08x)[0x%x] arrived with "
                   "unsupportable access width"
                   % (range.start, range.stop, block))

        if not aligned_rq is rq:
            range = aligned_rq.region
            iprint(" -> translating to aligned read [0x%08x,0x%08x) with "
                   "access width %d" % (range.start, range.stop,
                                        aligned_rq.access_width))

    read_start = range.start
    data_start = 0
    if max_trans is not None:
        max_length = max_trans * access_width
        length = min(max_length, aligned_rq.length)
        aligned_rq.data = [0]*aligned_rq.length
        while data_start < rq.length:
            aligned_rq.data[data_start:data_start+length] = \
                reader(subsys, block, read_start, access_width, length)
            data_start += length
            read_start += length
            length = min(max_length, aligned_rq.length-data_start)
    else:
        aligned_rq.data = reader(subsys, block, read_start, access_width, 
                                 aligned_rq.length)

    if reverse:
        _reverse(aligned_rq.data, access_width)

    if not aligned_rq is rq:
        rq.dealign_read_data(aligned_rq)

    if verbose:
        len_to_print = min(len(rq.data),4)
        if len_to_print < len(rq.data):
            iprint("["+",".join([hex(v) for v in rq.data[:len_to_print]]) + "...]")
        else:
            iprint("["+",".join([hex(v) for v in rq.data[:len_to_print]])+"]")


def trb_aligned_write_helper(subsys, rq, reader, writer,
                             reverse=False, max_trans=None,
                             verbose=False, dongle_id=None):
    """
    Helper function that handles the generic logic associated with converting
    a potentially unaligned write request to the right alignment before
    submitting it to a suitable trb-like transport interface.  If the alignment
    needs adjusting, performs a read-modify-write.
    
    :param subsys bus ID of target subsystem
    :param rq The original request
    :param reader Callable to be invoked with the modified request as part of 
    the implicit read-modify-write.  Should have
    the same signature and semantics as trbtrans.read
    :param writer Callable to be invoked with the modified request.  Should have
    the same signature and semantics as trbtrans.write
    :param reverse Flag indicating the bytes returned by the reader call need 
    to be reversed (in a per-transaction-width sense)
    :param max_trans Largest number of transactions that can be submitted in one
    call to reader.  None => no limit
    :param verbose Flag indicating whether to comment on what's being submitted
    :param dongle_id Identifier for the dongle, for use in the verbose output:
    must be convertible to string
    """
    
    range = rq.region
    block = rq.block_id

    if verbose:
        try:
            if dongle_id is not None:
                iprint(""""TRB write request for [0x%08x,0x%08x)[0x%x] access_width %d on device %s:
%s"""  % (range.start, range.stop, rq.block_id, rq.access_width, str(dongle_id), rq.data))
            else:
                iprint(""""TRB write request for [0x%08x,0x%08x)[0x%x] access_width %d:
%s"""  % (range.start, range.stop, rq.block_id, rq.access_width, rq.data))
            len_to_print = min(len(rq.data),4)
            if len_to_print < len(rq.data):
                iprint("["+",".join([hex(v) for v in rq.data[:len_to_print]]) + "...]")
            else:
                iprint("["+",".join([hex(v) for v in rq.data[:len_to_print]])+"]")

            
        except AccessAlignmentError:
            iprint("TRB write request for [0x%08x,0x%08x) arrived with "
                   "unsupportable access width"
                   % (range.start, range.stop))

    # Get the aligned read corresponding to this write request (will return
    # the original (write) request if no alignment issues)
    read_rq = rq.aligned_read
    if read_rq is not rq:
        # If there's any actual need for realignment, read the aligned data
        trb_aligned_read_helper(subsys, read_rq, reader, reverse=reverse,
                                max_trans=max_trans)
        # Adjust what we were planning to write with the extra data 
        rq = rq.realign_write_data(read_rq.data)
        range = rq.region
        
        if verbose:
            iprint(" -> translating to aligned write [0x%08x,0x%08x) with "
                   "access width %d" % (range.start, range.stop,
                                        rq.access_width))

    access_width = rq.access_width

    if reverse:
        # Make a deep copy of rq.data first in case it's a reference to a
        # persistent object.  It would be nice if trbtrans could give us
        # an interface to avoid this extra copy
        data = rq.data[:]
        _reverse(data, access_width)
    else:
        data = rq.data

    write_start = range.start
    data_start = 0
    if max_trans is not None:
        max_length = max_trans * access_width
        length = min(max_length, rq.length)
        while data_start < rq.length:
            writer(subsys, block, write_start, access_width,
                                data[data_start:data_start+length])
            data_start += length
            write_start += length
            length = min(max_length, rq.length-data_start)
    else:
        writer(subsys, block, write_start, access_width, data)



class TrbTransConnection(MasterPort):
    """
    This class represents the device's outermost transaction bridge interface,
    i.e. the same thing that the host drivers connect to.  As an
    AddressMasterPort it is manually connected to  the chip's transaction
    bridge adapter by DeviceFactory. Access requests (which must be
    TrbAccessRequests containing the subsystem bus address) are received from
    the chip and passed to trbtrans for processing.
    """
    @staticmethod    
    def create_connection_from_url(path):
        """
        The path takes the following form:
          [url]/[dongle_type]:[dongle serial num]
        where:
          url is one of
            hostname
            hostname:port
            ip_address
            ip_address:port
          (the default port is 8000)
          dongle_type is one of
            scar, scarlet, sc, usb2trb
            qs, quicksilver
            mur, murphy
          (the default dongle type is quicksilver)
          dongle serial num is the numerical part of the serial number.  This is
          only relevant to Scarlets: Quicksilvers are assumed to be unique to
          a given machine
        """

        url_and_dongle = path.split("/")
        if len(url_and_dongle) == 1:
            url = None
            dongle_spec = path
        elif len(url_and_dongle) == 2:
            url, dongle_spec = url_and_dongle
            if ":" not in url:
                url = "%s:8000" % url
            if not dongle_spec:
                dongle_spec = "qs"
        else:
            raise ValueError("-d option cannot contain more than one '/'")

        dongle_id = dongle_spec.split(":",1)
        if len(dongle_id) == 2:
            dongle,id = dongle_id
        elif len(dongle_id) == 1:
            dongle = dongle_id[0]
            id = None
        else:
            raise ValueError("Too many colons in trbtrans options!")

        if dongle in ("mur", "murphy"):
            debug_connection = MRTSTrbTransConnection(url=url)
        elif dongle in ("scar", "scarlet", "sc", "usb2trb"):
            debug_connection = ScarletTrbTransConnection(id=id, url=url)
        elif dongle in ("qs", "quicksilver", None):
            debug_connection = QSTrbTransConnection(url=url)
        else:
            raise ValueError("Unknown TRB dongle type '%s'" % dongle)

        return debug_connection

    MAX_TRANSACTIONS = 5000
    is_remote = False
    _rb = None

    def __init__(self, url=None):

        MasterPort.__init__(self)

        self._trace = False
        self._verbose = False

        self._connection = None
        self._access_path = None

        self._url = url
        self._trb = None

        if url:
            try:
                from mrts.xmlrpc import eProxy
            except ImportError:
                raise RuntimeError("mrts.xmlrpc is missing!")
            # Ideally TrbtransProxy or something like it would live in mrts.xmlrpc
            # not in pylib.
            from csr.transport.trbtrans_proxy import TrbtransProxy
            if self._rb == None:
                self._rb = eProxy('http://%s' % url)
            # Create a proxy for the remote trbtrans
            self._trb = TrbtransProxy(eProxy(self._rb.getResource('trbtrans')))
            self._open(self.dongle_name, self.dongle_id)
        else: 
            self._trb = PydbgTrb()
            self._open(self.dongle_name, self.dongle_id)

    def _open(self, dongle_name=None, dongle_id=None):
        """
        Helper function for TRB open
        """
        if dongle_name is None:
            # most likely locally running on murphy (qs),no params for the mrts
            self._trb.open()
        elif dongle_id is None:
            # work around xmlrpc's inability to marshal "None" arguments
            self._trb.open(dongle_name)
        else:
            # most likely locally running on scarlet
            self._trb.open(dongle_name, dongle_id)

    def close(self):
        """
        Helper function for TRB close
        """
        self._trb.close()

    def wait_for_boot(self, curator):
            # We think we successfully reset the chip.
            # Wait for it to reboot.
            WAIT_TIME_IN_SECS = 2
            start_time = timeout_clock()
            while True:
                try:
                    curator.core.data[0x8000]
                    break
                except AddressSpace.ReadFailure:
                    if timeout_clock() > start_time + WAIT_TIME_IN_SECS:
                        try:
                            # Try one last time
                            curator.core.data[0x8000]
                            break
                        except AddressSpace.ReadFailure:
                            raise RuntimeError("Chip didn't respond within {} seconds after reset".format(
                                WAIT_TIME_IN_SECS))

    def killRemoteResources(self):
        """
        Close all the remote resources
        """

        from csr.wheels.open_source import timeout
        timeout(self._killRemoteResources, timeout_duration=5)

    def _killRemoteResources(self):
        """
        Internal version of killRemoteResources, which does the work
        """
        if self.is_remote:
            from mrts.xmlrpc import eProxy
            self._rb = eProxy('http://%s' % self._url)
            try:
                eProxy(self._rb.killResource('dutconfig'))
            except AttributeError:
                pass # most likely the resource has been killed already
            try:
                eProxy(self._rb.killResource('trbtrans'))
            except AttributeError:
                pass # most likely the resource has been killed already

    def toggle_verbose(self):
        self._verbose = not self._verbose

    @property
    def dongle_name(self):
        """
        Name of the underlying TRB driver
        """
        raise PureVirtualError

    @property
    def dongle_id(self):
        """
        Use the default dongle by default
        """
        return None

    def reset(self, **kwargs):
        """
        Close and re-open the connection to clean up after poking the DBG_RESET
        register. Derived classes may support use of kwargs.
        """
        self.close()
        self._open(self.dongle_name, self.dongle_id)

    def reset_device(self, curator, override=None, reset_type=None):
        """
        Prototype method for doing a device reset. Transport-specific.
        """
        return NotImplemented

    def reset_by_transaction(self, mrts=False):
        """
        Send a reset transaction. Assumes that the chip is known to be one
        which understands it.
        :param mrts: boolean Defining whether transaction is being attempted on murphy.
             Used to determine if FPGA version is capable of TRB reset.
        """

        required_version = 0x500 if mrts else 0x214
        fpga_version_map = {0x214: "2.14",
                            0x500: "5.00"}
        # First, check that the FPGA version is new enough to support this using get_fpga_version
        try:
            if self._trb.get_fpga_version() < required_version:
                iprint("***********************************************************")
                iprint("WARNING: Your TRB FPGA is too old to support sending")
                iprint("a reset transaction. Upgrade to {} or later.".format(
                    fpga_version_map[required_version]))
                iprint("***********************************************************")
                raise NotImplementedError
        # If get_fpga not available use old register read method.
        except AttributeError:
            if self._trb.read_dongle_register(0, 0) < required_version:
                iprint("***********************************************************")
                iprint("WARNING: Your TRB FPGA is too old to support sending")
                iprint("a reset transaction. Upgrade to {} or later.".format(
                    fpga_version_map[required_version]))
                iprint("***********************************************************")
                raise NotImplementedError

        # OK to send a reset transaction
        self._trb.reset_dut()

    def hard_reset_doc(self, trans_obj_name):
        """
        Report how to perform a hard chip reset with just access to this object.
        
        By default, it's not possible.
        """
        
        return "There's no hard reset method available for your dongle type"

    def set_freq_mhz(self, target_mhz):
        """
        Set the TRB frequency in megahertz. Transport-specific.
        """
        return NotImplemented

    def get_freq_mhz(self):
        """
        Get the TRB frequency in megahertz. Transport-specific.
        """
        return NotImplemented

    def run_link_training(self):
        """
        Train the TRB frequency. Transport-specific.
        """
        return NotImplemented

    # MasterPort compliance

    def execute_outwards(self, access_request):

        '''Pass this access request to trbtrans.  We have to receive it as
        a TrbAccessRequest because it must specify the target subsystem'''

        TypeCheck(access_request, TrbAccessRequest)

        basic_request = access_request.basic_request

        if isinstance(basic_request, ReadRequest):
            self._read(access_request.subsys,
                       basic_request)
        elif isinstance(basic_request, WriteRequest):
            self._write(access_request.subsys,
                        basic_request)

    def _read(self, subsys, rq):
        '''
        Do the requested read from the specified subsystem over trbtrans
        '''
        try:
            trb_aligned_read_helper(subsys, rq, self._trb.read,
                                    reverse=True, max_trans=self.MAX_TRANSACTIONS,
                                    verbose=self._verbose, dongle_id=self.dongle_id)
        except Exception as e:
            decoded = self._error_decode(e)
            raise map_exception(e, True)(decoded)
            
        

    def _write(self, subsys, rq):
        '''
        Do the requested write to the specified subsystem over trbtrans
        '''
        try:
            trb_aligned_write_helper(subsys, rq, self._trb.read, self._trb.write,
                                     reverse=True, max_trans=self.MAX_TRANSACTIONS,
                                     verbose=self._verbose, dongle_id=self.dongle_id)
        except Exception as e:
            decoded = self._error_decode(e)
            raise map_exception(e, False)(decoded)

    def connect(self, trb_port):
        """\
        Logically connect self to the specified spi_space for access routing
        purposes.

        Also builds an extended AccessPath so that reachable AddressSpaces know
        "at a glance" what connections can reach them.
        """
        self._connection = PortConnection(self, trb_port)
        self._access_path = AccessPath(0, self)
        self._access_path.extend(trace=self._trace)

    def disconnect(self):
        """\
        Tear down access path and disconnect from Trb space.
        """
        del self._access_path
        del self._connection

    def xap_read(self, subsys, word_address, block_id=0):
        bytes = self._trb.read(subsys, block_id, word_address*2, 2, 2)
        return bytes[0]<<8 | bytes[1]

    def xap_write(self, subsys, word_address, value, block_id=0):
        bytes = [(value>>8), (value&0xff)]
        self._trb.write(subsys, block_id, word_address*2, 2, bytes)

    def mem_read32(self, subsys, address, block_id=0):
        bytes = self._trb.read(subsys, block_id, address, 4, 4)
        return bytes[0]<<24 | bytes[1]<<16 | bytes[2]<<8 | bytes[3]

    def mem_write32(self, subsys, address, value, block_id=0):
        bytes = [(value>>24)&0xff, (value>>16)&0xff,
                 (value>>8)&0xff, (value&0xff)]
        self._trb.write(subsys, block_id, address, 4, bytes)

    def block_read32(self, subsys, address, locations, block_id=0):
        """
        Reads a block of dwords using a sequence of Debug Read Request 
        transactions. Similar to trbtrans.read().
        """
        return bytes_to_dwords_be(self._trb.read(subsys, block_id, address, 4, 
                                                                4 * locations))
    
    def block_read16(self, subsys, address, locations, block_id=0):
        """
        Reads a block of 16-bit words using a sequence of Debug Read Request 
        transactions. Similar to trbtrans.read().
        """
        return bytes_to_words_be(self._trb.read(subsys, block_id, address, 2, 
                                                                2 * locations))
           
    def xap_block_read(self, subsys, word_address, len_words, block_id=0):
        return self.block_read16(subsys, word_address*2, len_words, block_id)
    
    def unlock_trb(self, debug_txn, key):
        """
        Unlock the T-bridge via debug writing or data writing the 128-bit key 
        to the curator memories with the address 0,2,4,6,...,14.
        The key is 128-bit secondary efuse data. The key needs to be supplied 
        as a list of octets in big-endian pairs corresponding to the words 
        that need to be written to the Curator memory space.
        """
        from csr.transport.trb_raw import TrbRaw
        trb_raw = TrbRaw(self._trb);
        if(debug_txn):
            num = len(key)
            for n in range(num):
                trb_raw.debug_write(0,n*2,2,key[n],True)
        else:
            num = len(key)
            for n in range(num):
                trb_raw.data_write(0,n*2,2,key[n],True)
    
    @property
    def trb(self):
        """
        Access the underlying trbtrans interface
        """
        return self._trb

    def _error_decode(self, error_msg):
        """
        Look for a substring of the form "TBUS status code <n>" in the supplied
        error message and add the meaning of the status code to it
        """
        match = re.search(r'TBUS status code (\d+)', str(error_msg))
        if not match:
            # Nothing to decode
            return error_msg
        return "%s (%s)" % (error_msg,
                            TBUS_STATUS_CODES[int(match.group(1))])

    def get_chip_id(self):
        ''' Returns the Chip ID from a register in the debug dongle '''
        raise PureVirtualError

    def load_ljs(self, file_path):
        """
        Loads an HTOL LJS file.
        See LJS data format specification (CS-227038-SP)
        """
        with open(file_path) as f:
            for line in f:
                to_print = line
                line = line.strip().split("#")[0].split()
                if not line:
                    continue
                transfer_type = line[0]
                line = line[1:]
                if transfer_type == "registerd":
                    ssid = int(line[0], 16)
                    block = int(line[1], 16)
                    size = int(line[2], 10)
                    addr = int("".join(line[3].split("_")), 16)
                    value = int("".join(line[4].split("_")), 16)
                    if size == 2:
                        data = words_to_bytes_be([value])
                    elif size == 4:
                        data = dwords_to_bytes_be([value])
                    else:
                        raise ValueError("Transfer size unsupported: %s" % size)
                    try:
                        self._trb.write(ssid, block, addr, size, data)
                    except:
                        iprint("Transaction failed: %s" % to_print)
                        raise
                elif transfer_type == "wait":
                    # 'wait' delays for X milliseconds but time.sleep() takes a number
                    # of seconds so we need to divide by 1000.
                    timeout = float(line[0])
                    time.sleep(timeout/1000)
                else:
                    raise ValueError("Transfer type unsupported: %s" % transfer_type)
    
class ScarletTrbTransConnection(TrbTransConnection):
    """ Scarlet specific settings """

    # For Scarlet register information see the following documents:
    # http://wiki.europe.root.pri/Scarlet/Registers/0x225
    # CS-202437

    CLK_ADDR = 0x1064 # T_BRIDGE_HTTB_CLK_PRG_DM
    MULTIPLY_BIT_POS = 0
    MULTIPLY_MASK = 0xff << MULTIPLY_BIT_POS
    DIVIDE_BIT_POS = 8
    DIVIDE_MASK = 0xff << DIVIDE_BIT_POS
    SCALING_MASK = MULTIPLY_MASK | DIVIDE_MASK
    ENABLE_TRAINING_BIT_POS = 19
    ENABLE_TRAINING_BIT = 1 << ENABLE_TRAINING_BIT_POS

    CHIP_ID_ADDR = 0x1054 # T_BRIDGE_DEVICE_CHIP_ID
    CHIP_VERSION_MASK = 0xffff
    TRAINING_SUPPORTED_BIT_POS = 16 + 9
    TRAINING_SUPPORTED_BIT = 1 << TRAINING_SUPPORTED_BIT_POS

    LINK_STATUS_ADDR = 0x105C # T_BRIDGE_LINK_STATUS_TX
    LINK_OK_BIT_POS = 15
    LINK_OK_BIT = 1 << LINK_OK_BIT_POS

    IF_MUX_ADDR = 0x4004 # IF_MUX
    IF_MUX_MASK = 0xf0

    class TrbClk:
        """ Access to the transaction bridge clock. """
        # Derated to 70MHz as some setups are unreliable at
        # max speed of 80MHz.
        DEFAULT_HDMI_CLK_MHZ = 70
        DEFAULT_WIDE_RIBBON_CLK_MHZ = 20
        DEFAULT_NARROW_RIBBON_CLK_MHZ = 10
        DEFAULT_UNKNOWN_CABLE_TYPE = 10

        # Reduced clock speed to help ensure stable HDMI TRB connection on Scarlet,
        # as some chips are unstable at the default. Determined experimentally.
        SAFE_HDMI_CLK_MHZ = 40

        def __init__(self, link):
            self.target_freq_env_var_set = False
            self._target_freq = self.DEFAULT_UNKNOWN_CABLE_TYPE
            self._link = link
            self._init_freq()

        def _init_freq(self):
            # Select the link frequency.
            # Don't run link training here as it resets the current connection
            # which is confusing as users are not expecting it.
            if not self._clock_was_trained():
                # If the clock wasn't trained then select based on the cable
                # type.

                def get_clock_env_var(cable, default):
                    freq_mhz = os.getenv("PYDBG_TRB_CLOCK_{}_MHZ".format(cable))
                    if freq_mhz is not None:
                        try:
                            freq_mhz = int(freq_mhz)
                        except ValueError:
                            wprint("WARNING: ignoring setting of PYDBG_TRB_CLOCK_{}_MHZ "
                                   "because '{}' is not an integer".format(cable, freq_mhz))
                        else:
                            return int(freq_mhz), True
                    return default, False

                cable_freq_mhz = {
                    "HDMI"          : ("HDMI", self.DEFAULT_HDMI_CLK_MHZ),
                    "WIDE_RIBBON"   : ("WIDE_RIBBON", self.DEFAULT_WIDE_RIBBON_CLK_MHZ),
                    "NARROW_RIBBON"   : ("NARROW_RIBBON", self.DEFAULT_NARROW_RIBBON_CLK_MHZ)
                }

                link_type = self._link._cable_type()
                if link_type in cable_freq_mhz:
                    cable, default = cable_freq_mhz[link_type]
                    self._target_freq, self.target_freq_env_var_set = get_clock_env_var(cable, default)
                    self.set_freq_mhz(self._target_freq)
                # Otherwise, leave the clock how it is.

        def _clock_was_trained(self):
            val = self._link._trb.read_dongle_register(0, self._link.CLK_ADDR)
            return bool(val & self._link.ENABLE_TRAINING_BIT)

        def _freq_from_scaling(self, divide, multiply):
            BASE_FREQ_MHZ = 40
            return BASE_FREQ_MHZ / divide * multiply

        def set_freq_mhz(self, target_mhz):
            """
            @brief Set clock frequency in T_BRIDGE_HTBB_CLK_PRG_DM.

            @see http://wiki.europe.root.pri/Scarlet/Registers/0x225

            Bit[15: 8] - clock divide value   (default 8)
            Bit[ 7: 0] - clock multiply value (default 2)

            Base frequency is 40MHz.
            0x1002 means 5MHz (40 / 16 * 2) @note 0x0801 is not a valid input.
            0x0802 means 10MHz (40 / 8 * 2)
            0x0102 means 80MHz (40 / 1 * 2)

            @note The output frequency must be in the range of 5 MHz to 120Mhz.

            e.g. for a target_mhz of 10 this function performs:
            trans.trb.write_dongle_register(0, 0x1064, 0x802)
            """
            if not 5 <= target_mhz <= 120:
                raise ValueError("target_mhz must be in the range [5, 120]")

            for divide in range(1, 256):
                # A multiply of 1 is invalid
                for multiply in range(2, 256):
                    freq_mhz = self._freq_from_scaling(divide, multiply)
                    if freq_mhz == target_mhz:
                        write_val = self._link._trb.read_dongle_register(0,
                                                        self._link.CLK_ADDR)
                        write_val &= ~self._link.ENABLE_TRAINING_BIT
                        write_val &= ~self._link.SCALING_MASK
                        write_val |= divide << self._link.DIVIDE_BIT_POS
                        write_val |= multiply << self._link.MULTIPLY_BIT_POS
                        self._link._trb.write_dongle_register(0,
                                            self._link.CLK_ADDR, write_val)
                        return
            raise ValueError("Could not set TRB frequency to %uMHz" %
                             target_mhz)

        def get_freq_mhz(self):
            read_value = self._link._trb.read_dongle_register(0,
                                                    self._link.CLK_ADDR)
            mul = (read_value & self._link.MULTIPLY_MASK) \
                                                >> self._link.MULTIPLY_BIT_POS
            div = (read_value & self._link.DIVIDE_MASK) \
                                                >> self._link.DIVIDE_BIT_POS
            return self._freq_from_scaling(div, mul)

        def _link_training_is_supported(self):
            props = self._link._trb.read_dongle_register(0,
                                                    self._link.CHIP_ID_ADDR)
            return bool(props & self._link.TRAINING_SUPPORTED_BIT)

        def _run_link_training(self):
            if self._link_training_is_supported():
                self._link._trb.write_dongle_register(0, self._link.CLK_ADDR,
                                                self._link.ENABLE_TRAINING_BIT)
                self._link.reset_by_transaction()
                self._link._wait_for_link_reset()
                return self.get_freq_mhz()
            return NotImplemented

        def run_link_training(self):
            trained = self._run_link_training()
            if NotImplemented == trained:
                iprint("***********************************************************")
                iprint("Frequency training is not supported by this device")
                iprint("***********************************************************")
            return trained

    def __init__(self, id=None, url=None):

        # Set dongle id before calling the generic constructor
        self._dongle_id = int(id) if id is not None else id

        TrbTransConnection.__init__(self, url=url)

        # Initialise the transaction bridge clock
        self._clk = self.TrbClk(self)
        
        # Set Clock speed to safe levels for QCC514X_QCC304X using HDMI cable to ensure stability
        chip_version = ChipVersion(self.get_chip_id())
        
        if (chip_version.major == 0x4B and self._cable_type() == "HDMI" and
            not self._clk.target_freq_env_var_set and not chip_version.is_fpga):
            self.set_freq_mhz(self._clk.SAFE_HDMI_CLK_MHZ)

    def _wait_for_link_status(self, ok):
        keep_trying = 1000
        read = self._trb.read_dongle_register
        while keep_trying and \
            bool(read(0, self.LINK_STATUS_ADDR) & self.LINK_OK_BIT) != ok:
            keep_trying = keep_trying - 1

    def _wait_for_link_reset(self):
        # Wait for the link to go down and come back up again
        self._wait_for_link_status(ok = False)
        self._wait_for_link_status(ok = True)

    def _cable_type(self):
        # Bits 4/5/6 of this get set depending on which connector Scarlet
        # last found a valid link on.
        # On Scarlet S13515v2 hardware:
        #   bit 4 seems to be the HDMI connection
        #   bit 5 seems to be the differential (wide) ribbon connection
        #   bit 6 seems to be the single-ended (narrow) ribbon connection
        # See TRB Front Processor Registers:
        # http://wiki.europe.root.pri/Scarlet/Registers/0x225
        value = self._trb.read_dongle_register(0, self.IF_MUX_ADDR)

        cable = {
            0x10 : "HDMI",
            0x20 : "WIDE_RIBBON",
            0x40 : "NARROW_RIBBON",
        }
        return cable.get(value & self.IF_MUX_MASK, "UNKNOWN")

    @property
    def dongle_name(self):
        return "scarlet"

    @property
    def dongle_id(self):
        return self._dongle_id

    def reset_device(self, curator, override=None, reset_type=None):
        """
        Reset the device.
        :param curator: CuratorxxxxSubsystem object
        :param override: string If set to "force_gpio then forces use of GPIO without 
               trying to find another way.
        :param reset_type: string Valid inputs are prefer_pbr, require_pbr, prefer_por, require_por
            'pbr' is Post Boot Reset which using Scarlett is done by sending a trb reset transaction.
            'por' is Power On Reset which on Scarlett is done by toggling the reset pin.
        """
        if reset_type is None:
            reset_type = "prefer_pbr"

        TRB_RESET = 'TRB transaction reset'
        GPIO_RESET = 'GPIO toggle reset'

        reset_map = {"prefer_pbr": [TRB_RESET],
                     "require_pbr": [TRB_RESET],
                     "prefer_por": [GPIO_RESET, TRB_RESET],
                     "require_por": [GPIO_RESET]}

        if reset_type not in reset_map:
            raise ValueError("Valid options for reset_type are [{}]".format(", ".join(reset_map)))

        if override is not None:
            overrides = string.split(override, ":", 1)
            if overrides[0] != self.dongle_name:
                iprint("***********************************************************")
                iprint("Scarlet transport doesn't understand override='%s'" % override)
                iprint("***********************************************************")
                return NotImplemented
            override = overrides[1]

        first_attempt = True
        for reset_method in reset_map[reset_type]:
            try:
                if not first_attempt:
                    iprint("Preferred method failed, attempting {}".format(reset_method))

                if reset_method == GPIO_RESET:
                    with curator.reset_protection():
                        # toggle_reset_pin raises NotImplementedError exception if
                        # gpio reset unavailable
                        self.toggle_reset_pin(override=override)
                        self.wait_for_boot(curator)

                elif reset_method == TRB_RESET:
                    # Some chips do not suport a transaction reset
                    if not isinstance(curator.chip, HasResetTransaction):
                        raise NotImplementedError
                    self.reset_by_transaction()
                    self.wait_for_boot(curator)
                break
            except NotImplementedError:
                pass
            first_attempt = False
        else:
            raise NotImplementedError("No available reset method for "
                                      "current transport and device combination.")

    def determine_toggle_availability(self, override=None):
        """
        Try to determine if reset by wiggling Scarlet GPIO(s), which may or may not be
        connected in parallel with TBridge proper to the chip's reset pin will work.
        Note: this requires the 1.02 Scarlet driver and the 2.07
        FPGA image, and works better with 2.08 or later.
        :param override: string If = "force_gpio" then try to use GPIO toggle regardless
        
        """

        # First, check that the FPGA version is late enough to support this
        if self._trb.read_dongle_register(0, 0) < 0x207:
            iprint("***********************************************************")
            iprint("WARNING: Your Scarlet FPGA is too old to support GPIO device reset")
            iprint("Upgrade to 2.07 or later, e.g. via any Hydra Scarlet driver package")
            iprint("Falling back on old, flakier DBG_RESET method")
            iprint("***********************************************************")
            raise NotImplementedError

        if override == "force_gpio":
            # Use GPIOs regardless, by your command.
            pass
        # else see if we can spot common connectivity problems
        elif self._trb.read_dongle_register(0, 0) < 0x208:
            iprint("***********************************************************")
            iprint("WARNING: Your Scarlet FPGA is too old for Pydbg to determine")
            iprint("which reset method to use. If you had FPGA 2.08 or above")
            iprint("(which you really should by now), Pydbg could be cleverer.")
            iprint("Blindly using Scarlet GPIO method, which might not reset your")
            iprint("device at all! If you have:")
            iprint(" - CSRA68100 r00/r01 silicon on R13568 connected via")
            iprint("   single-ended ribbon cable: should work")
            iprint(" - CSRA68100 r00/r01 silicon on R13568 connected via")
            iprint("   HDMI cable: does not work, your device hasn't been reset")
            iprint(" - Anything else: dunno")
            iprint("If it didn't work, reset by other means. (I'm resetting my")
            iprint("notion of chip state.)")
            iprint("***********************************************************")
        else:
            trb_if_mux = self._cable_type()
            if trb_if_mux == "NARROW_RIBBON":
                # Single-ended connection. We expect the GPIO method to work.
                pass
            elif trb_if_mux == "WIDE_RIBBON":
                # Differential ribbon cable.
                # jn01 tried this five times on a type B lab board (R13610v1)
                # and it Seemed To Work (tm). What more do you want?
                pass
            elif trb_if_mux == "HDMI":
                # HDMI connection. We expect the GPIO method NOT to work.
                iprint("***********************************************************")
                iprint("WARNING: Scarlet is connected via HDMI.")
                iprint("On CSRA68100 R13568 at least, the Scarlet GPIO reset method")
                iprint("doesn't work with this connectivity, so I'm not even going")
                iprint("to try it. I am attempting to reset your device with the")
                iprint("old, flakier DBG_RESET method.")
                iprint("")
                iprint("NOTE: there is a hack whereby you can GPIO-reset a R13568")
                iprint("whose TBridge comes via HDMI: if you connect *both* the HDMI")
                iprint("and the 16-way ribbon cable (which I can't detect), TBridge")
                iprint("data goes via HDMI and the reset signal via the ribbon cable.")
                iprint("If you have done this pass override=\"scarlet:force_gpio\"")
                iprint("to stop me trying to be clever.")
                iprint("***********************************************************")
                raise NotImplementedError
            else:
                iprint("***********************************************************")
                iprint("WARNING: Couldn't determine Scarlet connectivity.")
                iprint("Blindly using Scarlet GPIO reset method, which might not")
                iprint("reset your device at all!")
                iprint("If it didn't work, reset by other means. (I'm resetting my")
                iprint("notion of chip state.)")
                iprint("***********************************************************")

    def toggle_reset_pin(self, override=None):
        """
        Reset device by toggling relevant GPIOs.
        CSRA68100 and QCC512X_QCC302X's reset pin is internally strongly pulled at 3v3 but
        the transaction bridge pads could be at 1v8, therefore we can drive it low
        cannot drive it high. Therefore we just configure our GPIO into an
        input which will let it rise back to 3v3.
        :param override: string If = "force_gpio" then try to use GPIO toggle regardless
        """

        self.determine_toggle_availability(override=override)

        #  TRB is pin 14 on single ended ribbon and pin 16 on HDMI (see
        # http://wiki/Scarlet/Registers)
        RST_PIN_MASK = 1 << 14 | 1 << 16

        # Set GPIO direction to output for TRB_RST
        gpio_dir = self._trb.read_dongle_register(0, 0x4010)
        self._trb.write_dongle_register(0, 0x4010, gpio_dir | RST_PIN_MASK)

        # Set the TRB_RST pin low
        gpio_out = self._trb.read_dongle_register(0, 0x4008)
        self._trb.write_dongle_register(0, 0x4008, gpio_out & ~(RST_PIN_MASK))

        # Pete W says 5ms is a good amount of time to hold reset low.
        time.sleep(0.005)

        # Set GPIO direction to input because that's the only way we can
        # configure the pin to be open drain.
        gpio_dir = self._trb.read_dongle_register(0, 0x4010)
        self._trb.write_dongle_register(0, 0x4010, gpio_dir & ~(RST_PIN_MASK))

    def hard_reset_doc(self, trans_obj_name):
        """
        Report how to perform a hard chip reset with just access to this object.
        
        With a Scarlet you can toggle the chip reset line if you have a ribbon
        cable attached.
        """
        return """Call %s.toggle_reset_pin(override="force_gpio").
NB: on most boards it is necessary to have a ribbon cable connected for this to work""" % trans_obj_name

    def get_chip_id(self):
        ''' Read the register that holds the chip ID '''
        return self._trb.read_dongle_register(0, self.CHIP_ID_ADDR) \
                                                       & self.CHIP_VERSION_MASK

    def set_freq_mhz(self, target_mhz):
        """
        @brief Set the clock frequency for the transaction bridge.

        @param [in] target_mhz  Integer target frequency in megahertz.
                                Must be in the range [5, 120].
        @exception  ValueError  Raises a ValueError if the requested frequency
                                can't be set.
        """
        self._clk.set_freq_mhz(target_mhz)

    def get_freq_mhz(self):
        """
        @brief Get the current clock freqency for the transaction bridge.

        @return The current transaction bridge frequency in megahertz.
        """
        return self._clk.get_freq_mhz()

    def run_link_training(self):
        """
        @brief Automatically train the transaction bridge clock frequency
               to the highest stable value.

        @return The frequency the clock was trained to in megahertz.
        """
        return self._clk.run_link_training()

class QSTrbTransConnection(TrbTransConnection):
    """ Quicksilver specific settings """
    @property
    def dongle_name(self):
        return "quicksilver"

    def get_chip_id(self):
        ''' Read the register that holds the chip ID '''
        return self._trb.read_dongle_register(0, 0x54) & 0xffff

class MRTSTrbTransConnection(QSTrbTransConnection):
    """ Quicksilver on Murphy specific settings
        This class includes support for low-level Murphy functionality, in
        particular dutconfig, which is used for resets while the Murphy TRB
        driver remains unable to handle unresponsive transactions
    """

    def __init__(self, id=None, url=None):
        self._rb = None
        self._url = url
        self._lastrun_up_args = None

        # Reboot the board before opening a Trb session
        if self._url:
            self.is_remote = True
            try:
                from mrts.xmlrpc import eProxy
            except ImportError:
                raise RuntimeError("mrts.xmlrpc is missing!")
            # Reset the board
            self._rb = eProxy('http://%s' % self._url) # Grab a handle to broker
            self._dutconfig = eProxy(self._rb.getResource('dutconfig'))
        else:
            self.is_remote = False

        # Carry on with TRB initialisation
        TrbTransConnection.__init__(self, url=url)

    def _dutconfig_subp_opts(self, modules=None, quick=False):
        """
        Determine options for a dutconfig command either locally or remotely.
        If supplied use the modules named in the dutconfig command.
        """
        try:
            import dutconfig
        except ImportError:
            raise RuntimeError("No dutconfig python module! Contact DevSys.")
        # We want the args/options from the last_run if there was one;
        # but at end of this routine they are preserved for such a situation
        # when doing a quick reset, as it saves redoing the status command.
        if self._lastrun_up_args is not None and quick:
            return self._lastrun_up_args
        dut_lastrun_args = dutconfig.status()['last_run']['arguments']

        subp_opts = []
        if modules:
            _modules = modules
            subp_opts.append('-m')
            subp_opts.append(_modules)
        else:
            _modules = 'default'
            if 'modules' in dut_lastrun_args:
                _modules = dut_lastrun_args['modules']
                subp_opts.append('-m')
                subp_opts.append(_modules)

        _options = []
        if 'options' in dut_lastrun_args:
            for opt in dut_lastrun_args['options']:
                _options.append(opt)
                subp_opts.append('-o')
                subp_opts.append(opt)

        _timeout = "%d" % dutconfig.actions.DEFAULT_HYDRA_TIMEOUT
        if 'timeout' in dut_lastrun_args:
            _t = dut_lastrun_args['timeout']
            _timeout = _timeout if _t is None else  "%s" % _t
            subp_opts.append('--timeout')
            subp_opts.append(_timeout)

        _path = os.path.realpath(os.path.curdir)
        if 'path' in dut_lastrun_args:
            _path = dut_lastrun_args['path']
            subp_opts.append('-p')
            subp_opts.append(_path)

        self._lastrun_up_args = (subp_opts, _modules, _options, _timeout, _path)
        return self._lastrun_up_args

    def dutconfig(self, modules=None, quick=False):
        """
        Execute a dutconfig command either locally or remotely.
        If supplied use the modules named in the dutconfig command.
        """
        subp_opts, _modules, _options, _timeout, _path = (
            self._dutconfig_subp_opts(modules, quick))
        
        actual_start_time = time.time()
        if self.is_remote:
            try:
                # Python3 version of xmlrpc
                import xmlrpc.client
                xmlrpc_fault = xmlrpc.client.fault
            except ImportError:
                # Python2 version of xmlrpc
                import xmlrpclib
                xmlrpc_fault = xmlrpclib.Fault

            try:
                if quick:
                    ret = self._dutconfig.Exec.dut_reset()
                else:
                    ret = self._dutconfig.up(modules = _modules,
                                             options = _options,
                                             timeout = _timeout,
                                             path = _path)
            except xmlrpc_fault:
                self.killRemoteResources()
                raise RuntimeError("dutconfig up failed!")
        else:
            try:
                f = open('dutconfig.log', 'w')
            except IOError:
                f = open('/tmp/dutconfig.log', 'w')
            try:
                cmd = ['sudo', 'dutconfig']
                if quick:
                    cmd += ['exec'] + subp_opts + ['dut_reset']
                else:
                    # In theory we could use the up() method in the dutconfig,
                    # module but we'd need to run python as sudo.
                    cmd += ['up'] + subp_opts
                ret = subprocess.call(
                    cmd, stdout=f, stderr=subprocess.STDOUT,
                    universal_newlines=True)
            finally:
                f.close()
        if ret != 0:
            raise RuntimeError("dutconfig up failed!")
        iprint("Lab board reset actual time in %.2f s" % (time.time() - actual_start_time))
        return ret

    def reset_device(self, curator, reset_type=None, **kwargs):
        """
        Reset the device. If using 'por' uses 'dutconfig up' to reset the DUT
        (if this ever changes, edit the hard_reset_doc method below as needed)        
        but when keyword 'modules' is set,
        switching to using named hydra drivers modules
        e.g. pass modules='sdio' to cause loading off-chip filesystem
        
        :param curator: CuratorxxxxSubsystem object
        :param reset_type: string Valid inputs are prefer_pbr, require_pbr, prefer_por, require_por
            'pbr' is Post Boot Reset which on MRTS is done by sending a trb reset transaction.
            'por' is Power On Reset which on MRTS is done by cycling board power with dutconfig.
            defaulted to prefer_por
        
        """
        if reset_type is None:
            reset_type = "prefer_por"

        TRB_RESET = 'TRB transaction reset'
        DUTCONFIG_RESET = 'dutconfig reset'

        reset_map = {"prefer_pbr": [TRB_RESET, DUTCONFIG_RESET],
                     "require_pbr": [TRB_RESET],
                     "prefer_por": [DUTCONFIG_RESET],
                     "require_por": [DUTCONFIG_RESET]}

        first_attempt = True
        for reset_method in reset_map[reset_type]:
            try:
                if not first_attempt:
                    iprint("Prefered method failed, attempting {}".format(reset_method))
                if reset_method == DUTCONFIG_RESET:
                    self.close()
                    start_time = time.time()
                    self.dutconfig(modules=kwargs.get('modules', None), quick=kwargs.get('quick', False))
                    iprint("Overall Lab board reset in %.2f s" % (time.time() - start_time))
                    self._open(self.dongle_name, self.dongle_id)

                elif reset_method == TRB_RESET:
                    # Some chips do not suport a transaction reset
                    if not isinstance(curator.chip, HasResetTransaction):
                        raise NotImplementedError
                    self.reset_by_transaction(mrts=True)
                self.wait_for_boot(curator)
                break
            except NotImplementedError:
                pass
                first_attempt = False
        else:
            raise NotImplementedError("No available reset method for current transport and device combination.")

    def hard_reset_doc(self, trans_obj_name):
        """
        Report how to perform a hard chip reset with just access to this object.

        On Murphy the standard transport reset prompts a hard chip reset.
        """
        return "Call %s.reset()" % trans_obj_name

    def identify(self):
        """
        Returns the Murphy blade name when we are running multi-device tests in which
        a test controller is connected to multiple slave blades over XMLRCP
        """
        return self._url.split(":")[0]
