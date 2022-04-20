############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import contextlib
import time
from collections import namedtuple

from csr.wheels import build_le, flatten_le, iprint, bytes_to_dwords, \
    dwords_to_bytes, pack_unpack_data_le, iprint, wprint, timeout_clock
from csr.dev.hw.core.simple_space import SimpleRegisterSpace
from csr.dev.hw.core.meta.i_core_info import ICoreInfo
from csr.dev.hw.core.meta.i_layout_info import ILayoutInfo
from csr.dev.hw.core.meta.io_struct_io_map_info import IoStructIOMapInfo
from csr.dev.hw.io.arch.armv6_m_io_struct import c_reg, c_bits, c_enum
from csr.dev.hw.debug.utils import memory_write_helper, TransportAccessError
from csr.dev.hw.debug.coresight import CoresightAPAccessError, CoresightAPBusAccessError

class RISCVSystemBusError(TransportAccessError):
    """
    A system bus error was raised during an access
    """

class RISCVSystemBusBusyError(TransportAccessError):
    """
    The debugger initiated a follow-up access too soon while the 
    SBA was still busy implementing the last one.
    """

class RISCVRegisterSpaceDataInfo(ILayoutInfo):
    
    endianness = ILayoutInfo.LITTLE_ENDIAN
    
    def deserialise(self, addr_units):
        return build_le(addr_units, self.addr_unit_bits)
    
    def serialise(self, integer, num_units, wrapping=False):
        return flatten_le(integer, num_units, self.addr_unit_bits, wrapping)

    def to_words(self, addr_units):
        return bytes_to_dwords(addr_units)

    def from_words(self, words):
        return dwords_to_bytes(words)

    addr_unit_bits = 32
    data_word_bits = 32


class RISCVRegisterSpaceInfo(ICoreInfo):
    
    layout_info = RISCVRegisterSpaceDataInfo()

RISCVSBBusyCheck = namedtuple("RISCVSBBusyCheck", "addr mask value")

def create_reg(addr, readable=True, writeable=True, width=32,text=""):
    
    access_flags = {(True,False) : "R",
                    (False,True) : "W",
                    (True,True): "RW"}[(readable, writeable)]
    return c_reg(addr, readable, writeable, 0, access_flags, width, 
                 0, False, None, None, None, text, None,
                 None, None, True, False, None, None)

def create_bits(parent, lsb, msb, readable=True, writeable=True):
    
    access_flags = {(True,False) : "R",
                    (False,True) : "W",
                    (True,True): "RW"}[(readable, writeable)]
    width = msb+1-lsb
    mask = ((1 << width)-1)<<lsb
    return c_bits(parent, lsb, msb, mask, width, access_flags, "")

class E21_DMInfo(RISCVRegisterSpaceInfo):
    """
    Register definitions from 
    https://ipcatalog.qualcomm.com/swi/chip/375/version/8093/module/9350818#DMI_DATA
    """

    c_reg = c_reg
    c_enum = c_enum

    DMI_DATA = create_reg(0x60820010)
    DMI_DATA.dmi_data_0 = create_bits(DMI_DATA, 0, 7)
    DMI_DATA.dmi_data_1 = create_bits(DMI_DATA, 8, 15)
    DMI_DATA.dmi_data_2 = create_bits(DMI_DATA, 16, 23)
    DMI_DATA.dmi_data_3 = create_bits(DMI_DATA, 24, 31)

    DMCONTROL = create_reg(0x60820040)
    DMCONTROL.dmactive  = create_bits(DMCONTROL, 0,0)
    DMCONTROL.ndmreset  = create_bits(DMCONTROL, 1,1)
    DMCONTROL.clrresethaltreq  = create_bits(DMCONTROL, 2,2)
    DMCONTROL.setresethaltreq  = create_bits(DMCONTROL, 3,3)
    DMCONTROL.ackhavereset  = create_bits(DMCONTROL, 28,28)
    DMCONTROL.hartreset  = create_bits(DMCONTROL, 29,29)
    DMCONTROL.resumereq  = create_bits(DMCONTROL, 30,30)
    DMCONTROL.haltreq  = create_bits(DMCONTROL, 31,31)

    DMI_DMSTATUS = create_reg(0x60820044, writeable=False)
    DMI_DMSTATUS.impebreak = create_bits(DMI_DMSTATUS, 22, 22,writeable=False)
    DMI_DMSTATUS.allhavereset = create_bits(DMI_DMSTATUS, 19, 19,writeable=False)
    DMI_DMSTATUS.anyhavereset = create_bits(DMI_DMSTATUS, 18, 18,writeable=False)
    DMI_DMSTATUS.allresumeack = create_bits(DMI_DMSTATUS, 17, 17,writeable=False)
    DMI_DMSTATUS.anyresumeack = create_bits(DMI_DMSTATUS, 16, 16,writeable=False)
    DMI_DMSTATUS.allnonexistent = create_bits(DMI_DMSTATUS, 15, 15,writeable=False)
    DMI_DMSTATUS.anynonexistent = create_bits(DMI_DMSTATUS, 14, 14,writeable=False)
    DMI_DMSTATUS.allunavail = create_bits(DMI_DMSTATUS, 13, 13,writeable=False)
    DMI_DMSTATUS.anyunavail = create_bits(DMI_DMSTATUS, 12, 12,writeable=False)
    DMI_DMSTATUS.allrunning = create_bits(DMI_DMSTATUS, 11, 11,writeable=False)
    DMI_DMSTATUS.anyrunning = create_bits(DMI_DMSTATUS, 10, 10,writeable=False)
    DMI_DMSTATUS.allhalted = create_bits(DMI_DMSTATUS, 9, 9,writeable=False)
    DMI_DMSTATUS.anyhalted = create_bits(DMI_DMSTATUS, 8, 8,writeable=False)
    DMI_DMSTATUS.authenticated = create_bits(DMI_DMSTATUS, 7, 7,writeable=False)
    DMI_DMSTATUS.authbusy = create_bits(DMI_DMSTATUS, 6, 6,writeable=False)
    DMI_DMSTATUS.hasresethaltreq = create_bits(DMI_DMSTATUS, 5, 5,writeable=False)
    DMI_DMSTATUS.confstrptrvalid = create_bits(DMI_DMSTATUS, 4, 4,writeable=False)
    DMI_DMSTATUS.version = create_bits(DMI_DMSTATUS, 0, 3,writeable=False)

    DMI_HARTINFO = create_reg(0x60820048, writeable=False)
    DMI_HARTINFO.nscratch = create_bits(DMI_HARTINFO, 20, 23, writeable=False)
    DMI_HARTINFO.dataaccess = create_bits(DMI_HARTINFO, 16, 16, writeable=False)
    DMI_HARTINFO.datasize = create_bits(DMI_HARTINFO, 12, 15, writeable=False)
    DMI_HARTINFO.dataaddr = create_bits(DMI_HARTINFO, 0, 11, writeable=False)

    DMI_HALTSUM1 = create_reg(0x6082004C, writeable=False)

    DMI_ABSTRACTCS = create_reg(0x60820058)
    DMI_ABSTRACTCS.progbufsize = create_bits(DMI_ABSTRACTCS, 24, 28, writeable=False)
    DMI_ABSTRACTCS.busy = create_bits(DMI_ABSTRACTCS, 12, 12, writeable=False)
    DMI_ABSTRACTCS.cmderr = create_bits(DMI_ABSTRACTCS, 8, 10)
    DMI_ABSTRACTCS.datacount = create_bits(DMI_ABSTRACTCS, 0, 3, writeable=False)

    DMI_COMMAND = create_reg(0x6082005C)

    DMI_COMMAND__register_access = create_reg(0x6082005C)
    DMI_COMMAND__register_access.cmdtype = create_bits(DMI_COMMAND__register_access, 24,31)
    DMI_COMMAND__register_access.aarsize = create_bits(DMI_COMMAND__register_access, 20, 22)
    DMI_COMMAND__register_access.aarpostincrement = create_bits(DMI_COMMAND__register_access, 19, 19)
    DMI_COMMAND__register_access.postexec = create_bits(DMI_COMMAND__register_access, 18,18)
    DMI_COMMAND__register_access.transfer = create_bits(DMI_COMMAND__register_access, 17,17)
    DMI_COMMAND__register_access.write_ = create_bits(DMI_COMMAND__register_access, 16,16)
    DMI_COMMAND__register_access.regno = create_bits(DMI_COMMAND__register_access, 0,15)

    DMI_COMMAND__memory_access = create_reg(0x6082005C)
    DMI_COMMAND__memory_access.cmdtype = create_bits(DMI_COMMAND__memory_access, 24,31)
    DMI_COMMAND__memory_access.aamvirtual = create_bits(DMI_COMMAND__memory_access, 23, 23)
    DMI_COMMAND__memory_access.aamsize = create_bits(DMI_COMMAND__memory_access, 20, 22)
    DMI_COMMAND__memory_access.aampostincrement = create_bits(DMI_COMMAND__memory_access, 19, 19)
    DMI_COMMAND__memory_access.write_ = create_bits(DMI_COMMAND__memory_access, 16,16)
    DMI_COMMAND__memory_access.target_specific = create_bits(DMI_COMMAND__memory_access, 14,15)

    DMI_ABSTRACTAUTO = create_reg(0x60820060)
    DMI_ABSTRACTAUTO.autoexecprogbuf = create_bits(DMI_ABSTRACTAUTO, 16, 17)
    DMI_ABSTRACTAUTO.autoexecdata = create_bits(DMI_ABSTRACTAUTO, 0, 0)

    DMI_PROGBUF = create_reg(0x60820080)
    DMI_PROGBUF.dmi_progbuf3 = create_bits(DMI_PROGBUF, 24, 31)
    DMI_PROGBUF.dmi_progbuf2 = create_bits(DMI_PROGBUF, 16, 23)
    DMI_PROGBUF.dmi_progbuf1 = create_bits(DMI_PROGBUF, 8, 15)
    DMI_PROGBUF.dmi_progbuf0 = create_bits(DMI_PROGBUF, 0, 7)
    DMI_PROGBUF_1 = create_reg(0x60820084)
    DMI_PROGBUF_1.dmi_progbuf7 = create_bits(DMI_PROGBUF_1, 24, 31)
    DMI_PROGBUF_1.dmi_progbuf6 = create_bits(DMI_PROGBUF_1, 16, 23)
    DMI_PROGBUF_1.dmi_progbuf5 = create_bits(DMI_PROGBUF_1, 8, 15)
    DMI_PROGBUF_1.dmi_progbuf4 = create_bits(DMI_PROGBUF_1, 0, 7)

    DMI_DMCS2 = create_reg(0x608200C8)
    DMI_DMCS2.exttrigger = create_bits(DMI_DMCS2, 7, 10)
    DMI_DMCS2.haltgroup = create_bits(DMI_DMCS2, 2, 6)
    DMI_DMCS2.hgwrite = create_bits(DMI_DMCS2, 1, 1, readable=False)
    DMI_DMCS2.hgselect = create_bits(DMI_DMCS2, 0, 0)

    SBCS = create_reg(0x608200E0)
    SBCS.sbaccess8       = create_bits(SBCS, 0, 0, writeable=False)
    SBCS.sbaccess16      = create_bits(SBCS, 1, 1, writeable=False)
    SBCS.sbaccess32      = create_bits(SBCS, 2, 2, writeable=False)
    SBCS.sbaccess64      = create_bits(SBCS, 3, 3, writeable=False)
    SBCS.sbaccess128     = create_bits(SBCS, 4, 4, writeable=False)
    SBCS.sbasize         = create_bits(SBCS, 5,11, writeable=False)
    SBCS.sberror         = create_bits(SBCS,12,14)
    SBCS.sbreadondata    = create_bits(SBCS,15,15)
    SBCS.sbautoincrement = create_bits(SBCS,16,16)
    SBCS.sbaccess        = create_bits(SBCS,17,19)
    SBCS.sbreadonaddr    = create_bits(SBCS,20,20)
    SBCS.sbbusy          = create_bits(SBCS,21,21, writeable=False)
    SBCS.sbbusyerror     = create_bits(SBCS,22,22)
    SBCS.sbversion       = create_bits(SBCS,29,31, writeable=False)

    DMI_SBADDR0 = create_reg(0x608200E4)

    DMI_SBDATA0 = create_reg(0x608200F0)

    DMI_HALTSUM0 = create_reg(0x820100, writeable=False)

    @property
    def io_map_info(self):
        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.__class__, None,
                                                  self.layout_info)
        return self._io_map_info

class RISCV_DMInfoV1(E21_DMInfo):
    pass

sberror_codes = {0:  "There was no bus error",
                 1:  "There was a timeout",
                 2:  "A bad address was accessed",
                 3:  "There was an alignment error",
                 4:  "An access of unsupported size was requested",
                 7:  "Other"}

class RISCVAbstractCommands:
    AccessRegister = 0
    QuickAccess = 1
    AccessMemory = 2

cmderr_codes = {0 : "No error",
                1 : "Busy",
                2 : "Not supported",
                3 : "Exception",
                4 : "Halt/Resume",
                5 : "Bus",
                7 : "Other"}

class RISCVAbstractCommandError(RuntimeError):
    """
    abstractcs.cmderr was set after completing an abstract command
    """

class RISCVAbstractCommandNotSupported(RISCVAbstractCommandError):
    """
    "Not supported" (2) set in cmderr
    """

class RISCVInst:

    SYSTEM = 0b1110011

    class funct3:
        CSRRW = 0b001
        CSRRS = 0b010

    @classmethod
    def encode_system_instr(cls, funct3, csr_or_value, rs1, rd):
        return (((csr_or_value & 0xfff) << 20) | 
                ((rs1          & 0x1f)  << 15) | 
                ((funct3       & 7)     << 12) | 
                ((rd           & 0x1f)  <<  7) | 
                (cls.SYSTEM    & 0x7f)) 

    @classmethod
    def encode_csrw(cls, csr, rs1):
        return cls.encode_system_instr(cls.funct3.CSRRW, csr, rs1, 0)

    @classmethod
    def encode_csrr(cls, csr, rd):
        return cls.encode_system_instr(cls.funct3.CSRRS, csr, 0, rd)

    @classmethod
    def encode_ebreak(cls):
        return cls.encode_system_instr(0, 1, 0, 0)

def _get_address(address_or_slice):
    if isinstance(address_or_slice, slice):
        if address_or_slice.stop - address_or_slice.start != 1:
            raise ValueError("Can only do 4-byte accesses to Coresight hardware")
        return address_or_slice.start
    return address_or_slice



class RISCV_DM(SimpleRegisterSpace):

    def __init__(self, data_space, version, verbose=0, auto_boots=True, run_on_reset=True):

        self._verbose = verbose

        if self._verbose > 1:
            iprint("Setting up TME E21 access via APB")
            iprint("---------------------------------")

        port_info_type = [E21_DMInfo, RISCV_DMInfoV1][version]
        SimpleRegisterSpace.__init__(self, "CORESIGHT_DP", data=data_space, info=port_info_type())
        self.arch_version = version
        self._use_busy_check = False
        self._sbbusy_check = RISCVSBBusyCheck(self.fields.SBCS.start_addr, 
                                              self.fields.SBCS.sbbusy.mask,0)
        self._apb_ap = data_space.ap
        self._dp = self._apb_ap.dp

        self._data_access_addr = self.fields.DMI_SBDATA0.start_addr

        self._regnos_abstract_access_not_supported = set()

        self._auto_boots = auto_boots
        self._run_on_reset = run_on_reset
        with self._apb_ap.preselected_for_bus_access():
            self._bring_up()

            if self._verbose > 1:
                iprint(" (misc SBCS access checks and settings)")
            # set the appropriate auto-read/increment settings for fast memory
            # reading
            sbcs = self.fields.SBCS.capture()
            assert sbcs.sbasize  == 32
            assert sbcs.sbaccess32 == 1 # 32-bit accesses are supported

            sbcs.sbaccess=2  # 32-bit accesses
            sbcs.sbautoincrement=1 # increment address after each access
            sbcs.sbreadonaddr=1 # execute a read as soon as an address is written
            # If this isn't set for reads then data won't be available on the first read to
            # SBDATA0 after setting the address.  However, it must be disabled during 
            # write operations unless sbautoincrement is disabled, or else the auto-read
            # will trigger a change in the address so the write will go to a later address
            # than intended.
            sbcs.sbreadondata=0 # don't execute a new read as soon as a read is 
            # performed, by default.
            # This should only be set for the first N-1 accesses of a multiword read.  If the final
            # word is read with this set a read is executed off the end of the target address range
            # which can create problems if it happens to be invalid.  By implication for single
            # word reads (e.g. registers) this shouldn't be set at all.  Hence we ensure it is
            # zero except when we need it to be set.

            sbcs.flush()

    def ndm_reset(self, run_on_reset=None):
        if run_on_reset is None:
            run_on_reset = self._run_on_reset

        if self._verbose:
            if run_on_reset:
                iprint(" Performing system reset")
            else:
                iprint(" Performing system reset and halting CPU")
        # This is a special boot sequence that's needed on blank silicon
        dmcontrol = self.fields.DMCONTROL.capture(0)
        # First enable
        dmcontrol.dmactive = 1
        dmcontrol.flush()
        # Now set haltreq too
        dmcontrol.haltreq = 1
        dmcontrol.flush()

        if not self._run_on_reset:
            # Ensure the processor halts on release from reset
            dmcontrol.setresethaltreq = 1
        # Toggle ndmreset to reset the system
        dmcontrol.ndmreset = 1
        dmcontrol.flush()

        dmcontrol.ndmreset = 0
        dmcontrol.flush()

        # Toggle dmactive to reset the DM block
        dmcontrol.dmactive = 0
        dmcontrol.flush()

        dmcontrol.haltreq = 0
        dmcontrol.dmactive = 1
        if not self._run_on_reset:
            # Clear the sticky bit that causes halt-on-reset
            dmcontrol.setresethaltreq=0
            dmcontrol.clrresethaltreq=1
        dmcontrol.flush()

        if self._run_on_reset:
            # set resumereq just in case that helps
            dmcontrol.resumereq = 1
            dmcontrol.flush()

    def enable_dm(self):
        self.fields.DMCONTROL.writebits(0,dmactive=1)

    def reset_dm(self):
        dmcontrol = self.fields.DMCONTROL.capture(0)
        dmcontrol.flush()
        dmcontrol.dmactive = 1
        dmcontrol.flush()        

    def _bring_up(self):
        """
        """
        if self._auto_boots:
            self.enable_dm()
        else:
            self.ndm_reset()


    def enable_autoread(self):
        if self._verbose > 1:
            iprint(" (enabling readonaddr if necessary)")
        sbcs = self.fields.SBCS.capture()
        if sbcs.sbreadondata == 1:
            # readondata should only be set while reading the first n-1 words
            # of an n-word sequence, so shouldn't ever be seen outside a call 
            # to self.read_memory().
            wprint("WARNING: readondata is set")
        if sbcs.sbreadonaddr == 0:
            sbcs.sbreadonaddr = 1
            sbcs.flush()
    def disable_autoread(self):
        if self._verbose > 1:
            iprint(" (disabling readonaddr if necessary)")
        sbcs = self.fields.SBCS.capture()
        if sbcs.sbreadondata == 1:
            wprint("WARNING: readondata is set")
        if sbcs.sbreadonaddr == 1:
            sbcs.sbreadonaddr = 0
            sbcs.flush()

    @contextlib.contextmanager
    def busy_check_enabled(self):
        self._use_busy_check = True
        yield
        self._use_busy_check = False

    def read_memory(self, start_addr, end_addr):
        """
        Read memory by programming the SBA with the start
        address and then reading out the data 
        """
        with self._apb_ap.preselected_for_bus_access():
            if self._verbose > 1:
                iprint("\nReading {} bytes at 0x{:x} in E21".format(
                                end_addr-start_addr,start_addr))
                iprint("----------------------------")
            try:
                self.enable_autoread()
            except CoresightAPBusAccessError:
                self._bring_up()
                self.enable_autoread()
            if self._verbose > 1:
                iprint(" (Performing SBA read)")
            # For now we'll assume all accesses are 32-bit aligned
            start_access_addr = (start_addr // 4)*4
            end_access_addr = ((end_addr + 3)//4)*4

            self.fields.DMI_SBADDR0 = start_access_addr

            if end_access_addr - start_access_addr > 4:
                try:
                    if self._verbose > 1:
                        iprint("* Performing read from 0x{:x} to 0x{:x} without busy check".format(
                                                start_access_addr,end_access_addr-4))
                    self.fields.SBCS.sbreadondata = 1
                    words= self._apb_ap.poll_read(self._data_access_addr, 
                                                  (end_access_addr - 4 - start_access_addr)//4,
                                                  busy_check=None)
                    try:
                        self._check_sberrors()
                    except RISCVSystemBusBusyError:
                        if self._verbose > 1:
                            iprint("Busy error seen: falling back to step-by-step busy check")
                        words= self._apb_ap.poll_read(self._data_access_addr, 
                                                      (end_access_addr - 4 - start_access_addr)//4,
                                                      busy_check=self._sbbusy_check)
                        self._check_sberrors()

                finally:
                    # We must always switch sbreadondata off again, because we want to be able to assume
                    # that it is clear except when doing multiword reads.
                    self.fields.SBCS.sbreadondata = 0
            else:
                words = []

            # Now read the last word without triggering a readahead off the end of the 
            # target address range

            if self._verbose > 1:
                iprint("* Performing read at 0x{:x} with busy check".format(end_access_addr-4))
            # We apply the busy check unconditionally here because there's very little to be
            # gained by not doing so
            words += self._apb_ap.poll_read(self._data_access_addr, 1, 
                                            busy_check=self._sbbusy_check)
            self._check_sberrors()

            return pack_unpack_data_le(words, 
                                      from_width=32, 
                                      to_width=8)[start_addr-start_access_addr:
                                                    end_addr-start_access_addr]

    @property
    def supports_subword_access(self):
        sbcs = self.fields.SBCS.capture()
        return sbcs.sbaccess8 == 1 and sbcs.sbaccess16 == 1

    def set_access_size(self, size_expt):
        self.fields.SBCS.sbaccess = size_expt

    def write_out_simple(self, start_addr, bytes_data, num_writes):
        # We need to pack the supplied byte_data into a suitable sized word
        s_align = start_addr%4 
        e_align = (start_addr+len(bytes_data))%4
        bytes_per_write = (1 if (s_align % 2 !=0 or e_align % 2 !=0) else 
                            (2 if (s_align % 4 !=0 or e_align % 4 !=0) else 
                            4))
        assert num_writes*bytes_per_write == len(bytes_data)
        # It's slightly inefficient to call pack_unpack_data_le when from and to widths
        # are equal, but not really worth worrying about
        self.fields.DMI_SBADDR0 = start_addr
        self._apb_ap.poll_write(self._data_access_addr, 
            pack_unpack_data_le(bytes_data, from_width=8,to_width=8*bytes_per_write),
                                busy_check=self._sbbusy_check, bytes_per_write=bytes_per_write)

    def write_out_aligned_block(self, write_start, bytes_data):
        self.fields.DMI_SBADDR0 = write_start
        busy_check = self._sbbusy_check if self._use_busy_check else None
        self._apb_ap.poll_write(self._data_access_addr,
                                pack_unpack_data_le(bytes_data, from_width=8, to_width=32),
                                busy_check=busy_check)

    def read_word(self, addr):
        # Note: we don't make any change to sbreadondata (via enable|disableautoread) here
        # because this function is only intended for use with the unaligned write helper,
        # which manages that for us (it disables it)
        self.fields.DMI_SBADDR0 = addr
        return self._apb_ap.poll_read(self._data_access_addr, 1, 
                                      busy_check=self._sbbusy_check)[0]

    def _check_sberrors(self):
        if self._verbose > 1:
            iprint(" (Checking SBCS.sberror and SBCS.sbbusyerror)")
        sbcs = self.fields.SBCS.capture()
        if sbcs.sberror != 0:
            self.fields.SBCS.sberror = 0x7 # clear the error
            raise RISCVSystemBusError("RISC-V SBA error {} ({})".format( 
                    sbcs.sberror, sberror_codes.get(sbcs.sberror.read(), "<unknown>")))
        if sbcs.sbbusyerror != 0:
            self.fields.SBCS.sbbusyerror = 0 # clear the error
            raise RISCVSystemBusBusyError("RISC-V SBA busy error")

    def write_memory(self, start_addr, value_bytes):
        if self._verbose > 1:
            iprint("\nWriting 0x{:x} in E21".format(start_addr))
            iprint("----------------------------")
        with self._apb_ap.preselected_for_bus_access():
            try:
                self.disable_autoread()
            except CoresightAPBusAccessError:
                self._bring_up()
                self.disable_autoread()
            if self._verbose > 1:
                iprint(" (Performing SBA write)")
            try:
                memory_write_helper(self, start_addr, value_bytes)
            except CoresightAPAccessError:
                if self._dp.reset_triggered():
                    raise

                iprint("AP error writing to RISC-V subsystem: attempting ndmreset sequence")
                self._bring_up()
                memory_write_helper(self, start_addr, value_bytes)

            if not self._dp.reset_triggered():
                try:
                    self._check_sberrors()
                except RISCVSystemBusBusyError:
                    # Try again, but do the writes individually now
                    with self._busy_check_enabled():
                        memory_write_helper(self, start_addr, value_bytes)
                    self._check_sberrors()
            else:
                if self._verbose > 1:
                    iprint("Waiting for 1ms")
                time.sleep(0.001)
                self._dp.reset_complete()

    def _read_reg_abstractcmd(self, regno):
        command = self.fields.DMI_COMMAND__register_access.capture(0)
        command.cmdtype = RISCVAbstractCommands.AccessRegister
        command.aarsize = 2 # lowest 32-bits
        command.transfer = 1
        command.regno = regno
        cmderr = self._execute_command(int(command))
        if cmderr != 0:
            if cmderr == 2:
                raise RISCVAbstractCommandNotSupported
            raise RISCVAbstractCommandError("Error reading register 0x{:x} "
                        "via Abstract Command: {}".format(regno, cmderr_codes[cmderr]))
        return self.fields.DMI_DATA.read()

    def _execute_command(self, command):
        """
        Execute an abstract command.
        1. Waits for any currently executing abstract command to 
         complete
        2. Writes the command register
        3. Waits for the command to complete
        4. Checks and returns the cmderr field
        """
        while self.fields.DMI_ABSTRACTCS.busy == 1:
            pass
        self.fields.DMI_COMMAND = command
        while self.fields.DMI_ABSTRACTCS.busy == 1:
            pass
        cmderr = self.fields.DMI_ABSTRACTCS.cmderr.read()
        if cmderr != 0:
            # Clear the value
            self.fields.DMI_ABSTRACTCS.cmderr = 0x7
        return cmderr

    def _write_reg_abstractcmd(self, regno, value, postexec=False):
        """
        Use the abstract command mechanism to write a register, either
        CSR or GPR.  
        """
        command = self.fields.DMI_COMMAND__register_access.capture(0)
        command.cmdtype = RISCVAbstractCommands.AccessRegister
        command.aarsize = 2 # lowest 32-bits
        command.write_ = 1
        command.transfer = 1
        command.regno = regno
        command.postexec = int(postexec)
        self.fields.DMI_DATA = value
        cmderr = self._execute_command(int(command))
        if cmderr != 0:
            if cmderr == 2:
                raise RISCVAbstractCommandNotSupported
            raise RISCVAbstractCommandError("Error writing register 0x{:x} "
                "via Abstract Command: {}".format(regno, cmderr_codes[cmderr]))

    def _read_reg_progbuf(self, regno):
        """
        Following riscv-debug manual, section B.6.2, we access the given register
        indirectly via s0 (=x8), saving and restoring s0 as we go.
        """
        s0 = self._read_reg_abstractcmd(0x1008)
        try:
            # Set up the progbuf to read CSR regno into GPR 8
            self.fields.DMI_PROGBUF = RISCVInst.encode_csrr(regno, 8)
            self.fields.DMI_PROGBUF_1 = RISCVInst.encode_ebreak()
            # Execute command to do nothing but trigger progbuf execution 
            command = self.fields.DMI_COMMAND__register_access.capture(0)
            command.postexec = 1
            self._execute_command(int(command))
            # Now read the value in GPR 8
            value = self._read_reg_abstractcmd(0x1008)
        finally:
            self._write_reg_abstractcmd(0x1008, s0)
        return value

    def _write_reg_progbuf(self, regno, value):
        """
        Following riscv-debug manual, section B.6.2, we access the given register
        indirectly via s0 (=x8), saving and restoring s0 as we go.
        """
        s0 = self._read_reg_abstractcmd(0x1008)
        try:
            # Set up the progbuf to copy GPR 8 into CSR regno
            self.fields.DMI_PROGBUF = RISCVInst.encode_csrw(regno, 8)
            self.fields.DMI_PROGBUF_1 = RISCVInst.encode_ebreak()
            # Write the required value into GPR 8 and run the program buffer
            # afterwards.
            self._write_reg_abstractcmd(0x1008, value, postexec=True)
        finally:
            self._write_reg_abstractcmd(0x1008, s0)


    def read_registers(self, register_nums):
        """
        Read RISC-V CSRs identified by register numbers.

        Attempts to use the Access Register abstract command directly,
        and if that reports "not supported", uses a CSRRW instruction in
        the progbuf via the GPR s0 (saves and restores s0)
        """
        reg_values = []
        with self._apb_ap.preselected_for_bus_access():
            for regno in register_nums:
                value = None
                if regno not in self._regnos_abstract_access_not_supported:
                    try:
                        value = self._read_reg_abstractcmd(regno)
                    except RISCVAbstractCommandNotSupported:
                        self._regnos_abstract_access_not_supported.add(regno)
                if value is None:
                    value = self._read_reg_progbuf(regno)
                reg_values.append(value)

        return reg_values

    def write_registers(self, register_nums, values):
        """
        Write RISC-V CSRs identified by register numbers.

        Attempts to use the Access Register abstract command directly,
        and if that reports "not supported", uses a CSRRW instruction in
        the progbuf via the GPR s0 (saves and restores s0)
        """
        with self._apb_ap.preselected_for_bus_access():
            for regno, value in zip(register_nums, values):
                written = False
                if regno not in self._regnos_abstract_access_not_supported:
                    try:
                        self._write_reg_abstractcmd(regno, value)
                        written = True
                    except RISCVAbstractCommandNotSupported:
                        self._regnos_abstract_access_not_supported.add(regno)
                if not written:
                    self._write_reg_progbuf(regno, value)

    def write_run_ctrl(self, ctrl_data):
        """
        Drive the processor via the DMCONTROL register - allows halting and resuming.
        """
        if ctrl_data == "halt":
            self.fields.DMCONTROL.haltreq=1
            while self.fields.DMI_DMSTATUS.allhalted == 0:
                pass
            # Now the halt has taken effect we can clear the req
            self.fields.DMCONTROL.haltreq = 0
        elif ctrl_data == "resume":
            self.fields.DMCONTROL.resumereq=1
            t0 = timeout_clock()
            while self.fields.DMI_DMSTATUS.allresumeack == 0:
                # Other settings might mean that it can't run
                if timeout_clock() - t0 > 1:
                    wprint("DMCONTROL.allresumeack not set after 1sec - giving up")
                    break
            # Now the resume has taken effect, so clear the request
            self.fields.DMCONTROL.resumereq = 0
        
    def read_run_ctrl(self, ctrl_data):
        """
        Check the run status of the processor
        """
        if ctrl_data == "halted":
            return self.fields.DMI_DMSTATUS.allhalted == 1
        if ctrl_data == "running":
            return self.fields.DMI_DMSTATUS.allrunning == 1
        if ctrl_data == "status":
            return self.fields.DMI_DMSTATUS.capture()
