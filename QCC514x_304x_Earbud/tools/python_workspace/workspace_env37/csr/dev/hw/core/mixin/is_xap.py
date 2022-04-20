############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressMap, AddressConnection,\
    AddressSlavePort, AddressMasterPort, AddressSpace
from csr.wheels.bitsandbobs import NameSpace, PureVirtualError
from csr.dev.model import interface
from csr.dev.hw.subsystem.host_subsystem import CoreHostBlockViewManager
from csr.wheels.bitsandbobs import build_be, display_hex
from csr.dev.adaptor.text_adaptor import TextAdaptor
from .has_reg_based_breakpoints import HasRegBasedBreakpoints
import sys

class XAPRegistersCoreMixin(object):
    """
    Mixin for a XAP base core that provides access to the processor registers
    via the 
    """
    
    @property
    @display_hex
    def pc(self):
        #This slightly ugly looking combination is required because
        #we need to read PROC_PC_SNOOP.start_addr first, to latch the value in
        #PROC_PC_SNOOP.start_addr+1
        addr = self.fields.PROC_PC_SNOOP.start_addr
        try:
            return build_be([self.data[addr], self.data[addr+1]])
        except AddressSpace.NoAccess:
            # Some coredumps (e.g. QCC514X_QCC304X 1.2) omit PROC_PC_SNOOP, so we try
            # using PCH/PCL, which is safe for a coredump because it would have
            # paused the processor before dumping.
            return build_be([self.fields.XAP_PCH,self.fields.XAP_PCL])

    @pc.setter
    def pc(self, x):
        self.fields["XAP_PCH"] = (x >> 16) & 0x00ff
        self.fields["XAP_PCL"] = x & 0xffff

    @property
    @display_hex
    def xap_ux(self):
        return build_be([self.xap_uxh, self.xap_uxl])

    ux = xap_ux

    @property
    @display_hex
    def xap_uxl(self):
        return self.fields["XAP_UXL"]

    uxl = xap_uxl

    @property
    @display_hex
    def xap_uxh(self):
        return self.fields["XAP_UXH"]

    uxh = xap_uxh

    @property
    @display_hex
    def xap_uy(self):
        return self.fields["XAP_UY"]

    uy = xap_uy

    @property
    @display_hex
    def xap_al(self):
        return self.fields["XAP_AL"]

    al = xap_al

    @property
    @display_hex
    def xap_ah(self):
        return self.fields["XAP_AH"]

    ah = xap_ah

    @property
    @display_hex
    def xap_ix(self):
        return build_be([self.fields["XAP_IXH"], self.fields["XAP_IXL"]])

    ix = xap_ix

    @property
    @display_hex
    def xap_ixh(self):
        return self.fields["XAP_IXH"]

    ixh = xap_ixh

    @property
    @display_hex
    def xap_ixl(self):
        return self.fields["XAP_IXL"]

    ixl = xap_ixl

    @property
    @display_hex
    def xap_iy(self):
        return self.fields["XAP_IY"]

    iy = xap_iy


class IsXAP (XAPRegistersCoreMixin, HasRegBasedBreakpoints):
    """\
    Mixin for CPU Cores with XAP CPU

    Potential extension:: Rename IsXapCore

    Implementations and extensions common to all known XAP cores.
    Includes those in dual-core WLAN subsystem.
    """
    def __init__(self):
        """\
        NB 2 phase initialisation required. Call populate once object is fully
        constructed.
        """
        self.brk_reset_cache()

    def populate(self, access_cache_type):
        """\
        Populate this model.

        Can not be done during construction as depends on virtual
        specialisation methods.
        """
        # Create sub-space for mixin state to avoid name clashes with mixee.
        #
        self._is_xap = NameSpace()
        self._is_xap.components = NameSpace()
        comps = self._is_xap.components # shorthand

        # Memory model for access routing purposes
        #
        # This is OO equivalent of "scripted memory map" (and some)
        #
        # Potential extension:: looks like some of the components here should be promoted
        # to subsystem level - consider multi xap core subsystems
        #
        # Standard XAP Core/Subsystem Address spaces.
        #
        comps.proc_data_map = AddressMap("PROC_DATA", access_cache_type)
        comps.common_data_map = AddressMap("COMMON_DATA", access_cache_type)
        comps.prog_map = AddressMap("PROG", access_cache_type,
                                    length=1<<24, word_bits=16)
        comps.io_log_space = AddressSlavePort("IO_LOG", access_cache_type)

        hif_subsystem_view = self._hif_subsystem_view
        if hif_subsystem_view:
            comps.hif_block_view = \
                CoreHostBlockViewManager("PROC_HIF_VIEW", self,
                                         "MMU_HOST_SUBSYSTEM_BLOCK_SELECT",
                                         hif_subsystem_view,
                                         access_cache_type)

        # Components needed to populate the memory maps.
        #
        create_gwin = self._create_generic_window # virtual
        comps.proc_gw1 = create_gwin("PROC_GW1", access_cache_type)
        comps.proc_gw2 = create_gwin("PROC_GW2", access_cache_type)
        comps.proc_gw3 = create_gwin("PROC_GW3", access_cache_type)
        # SPI GW1 disabled out as GenericWindow class needs further tweaking
        # and testing to deal with shadows (e.g. addresses 0 - 0x80 _modulo_
        # page size via GW1. The inability of GW1 to serve absolute 0 - 0x80 is
        # already modelled in a generic way)
        #
        #comps.spi_gw1 = create_gwin("SPI_GW1", access_cache_type)
        comps.spi_gw2 = create_gwin("SPI_GW2", access_cache_type)
        comps.spi_gw3 = create_gwin("SPI_GW3", access_cache_type)

        # Wire up gw output ports
        #
        comps.cons = []
        cons = comps.cons
        for gw in (comps.proc_gw1,
                   comps.proc_gw2,
                   comps.proc_gw3,
                   # SPI GW1 disabled
                   #comps.spi_gw1,
                   comps.spi_gw2,
                   comps.spi_gw3, ):
            cons.append(AddressConnection(gw.io_log_port, comps.io_log_space))
            cons.append(AddressConnection(gw.prog_port, comps.prog_map.port))
            # ask subtype to make any non-standard connections
            cons += self._create_extra_gw_connections(gw)

        # Populate the main PROC & SPI memory maps for this core.
        #
        # SPI sees the same as PROC - but different.  See _create_spi_data_map
        #
        # Potential extension:: Steal these magic numbers from digits

        #
        comps.proc_data_map.add_mapping(
            0x0000, 0x0080, comps.common_data_map.port),
        comps.proc_data_map.add_mapping(
            0x0080, 0x2000, comps.proc_gw1.in_port, 0x0080, group="const space")
        comps.proc_data_map.add_mappings(
            (0x2000, 0x4000, comps.proc_gw2.in_port),
            (0x4000, 0x6000, comps.common_data_map.port,0x4000))
            # Potential extension:: MMU r/w here
        comps.proc_data_map.add_mapping(
            0x8000, 0xf000, comps.common_data_map.port, 0x8000, group="local ram")
        comps.proc_data_map.add_mappings(  
            (0xf000, 0xf700, comps.common_data_map.port, 0xf000),
            # 0xf700:0xf800 - HIF blocks window will be added below
            (0xf800, 0x10000, comps.common_data_map.port, 0xf800)
        )

        # Window for HIF blocks
        try:
            comps.proc_data_map.add_mapping(
                0xf700, 0xf800, comps.hif_block_view.port)
        except AttributeError:
            # host subsystem is not present
            pass

        #We have to remember this so that SPI map construction can refer to
        #it whenever it is called upon
        self.access_cache_type = access_cache_type


    # BaseCore Compliance

    @property
    def data(self):
        return self._is_xap.components.proc_data_map.port

    @property
    def program_space(self):
        return self._is_xap.components.prog_map.port

    @property
    def register_space(self):
        return self._is_xap.components.proc_data_map.port

    def map_lpc_slave_regs_into_prog_space(self):
        # Potential extension:: Encapsulate this logic in explcit NVRAM mux model and merge
        # with the explicit memory access model

        # Set nvmem to map lpc to start of prog space.
        #
        # Potential extension:: lookup field values
        NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_LPC = 0x0001
        self.fields["NV_MEM_ADDR_MAP_CFG_ORDER"] = NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_LPC

        # Set LPC master offset to 0x20000 to pull LPC Slave regs
        # to prog address 0.
        #
        from ...lpc import LPC
        self.lpc_master.offset = LPC.ProgWin.OFFSET

    # Extensions

    @property
    def registers(self):
        """\
        Core-relative RegisterField dictionary.
        An alias for field_refs.
        """
        return self.field_refs
        #
    @property
    def proc_gw3(self):
        """\
        Processor Generic Window 3
        """
        return self._is_xap.components.proc_gw3

    @property
    def spi_gw3(self):
        """\
        SPI Generic Window 3
        """
        return self._is_xap.components.spi_gw3


    #Extension: register reporting

    def brk_reset_cache(self):
        self._brk_addr = [self.brk_default_address_reg]*self.num_brk_regs

    def pause(self):
        ''' Pause the processor by writing to DBG_EMU_CMD '''
        self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_RUN_B = 1

    def run(self):
        ''' Set the processor running by writing to DBG_EMU_CMD '''
        # Are any breakpoints set?
        debug_mode = False
        for i in range(self.num_brk_regs):
            if self.brk_is_enabled(i):
                debug_mode = True
                break
        if debug_mode:
            # To run with breakpoints set we have to have the step bit set
            self.fields["DBG_EMU_CMD"] = 2 # Pause without step bit set
            self.fields["DBG_EMU_CMD"] = 1 # Run with step bit set
        else:
            # To run with no breakpoints set we need to have the step bit unset
            self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_STEP = 0
        self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_RUN_B = 0

    def step(self):
        ''' Single step the processor by writing to DBG_EMU_CMD '''
        self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_STEP = 1
        self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_RUN_B = 1
        self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_STEP = 0

    def reset_pc(self, start_addr=0):
        self.pc = start_addr
        self.fields["PROC_LOCAL"] = 0

    def clear_um_flag(self):
        self.fields["XAP_FLAGS"] = (self.fields["XAP_FLAGS"] &
                                (~self.fw.env.abs["XAP_FLAGS_USER_MODE_MASK"]))

    def restart(self, start_addr=0, run=True):
        self.pause()
        self.reset_pc(start_addr)
        self.clear_um_flag()
        if (run):
            self.run()

    @property
    @display_hex
    def stack_check_octet(self):
        '''
        If the bit 'PROC_STACK_SIGNATURE_EN' (3) is set in PROC_ENHANCED_MODE,
        then the stack check hardware is enabled, so the check octet is 0x42.
        Otherwise, that octet will contain 0.
        '''
        if self.fields["PROC_ENHANCED_MODE"] & (1 << 3):
            return 0x42
        return 0

    def pc_report(self):
        """
        Show the current value of the program counter (XAP_PCL | XAP_PCH << 16)
        """
        return interface.Code("0x%08x" % (self.pc))

    # Breakpoint stuff

    @property
    def num_brk_regs(self):
        return 4

    def _brk_enable(self, regid):
        """
        Do nothing: breakpoints are enabled iff they are set
        """
        pass

    def _brk_disable(self, regid):
        """
        Do nothing: breakpoints are enabled iff they are set
        """
        pass

    def brk_is_enabled(self, regid):
        """
        Is a non-default address set for this breakpoint?
        """
        return self.brk_address(regid) != self.brk_default_address_reg

    @display_hex
    def brk_address(self, regid):
        return self._brk_addr[regid]

    @property
    def is_running(self):
        return self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_RUN_B == 0

    def _brk_set_reg(self, regid, address, overwrite=True, enable=True):
        """
        Set the given breakpoint to the given address if it's currently unused
        or overwrite is set
        """
        if overwrite or not self.brk_is_enabled(regid):
            is_paused = self.fields.DBG_EMU_CMD.DBG_EMU_CMD_XAP_RUN_B == 1
            self.fields["DBG_EMU_CMD"] = 0x2
            self.fields["XAP_BRK_INDEX"] = regid
            self.fields["XAP_BRK_REGL"] = address & 0xffff
            self.fields["XAP_BRK_REGH"] = address >> 16
            self._brk_addr[regid] = address
            if not is_paused:
                self.fields["DBG_EMU_CMD"] = 0x1
            return True

        return False

    @property
    def brk_default_address_reg(self):
        """
        The breakpoint address registers seem to be reset to this value on
        device.reset()
        """
        return 0xffffff

    def proc_state(self, report=False):
        if hasattr(self.fw, "env"):
            ret = interface.Code("PC: 0x%06x  >>   %s   (al=0x%04x, ah=0x%04x, "
                              "x=0x%04x, y=0x%04x)" %
                              (self.pc,
                               self.fw.env.build_info.asm_listing.instructions[self.pc],
                               self.xap_al, self.xap_ah, self.xap_ux, self.xap_uy))
        else:
            ret = interface.Code("PC: 0x%06x  >>  (al=0x%04x, ah=0x%04x, "
                              "x=0x%04x, y=0x%04x)" %
                              (self.pc,
                               self.xap_al, self.xap_ah, self.xap_ux, self.xap_uy))

        if not report:
            TextAdaptor(ret, gstrm.iout)

    def step_state(self, report=False):
        self.step()
        return self.proc_state(report=report)

    # Protected / Overrideable

    def _create_extra_gw_connections(self, generic_window):
        """\
        Create any non-standard connections to generic-window.
        E.g. WLAN will connect one GW port to SHARED memory.
        CURATOR will connect one GW port to Remote TRB space.
        """
        return tuple()




    # Protected / Required

    def _create_generic_window(self, name, access_cache_type):
        """\
        Construct GenericWindow model.
        Varies with core subtype.
        """
        raise PureVirtualError(self)

    def _create_spi_data_map(self):
        """
        The Core that mixes this class in (e.g. CuratorCore) can present this
        to give the subsystem its SPI memory access
        """

        spi_data_map = AddressMap("SPI_DATA", self.access_cache_type)

        comps = self._is_xap.components
        spi_data_map.add_mappings(
            (0x0000, 0x0080, comps.common_data_map.port),
            # SPI GW1 disabled
            #(0x0080, 0x2000, comps.spi_gw1.in_port, 0x0080),
            (0x2000, 0x4000, comps.spi_gw2.in_port),
            (0x4000, 0x6000, comps.spi_gw3.in_port),
            (0x8000, 0x10000, comps.common_data_map.port, 0x8000),
        )
        return spi_data_map

    def _create_trb_map(self):

        trb_map = AddressMap("TRB_CURATOR", cache_type = self.access_cache_type,
                             length = 0x2026000, word_bits = 8,
                             max_access_width = 2)

        comps = self._is_xap.components
        trb_map.add_mappings(
                             # Map in registers and RAM
                             (0x0, 0x100, comps.common_data_map.port),
                             (0x8000,0xc000, comps.common_data_map.port,0x4000),
                             (0x10000, 0x20000, comps.common_data_map.port,
                              0x8000),
                             # Program memory maps in on trb at 0x20000
                             (0x20000, 0x2020000, comps.prog_map.port, 0)
                             #Potential extension: Add generic windows (and MMU ports?)
        )
        return trb_map

    def create_tc_map(self):
        """
        Set up the map that toolcmd windowed accesses see
        """
        tc_map = AddressMap("TOOLCMD_MAP", cache_type = self.access_cache_type,
                                             length=1<<24, word_bits=16)
        comps = self._is_xap.components
        tc_map.add_mappings((0,      0x80,   comps.common_data_map.port),
                            (0x8000, 0x10000, comps.common_data_map.port, 0x8000),
                            (0x10000,0x1000000, comps.prog_map.port))
        return tc_map

    @property
    def _hif_subsystem_view(self):
        """
        The core can't officially see the subsystem, so we have to leave it to
        an override to return the subsystem's hif property
        """
        raise PureVirtualError(self)


