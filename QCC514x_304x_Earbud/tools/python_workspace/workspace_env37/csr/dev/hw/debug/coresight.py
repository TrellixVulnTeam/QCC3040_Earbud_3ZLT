############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
try:
    from collections.abc import Sequence
except ImportError:
    # Py 2
    from collections import Sequence
import contextlib
from csr.wheels.bitsandbobs import pack_unpack_data_le
import os

from csr.wheels import build_le, flatten_le, iprint, bytes_to_dwords, \
    dwords_to_bytes, gstrm, timeout_clock
from csr.dev.model.base_component import BaseComponent
from csr.dev.hw.core.simple_space import SimpleRegisterSpace
from csr.dev.hw.core.meta.i_core_info import ICoreInfo
from csr.dev.hw.core.meta.i_layout_info import ILayoutInfo
from csr.dev.hw.core.meta.io_struct_io_map_info import IoStructIOMapInfo
from csr.dev.hw.io.arch.cortex_m0_io_struct import c_reg, c_bits, c_enum
from csr.dev.hw.debug.swd import SWD
from csr.dev.hw.debug.utils import memory_write_helper, TransportAccessError
from csr.dev.model.interface import Table
from csr.dev.adaptor.text_adaptor import TextAdaptor

class CoresightError(RuntimeError):
   """
   General error relating to accessing the coresight block
   """
   
class CoresightVersionError(CoresightError):
   """
   Unknown/unsupported Coresight DP/AP version
   """
class CoresightStickyError(CoresightError):
    """
    Sticky errors are set in CTRL/STAT
    """

class CoresightDisabledAP(CoresightError):
    """
    The AP's CSW.DeviceEn flag is not set.
    """

class CoresightAPAccessError(CoresightError, TransportAccessError):
    """
    The AP access failed
    """
class CoresightAPDisabledError(CoresightError, TransportAccessError):
    """
    AP.DRW access failed because AP.CSW.DeviceEn is not set.
    """
class CoresightAPBusAccessError(CoresightError, TransportAccessError):
    """
    AP.CSW.DeviceEn is set but there was still an error accessing
    AP.DRW.
    """

class CoresightDPAccessError(CoresightError, TransportAccessError):
    """
    Write to DP.SELECT raised a transport error
    """


def create_reg(addr, readable=True, writeable=True, width=32,text=""):
    
    access_flags = {(True,False) : "R",
                    (False,True) : "W",
                    (True,True): "RW"}[(readable, writeable)]
    return c_reg(addr, readable, writeable, 0, access_flags, width, 
                 0, False, None, None, None, text, None,
                 None, None, True, False, None, None)

def create_bits(parent, lsb, msb):
    
    width = msb+1-lsb
    mask = ((1 << width)-1)<<lsb
    return c_bits(parent, lsb, msb, mask, width, parent.rw_flags, "")

class c_regarray(object):
   def __init__(self, addr, num_elements, element):
      self.addr = addr
      self.num_elements  = num_elements
      self.element = element



class ArmRegisterSpaceDataInfo(ILayoutInfo):
    
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

class ArmRegisterSpaceInfo(ICoreInfo):
    
    layout_info = ArmRegisterSpaceDataInfo()



class CoresightDebugPortInfoDPv1(ArmRegisterSpaceInfo):
    
    """
    Contains ARM Debug Port register map, as defined in debug_interface_v5_2_architecture_specification_IHI0031D.pdf,
    section B2.2.
    """
    c_reg = c_reg
    c_enum = c_enum

    ABORT = create_reg(0x0, readable=False)
    ABORT.DAPABORT = create_bits(ABORT,0,0)
    ABORT.STKCMPCLR =  create_bits(ABORT,1,1)
    ABORT.STKERRCLR =  create_bits(ABORT,2,2)
    ABORT.WDERRCLR =   create_bits(ABORT,3,3)
    ABORT.ORUNERRCLR = create_bits(ABORT,4,4)
    
    BASEPTR0 = create_reg(0x0, writeable=False, text="Base pointer 0. * Must set DPBANKSEL=2 *")
    BASEPTR0.VALID = create_bits(BASEPTR0, 0,0)
    BASEPTR0.PTR = create_bits(BASEPTR0, 12,31)
    BASEPTR0.__bank__ = ("SELECT",2)
    BASEPTR1 = create_reg(0x0, writeable=False, text="Base pointer 1. * Must set DPBANKSEL=3 *")
    BASEPTR1.PTR = create_bits(BASEPTR0, 0,31)
    BASEPTR1.__bank__ = ("SELECT",3)
    
    # TODO Include bitfield level access permissions
    CTRL_STAT = create_reg(0x4)
    CTRL_STAT.ORUNDETECT = create_bits(CTRL_STAT,0,0)
    CTRL_STAT.STICKYORUN = create_bits(CTRL_STAT,1,1)
    CTRL_STAT.TRNMODE = create_bits(CTRL_STAT,2,3)
    CTRL_STAT.STICKYCMP = create_bits(CTRL_STAT,4,4)
    CTRL_STAT.STICKYERR = create_bits(CTRL_STAT,5,5)
    CTRL_STAT.READOK = create_bits(CTRL_STAT,6,6)
    CTRL_STAT.WDATAERR = create_bits(CTRL_STAT,7,7)
    CTRL_STAT.MASKLANE = create_bits(CTRL_STAT,8,11)
    CTRL_STAT.TRNCNT = create_bits(CTRL_STAT,12,23)
    CTRL_STAT.ERRMODE = create_bits(CTRL_STAT,24,24)
    CTRL_STAT.CDBGRSTREQ = create_bits(CTRL_STAT,26,26)
    CTRL_STAT.CDBGRSTACK = create_bits(CTRL_STAT,27,27)
    CTRL_STAT.CDBGPWRUPREQ = create_bits(CTRL_STAT,28,28)
    CTRL_STAT.CDBGPWRUPACK = create_bits(CTRL_STAT,29,29)
    CTRL_STAT.CSYSPWRUPREQ = create_bits(CTRL_STAT,30,30)
    CTRL_STAT.CSYSPWRUPACK = create_bits(CTRL_STAT,31,31)

    DLCR = create_reg(0x4, text="* Must set DPBANKSEL=1 *")
    DLCR.TURNROUND = create_bits(DLCR, 8,9)
    DLCR.__bank__ = ("SELECT",1)

    DPIDR = create_reg(0x0, writeable=False)
    DPIDR.DESIGNER = create_bits(DPIDR,1,11)
    DPIDR.VERSION = create_bits(DPIDR,12,15)
    DPIDR.PARTNO = create_bits(DPIDR,20,27)
    DPIDR.REVISION = create_bits(DPIDR,28,31)
    
    RDBUFF = create_reg(0xc, writeable=False)

    RESEND = create_reg(0x8, writeable=False)
    
    SELECT = create_reg(0x8, readable=False)
    SELECT.DPBANKSEL = create_bits(SELECT, 0,3)
    SELECT.ADDR = create_bits(SELECT,4,31)

    @property
    def io_map_info(self):
        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.__class__, None,
                                                  self.layout_info)
        return self._io_map_info


class CoresightDebugPortInfoDPv2(CoresightDebugPortInfoDPv1):

    DLPIDR = create_reg(0x4, writeable=False, text="* Must set DPBANKSEL= 3 *")
    DLPIDR.PROTVSN = create_bits(DLPIDR, 0,3)
    DLPIDR.TINSTANCE = create_bits(DLPIDR, 28,31)
    DLPIDR.__bank__ = ("SELECT",3)

    EVENTSTAT = create_reg(0x4, writeable=False, text="* Must set DPBANKSEL=4 *")
    EVENTSTAT.EA = create_bits(EVENTSTAT, 0,0)
    EVENTSTAT.__bank__ = ("SELECT",4)
    
    TARGETID = create_reg(0x4, writeable=False, text=" *Must set DPBANKSEL=2 *")
    TARGETID.TDESIGNER = create_bits(TARGETID, 1,11)
    TARGETID.TPARTNO = create_bits(TARGETID,12,27)
    TARGETID.TREVISION = create_bits(TARGETID,28,31)
    TARGETID.__bank__ = ("SELECT",2)

    TARGETSEL = create_reg(0xC, readable=False, text=" *Must set DPBANKSEL=2 *")
    TARGETSEL.TDESIGNER = create_bits(TARGETSEL, 1,11)
    TARGETSEL.TPARTNO = create_bits(TARGETSEL,12,27)
    TARGETSEL.TREVISION = create_bits(TARGETSEL,28,31)
    TARGETSEL.__bank__ = ("SELECT",2)

class CoresightDebugPortInfoDPv3(CoresightDebugPortInfoDPv2):
    """
    Contains ARM Debug Port register map, as defined in debug_interface_v6_0_architecture_specification_IHI0074C.pdf,
    section B2.2.
    """
    DPIDR1 = create_reg(0x0, writeable=False, text="* Must set DPBANKSEL=1 *")
    DPIDR1.ASIZE = create_bits(DPIDR1,0,6)
    DPIDR1.ERRMODE = create_bits(DPIDR1,7,7)
    DPIDR1.__bank__ = ("SELECT",1)

    SELECT1 = create_reg(0x8, readable=False, text="* Must set DPBANKSEL=5 *")
    SELECT1.ADDR = create_bits(SELECT1,0,31)
    SELECT1.__bank__ = ("SELECT", 5)

_USE_OVERRUN_DETECTION = False # This completely breaks the JLink.

class CoresightDebugPort(SimpleRegisterSpace):
    
    def __init__(self, data_space, version, verbose=0):
        
        if version > 3:
           raise CoresightVersionError("Unknown/unsupported DP version {}".format(version))
        port_info_type = [CoresightDebugPortInfoDPv1, CoresightDebugPortInfoDPv1,
                          CoresightDebugPortInfoDPv2, CoresightDebugPortInfoDPv3][version]
        SimpleRegisterSpace.__init__(self, "CORESIGHT_DP", data=data_space, info=port_info_type())
        self.arch_version = version
        if _USE_OVERRUN_DETECTION:
            self.fields.CTRL_STAT.ORUNDETECT = 1
        self._verbose = verbose
        self._frozen_select = None
        self._reset_mode = False

    def get_chip_version(self):
        if self.arch_version > 1:
            targetid = self.fields.TARGETID.capture()
            instance = self.fields.DLPIDR.TINSTANCE.read()
            return (targetid.TDESIGNER.read(),
                    targetid.TPARTNO.read(),
                    targetid.TREVISION.read(),
                    instance)   

    def select_ap(self, ap_index, ap_reg_addr):
        
        # We only have a non-zero upper nibble in the ap_reg_addr if we have an
        # ADIv6.0 AP, so it has no effect otherwise.
        select_write = ap_index << 24 | (ap_reg_addr & 0xff0)
        if self._frozen_select != select_write:
            self.fields.SELECT = select_write
            self._last_select_write = select_write
            self.unfreeze_select()

    def freeze_select(self):
        self._frozen_select = self._last_select_write

    def unfreeze_select(self):
        self._frozen_select = None

    def select_is_frozen(self):
        return self._frozen_select is not None

    def check_for_sticky_errors(self):
        ctrl_stat = self.fields.CTRL_STAT.capture()
        if (ctrl_stat.STICKYERR == 1 or
            ctrl_stat.STICKYCMP == 1 or
            ctrl_stat.STICKYORUN == 1 or
            ctrl_stat.WDATAERR == 1):
            # Keep reading until the answer stabilises: on Trace32 on the RIOG RUMI5
            # we can get a transient spurious-looking reading after a write 
            ctrl_stat2 = None
            count_unstable = -1
            while ctrl_stat2 != ctrl_stat:
                ctrl_stat2 = ctrl_stat
                ctrl_stat = self.fields.CTRL_STAT.capture()
                count_unstable += 1
                if count_unstable > 10:
                    break
            if (ctrl_stat.STICKYERR == 1 or
                ctrl_stat.STICKYCMP == 1 or
                ctrl_stat.STICKYORUN == 1 or
                ctrl_stat.WDATAERR == 1):
                raise CoresightStickyError("Sticky errors set in CTRL_STAT: "
                                            "{}".format(repr(ctrl_stat)))

    def clear_sticky_errors(self):
        abort = self.fields.ABORT.capture(0)
        abort.WDERRCLR = 1
        abort.STKERRCLR = 1
        abort.STKCMPCLR = 1
        if _USE_OVERRUN_DETECTION:
            abort.ORUNERRCLR = 1
        abort.flush()

    def triggering_reset(self):
        self._reset_mode = True

    def reset_triggered(self):
        return self._reset_mode

    def reset_complete(self):
        self._reset_mode = False


class CoresightMEMAPInfoADIv5ptx(ArmRegisterSpaceInfo):
    
    c_reg = c_reg
    c_enum = c_enum
    c_regarray = c_regarray

    CSW = create_reg(0x00)
    CSW.Size = create_bits(CSW,0,2)
    CSW.AddrInc = create_bits(CSW,4,5)
    CSW.DeviceEn = create_bits(CSW,6,6)
    CSW.TrInProg = create_bits(CSW,7,7)
    CSW.Mode = create_bits(CSW,8,11)
    #CSW.Type = create_bits(CSW,12,14) # when memory tagging control is implemented
    #CSW.MTE = create_bits(CSW,15,15) # when memory tagging control is implemented
    CSW.Type = create_bits(CSW,12,15) # when memory tagging control is not implemented
    CSW.ERRNPASS = create_bits(CSW,16,16)
    CSW.ERRSTOP = create_bits(CSW,17,17)
    CSW.SDeviceEn = create_bits(CSW,23,23)
    CSW.Prot = create_bits(CSW,24,30)
    CSW.DbgSwEnable = create_bits(CSW,31,31)

    TAR = create_reg(0x04)
    
    DRW = create_reg(0x0c)
    

    BD0 = create_reg(0x10)
    BD1 = create_reg(0x14)
    BD2 = create_reg(0x18)
    BD3 = create_reg(0x1c)

    
    MBT = create_reg(0x20)
    
    
    CFG = create_reg(0xf4, writeable=False)
    CFG.BE = create_bits(CFG,0,0)
    CFG.LA = create_bits(CFG,1,1)
    CFG.LD = create_bits(CFG,2,2)
    CFG.DARSIZE = create_bits(CFG,4,7)
    CFG.ERR = create_bits(CFG,8,11)
    CFG.TARINC = create_bits(CFG,16,19)
    
    BASE = create_reg(0xf8, writeable=False)
    BASE.P = create_bits(BASE,0,0)
    BASE.Format = create_bits(BASE,1,1)
    BASE.BASEADDR = create_bits(BASE,12,31)
    
    IDR = create_reg(0xfc, writeable=False)
    IDR.TYPE = create_bits(IDR,0,3)
    IDR.VARIANT = create_bits(IDR,4,7)
    IDR.CLASS = create_bits(IDR,13,16)
    IDR.DESIGNER = create_bits(IDR,17,27)
    IDR.REVISION = create_bits(IDR,28,31)
    
    
    @property
    def io_map_info(self):
        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.__class__, None,
                                                  self.layout_info)
        return self._io_map_info
        

class CoresightMEMAPInfoADIv6pt0(ArmRegisterSpaceInfo):
    
    c_reg = c_reg
    c_enum = c_enum
    c_regarray = c_regarray
    
    AUTHSTATUS = create_reg(0xfb8, writeable=False)
    AUTHSTATUS.NSID = create_bits(AUTHSTATUS,0,1)
    AUTHSTATUS.NSNID = create_bits(AUTHSTATUS,2,3)
    AUTHSTATUS.SID = create_bits(AUTHSTATUS,4,5)
    AUTHSTATUS.SNID = create_bits(AUTHSTATUS,6,7)
    AUTHSTATUS.HID = create_bits(AUTHSTATUS,8,9)
    AUTHSTATUS.HNID = create_bits(AUTHSTATUS,10,11)
    
    BASE = create_reg(0xdf8, writeable=False)
    BASE.P = create_bits(BASE,0,0)
    BASE.Format = create_bits(BASE,1,1)
    BASE.BASEADDR = create_bits(BASE,12,31)
    
    BD0 = create_reg(0x10)
    BD1 = create_reg(0x14)
    BD2 = create_reg(0x18)
    BD3 = create_reg(0x1c)
    
    CFG = create_reg(0xdf4, writeable=False)
    CFG.BE = create_bits(CFG,0,0)
    CFG.LA = create_bits(CFG,1,1)
    CFG.LD = create_bits(CFG,2,2)
    CFG.DARSIZE = create_bits(CFG,4,7)
    CFG.ERR = create_bits(CFG,8,11)
    CFG.TARINC = create_bits(CFG,16,19)
    
    CFG1 = create_reg(0xde0, writeable=False)
    CFG1.TAG0GRAN = create_bits(CFG1,0,3)
    CFG1.TAG0SIZE = create_bits(CFG1,4,7)
    
    CIDR0 = create_reg(0xff0, writeable=False)
    CIDR0.PRMBL_0 = create_bits(CIDR0,0,7)
    CIDR1 = create_reg(0xff4, writeable=False)
    CIDR1.PRMBL_0 = create_bits(CIDR1,0,3)
    CIDR1.CLASS = create_bits(CIDR1,4,7)
    CIDR2 = create_reg(0xff8, writeable=False)
    CIDR2.PRMBL_2 = create_bits(CIDR2,0,7)
    CIDR3 = create_reg(0xffc, writeable=False)
    CIDR3.PRMBL_3 = create_bits(CIDR3,0,7)
    
    CLAIMSET = create_reg(0xfa0)
    CLAIMSET.ClaimTag0 = create_bits(CLAIMSET, 0,0)
    CLAIMSET.ClaimTag1 = create_bits(CLAIMSET, 1,1)
    CLAIMCLR = create_reg(0xfa4)
    CLAIMCLR.ClaimTag0 = create_bits(CLAIMCLR, 0,0)
    CLAIMCLR.ClaimTag1 = create_bits(CLAIMCLR, 1,1)
    
    CSW = create_reg(0xd00)
    CSW.Size = create_bits(CSW,0,2)
    CSW.AddrInc = create_bits(CSW,4,5)
    CSW.DeviceEn = create_bits(CSW,6,6)
    CSW.TrInProg = create_bits(CSW,7,7)
    CSW.Mode = create_bits(CSW,8,11)
    #CSW.Type = create_bits(CSW,12,14) # when memory tagging control is implemented
    #CSW.MTE = create_bits(CSW,15,15) # when memory tagging control is implemented
    CSW.Type = create_bits(CSW,12,15) # when memory tagging control is not implemented
    CSW.ERRNPASS = create_bits(CSW,16,16)
    CSW.ERRSTOP = create_bits(CSW,17,17)
    CSW.SDeviceEn = create_bits(CSW,23,23)
    CSW.Prot = create_bits(CSW,24,30)
    CSW.DbgSwEnable = create_bits(CSW,31,31)
    
    DAR = c_regarray(0, 256, create_reg(None))
    
    DEVARCH = create_reg(0xfbc, writeable=False)
    DEVARCH.ARCHID = create_bits(DEVARCH,0,15)
    DEVARCH.REVISION = create_bits(DEVARCH,16,19)
    DEVARCH.PRESENT = create_bits(DEVARCH,20,20)
    DEVARCH.ARCHITECT = create_bits(DEVARCH,21,31)
    
    # Missing out DEVID and DEVID1/2 which appear to have no content
    
    DEVTYPE = create_reg(0xfcc, writeable=False)
    DEVTYPE.MAJOR = create_bits(DEVTYPE,0,3)
    DEVTYPE.SUB = create_bits(DEVTYPE,4,7)
    
    DRW = create_reg(0xd0c)
    
    IDR = create_reg(0xdfc, writeable=False)
    IDR.TYPE = create_bits(IDR,0,3)
    IDR.VARIANT = create_bits(IDR,4,7)
    IDR.CLASS = create_bits(IDR,13,16)
    IDR.DESIGNER = create_bits(IDR,17,27)
    IDR.REVISION = create_bits(IDR,28,31)
    
    ITCTRL = create_reg(0xf00)
    ITCTRL.IME = create_bits(ITCTRL,0,0)
    
    LAR = create_reg(0xfb0, readable=False)
    LAR.SLI = create_bits(LAR,0,0)
    LAR.SLK = create_bits(LAR,1,1)
    LAR.nTT = create_bits(LAR,2,2)
    
    LSR = create_reg(0xfb4, writeable=False)
    LSR.KEY = create_bits(LSR,0,31)
    
    
    MBT = create_reg(0xd20)
    
    PIDR0 = create_reg(0xfd0, writeable=False)
    PIDR0.PART_0 = create_bits(PIDR0,0,7)
    PIDR1 = create_reg(0xfd4, writeable=False)
    PIDR1.PART_1 = create_bits(PIDR1,0,3)
    PIDR1.DES_0 = create_bits(PIDR1,4,7)
    PIDR2 = create_reg(0xfd8, writeable=False)
    PIDR2.DES_1 = create_bits(PIDR2,0,2)
    PIDR2.JEDEC = create_bits(PIDR2,3,3)
    PIDR2.REVISION = create_bits(PIDR2,4,7)
    PIDR3 = create_reg(0xfdc, writeable=False)
    PIDR3.CMOD = create_bits(PIDR3,0,3)
    PIDR3.REVAND = create_bits(PIDR3,4,7)
    PIDR4 = create_reg(0xfe0, writeable=False)
    PIDR4.DES_2 = create_bits(PIDR4,0,3)
    PIDR4.SIZE = create_bits(PIDR4,4,7)
    #PIDR5 = create_reg(0xfe4, writeable=False)
    #PIDR6 = create_reg(0xfe8, writeable=False)
    #PIDR7 = create_reg(0xfec, writeable=False)
    
    TAR = create_reg(0xd04)
    
    T0TR = create_reg(0xd30)
    T0TR.T0 = create_bits(T0TR,0,3)
    T0TR.T1 = create_bits(T0TR,4,7)
    T0TR.T2 = create_bits(T0TR,8,11)
    T0TR.T3 = create_bits(T0TR,12,15)
    T0TR.T4 = create_bits(T0TR,16,19)
    T0TR.T5 = create_bits(T0TR,20,23)
    T0TR.T6 = create_bits(T0TR,24,27)
    T0TR.T7 = create_bits(T0TR,28,31)

    TRR = create_reg(0xd24)
    TRR.ERR = create_bits(TRR,0,0)
    
    
    @property
    def io_map_info(self):
        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.__class__, None,
                                                  self.layout_info)
        return self._io_map_info

class CoresightMEMAPNotPresent(ValueError):
    """
    Indicates that the attempt to create a MEMAP interface detected that the
    given AP index has an IDR of 0, meaning it's not present 
    """

class APBData:
    """
    Simple class wrapping an APB-AP as a data space
    """
    def __init__(self, apb_ap, verbose=0):

        self._apb_ap = apb_ap
        self._verbose = verbose

    def __getitem__(self, addr_or_slice):

        addr = _get_address(addr_or_slice)
        if self._verbose > 1:
            iprint("APB:Reading 0x{:x}".format(addr))
        self._apb_ap.fields.TAR = addr
        try:
            return [self._apb_ap.fields.DRW.read()]
        except CoresightAPAccessError:
            if self._apb_ap.fields.CSW.DeviceEn == 0:
                raise CoresightAPDisabledError("Error reading through AP {}: "
                                "AP is disabled".format(self._apb_ap.data.index))
            raise CoresightAPBusAccessError("Unknown error reading through "
                                    "AP {}".format(self._apb_ap.data.index))


    def __setitem__(self, addr_or_slice, value):
        addr = _get_address(addr_or_slice)
        if self._verbose > 1:
            iprint("APB:Writing 0x{:x}".format(addr))
        self._apb_ap.fields.TAR = addr
        try:
            self._apb_ap.fields.DRW = value[0]
        except CoresightAPAccessError:
            if self._apb_ap.fields.CSW.DeviceEn == 0:
                raise CoresightAPDisabledError("Error writing through AP {}: "
                                            "AP is disabled".format(self._apb_ap.data.index))
            raise CoresightAPBusAccessError("Unknown error writing through "
                                                "AP {}".format(self._apb_ap.data.index))

    @property
    def ap(self):
        return self._apb_ap



class CoresightMEMAP(SimpleRegisterSpace):
        
    def __init__(self, data_space, version, name=None, autoinc=True, verbose=0, **extra_args):
        
        mem_ap_info_type = CoresightMEMAPInfoADIv5ptx if version < 3 else CoresightMEMAPInfoADIv6pt0
        SimpleRegisterSpace.__init__(self, "CORESIGHT_"+(name.upper() if name is not None else "MEM-AP"), 
                                     data=data_space, info=mem_ap_info_type())
        self.dp = self.data.dp
        self._verbose = verbose
        if self.fields.IDR == 0:
            raise CoresightMEMAPNotPresent
        
        self._default_autoinc = bool(autoinc)
        self.fields.CSW.AddrInc=int(self._default_autoinc)

        self._drw_addr = mem_ap_info_type.DRW.addr & 0xf
        self._drw_ind = (self._drw_addr) >> 2

        self._extra_args = extra_args

    @property
    def riscv(self):
        """
        A wrapper around this AP for implementing debug access using RISCV DM
        """
        try:
            self._riscv
        except AttributeError:
            # Only do the import here, because riscv.py imports this module.
            from csr.dev.hw.debug.riscv import RISCV_DM
            riscvdm_data = APBData(self, verbose=self._verbose)
            # TODO Check the actual version
            self._riscv = RISCV_DM(riscvdm_data, version=1, verbose=self._verbose, 
                                    **self._extra_args)
        return self._riscv

    @contextlib.contextmanager
    def ensure_autoinc(self, on_not_off):
        on_not_off = bool(on_not_off)
        if on_not_off == self._default_autoinc:
            # If the setting requested is the same as the
            # default then there's nothing to do
            yield
        else:
            # Temporarily turn autoinc on or off as requested,
            # ensuring we always return to the default state
            self.fields.CSR.AddrInc = int(on_not_off)
            yield
            self.fields.CSR.AddrInc = int(not on_not_off)

    @contextlib.contextmanager
    def preselected_for_bus_access(self):
        """
        Context manager that lets us avoid repeatedly writing the same value to DP.SELECT
        during blocks of accesses.  This is most useful when writing configuration
        sequences to slave debug blocks on an APB; for straight memory access sequences
        we already have a mechanism to skip rewriting SELECT.
        
        Note that the value we write into select is for the TAR/DRW register bank in this
        AP.  Any accesses to other AP banks while inside this context will force us out of 
        the preselection state.  But this is not the intended use case.

        Note also that this context manager is reentrant: entry from within an existing
        context has no effect (unless the previous context was exited prematurely as
        mentioned above.)
        """    
        if not self.dp.select_is_frozen():
            self.dp.select_ap(self.data.index, self._drw_addr)
            self.dp.freeze_select()
            yield
            self.dp.unfreeze_select()
        else:
            yield

    def read_drw_fast(self):
        """
        Execute a read on DRW without explicitly selecting the AP and register
        bank.  For use in read sequences.
        """
        try:
            return flatten_le(self.data.read_ap_reg(self._drw_ind), num_words=4, word_width=8)
        except CoresightAPAccessError:
            if self.fields.CSW.DeviceEn == 0:
                raise CoresightAPDisabledError("Error reading through AP {}: AP is disabled".format(self.data.index))
            raise CoresightAPBusAccessError("Unknown error reading DRW "
                                                "in AP {}".format(self.data.index))

    def write_drw_fast(self, value_bytes):
        """
        Execute a write on DRW without explicitly selecting the AP and register
        bank.  For use in write sequences.
        """
        try:
            return self.data.write_ap_reg(self._drw_ind, build_le(value_bytes, word_width=8))
        except CoresightAPAccessError:
            if self.fields.CSW.DeviceEn == 0:
                raise CoresightAPDisabledError("Error writing through AP {}: AP is disabled".format(self.data.index))
            raise CoresightAPBusAccessError("Unknown error writing through "
                                                "AP {}".format(self.data.index))

    @property
    def supports_subword_access(self):
        """
        Predicate indicating whether the implementation supports sub-word
        accesses or not.
        """
        # If subword accesses are not supported, CSW.Size is read-only and
        # always reads back 0b10.  So we just see if we can set it to something
        # else
        try:
            self._subword_access_supported
        except AttributeError:
            start_value = self.fields.CSW.Size.read()
            if start_value != 2:
                # Look no further
                self._subword_access_supported = True
            else:
                self.fields.CSW.Size = 0
                self._subword_access_supported = (self.fields.CSW.Size == 0)
                self.fields.CSW.Size = start_value
        return self._subword_access_supported

    def read_memory(self, start_addr, end_addr):
        """
        Read memory by setting self.fields.TAR and then reading ap.fields.DRW.
        This assumes auto-increment mode is active.
        
        Handles non-word aligned ranges, and knows about the limitations on
        auto-increment at 1KB boundaries.
        """
        result = []
        read_start = 4*(start_addr//4)
        read_end = 4*((end_addr+3)//4)
        if self.data.supports_block_access:
            result = pack_unpack_data_le(self.data.read_drw_block(read_start, (read_end-read_start)//4),
                                         32, 8)
        else:
            self.fields.TAR = last_tar_write = read_start
            with self.ensure_autoinc(True):
                for addr in range(read_start, read_end, 4):
                    if addr > last_tar_write and addr & 0x3ff == 0:
                        # Auto-increment doesn't work across 1K boundaries so we need to
                        # reset TAR here (see C2.2.2 in 
                        # debug_interface_v5_2_architecture_specification_IHI0031F.pdf)
                        self.fields.TAR = last_tar_write = addr
                    result += self.read_drw_fast()
        self.dp.check_for_sticky_errors()
        return result[start_addr-read_start:end_addr-read_start]

    # API for the generic memory write helper
    def set_access_size(self, size_expt):
        self.fields.CSW.Size = size_expt

    def write_out_simple(self, start_addr, byte_data, num_writes):
        self.fields.TAR = start_addr
        end_addr = start_addr + len(byte_data) 
        if start_addr%4 > 0:
            num_ldg_bytes = start_addr%4
            byte_data = [0]*num_ldg_bytes + byte_data
        if end_addr%4 > 0:
            num_trlg_bytes = 4-(end_addr%4)
            byte_data = byte_data + [0]*num_trlg_bytes
        assert(len(byte_data)==4)
        for _ in range(num_writes):
            self.write_drw_fast(byte_data)

    def write_out_aligned_block(self, write_start, byte_data):
        if self.data.supports_block_access:
            self.data.write_drw_block(write_start, pack_unpack_data_le(byte_data, 8, 32))
        else:
            self.fields.TAR = last_tar_write = write_start
            for offset in range(0, len(byte_data), 4):
                addr = write_start + offset
                if addr > last_tar_write and addr & 0x3ff == 0:
                    # Auto-increment doesn't work across 1K boundaries so we need to
                    # reset TAR here (see C2.2.2 in 
                    # debug_interface_v5_2_architecture_specification_IHI0031F.pdf)
                    self.fields.TAR = last_tar_write = addr
                self.write_drw_fast(byte_data[offset:offset+4])

    def read_word(self, addr):
        self.fields.TAR = addr
        return self.data.read_ap_reg(self._drw_ind)

    def write_memory(self, start_addr, byte_array):
        """
        Write memory by setting ap.fields.TAR and then writing the values into 
        self.fields.DRW one by one.

        Handles non-word aligned ranges, either with the hardware's sub-word
        access support if that is available, or simulating that with RMW. 
        Knows about the limitations on auto-increment at 1KB boundaries.
        """
        with self.preselected_for_bus_access():
            with self.ensure_autoinc(True):
                memory_write_helper(self, start_addr, byte_array)
        self.dp.check_for_sticky_errors()


    def poll_read(self, address, num_reads, busy_check=None):
        """
        Read the given address a fixed number of times. 
        Automatically disables autoinc mode if necessary.

        This is useful for accessing autoincrementing keyholes
        such as the RISC-V SBA registers 

        Note: this method returns a list of words, not a list of
        bytes.
        """
        with self.preselected_for_bus_access():
            if self.data.supports_block_access:
                result=self.data.read_drw_block(address, num_reads, addr_inc=0,
                                                busy_check=busy_check)
            else:
                def wait_until_not_busy(target_address):
                    if busy_check:
                        self.fields.TAR = busy_check.addr
                        while True:
                            if self.fields.DRW & busy_check.mask == busy_check.value:
                                self.fields.TAR = target_address
                                return 
                result = []
                if self._verbose > 1:
                    iprint("APB:Reading 0x{:x} {} times".format(address, num_reads))
                self.fields.TAR = address
                with self.ensure_autoinc(False):
                    for _ in range(num_reads):
                        wait_until_not_busy(address)
                        result.append(self.data.read_ap_reg(self._drw_ind))
        self.dp.check_for_sticky_errors()
        return result

    def poll_write(self, address, words, busy_check=None, bytes_per_write=None):
        """
        Write the given address a fixed number of times. 
        Automatically disables autoinc mode if necessary.

        This is useful for accessing autoincrementing keyholes
        such as the RISC-V SBA registers 
        """
        with self.preselected_for_bus_access():
            if self.data.supports_block_access:
                with self.ensure_autoinc(False):
                    self.data.write_drw_block(address, words, addr_inc=0, 
                                    busy_check=busy_check, bytes_per_write=bytes_per_write)
            else:
                def wait_until_not_busy(target_address):
                    if busy_check:
                        self.fields.TAR = busy_check.addr
                        while True:
                            if self.fields.DRW & busy_check.mask == busy_check.value:
                                self.fields.TAR = target_address
                                return

                self.fields.TAR = address
                if self._verbose > 1:
                    iprint("APB:Writing {} words to 0x{:x}".format(len(words), address))
                with self.ensure_autoinc(False):
                    for word in words:
                        wait_until_not_busy(address)
                        self.data.write_ap_reg(self._drw_ind, word)
        # We don't want any extra SWD accesses if we've just written a reset reg. 
        if not self.dp.reset_triggered():
            self.dp.check_for_sticky_errors()


def _get_address(address_or_slice):
    if isinstance(address_or_slice, slice):
        if address_or_slice.stop - address_or_slice.start != 1:
            raise ValueError("Can only do 4-byte accesses to Coresight hardware")
        return address_or_slice.start
    return address_or_slice


class CoresightMEMAPData(object):
    """
    Abstract class representing a connection to a Coresight MEM-AP.  Uses a
    CoresightDebugPort instance to implicitly set the SELECT register.
    """
    def __init__(self, index, dp, verbose=0):
        
        self.index = index
        self._verbose=verbose
        self.dp = dp
        self.cached_register_names = [] # something the register impl needs
        self.supports_block_access = False
        
        
    def _select(self, ap_reg_address):
        self.dp.select_ap(self.index, ap_reg_address)
        
    def read_ap_reg(self, reg_index):
        raise NotImplementedError

    def write_ap_reg(self, reg_index, value):
        raise NotImplementedError
    
    
    def __getitem__(self, address_or_slice):
        
        address = _get_address(address_or_slice)
        self._select(address)
        return [self.read_ap_reg((address&0xf) >> 2)]

    def __setitem__(self, address_or_slice, value):
        
        address = _get_address(address_or_slice)
        self._select(address)
        self.write_ap_reg((address&0xf) >> 2, value[0])


class CoresightDPData(object):
    """
    Abstract class representing a connection to a Coresight DP.
    """
    def _read_dp_reg(self, reg_index, retry=True):
        raise NotImplementedError

    def _write_dp_reg(self, reg_index, value, retry=True):
        raise NotImplementedError
    
    def line_reset(self):
        raise NotImplementedError
    
    def __getitem__(self, address_or_slice):
        
        address = _get_address(address_or_slice)
        return [self._read_dp_reg(address >> 2)]

    def __setitem__(self, address_or_slice, value):
        
        address = _get_address(address_or_slice)
        self._write_dp_reg(address >> 2, value[0])

    def _reassert_power(self):
        """
        Reassert DAP power requests if they are clear.

        Return True if the power requests were clear and are now
        asserted; False if they were not clear in the first place;
        raise RuntimeError if the acks are not seen a second after
        asserting the requests.
        """
        # Set retry flag to False to avoid infinite recursion if 
        # these DP accesses fail.
        try:
            ctrl_stat = self._read_dp_reg(1, retry=False)
            if ctrl_stat & 0xf0000000 != 0:
                return False
        except CoresightDPAccessError:
            ctrl_stat = 0
        # now set the request bits
        ctrl_stat |= ((1<<30) | (1<<28))
        self._write_dp_reg(1, ctrl_stat, retry=False)
        # Now wait for the ack bits to be set
        t_start = timeout_clock()
        while ctrl_stat & (1<<31) == 0 or ctrl_stat & (1<<29) == 0:
            ctrl_stat = self._read_dp_reg(1, retry=False)
            if timeout_clock() > t_start + 1:
                raise RuntimeError("Waited more than 1s for DP power requests to be acked!")
        return True


class CoresightRawDriverDPData(CoresightDPData):
    """
    JTAG-based DP data access
    """
    def __init__(self, dp_driver, verbose=0):
        self._dp_driver = dp_driver
        self._verbose = verbose

    def _read_dp_reg(self, reg_index, retry=False):
        if self._verbose > 2:
            iprint("DP read reg_index {}".format(reg_index))
        return self._dp_driver.dp_read(reg_index)


    def _write_dp_reg(self, reg_index, value, retry=True):
        if self._verbose > 2:
            iprint("DP write reg_index {}".format(reg_index))
        return self._dp_driver.dp_write(reg_index, value)


class CoresightRawDriverMEMAPData(CoresightMEMAPData):
    """
    JTAG-based MEM-AP data access
    """
    def __init__(self, index, dp, ap_driver, verbose=0):
        CoresightMEMAPData.__init__(self, index, dp, verbose=verbose)
        self._ap_driver = ap_driver

    def read_ap_reg(self, reg_index):
        """
        Return the UINT32 value of the given register index in this AP.
        """
        if self._verbose > 2:
            iprint("AP read reg_index {}".format(reg_index))
        return self._ap_driver.ap_read(reg_index)

    def write_ap_reg(self, reg_index, value):
        if self._verbose > 2:
            iprint("AP write reg_index {}, value = 0x{:x}".format(reg_index, value))
        self._ap_driver.ap_write(reg_index, value)


def get_coresight_debug_port_version(dp_data, name=None):
    
    # Read the DPIDR register to figure out what architecture we're dealing with
    v5_space = SimpleRegisterSpace(name or "CORESIGHT_DP", data=dp_data,
                                   info=CoresightDebugPortInfoDPv1())
    dpidr = v5_space.fields.DPIDR.capture() 
    if dpidr == 0xffffffff:
        return 0

    return int(dpidr.VERSION) or 2
    
def create_coresight_mem_access_port(ap_data, dp, name=None, autoinc=True, verbose=0, **extra_args):
    """
    Create a new MEM-AP object, using the given raw data access object (usually
    a transport-specific subclass of CoresigthMEMAPData), the corresponding
    DP interface, and optionally specifying whether the AP should set up 
    autoincrement mode by default or not (defaults to autoinc on, although the
    main client of this function, get_connection() always sets it explicitly.)
    """
    try:
        ap = CoresightMEMAP(ap_data, dp.arch_version, name=name,
                            autoinc=autoinc, verbose=verbose, **extra_args)
    except CoresightMEMAPNotPresent:
        return None
    else:
        csw = ap.fields.CSW.capture()
        if csw.Size !=2: # 32-bit accesses
            csw.Size = 2
            csw.flush()
        return ap





class CoresightTransport(object):

    def __init__(self, raw_driver=None, verbose=0, extra_ap_args=None):

        self._verbose = verbose
        self._raw_driver = raw_driver
        self._extra_ap_args = extra_ap_args or {}
        
        self._aps = {}
        
    def get_chip_version(self):
        # Don't attempt a target select - see what we can see without one
        return self.create_dp().get_chip_version()
    
    def get_connection(self, ap_index, autoinc=None, peripheral=None, 
                       allow_disabled=False):
        """
        Create the requested connection if required, and return it.
        This involves creating or retrieving the AP instance that has been requested.  

        Note that "autoinc" is only used when the connection is first
        made. In subsequent calls it is ignored, even if it differs from the
        original call. If autoinc is not specified, its value is determined by whether the 
        AP is AHB or not (AHB=>autoincrement is on by default, otherwise off). 
        """
        
        dp = self.get_dp()
        
        try:
            index = ap_index.ap_number
            ap_type = ap_index.ap_type
        except AttributeError:
            index = ap_index
            ap_type = "probe"

        if autoinc is None:
            # By default, make APs that are looking at AHB buses auto-increment
            # and others not.  If we're going to wrap the AP to perform RISCV
            # accesses or similar then we also don't want autoinc (though this will 
            # probably always be with a non-AHB bus anyway).
            autoinc = (ap_type == "AHB") and peripheral is None
        try:
            ap = self._aps[index]
        except KeyError:
            if self._raw_driver is not None:
                ap_raw_data = CoresightRawDriverMEMAPData(index, dp, self._raw_driver, verbose=self._verbose)
            else:
                ap_raw_data = self.MEMAP_DATA_TYPE(index, dp, self._dll, verbose=self._verbose)
            ap = create_coresight_mem_access_port(ap_raw_data,
                                       dp=dp,
                                       name="MEM-AP[{}]".format(index),
                                       autoinc=autoinc,
                                       verbose=self._verbose, 
                                       **self._extra_ap_args.get(index, {}))
            if ap is None:
                return None
            if not allow_disabled and ap.fields.CSW.DeviceEn == 0:
                raise CoresightDisabledAP("Error getting connection to AP {}: requested "
                                            "AP is disabled".format(index))
            self._aps[index] = ap
        if peripheral == "riscvdm":
            return ap.riscv
        elif peripheral is not None:
            raise ValueError("Unknown APB peripheral '{}'".format(peripheral))
        return ap
    
    def memory_read(self, conn_id, start_addr, end_addr, peripheral=None):
        """
        Read the given range of memory addresses on the given AP
        """
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't read memory via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            return ap.read_memory(start_addr, end_addr)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            return ap.read_memory(start_addr, end_addr)
    
    def memory_write(self, conn_id, start_addr, byte_array, peripheral=None):
        """
        Read the given range of memory addresses on the given AP
        """
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't write memory via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            ap.write_memory(start_addr, byte_array)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            ap.write_memory(start_addr, byte_array)

    def register_read(self, conn_id, register_nums, peripheral=None):
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't read core registers via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            return ap.read_registers(register_nums)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            return ap.read_registers(register_nums)

    def register_write(self, conn_id, register_nums, register_values, peripheral=None):
        """
        Read the given range of memory addresses on the given AP
        """
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't write memory via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            ap.write_registers(register_nums, register_values)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            ap.write_registers(register_nums, register_values)

    def run_ctrl_write(self, conn_id, ctrl_data, peripheral=None):
        """
        Make changes to the run control state of the CPU associated with the given AP (halt/resume etc)
        """
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't write run control registers via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            ap.write_run_ctrl(ctrl_data)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            ap.write_run_ctrl(ctrl_data)

    def run_ctrl_read(self, conn_id, ctrl_data, peripheral=None):
        """
        Report the run control state of the CPU associated with the given AP (halted/running etc)
        """
        ap = self.get_connection(conn_id, peripheral=peripheral)
        if ap is None:
            raise CoresightMEMAPNotPresent("Can't read run control registers via AP {}: "
                        "there appears to be no such AP".format(conn_id.ap_number))
        try:
            return ap.read_run_ctrl(ctrl_data)
        except CoresightStickyError:
            ap.dp.clear_sticky_errors()
            return ap.read_run_ctrl(ctrl_data)


    def get_dp(self):
        """
        Return the DP instance corresponding to the given targsetsel, if any.
        Create it if necessary.
        """
        try:
            return self._dp
        except AttributeError:
            new_dp = self.create_dp()
            self._dp = new_dp
            return new_dp


    def scan_aps(self,extra_details=False):
        """
        Analyse the APs present in a DP to see what is present, what is enabled, what type of 
        bus they are connected to, etc.
        """
        dp = self.get_dp()

        ap_data = []

        def get_ap_type(ap_type, ap_class, simplified=False):
            if ap_type == 0:
                return "JTAG" if ap_class == 0 else "COM-AP"

            if simplified:
                return {1 : "AHB",
                        2 : "APB",
                        4 : "AXI",
                        5 : "AHB",
                        6 : "APB"}[ap_type]
            return {1 : "AHB3",
                    2 : "APB2/3",
                    4 : "AXI3/4",
                    5 : "AHB5",
                    6 : "APB4"}[ap_type]

        access_size_names = {0 : "byte", 1 : "halfword", 2 : "word", 3 : "doubleword", 4 : "128 bit", 5: "256 bit"}

        for index in range(256):
            ap_raw_data = self.MEMAP_DATA_TYPE(index, dp, self._dll)
            ap = create_coresight_mem_access_port(ap_raw_data,
                                                  dp=dp,
                                                  name="MEM-AP[{}]".format(index))
            if ap is None:
                break
            
            # Gather data about the AP.
            IDR = ap.fields.IDR.capture()
            CSW = ap.fields.CSW.capture()
            ap_details = dict(enabled=CSW.DeviceEn==1,
                            ap_class="MEM-AP" if IDR.CLASS == 8 else ("COM-AP" if IDR.CLASS == 1 else None),
                            bus_type=get_ap_type(IDR.TYPE, IDR.CLASS, simplified=not extra_details))
            extra_ap_details = dict(
                            revision=IDR.REVISION,
                            designer="Arm" if IDR.DESIGNER == 0x23b else "Unknown",
                            variant=IDR.VARIANT,
                            access_size=access_size_names[CSW.Size],
                            tr_in_prog=CSW.TrInProg==1)
            if extra_details:
                ap_details.update(extra_ap_details)
            ap_data.append(ap_details)

        return ap_data

    def ap_scan(self, report=False, detailed=False):
        """
        Present the data from scan_aps as a table.
        """

        ap_data = self.scan_aps(extra_details=detailed)

        if detailed:
            ap_table = Table(["Index", "Class and Bus Type", "Variant", "Designer", "Revision", 
                                "Access size", "Enabled?", "Xfer in progress?"])
        else:
            ap_table = Table(["Index", "Class and Bus Type", "Enabled?"])

        for i, ap_details in enumerate(ap_data):

            if ap_details["ap_class"] is None:
                class_and_bus_type = ap_details["bus_type"]
            else:
                class_and_bus_type = "{}: {}".format(ap_details["ap_class"], ap_details["bus_type"]) 
            if detailed:
                ap_table.add_row([i, class_and_bus_type, ap_details["variant"], 
                                ap_details["designer"], 
                                ap_details["revision"], 
                                ap_details["access_size"], 
                                "Y" if ap_details["enabled"] else "N", 
                                "Y" if ap_details["tr_in_prog"] else "N"])
            else:
                ap_table.add_row([i, class_and_bus_type, "Y" if ap_details["enabled"] else "N"])
        if report:
            return ap_table
        TextAdaptor(ap_table, gstrm.iout)


class CoresightJTAGDPTooManyWaitAcks(RuntimeError):
    pass

def from_bits(bit_list):
    return build_le(bit_list, word_width=1)

def to_bits(value, num_bits):
    return flatten_le(value, word_width=1, num_words=num_bits)


class CoresightJTAGDPDriver(object):
    """
    Implements a low-level interface to the JTAG-DP, i.e. drives the DPACC and 
    APACC commands
    """
    ABORT = 0x8
    DPACC = 0xa
    APACC = 0xb
    IDCODE = 0xe
    BYPASS = 0xf

    READ_RDBUFF = [1, 1, 1] + [0]*32

    OK_FAULT = 0x2
    WAIT = 0x1

    WAIT_THRESHOLD = 3

    def __init__(self, jtag_controller):

        self._jtag = jtag_controller

        self.dp_write(0x1, 0x50000000) # assert power request bits

    def _set_ir(self, ap_not_dp):
        if ap_not_dp and self._jtag.ir_state != self.APACC:
            # Need to switch to the APACC instruction
            self._jtag.ir_scan(self.APACC)
        elif not ap_not_dp and self._jtag.ir_state != self.DPACC:
            self._jtag.ir_scan(self.DPACC)


    def _process_result(self, result_bits):
        ack, data = from_bits(result_bits[0:3]), from_bits(result_bits[3:])
        if ack == self.WAIT:
            return None # WAIT ack
        if ack != self.OK_FAULT:
            raise RuntimeError("Unrecognised ACK value {}".format(ack))
        return data 

    def _apdp_read(self, ap_not_dp, reg_index_list):
        """
        Implements a pipelined sequence of N APACC or DPACC reads, which involves
        N+1 scans.  Handles WAIT ACKs by retrying up to 3 times.
        """
        self._set_ir(ap_not_dp)

        result_words = []
        first_scan = True
        for reg_index in reg_index_list:
            reg_index_bits = [reg_index&1, (reg_index>>1)&1]
            prev_data = None
            waits_seen = -1
            while prev_data is None:
                waits_seen += 1
                if waits_seen > self.WAIT_THRESHOLD:
                    raise CoresightJTAGDPTooManyWaitAcks(waits_seen)
                # Now scan in the 3 bits of header + reg_index and 32 empty bits
                prev_result = self._jtag.dr_scan_bits([1] + reg_index_bits + [0]*32)
                prev_data = self._process_result(prev_result)
            if not first_scan:
                result_words.append(prev_data)
            first_scan = False

        # Now set up a dummy read of RDBUFF to give us a side-effect free
        # scanning out of the last read 
        self._set_ir(False)
        prev_data = None
        waits_seen = -1
        while prev_data is None:
            prev_result = self._jtag.dr_scan_bits(self.READ_RDBUFF)
            prev_data = self._process_result(prev_result)
        result_words.append(prev_data)
        return result_words

    def _apdp_write(self, ap_not_dp, reg_index_list, value_list):

        self._set_ir(ap_not_dp)
        for reg_index, value in zip(reg_index_list, value_list):
            reg_index_bits = [reg_index&1,(reg_index>>1)&1]
            prev_data = None
            waits_seen = -1
            while prev_data is None:
                waits_seen += 1
                if waits_seen > self.WAIT_THRESHOLD:
                    raise CoresightJTAGDPTooManyWaitAcks(waits_seen)
                prev_result = self._jtag.dr_scan_bits([0]+reg_index_bits+to_bits(value, 32))
                prev_data = self._process_result(prev_result)



    def ap_read(self, reg_index):
        return self._apdp_read(True, [reg_index])[0]

    def dp_read(self, reg_index):
        return self._apdp_read(False, [reg_index])[0]

    def ap_write(self, reg_index, value):
        self._apdp_write(True, [reg_index], [value])

    def dp_write(self, reg_index, value):
        self._apdp_write(False, [reg_index], [value])


class CoresightSWDDPDriver(object):

    def __init__(self, swd_driver):

        self._swd = SWD(swd_driver)

    def ap_read(self, reg_index):
        return self._swd.read_apdp_register(reg_index<<2, ap_not_dp=True)

    def dp_read(self, reg_index):
        return self._swd.read_apdp_register(reg_index<<2, ap_not_dp=False)

    def ap_write(self, reg_index, value):
        self._swd.write_apdp_register(reg_index<<2, value, ap_not_dp=True)

    def dp_write(self, reg_index, value):
        self._swd.write_apdp_register(reg_index<<2, value, ap_not_dp=False)
