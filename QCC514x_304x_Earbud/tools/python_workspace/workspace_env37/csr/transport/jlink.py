############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import ctypes
import platform
import struct
from glob import glob
import os
import atexit
import tempfile
import shutil
from os.path import join

from csr.dev.hw.debug.coresight import CoresightDPData, CoresightMEMAPData,\
    get_coresight_debug_port_version, CoresightRawDriverDPData, CoresightRawDriverMEMAPData, \
    CoresightJTAGDPDriver, CoresightSWDDPDriver, CoresightStickyError, \
    CoresightTransport, CoresightDebugPort, CoresightAPAccessError, CoresightDPAccessError
from csr.dev.hw.debug.jtag import JTAGUtil, JTAG2AHBTransport
from csr.dev.hw.debug.swd import SWD
from csr.wheels import iprint, wprint, pack_unpack_data_le,dprint, build_le, flatten_le, \
    CLang, autolazy

# JLink host interface types defined in the JLink SDK
JLINKARM_HOSTIF_USB = (1 << 0)
JLINKARM_HOSTIF_IP = (1 << 1)


class JLINK_FUNC_INDEX:
  JLINK_IFUNC_CORESIGHT_ACC_APDP_REG_MUL=24

class JLINK_CORESIGHT_APDP_ACC_DESC(ctypes.Structure):
    """
    Descriptor struct used for multi-APDP access operations 
    """
    _fields_ = [("Data", ctypes.c_uint32),
                ("Mask", ctypes.c_uint32),
                ("CompVal", ctypes.c_uint32),
                ("Status", ctypes.c_int),
                ("TimeoutMsReadUntil", ctypes.c_int),
                ("RegIndex", ctypes.c_uint8),
                ("APnDP",    ctypes.c_uint8),
                ("RnW",      ctypes.c_uint8)]

class JLinkError(RuntimeError):
    pass

class JLinkSetupError(JLinkError):
    pass

class JLinkConnectionError(JLinkSetupError):
    """
    Something went wrong making setup calls
    """

class JLinkCoresightAPAccessError(JLinkError, CoresightAPAccessError):
    """
    The AP access failed
    """
class JLinkCoresightDPAccessError(JLinkError, CoresightDPAccessError):
    """
    The DP access failed
    """

class JLinkTrans:
    JTAG = 0
    SWD = 1
    RAW_JTAG = 2

def jlink_exec_command(dll, cmd, error_buf=None, len_error_buf=None):
    if error_buf is None:
        len_error_buf = 256
        error_buf = (ctypes.c_char * len_error_buf)()

    ret = dll.JLINKARM_ExecCommand(cmd.encode("ascii"), error_buf, len_error_buf)
    if error_buf:
        if error_buf[:][0] != 0:
            error_string = b"".join(c for c in error_buf if c).decode("ascii")
            raise JLinkConnectionError("Jlink ExecCommand failed executing "
                                    "'{}': {}".format(cmd, error_string))
    if ret < 0:
        raise JLinkConnectionError("'{}' failed".format(cmd))


class JLinkSWDDriver(object):
    """
    Simple class that sends and receives bits on the SWD data line using the JLINK
    API.
    """
    def __init__(self, dll, verbose=False):
        self._dll = dll
        self._verbose = verbose

    def drive_line(self, line_bits):
        """
        Send/receive the given set of bits on the SWDIO line.

        :param line_bits: SWDXyzBits object, with two attributes:
         - bit_list, a list of the bits to send/0s to mark receive clocks
         - dir_bits, a list of bit-flags indicating send/receive (usually all 1s or 
         all 0s, but this is not required)
        """        
        bit_sequence = line_bits.bit_list
        dir_bit_sequence = line_bits.dir_bits

        # Count the number of trailing to-host direction bits.  Those
        # are the bits (if any) that we want to return from the to-host
        # buffer.
        try:
            num_final_read_bits = dir_bit_sequence[::-1].index(1)
        except ValueError: # all read bits
            num_final_read_bits = len(dir_bit_sequence)
        num_ignore_bits = len(dir_bit_sequence) - num_final_read_bits

        missing = 8*((len(bit_sequence) + 7)//8) - len(bit_sequence)
        padded_bit_sequence = bit_sequence + [0]*missing
        padded_dir_bit_sequence = dir_bit_sequence + [0]*missing
        assert len(padded_bit_sequence) % 8 == 0
        
        byte_sequence = pack_unpack_data_le(padded_bit_sequence, from_width=1, to_width=8)
        dir_byte_sequence = pack_unpack_data_le(padded_dir_bit_sequence, from_width=1, to_width=8)
        
        assert(len(byte_sequence)==len(dir_byte_sequence))
        
        data_buf = (ctypes.c_uint8 * len(byte_sequence))(*byte_sequence)
        dir_buf = (ctypes.c_uint8 * len(dir_byte_sequence))(*dir_byte_sequence)
        
        if self._verbose:
            if all(dir_bit_sequence):
                iprint("JLinkSWDDriver: Sending {} bits: {}".format(len(bit_sequence), bit_sequence))
            elif not any (dir_bit_sequence):
                iprint("JLinkSWDDriver: Receiving {} bits".format(len(dir_bit_sequence)))
            else:
                iprint("JLinkSWDDriver: Sending receiving: {}".format(bit_sequence))
                iprint("                   {}".format(dir_bit_sequence))

        def fmt_buf(buf):
            return "[" + (",".join(hex(v) for v in buf)) + "]"
        if num_final_read_bits == 0:
            self._dll.JLINK_SWD_StoreRaw(dir_buf, data_buf, len(bit_sequence))
            self._dll.JLINK_SWD_SyncBits()
        else:
            inbuf = (ctypes.c_uint8 * len(byte_sequence))()
            self._dll.JLINK_SWD_StoreGetRaw(dir_buf, data_buf, inbuf, len(bit_sequence))
            return pack_unpack_data_le(inbuf, 8, 1)[num_ignore_bits:len(bit_sequence)]



class JLinkCoresightDPData(CoresightDPData):
    """
    Concrete CoresightMEMAP implementation which accesses MEM-AP registers via the
    JLink DLL
    """
    def __init__(self, jlink_dll, verbose=0):
        self._dll = jlink_dll
        self._verbose = verbose
        self._raw_read_buf = ctypes.create_string_buffer(4)
        self._read_buf = ctypes.cast(self._raw_read_buf, ctypes.POINTER(ctypes.c_uint32))
        self._len_error_buf = 256
        self._error_buf = ctypes.create_string_buffer(self._len_error_buf)

    
    def _read_dp_reg(self, reg_index, retry=True):
        """
        Return the UINT32 value of the given register index in this AP.
        """
        if self._verbose > 2:
            iprint("DP read reg_index {}".format(reg_index))
        ret = self._dll.JLINKARM_CORESIGHT_ReadAPDPReg(reg_index, 0, self._read_buf)
        if ret < 0:
            if retry and self._reassert_power():
                # power was off and is now on
                iprint("Reasserted DAP power requests after DP access failure")
                ret = self._dll.JLINKARM_CORESIGHT_ReadAPDPReg(reg_index, 0, self._read_buf)
            if ret < 0:
               raise JLinkCoresightDPAccessError("Error reading register index {} in "
                                                "DP".format(reg_index))
        if ret > 0 and _RAISE_WAITS_AS_ERRORS:
            raise CoresightStickyError("WAIT condition encountered during DP reg read")
        return self._read_buf.contents.value


    def _write_dp_reg(self, reg_index, value, retry=True):
        if self._verbose > 2:
            iprint("DP write reg_index {}, value 0x{:x}".format(reg_index, value))
        ret = self._dll.JLINKARM_CORESIGHT_WriteAPDPReg(reg_index, 0, value)
        if ret < 0:
            if retry and self._reassert_power():
                # power was off and is now on
                iprint("Reasserted DAP power requests after DP access failure")
                ret = self._dll.JLINKARM_CORESIGHT_WriteAPDPReg(reg_index, 0, value)
            if ret < 0:
                raise JLinkCoresightDPAccessError("Error writing register index {} in "
                                               "DP".format(reg_index))
        if ret > 0 and _RAISE_WAITS_AS_ERRORS:
            raise CoresightStickyError("WAIT condition encountered during DP reg write")

    
        

class TargetSelectionError(RuntimeError):
    """
    Failure to switch to the intended target after a given number of attempts
    """

_RAISE_WAITS_AS_ERRORS = True


class JLinkCoresightMEMAPData(CoresightMEMAPData):
    """
    Concrete CoresightMEMAP implementation which accesses MEM-AP registers via the
    JLink DLL
    """
    _warning_printed = False
    def __init__(self, index, dp, jlink_dll, verbose=0):
        CoresightMEMAPData.__init__(self, index, dp, verbose=verbose)
        self._dll = jlink_dll
        self._raw_read_buf = ctypes.create_string_buffer(4)
        self._read_buf = ctypes.cast(self._raw_read_buf, ctypes.POINTER(ctypes.c_uint32))

        addr = self._dll.JLINK_GetpFunc(JLINK_FUNC_INDEX.JLINK_IFUNC_CORESIGHT_ACC_APDP_REG_MUL)
        if addr is not None:
            if platform.system() == "Windows":
                JLINK_FUNC_ACC_APDP_REG_MUL = ctypes.WINFUNCTYPE(ctypes.c_int, 
                                              ctypes.POINTER(JLINK_CORESIGHT_APDP_ACC_DESC),
                                              ctypes.c_int)
            else:
                JLINK_FUNC_ACC_APDP_REG_MUL = ctypes.CFUNCTYPE(ctypes.c_int,
                                              ctypes.POINTER(JLINK_CORESIGHT_APDP_ACC_DESC),
                                              ctypes.c_int)
            self._JLINK_CORESIGHT_APDPRegMul = JLINK_FUNC_ACC_APDP_REG_MUL(addr)
            self.supports_block_access = True
        else:
            # Need to figure out the right way to flag that the interface is unavailable.
            # ANd suggest people update their JLink software.
            if not self._warning_printed:
                wprint("WARNING: J-Link software is too old to support fast memory access.  Please update from\n"
                "https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack")
                self.__class__._warning_printed = True

    
    def read_ap_reg(self, reg_index):
        """
        Return the UINT32 value of the given register index in this AP.
        """
        if self._verbose > 2:
            iprint("AP read reg_index {}".format(reg_index))
        ret = self._dll.JLINKARM_CORESIGHT_ReadAPDPReg(reg_index, 1, self._read_buf)
        if ret < 0:
            raise JLinkCoresightAPAccessError("Error reading register index {} in "
                                              "MEM-AP {}".format(reg_index, self.index))
        if ret > 0 and _RAISE_WAITS_AS_ERRORS:
            raise CoresightStickyError("WAIT condition encountered during AP reg read")
        return self._read_buf.contents.value


    def write_ap_reg(self, reg_index, value):
        if self._verbose > 2:
            iprint("AP write reg_index {}, value = 0x{:x}".format(reg_index, value))
        ret = self._dll.JLINKARM_CORESIGHT_WriteAPDPReg(reg_index, 1, value)
        if ret < 0:
            raise JLinkCoresightAPAccessError("Error writing register index {} in "
                                              "MEM-AP {}".format(reg_index, self.index))
        if ret > 0 and _RAISE_WAITS_AS_ERRORS:
            raise CoresightStickyError("WAIT condition encountered during AP reg read")

    def read_drw_block(self, read_start, num_reads, addr_inc=4, busy_check=None):
        """
        Read a block of word-aligned data through AP.DRW.
         - Assumes that AP.SELECT is already set for the target AP's TAR/DRW block
         - Assumes that if successive addresses are to be read, AP.CSW.AutoInc is set.
          However, it does handle setting TAR initially and re-setting it on 1KB boundaries.
        If busy_check is supplied it should be an object with attributes "addr", "mask" and "value".
        This will precede every DRW access with a polling access to the given bus address
        which completes when the value read from that address matches the given value subject to
        the given mask.   This is typically used to check that a busy bit is clear/ready bit is set
        before submitting the next operation.  In practice this results in 4 AP/DP operations being
        sent for every read, whereas without the busy_check it is just one (plus occasional re-setting
        of AP.TAR).  Typically it's therefore best to call this function with busy_check not set,
        check the relevant sticky bit afterwards to see if any busy errors were raised during the
        sequence, and resubmit with busy checks if so.  Usually the debugger runs slowly enough 
        that the hardware is able to complete requests prior to the next request arriving.
        """
        CHUNK_SIZE = 1024
        data = []
        while len(data) < num_reads:
            num_block_reads = min(num_reads - len(data), CHUNK_SIZE)
            data += self._read_drw_block(read_start + 4*len(data), num_block_reads, 
                                         addr_inc=addr_inc, busy_check=busy_check) 
        return data

    def _read_drw_block(self, read_start, num_reads, addr_inc=4, busy_check=None):

        if addr_inc and busy_check:
            # I don't think this will ever be needed as accesses to the bus the AP is connected
            # to have busy-ness signalled directly through the APACC/SWD protocol, which J-Link
            # handles for us.
            raise NotImplementedError("Busy check unsupported for auto-incrementing bulk read")

        if busy_check:
            # This is for non-native architectures
            # After every read we have to wait until an (obviously) out-of-line register
            # contains a certain value.  That means we have to reset TAR every time, so
            # for every read there are two writes to TAR and one "wait" in addition to the
            # target read itself, so there are four transactions per word read.
            n_trans = num_reads * 4
        else:
            read_end = read_start + num_reads * addr_inc
            start = read_start
            # Count how many 1K boundaries we will cross
            n_bdy = (((read_end-1) & ~0x3ff) - (read_start & ~0x3ff))//0x400
            n_trans = (num_reads + n_bdy + 1)

        desc_array = (JLINK_CORESIGHT_APDP_ACC_DESC * n_trans)()

        def write_tar(desc, addr):
            desc.Data = addr
            desc.RegIndex = 1
            desc.APnDP = 1
            desc.RnW = 0
        def read_data(desc):
            desc.RegIndex =  3 # DRW
            desc.APnDP =     1
            desc.RnW =       1
        def read_data_until(desc, mask, value):
            desc.RegIndex = 3
            desc.APnDP = 1
            desc.RnW = 1
            desc.Mask = mask
            desc.CompVal = value
            desc.TimeoutMsReadUntil = 10

        if busy_check is None:
            bdy_indices = []

            i_desc = 0
            i_data = 0

            # Add the initial write to TAR
            write_tar(desc_array[i_desc],read_start)
            bdy_indices.append(i_desc)
            i_desc += 1
            if addr_inc:
                while True:
                    # get first 1K boundary after start
                    bdy = (start & ~0x3ff) + 0x400
                    end = min(bdy, read_end)
                    # either write to the boundary or to the end of the write
                    addr = start
                    while addr < end:
                        read_data(desc_array[i_desc])
                        i_desc += 1
                        i_data += 1
                        addr += addr_inc
                        if busy_check is not None:
                            desc_array[i_desc].RegIndex = 1
                    if end >= read_end:
                        break
                    # Add in a write to TAR for this boundary
                    write_tar(desc_array[i_desc], bdy)
                    bdy_indices.append(i_desc)
                    i_desc += 1
                    start = end
            else:
                for i_desc in range(i_desc, i_desc+num_reads):
                    read_data(desc_array[i_desc])

        else:
            # Wait until the slave bus master is no longer busy before initiating the next read
            for i in range(0, len(desc_array),4):
                write_tar(desc_array[i], busy_check.addr)
                read_data_until(desc_array[i+1], busy_check.mask, busy_check.value)
                write_tar(desc_array[i+2], read_start)
                read_data(desc_array[3])

        if self._verbose > 1:
            iprint("Submitting {} transactions to J-Link".format(len(desc_array)))
        if self._verbose > 2:
            for desc in desc_array:
                if desc.RnW == 1:
                    if desc.Mask != 0:
                        iprint(" -> Read DRW until value & 0x{:x} == 0x{:x}".format(desc.Mask, desc.CompVal))
                    else:
                        iprint(" -> Read DRW")
                else:
                    iprint(" -> Write 0x{:x} to {}".format(desc.Data, "TAR" if desc.RegIndex == 1 else "DRW"))
        ret = self._JLINK_CORESIGHT_APDPRegMul(desc_array, len(desc_array))
        if ret < 0:
            raise JLinkCoresightAPAccessError("Error doing block write to DRW in "
                                              "MEM-AP {}".format(self.index))
        
        # Now we have to extract the data
        if busy_check is None:
            return [desc.Data for (idesc, desc) in enumerate(desc_array) 
                                        if idesc not in bdy_indices]
        else:
            # Every fourth entry is actually data we wanted
            return [desc_array[i+3].Data for i in range(0, len(desc_array), 4)]


    def write_drw_block(self, write_start, word_data, addr_inc=True, busy_check=None,
                        bytes_per_write=None):
        """
        Write a block of word-aligned data through AP.DRW.
         - Assumes that AP.SELECT is already set for the target AP's TAR/DRW block
         - Assumes that if successive addresses are to be written, AP.CSW.AutoInc is set.
          However, it does handle setting TAR initially and re-setting it on 1KB boundaries.
        :param addr_inc: should be True if AP.CSR.AutoInc is set and False otherwise.
        If busy_check is supplied it should be an object with attributes "addr", "mask" and "value".
        This will precede every DRW access with a polling access to the given bus address
        which completes when the value read from that address matches the given value subject to
        the given mask.   This is typically used to check that a busy bit is clear/ready bit is set
        before submitting the next operation.  In practice this results in 4 AP/DP operations being
        sent for every write, whereas without the busy_check it is just one (plus occasional re-setting
        of AP.TAR).  Typically it's therefore best to call this function with busy_check not set,
        check the relevant sticky bit afterwards to see if any busy errors were raised during the
        sequence, and resubmit with busy checks if so.  Usually the debugger runs slowly enough 
        that the hardware is able to complete requests prior to the next request arriving.
        """

        CHUNK_SIZE = 1024
        num_written = 0
        while num_written < len(word_data):
            self._write_drw_block(write_start + 4*num_written, 
                                  word_data[num_written:num_written+CHUNK_SIZE], 
                                  addr_inc=addr_inc, busy_check=busy_check)
            num_written += CHUNK_SIZE


    def _write_drw_block(self, write_start, word_data, addr_inc=True, busy_check=None):

        if busy_check:
            # This is for non-native architectures
            # After every read we have to wait until an (obviously) out-of-line register
            # contains a certain value.  That means we have to reset TAR every time.
            n_trans = len(word_data) * 4
        else:
            write_end = write_start + len(word_data)*4
            start = write_start
            # Count how many 1K boundaries we will cross
            n_bdy = (((write_end-1) & ~0x3ff) - (write_start & ~0x3ff))//0x400 if addr_inc else 0
            n_trans = len(word_data) + n_bdy + 1

        desc_array = (JLINK_CORESIGHT_APDP_ACC_DESC * n_trans)()

        def write_tar(desc, addr):
            desc.Data = addr
            desc.RegIndex = 1
            desc.APnDP = 1
            desc.RnW = 0
        def write_data(desc, value):
            desc.Data = value
            desc.RegIndex =  3 # DRW
            desc.APnDP =     1
            desc.RnW =       0
        def read_data_until(desc, mask, value):
            desc.RegIndex = 3
            desc.APnDP = 1
            desc.RnW = 1
            desc.Mask = mask
            desc.CompVal = value
            desc.TimeoutMsReadUntil = 10

        if busy_check is None:
            i_desc = 0
            i_data = 0
            # Add the initial write to TAR
            write_tar(desc_array[i_desc], write_start)
            i_desc += 1
            while True:
                if addr_inc:
                    # get first 1K boundary after start 
                    # either write up to the boundary or to the end of the write
                    bdy = (start & ~0x3ff) + 0x400
                    end = min(bdy, write_end)
                else:
                    # We can submit the entire write in one go
                    end = write_end
                addr = start
                while addr < end:
                    write_data(desc_array[i_desc], word_data[i_data])
                    i_desc += 1
                    i_data += 1
                    addr += 4
                if end >= write_end:
                    break
                # Add in a write to TAR for this boundary
                write_tar(desc_array[i_desc], bdy)
                i_desc += 1
                start = end

        elif not addr_inc:
            # Wait until the slave bus master is no longer busy before initiating the next write
            # This means we have to keep switching AP.TAR between the write keyhole and the status reg
            # so we simply have 4N transactions to execute.
            for i in range(0, len(desc_array),4):
                write_tar(desc_array[i], busy_check.addr)
                read_data_until(desc_array[i+1], busy_check.mask, busy_check.value)
                write_tar(desc_array[i+2], write_start)
                write_data(desc_array[i+3], word_data[i//4])
        else:
            # I don't think this will ever be needed as accesses to the bus the AP is connected
            # to have busy-ness signalled directly through the APACC/SWD protocol, which J-Link
            # handles for us.
            raise NotImplementedError("Support for auto-incrementing bulk read with busy check"
                                      "not available")

        if self._verbose > 1:
            iprint("Submitting {} transactions to J-Link".format(len(desc_array)))
        if self._verbose > 2:
            for desc in desc_array:
                if desc.RnW == 1:
                    if desc.Mask != 0:
                        iprint(" -> Read DRW until value & 0x{:x} == 0x{:x}".format(desc.Mask, desc.CompVal))
                    else:
                        iprint(" -> Unexpected read operation!")
                else:
                    iprint(" -> Write 0x{:x} to {}".format(desc.Data, "TAR" if desc.RegIndex == 1 else "DRW"))
        ret = self._JLINK_CORESIGHT_APDPRegMul(desc_array, len(desc_array))
        if ret < 0:
            raise JLinkCoresightAPAccessError("Error doing block write to DRW in "
                                              "MEM-AP {}".format(self.index))



class JLinkLibraryNotFound(JLinkSetupError):
    """
    We can't find the copy of Jlink.dll/so that we need either in the standard
    location or in a location the user has given us.
    """

class JLinkDLLWrapper(object):
    """
    The JLINKARM_SWD_* functions use a different calling convention from everything else
    we use in the JLink DLL.
    """
    def __init__(self, dll, windll):
        self._dll = dll
        self._windll = windll

    def __getattr__(self, attr):
        if attr.startswith("JLINK_") or attr.startswith("JLINKARM_SWD"):
            return getattr(self._windll, attr)
        return getattr(self._dll, attr)

def load_jlink_dll(s_dllpath):
    active_os = platform.system()
    py_arch   = (struct.calcsize("P") * 8)   # Determine if 32 or 64 bit python is running
    # Look for DLL relative to this script's directory
    
    if s_dllpath is None:
        standard_install=True
        if active_os == 'Windows':
            def look_for_jlink_installation(program_files_path):
                # First look for a standard J-Link software installation
                s_dllpath = os.path.join(program_files_path, "SEGGER","JLink")
                if not os.path.exists(s_dllpath):
                    # Maybe they've got the SDK installed
                    sdks = sorted(glob(os.path.join(program_files_path, 
                                                    "SEGGER","JLink_SDK_*")))
                    if not sdks:
                        return None
                    s_dllpath = sdks[-1]
                    
                return os.path.join(s_dllpath, "JLinkARM.dll" if py_arch==32 else "JLink_x64.dll")


            s_dllpath = (look_for_jlink_installation(r"C:\Program Files (x86)") or 
                            look_for_jlink_installation(r"C:\Program Files"))
            if s_dllpath is None:
                raise JLinkLibraryNotFound("No J-Link software/SDK installation "
                                            r"found in C:\Program Files (x86) or C:\Program Files. Install "
                                            "from https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack "
                                            "or pass DLL path via -d jlink:path:<DLL path>")
        elif active_os == "Linux":
            raise JLinkLibraryNotFound("Pass path to libjlinkarm.so via -d jlink:path:<.so path>")
        
    else:
        standard_install=False
        if os.path.isdir(s_dllpath):
            if active_os == "Windows":
                s_dllpath = os.path.join(s_dllpath, "JLinkARM.dll" if py_arch==32 else "JLink_x64.dll")
            else:
                s_dllpath = os.path.join(s_dllpath, "libjlinkarm.so")
        elif not os.path.isfile(s_dllpath):
            raise JLinkLibraryNotFound("Supplied path '{}' is not a file or directory".format(s_dllpath))
    
    if not os.path.exists(s_dllpath):
        if standard_install:
            raise JLinkLibraryNotFound("Didn't find the expected library file "
                                        "{}. Please check your installation".format(s_dllpath))
        else:
            raise JLinkLibraryNotFound("Didn't find the expected library file "
                                        "{}. Please check the supplied path.".format(s_dllpath))


    dll = ctypes.cdll.LoadLibrary(s_dllpath)

    if active_os == "Windows":
        # For some reason the JLINK_SWD_* functions use an alternative calling convention, so we
        # need a different ctypes interface for them!
        windll = ctypes.windll.LoadLibrary(s_dllpath)
    else:
        # On Linux everything is fine (presumably)
        windll = dll
    dll.JLINKARM_Open.restype = ctypes.c_char_p
    dll.JLINK_SWD_GetU32.restype = ctypes.c_uint32
    windll.JLINK_GetpFunc.restype = ctypes.c_void_p
    return JLinkDLLWrapper(dll, windll)


class JLinkTAPConnection(object):

    def __init__(self, api, get_scan_chain, transport_type, 
                 **transport_kwargs):
        self._dll = api.dll
        self._get_scan_chain = get_scan_chain
        self._api = api
        self._is_connected = False
        self._ir_len = None
        self._transport = None
        self._transport_type = transport_type
        self._transport_kwargs = transport_kwargs

    def connect(self):
        if self._is_connected:
            return

        # Reset everything and recreate the connection

        scan_chain = self._get_scan_chain()

        scan_chain_settings = ("IRPre={IRPre};"
                                "DRPre={DRPre};"
                                "IRPost={IRPost};"
                                "DRPost={DRPost};"
                                "IRLenDevice={IRLength}".format(**scan_chain)).encode("ascii")
        self._ir_len = scan_chain["IRLength"]
        ret = self._dll.JLINKARM_CORESIGHT_Configure(scan_chain_settings)
        if ret < 0:
            raise JLinkConnectionError("JLink CORESIGHT configuration "
                                    "failed with error code {}".format(ret))
        dprint("JLink device configured successfully.")

        # Done
        self._is_connected = True

    def disconnect(self):
        self._is_connected = False
        self._ir_len = None
        self._transport = None # we'll need to recreate the transport when we come back

    def get_transport(self):
        if not self._is_connected:
            raise RuntimeError("Attempting to retrieve transport while not connected!")
        if self._transport is None:
            if self._transport_type == "jtag2ahb":
                self._transport = JTAG2AHBTransport(self._api, JLinkJTAGDriver,
                                                    **self._transport_kwargs)
            elif self._transport_type == "probe":
                jtag = JLinkJTAGDriver(self._api, self._ir_len)
                self._transport = JTAGUtil(jtag)
            else:
                raise ValueError("JLink32TAPConnection: unknown transport "
                                 "type '{}'".format(self._transport_type))
        return self._transport

    def get_jtag_controller(self):
        if not self._is_connected:
            raise RuntimeError("Attempting to construct JTAG controller while not connected!")
        return JLinkJTAGDriver(self._api, self._ir_len)

class JLinkDAPConnection:
    """
    Captures the logic to initialise an SWD/JTAG DAP connection in the JLink API and
    create a JLinkCoresightTransport object to wrap it.  Needs to be subclassed to add
    a JTAG/SWD-specific connect method.
    """
    def __init__(self, api, transport_type, 
                 verbose=0, **transport_kwargs):

        self._dll = api.dll
        self._api = api
        self._is_connected = False
        self._verbose = verbose
        self._transport = None
        self._transport_type = transport_type
        self._transport_kwargs = transport_kwargs

    def disconnect(self):
        self._is_connected = False
        self._transport = None

    def get_transport(self):
        assert self._is_connected
        if self._transport is None:
            self._transport = JLinkCoresightTransport(self._api, verbose=self._verbose,
                                                        **self._transport_kwargs)
        return self._transport


class JLinkJTAGDAPConnection(JLinkDAPConnection):

    def __init__(self, api, get_scan_chain, transport_type, 
                 verbose=0, **transport_kwargs):
        JLinkDAPConnection.__init__(self, api, transport_type, 
                                    verbose=verbose, **transport_kwargs)
        self._get_scan_chain = get_scan_chain

    def connect(self):
        if self._is_connected:
            return

        # Reset everything and recreate the connection

        # Configure 

        scan_chain_settings = ("IRPre={IRPre};"
                               "DRPre={DRPre};"
                               "IRPost={IRPost};"
                               "DRPost={DRPost};"
                               "IRLenDevice={IRLength}".format(**self._get_scan_chain())).encode("ascii")
        ret = self._dll.JLINKARM_CORESIGHT_Configure(scan_chain_settings)
        if ret < 0:
            raise JLinkConnectionError("JLink CORESIGHT configuration "
                                    "failed with error code {}".format(ret))
        dprint("JLink device configured successfully.")

        # Done
        self._is_connected = True


class JLinkSWDDAPConnection(JLinkDAPConnection):

    TARGETSEL_ADDR = 0xc
    MAX_NUM_TARGETSEL_ATTEMPTS = 3

    def __init__(self, api, targetsel, transport_type, 
                 verbose=0, **transport_kwargs):
        JLinkDAPConnection.__init__(self, api, transport_type, 
                                    verbose=verbose, **transport_kwargs)
        self._targetsel = targetsel

    def connect(self):
        if self._is_connected:
            return

        if self._targetsel is not None:
            dprint("Initialising SWD for 0x{:x}:0x{:x}".
                format(self._targetsel[0],self._targetsel[1]))

            ret = self._dll.JLINKARM_CORESIGHT_Configure("")
            if ret < 0:
                raise JLinkConnectionError("JLink CORESIGHT configuration "
                                        "failed with error code {}".format(ret))
            dprint("JLink device configured successfully.")

            # Create a temporary transport instance to enable DP register reads
            trans = JLinkCoresightTransport(self._api, **self._transport_kwargs)
            dp = trans.get_dp()

            num_attempts = 0
            swd = SWD(JLinkSWDDriver(self._dll))
            if self._verbose > 1:
                iprint("Executing target-select sequence: 0x{:x}".format(
                                    self._targetsel[0]+(self._targetsel[1]<<28)))
            while num_attempts < self.MAX_NUM_TARGETSEL_ATTEMPTS:
                swd.line_reset_and_write(self.TARGETSEL_ADDR, 
                                         self._targetsel[0]+(self._targetsel[1]<<28))
                # The spec requires DPIDR to be read straight after 
                dp.fields.DPIDR.read()
                # Check that the TARGETID and INSTANCEID match what they should
                selected_targetid = dp.fields.TARGETID.read() & 0x0fffffff
                selected_instance_id = dp.fields.DLPIDR.TINSTANCE.read()
                if (selected_targetid, selected_instance_id) == self._targetsel:
                    break
                num_attempts += 1
            
            if num_attempts == self.MAX_NUM_TARGETSEL_ATTEMPTS:
                raise TargetSelectionError("Failed to select target 0x{:x}:{:x}"
                                            " after {} attempts".
                                            format(*(self._targetsel + (num_attempts,))))
        else:
            dprint("Initialising SWD")

        ret = self._dll.JLINKARM_CORESIGHT_Configure("")
        if ret < 0:
            raise JLinkConnectionError("JLink CORESIGHT configuration "
                                        "failed with error code {}".format(ret))
        dprint("JLink device configured successfully.")

        self._is_connected = True


class JLinkTransportAPI(object):

    TAPConnectionType = JLinkTAPConnection
    JTAGDAPConnectionType = JLinkJTAGDAPConnection
    SWDDAPConnectionType = JLinkSWDDAPConnection

    def __init__(self, s_dllpath=None,
                 ap_mapping=None):

        if ap_mapping is not None:
            self._ap_mapping = {ap.ap_number : (ap.ap_type, ap.ap_device) for ap in ap_mapping.values()}

        self.dll = load_jlink_dll(s_dllpath)            

    def open(self, serial_no=None, ip_addr=None,
                 target_interface=JLinkTrans.SWD,
                 speed=4000, verbose=None):
        """
        Open a JLink DLL session with the given parameters.  Final call is to
        JLINKARM_CORESIGHT_Configure, after which APDP register accesses can be
        issued.
        """
        if verbose is None:
            # Get it from the environment
            verbose = int(os.getenv("PYDBG_CORESIGHT_VERBOSITY", 0))

        self._target_interface = target_interface

        if verbose >= 0:
            if serial_no is not None:
                iprint("Connecting to J-link with parameters: serial_no={}, "
                       "speed={}kHz".format(serial_no, speed))
            else:
                iprint("Connecting to J-link with parameters: speed={}kHz".format(speed))

        if serial_no is not None:
            try:
                int(serial_no)
            except ValueError:
                raise JLinkConnectionError("Supplied JLink serial number '{}' appears "
                                           "invalid: should be a decimal integer".format(serial_no))
            self._current_device = serial_no
            # JLINKARM_EMU_SelectByUSBSN needs to be called before JLINKARM_Open
            if self.dll.JLINKARM_EMU_SelectByUSBSN(int(serial_no)) < 0:
                raise JLinkConnectionError("JLink serial number-based selection failed: "
                                               "is '{}' correct?".format(serial_no))
        elif ip_addr is not None:
            ip_arr = (ctypes.c_char * (len(ip_addr)+1))()
            ip_arr[:] = [v for v in ip_addr.encode("ascii")] + [0]
            if self.dll.JLINKARM_SelectIP(ip_arr, 0) < 0:
                raise JLinkConnectionError("JLink IP-based selection failed: "
                                               "is '{}' correct?".format(ip_addr))


        errmsg = self.dll.JLINKARM_Open()
        if errmsg:
            raise JLinkConnectionError(ctypes.c_char_p(errmsg).value.decode("ascii"))
        atexit.register(self.close)

        if self.dll.JLINKARM_TIF_Select(int(self._target_interface)) < 0:
            raise JLinkConnectionError("JLink API call failed setting target interface")

        self.set_speed(int(speed))

        # We delegate configuring the link to the Connection classes

    @property
    @autolazy
    def dongle_name(self):
        name_len = 256
        dongle_name = (ctypes.c_char * name_len)()
        if self.dll.JLINKARM_EMU_GetProductName(dongle_name, name_len) < 0:
            dongle_name = None
        return CLang.get_string(dongle_name[:])

    def close(self):
        """
        Close the DLL session.  This can be reopened with self.reopen.
        """
        self.dll.JLINKARM_Close()
        try:
            atexit.unregister
        except AttributeError:
            pass
        else:
            atexit.unregister(self.close)

    @property
    def interface(self):
        return "jtag" if self._target_interface == JLinkTrans.JTAG else "swd"

    def set_speed(self, speed_khz):
        """
        Set the speed of the connection in khz
        """
        if self.dll.JLINKARM_SetSpeed(speed_khz) < 0:
            raise JLinkConnectionError("JLink API call failed setting interface speed")

    def collect_jlink_info(self, verbose=False):
        """
        A method to collect information about the connected JLink emulators
        Create a C struct to hold the emulator info returned by
        JLINKARM_EMU_GetList(). The pointer to the array of this
        struct is passed into the JLINKARM_EMU_GetList call.
        """
        emulator_count = 50
        emulator_dict = {}
        currently_in_use = False

        class JlinkEmuConnectInfo(ctypes.Structure):
            _fields_ = [("SerialNumber", ctypes.c_uint32),
                        ("Connection", ctypes.c_uint),
                        ("USBAddr", ctypes.c_uint32),
                        ("aIPAddr", ctypes.c_uint8 * 16),
                        ("Time", ctypes.c_int),
                        ("Time_us", ctypes.c_uint64),
                        ("HWVersion", ctypes.c_uint32),
                        ("abMACAddr", ctypes.c_uint8 * 6),
                        ("acProduct", ctypes.c_char * 32),
                        ("acNickName", ctypes.c_char * 32),
                        ("acFWString", ctypes.c_char * 112),
                        ("IsDHCPAssignedIP", ctypes.c_char),
                        ("IsDHCPAssignedIPIsValid", ctypes.c_char),
                        ("NumIPConnections", ctypes.c_char),
                        ("NumIPConnectionsIsValid", ctypes.c_char),
                        ("aPadding", ctypes.c_uint8 * 34)]

        # Array of JlinkEmuConnectInfo struct. Assume 50 is the
        # maximum number of emulators you can have.
        jlink_connect_arr = (JlinkEmuConnectInfo * emulator_count)()
        hostif = JLINKARM_HOSTIF_USB | JLINKARM_HOSTIF_IP
        emulators = self.dll.JLINKARM_EMU_GetList(hostif, jlink_connect_arr,
                                                   emulator_count)
        if emulators < 0:
            return JLinkError("Error getting JLink emulator information")
        elif emulators == 0:
            return JLinkError("No connected JLink emulator found!")
        else:
            if emulators == 1:
                # If only one emulator then it has to be the one in use
                currently_in_use = True
            for i in range(emulators):
                # Copy the contents of the pointer to the array of struct
                # into a dictionary
                serial_number = jlink_connect_arr[i].SerialNumber
                if (emulators > 1 and
                        self._current_device == serial_number):
                    # If there are more than one emulator and the serial number
                    # of this emulator matches the serial number of the current
                    # device, then mark this as currently in use
                    currently_in_use = True
                connection_type = jlink_connect_arr[i].Connection
                if connection_type == 1:
                    connection = "USB"
                elif connection_type == 2:
                    connection = "IP"
                else:
                    raise RuntimeError("Wrong connection type. Can only be USB or IP")
                if verbose:
                    product = ""
                    for j in range(0, len(jlink_connect_arr[i].acProduct)):
                        product += chr(jlink_connect_arr[i].acProduct[j])
                    if connection == "IP":
                        # Only print these details if it is an IP connection
                        # otherwise these details are irrelevant.
                        ipaddr = ""
                        for j in range(len(jlink_connect_arr[i].aIPAddr)):
                            ipaddr += str(jlink_connect_arr[i].aIPAddr[j]) + ":"
                        mac_addr = ""
                        for j in range(len(jlink_connect_arr[i].abMACAddr)):
                            mac_addr += format(jlink_connect_arr[i].abMACAddr[j], "x") + ":"
                        nickname = ""
                        for j in range(len(jlink_connect_arr[i].acNickName)):
                            nickname += str(jlink_connect_arr[i].acNickName[j])
                        fw_string = ""
                        for j in range(len(jlink_connect_arr[i].acFWString)):
                            fw_string += str(jlink_connect_arr[i].acFWString[j])
                        dhcp_ip_valid = jlink_connect_arr[i].IsDHCPAssignedIPIsValid
                        dhcp_assignedip = jlink_connect_arr[i].IsDHCPAssignedIP.decode("ascii")
                        num_conn_valid = jlink_connect_arr[i].NumIPConnectionsIsValid
                        num_ip_conn = jlink_connect_arr[i].NumIPConnections.decode("ascii")
                        emulator_dict[serial_number] = \
                            {"Connection": connection,
                             "aIPAddr": ipaddr,
                             "Time": jlink_connect_arr[i].Time,
                             "Time_us": jlink_connect_arr[i].Time_us,
                             "HWVersion": jlink_connect_arr[i].HWVersion,
                             "abMACAddr": mac_addr,
                             "acProduct": product,
                             "acNickName": nickname,
                             "acFWString": fw_string,
                             "IsDHCPAssignedIP": dhcp_assignedip,
                             "IsDHCPAssignedIPIsValid": dhcp_ip_valid,
                             "NumIPConnections": num_ip_conn,
                             "NumIPConnectionsIsValid": num_conn_valid,
                             "CurrentlyInUse": currently_in_use}
                    else:
                        emulator_dict[serial_number] = \
                            {"Connection": connection,
                             "USBAddr": jlink_connect_arr[i].USBAddr,
                             "acProduct": product,
                             "CurrentlyInUse": currently_in_use}
                else:
                    emulator_dict[serial_number] = \
                        {"Connection": connection,
                         "CurrentlyInUse": currently_in_use}

        return emulator_dict

    def probe_jtag(self):
        """
        Probe the scan chain to determine what TAPs are present.
        Returns a list of IDCODE values.
        """
        id_codes = []
        while True:
            result = self.dll.JLINKARM_JTAG_GetDeviceId(len(id_codes))
            if result == 0:
                return id_codes
            id_codes.append(result)
        
    def connect_workaround(self, device, ap_number):
        """
        Get J-Link to connect to the given processor.  Without doing this the CORESIGHT APIs
        seem to fail sometimes.   This must be called after JTAG probing is done.
        """
        
        iprint("Performing J-Link connection to '{}' on AP {} to complete J-Link "
                "initialisation".format(device, ap_number))
        scriptfile="""
void ConfigTargetSettings(void) {{
    JLINK_ExecCommand("CORESIGHT_AddAP = Index={} Type=AHB-AP BaseAddr=0x00000000");
    JLINK_ExecCommand("CORESIGHT_SetIndexAHBAPToUse = {}");
    JLINK_ExecCommand("CORESIGHT_SetCoreBaseAddr = 0xE0000000");}}
    """.format(ap_number, ap_number)
    
        tempd = tempfile.mkdtemp()
        try:
            scriptfile_name = join(tempd,"slate.jlinkscript")
            with open(scriptfile_name,"w") as out:
                out.write(scriptfile)
            
            self.dll.JLINKARM_ExecCommand(r"device={}".format(device).encode("ascii"),0,0)
            self.dll.JLINKARM_ExecCommand(r"scriptfile = {}".format(scriptfile_name).encode("ascii"),0,0)
            self.dll.JLINK_Connect()
        finally:
            shutil.rmtree(tempd)



class JLinkCoresightTransport(CoresightTransport):
    """
    Coresight architecture level transport which implements low-level reads
    and writes of the debug elements attached to arbitrary MEM-APs using the
    low-level JLink AP/DP register read/write API.
    """
    MEMAP_DATA_TYPE = JLinkCoresightMEMAPData

    def __init__(self, api, raw_driver=False, verbose=0, extra_ap_args=None):

        CoresightTransport.__init__(self, verbose=verbose,
                                    extra_ap_args=extra_ap_args)
        self._api = api
        self._dll = api.dll
        if raw_driver:
            if self._api.interface == "jtag":
                jtag_driver = JLinkJTAGDriver(self._api, ir_length=4)
                self._raw_driver = CoresightJTAGDPDriver(jtag_driver)
            else:
                swd_driver = JLinkSWDDriver(self._api.dll)
                self._raw_driver = CoresightSWDDPDriver(swd_driver)
        else:
            self._raw_driver = None

    def create_dp(self):
        if self._raw_driver:
            self._dp_data = CoresightRawDriverDPData(self._raw_driver, verbose=self._verbose)
        else:
            self._dp_data = JLinkCoresightDPData(self._dll, verbose=self._verbose)
        dp_version = get_coresight_debug_port_version(self._dp_data)
        return CoresightDebugPort(self._dp_data, dp_version, verbose=self._verbose)





class JLinkJTAGDriver(object):

    def __init__(self, api, ir_length, **scan_chain):

        self._ir_length = ir_length
        self.ir_state = None

        self._dll = api.dll

        self.reset()

    def reset(self):
        # Reset the TAP
        reset_tms = ctypes.c_uint8(0x1f)
        reset_tdi = ctypes.c_uint8(0)
        self._dll.JLINKARM_JTAG_StoreRaw(ctypes.byref(reset_tdi), 
                                         ctypes.byref(reset_tms), 6)

    def ir_scan(self, ir_value):
        """
        """
        num_uint8s = (self._ir_length + 7)//8
        tdi_buffer = (ctypes.c_uint8 * num_uint8s)()
        for i in range(num_uint8s):
            tdi_buffer[i] = (ir_value >> (8*i))&0xff
        self._dll.JLINKARM_JTAG_StoreInst(tdi_buffer, self._ir_length)
        self.ir_state = ir_value

    def ir_scan_bits(self, ir_bits):
        num_uint8s = (len(ir_bits) + 7)//8
        tdi_buffer = (ctypes.c_uint8 * num_uint8s)()
        for i in range(num_uint8s):
            tdi_buffer[i] = build_le(ir_bits[8*i:8*(i+1)], word_width=1)
        self._dll.JLINKARM_JTAG_StoreInst(tdi_buffer, len(ir_bits))
        self.ir_state = None # we assume the bits weren't a real command

    def dr_scan_bits(self, dr_bits):
        num_uint8s = (len(dr_bits) + 7) // 8

        tdi_buffer = (ctypes.c_uint8 * num_uint8s)()
        tdo_buffer = (ctypes.c_uint8 * num_uint8s)()
        for i in range(num_uint8s):
            tdi_buffer[i] = build_le(dr_bits[8*i:8*(i+1)], word_width=1)
        self._dll.JLINKARM_JTAG_StoreGetData(tdi_buffer, tdo_buffer, len(dr_bits))
        return sum((flatten_le(val, word_width=1, num_words=8) for val in tdo_buffer), [])[:len(dr_bits)]


    # Extra function which attempts to drive the scan chain directly.  Probably not needed.
    def dr_scan_bits_raw(self, dr_bits):

        tms_bits_to_dr_scan = [1,0,0]  # we start at Update-IR/Update-DR
        tms_bits_from_dr_scan = [1,1]  # go from DR-scan to Update-DR

        all_tms_bits = tms_bits_to_dr_scan + [0]*(len(dr_bits)-1) + tms_bits_from_dr_scan
        all_dr_bits = [0]*len(tms_bits_to_dr_scan) + dr_bits + [0]*(len(tms_bits_from_dr_scan)-1)

        num_uint8s_in = (len(all_tms_bits) + 7) // 8
        num_uint8s_out = (len(dr_bits) + 7) // 8
        tms_buffer = (ctypes.c_uint8 * num_uint8s_in)()
        tdi_buffer = (ctypes.c_uint8 * num_uint8s_in)()

        for i in range(num_uint8s_in):
            tms_buffer[i] = build_le(all_tms_bits[8*i:8*(i+1)], word_width=1)
            tdi_buffer[i] = build_le(all_dr_bits[8*i:8*(i+1)], word_width=1)

        bitpos = self._dll.JLINKARM_JTAG_StoreRaw(tdi_buffer, tms_buffer, len(all_tms_bits))
        out_bytes = []
        for i in range(num_uint8s_out):
            out_bytes.append(self._dll.JLINKARM_JTAG_GetU8(bitpos))
        return sum((flatten_le(val, word_width=1, num_words=8) for val in out_bytes), [])[:len(dr_bits)]


