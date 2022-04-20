############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.hw.address_space import AddressSlavePort, AddressMasterPort, \
    ReadRequest, WriteRequest, AddressRange, AccessPath
from csr.dev.hw.register_field.register_field import AdHocBitField
from csr.wheels.bitsandbobs import PureVirtualError

class BaseGenericWindow (object):
    """\
    XAP Generic Window Model (Base)

    Supports automatic access routing and explicit window control (for legacy
    scripts).

    Example usage:
        gw = my_core.spi_gw3
        gw.map_program(0x200000)
        data = gw[0:2]
    """
    def __init__(self, core, name):
        """\
        Construct BaseGenericWindow.
                
        N.B. 2-phase construction to allow specialised composition. populate()
        must be called after init.
        """
        self.name = name
        self.core = core

    def populate(self, access_cache_type):
        """\
        Populate with subcomponents.
        
        Subclasses should override-extend this method to install additional
        sub-components.
        """
        # Construct ports common to all variants so far.
        self._in_port = self.InPort(self, access_cache_type)
        self._io_log_port = self.OutPort(self, self._map_io_log_code)
        self._prog_port = self.OutPort(self, self._map_program_code)
    
    @property
    def in_port(self):
        """Input InPort"""
        return self._in_port

    @property
    def prog_port(self):
        """Program Ram OutPort"""
        return self._prog_port

    @property
    def io_log_port(self):
        """IO Log OutPort"""
        return self._io_log_port

    @property
    def out_ports(self):
        """\
        The full set of OutPorts.
        Used for automated routing.
        Varies with subtype.
        """
        return self._out_ports

    @property
    def selected_out_port(self):
        """\
        Currently selected OutPort object
        """
        # Match hardware register field value to OutPort
        mapping_code = self._mapping_field.read()
        for port in self.out_ports:
            if port.mapping_code == mapping_code:
                return port
        return None

    def map_io_log(self, prog_addr):
        """\
        Map the subsystem's io log space into this GenericWindow.

        Return the window-relative address that it maps to.

        N.B. caller must be aware that the window size is limited.

        Prefer to address the target space directly and let automatic access
        routing sort things out.
        """
        self._map_addr(self._map_io_log_code, prog_addr)

    def map_program(self, prog_addr):
        """\
        Map the subsystem's program memory address into this GenericWindow.

        Return the window-relative address that it maps to.

        N.B. caller must be aware that the window size is limited.

        Prefer to address the target space directly and let automatic access
        routing sort things out.
        """
        self._map_addr(self._map_program_code, prog_addr)

    def __getitem__(self, index):
        """\
        Memory Read w.r.t. this GenericWindow.
        Example Use:
           data = gw[0:2]
        """
        # Delegate to the in_port as it already handles slicing and accesses
        # mapped from data/spi AddressSpaces.
        #
        return self.in_port[index]

    def __setitem__(self, index, data):
        """\
        Memory Write w.r.t. this GenericWindow.
        Example Use:
           gw[0:2] = (0x1234, 0x5678)
        """
        # Delegate to the in_port as it already handles slicing and accesses
        # mapped from data/spi AddressSpaces.
        #
        self.in_port[index] = data

    # -------------------------------------------------------------------------
    # Protected / Required
    # -------------------------------------------------------------------------

    @property
    def _out_ports(self):
        """\
        The full set of output ports.
        Used for automated routing.
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    @property
    def _mapping_field(self):
        """\
        Field to set GW mapping
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    @property
    def _map_io_log_code(self):
        """\
        Code to map IO Log Space (written to _mapping_field)
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    @property
    def _map_program_code(self):
        """\
        Code to map Program Space (written to _mapping_field)
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    @property
    def _bank_field(self):
        """\
        Field to set Bank.
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    @property
    def _bank_shift(self):
        """\
        Bank select exponent (2^n)
        Varies with subsystem.
        """
        raise PureVirtualError(self)

    # -------------------------------------------------------------------------
    # Protected / Provided
    # -------------------------------------------------------------------------

    @property
    def _bank_offset(self):
        # Current address offset (inferred from bank register)
        bank = self._bank_field.read()
        return bank << self._bank_shift

    def _map_addr(self, mapping_code, addr):
        """\
        Maps target address using specified mapping code.
        Returns the window-relative address that it maps to.
        """
        self._mapping_field.write(mapping_code)
        self._bank_field.write(addr >> self._bank_shift)
        mask = (1 << self._bank_shift) - 1
        return addr & mask


    class InPort (AddressSlavePort):
        """\
        Models Generic Window proc/spi-space facing port.
        """
        def __init__(self, gw, *args, **kwargs):
            AddressSlavePort.__init__(self, gw.name, *args, **kwargs)
            self.gw = gw

        # AddressSlavePort compliance

        def _extend_access_path(self, path):
            # A non trivial case: Access paths widen as they pass through the
            # window - that's the whole point!
            #
            # Delegate to the out ports.
            #
            for port in self.gw.out_ports:
                port.extend_access_path(path)

        def resolve_access_request(self, rq):

            # Offset the request according to bank register state
            offset_region = rq.region + self.gw._bank_offset
            if isinstance(rq, ReadRequest):
                offset_rq = ReadRequest(offset_region)
            else:
                offset_rq = WriteRequest(offset_region, rq.data)

            # And pass on via currently selected output port.
            selected_out_port = self.gw.selected_out_port
            if not selected_out_port:
                raise RuntimeError("Generic window in unknown state")
            selected_out_port.resolve_access_request(offset_rq)

            # Patch data back into original request
            if isinstance(rq, ReadRequest):
                rq.data = offset_rq.data


    class OutPort (AddressMasterPort):
        """\
        Models Generic Window program-space facing port.
        """
        def __init__(self, gw, mapping_code):
            AddressMasterPort.__init__(self)
            self.gw = gw
            self.mapping_code = mapping_code

        # AddressMasterPort compliance

        def execute_outwards(self, rq):

            gw = self.gw

            # Map target region and address into window
            win_start = gw._map_addr(self.mapping_code, rq.region.start)

            # Create window-relative request
            win_range = AddressRange(win_start, win_start + rq.region.length)
            if win_range.stop > 0x2000:
                # If hit this, then its time to create general solution for
                # scatter-gather. Probably a Window base class.
                raise NotImplementedError("GenericWindow doesn't support scatter gather (region %s)" % rq.region)
            if isinstance(rq, ReadRequest):
                win_rq = ReadRequest(win_range)
            else:
                win_rq = WriteRequest(win_range, rq.data)

            # Pass out of the in_port (obviously)
            gw.in_port.execute_outwards(win_rq)

            # Patch read data back to original request 'as is'
            if isinstance(rq, ReadRequest):
                rq.data = win_rq.data
            # Patch metadata back in too
            rq.merge_metadata(win_rq.metadata)

        # Extensions
        
        def extend_access_path(self, path):
            """\
            Widen and extend the access path to map the entire the connected
            slave address space.
            
            Almost!... Page offsets can only be positive - so we can't actually
            map an address _lower_ than the incoming path can address. E.g. We
            can not access program memory < 0x80 via GW1.
            """
            if self.slave:
                accessible_range = AddressRange(path.range.start, 
                                                len(self.slave))
                path.add_fork(AccessPath(path.path_name, path.rank + 1,
                                         self, accessible_range,
                                         path.latency, path.speed))
        

class CuratorGenericWindow (BaseGenericWindow):
    """\
    Curator Generic Window Specialisation
    
    Adds: trb_port - A TRB facing port.
    """
    def __init__(self, core, name):

        BaseGenericWindow.__init__(self, core, name)

    def populate(self, access_cache_type):

        core = self.core
        layout_info = core._info.layout_info

        # Compensate for missing hardware defs !*!*
        #
        # Derive config register sub-fields
        #
        config_reg = core.registers["MMU_%s_CONFIG" % self.name]

        # Bank select
        self.__bank_select_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=0, num_bits=21,
            is_writeable=config_reg.is_writeable
        )
        # MMU_GW_CONFIG_BLOCK_ID    21-24    32    -    Block ID for remote subsystem accesses
        self.__remote_block_id_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=21, num_bits=4,
            is_writeable=config_reg.is_writeable
        )
        # MMU_GW_CONFIG_SUBSYSTEM_ID    25-28    32    -    Subsystem ID for remote subsystem accesses
        self.__remote_susbsystem_id_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=25, num_bits=4,
            is_writeable=config_reg.is_writeable
        )
        # MMU_GW_CONFIG_MAPPING    29-31    32    -    Mapping for which memory region to access
        self.__mapping_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=29, num_bits=3,
            is_writeable=config_reg.is_writeable
        )

        # Cache some related enums
        #
        enums = core.misc_io_values

        self.__map_io_log_code = enums['MMU_GW_CONFIG_IO_LOG']
        self.__map_program_code = enums['MMU_GW_CONFIG_PROGRAM_MEMORY_EXT']
        self.__map_trb_code = enums['MMU_GW_CONFIG_REMOTE_SUBSYSTEM']
        self.__bank_select_shift = enums['MMU_GW_CONFIG_BANK_SELECT_SHIFT']

        # Additional output ports
        #
        # This port needs specialising to set subsystem field.
        #
        self.__trb_port = self.OutPort(self, self.__map_trb_code)

        # Finally populate the base
        #
        BaseGenericWindow.populate(self, access_cache_type)

    # -------------------------------------------------------------------------
    # Extensions
    # -------------------------------------------------------------------------

    @property
    def trb_port(self):
        return self.__trb_port

    def map_trb(self, remote_ssid, remote_trb_addr):
        """\
        Map remote subsystem TRB memory address into this window.

        Returns the address it maps to wrt. this GenericWindow.

        Prefer to address the target space directly and let automatic access
        routing sort things out.
        """

        if remote_trb_addr % 2:
            raise ValueError("Only 16bit aligned accesses supported by GW")

        raise NotImplementedError()

        """ Heres the c code...
        ( \
            ((void) ((MMU_PROC_GW3_CONFIG_MSW) = (((uint16)((((remote_addr)/2) >> MMU_GW_CONFIG_BANK_SELECT_SHIFT) >> 16))))) , \
            ((void) ((MMU_PROC_GW3_CONFIG_LSW) = (((uint16)((((remote_addr)/2) >> MMU_GW_CONFIG_BANK_SELECT_SHIFT) & 0xffff))))) , \
                                       \
            ((MMU_PROC_GW3_CONFIG_MSW) = ((MMU_PROC_GW3_CONFIG_MSW) & ~((((MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_MSB_POSN) >= (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_LSB_POSN)) ? (uint16)((1uL << (((MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_MSB_POSN) - (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_LSB_POSN)) + 1)) - 1u) : 0u) << (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_LSB_POSN))) |   \
             (((MMU_GW_CONFIG_REMOTE_SUBSYSTEM)) << (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_MAPPING_LSB_POSN))) , \
                                       \
            ((MMU_PROC_GW3_CONFIG_MSW) = ((MMU_PROC_GW3_CONFIG_MSW) & ~((((MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_MSB_POSN) >= (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_LSB_POSN)) ? (uint16)((1uL << (((MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_MSB_POSN) - (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_LSB_POSN)) + 1)) - 1u) : 0u) << (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_LSB_POSN))) |   \
             (((ss_id)) << (MMU_GW_CONFIG_MSW_MMU_GW_CONFIG_SUBSYSTEM_ID_LSB_POSN))) , \
            ((void *)((char *)MMU_PROC_GW3_START + ((uint16)((remote_addr)/2) & ((1 << MMU_GW_CONFIG_BANK_SELECT_SHIFT) - 1)))) \
        )        return MMU_GW3_PAGE_REMOTE_MEM(ss_id, remote_addr);
        """

    # -------------------------------------------------------------------------
    # Protected / BaseGenericWindow compliance
    # -------------------------------------------------------------------------

    @property
    def _out_ports(self):
        return (self.prog_port, self.trb_port)

    @property
    def _mapping_field(self):
        return self.__mapping_field

    @property
    def _map_io_log_code(self):
        return self.__map_io_log_code

    @property
    def _map_program_code(self):
        return self.__map_program_code

    @property
    def _bank_field(self):
        return self.__bank_select_field

    @property
    def _bank_shift(self):
        return self.__bank_select_shift


class WLANGenericWindow (BaseGenericWindow):
    """\
    WLAN Generic Window Specialisation

    Adds: shared_ram_port - A Shared Ram facing port.
    """
    def __init__(self, core, name):

        BaseGenericWindow.__init__(self, core, name)

    def populate(self, access_cache_type):

        core = self.core
        layout_info = core._info.layout_info
        # Compensate for missing hardware defs...
        #
        # Derive config register sub-fields
        #
        config_reg = core.registers["MMU_%s_CONFIG" % self.name]

        # 12: 0 - Bank select
        self.__bank_select_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=0, num_bits=13,
            is_writeable=config_reg.is_writeable
        )
        # 14:13 - Window mapping;
        self.__mapping_field = AdHocBitField(
            config_reg.mem, layout_info, config_reg.start_addr,
            start_bit=13, num_bits=2,
            is_writeable=config_reg.is_writeable
        )

        # Cache some related enums
        #
        enums = core.misc_io_values
        self.__map_io_log_code = enums['MMU_GW_CONFIG_IO_LOG']
        self.__map_program_code = enums['MMU_GW_CONFIG_PROGRAM_MEMORY']
        self.__map_shared_ram_code = enums['MMU_GW_CONFIG_SHARED_MEMORY']
        # Not in digits!
        # self.__bank_select_shift = enums['MMU_GW_CONFIG_BANK_SELECT_SHIFT']
        self.__bank_select_shift = 11

        # Additional Output Ports
        #
        self.__shared_ram_port = self.OutPort(self, self.__map_shared_ram_code)

        # Finally populate the base
        #
        BaseGenericWindow.populate(self, access_cache_type)

    # -------------------------------------------------------------------------
    # Extensions
    # -------------------------------------------------------------------------

    @property
    def shared_ram_port(self):
        return self.__shared_ram_port

    def map_shared_ram(self, ram_addr):
        """\
        Map the subsystem's shared memory address into this GenericWindow.

        Return the window-relative address that it maps to.

        N.B. caller must be aware that the window size is limited.

        Prefer to address the target space directly and let automatic access
        routing sort things out.
        """
        self._map_addr(self.__map_shared_ram_code, ram_addr)

    # -------------------------------------------------------------------------
    # Protected / BaseGenericWindow compliance
    # -------------------------------------------------------------------------

    @property
    def _out_ports(self):
        return (self.io_log_port, self.prog_port, self.shared_ram_port)

    @property
    def _mapping_field(self):
        return self.__mapping_field

    @property
    def _map_io_log_code(self):
        return self.__map_io_log_code

    @property
    def _map_program_code(self):
        return self.__map_program_code

    @property
    def _bank_field(self):
        return self.__bank_select_field

    @property
    def _bank_shift(self):
        return self.__bank_select_shift
