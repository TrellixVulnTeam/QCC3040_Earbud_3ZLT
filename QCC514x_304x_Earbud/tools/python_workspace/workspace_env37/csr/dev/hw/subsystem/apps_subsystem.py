############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import time
import os
from contextlib import contextmanager
from csr.wheels.bitsandbobs import PureVirtualError, NameSpace, unique_subclass,\
build_le
from csr.wheels import iprint
from csr.dev.hw.subsystem.hydra_subsystem import HydraSubsystem
from csr.dev.hw.subsystem.mixins.has_mmu import HasMMU
from csr.dev.model import interface
from csr.dev.hw.address_space import AddressSpace, BlockIDAccessView, \
 AddressSlavePort, AccessView, AddressMap, BankedAddressSpace, NullAccessCache,\
 ExtremeAccessCache
from csr.dev.hw.sqif import AppsSqifInterfaces
from csr.dev.hw.core.meta.i_layout_info import Kalimba32DataInfo
from csr.dev.hw.trace import TraceLogger
from csr.dev.tools.flash_image_builder import ImageBuilder


class AppsSubsystem(HasMMU, HydraSubsystem):
    '''
    Class representing a generic Apps subsystem
    '''
    layout_info = Kalimba32DataInfo()

    BUS_INT_REGISTER_BANK_LENGTH_WORDS = 6
    
    USB_HOST_REG_START = 0x48000000 # MAP_USB_HOST_LOWER
    USB_HOST_REG_END =   0x48040000 # MAP_USE_HOST_UPPER
    
    COMMON_REG_START = 0xffff9000
    PIO_REG_START =   0xffff93cc
    # In this region there is a set of three 96 bit wide PIO registers in each
    # processor. This region is mapped separately for each core.
    PIO_REG_END = 0xffff93f0
    COMMON_REG_END = 0xffffa100

    SDIO_HOST_REG_START = 0xffffd000    
    SDIO_HOST_REG_END = 0xffffe000
     
    class IntBankSelect(object):
        SelectReg = "INT_SELECT"
        SelectRangeMax = 15
        BankedLoReg = "INT_PRIORITY"
        BankedHiReg = "INT_PRIORITY"

    class BusIntBankSelect(object):
        SelectReg = "BUS_INT_SELECT"
        SelectRangeMax = 15
        BankedLoReg = "BUS_INT_MASK"
        BankedHiReg = "BUS_INT_MASK_STATUS"
    


    def __init__(self, chip, ss_id, access_cache_type):

        HydraSubsystem.__init__(self, chip, ss_id, access_cache_type)

        self._sqif_flash0 = AddressSlavePort("SQIF_0", access_cache_type,
                                             length = 0x20000000, 
                                             layout_info=self.layout_info,
                                             view=AccessView.RAW)
        self._sqif_flash1 = AddressSlavePort("SQIF_1", access_cache_type,
                                             length = 0x20000000, 
                                             layout_info=self.layout_info,
                                             view=AccessView.RAW)

        # Subsystem level registers, divided into coarse-grained nominal blocks.
        # Only a small percentage of the total available addresses are actually
        # populated with registers, but that doesn't really matter.
        #
        # If we wanted to implement proper access control modelling, we'd need
        # to break these blocks down into per-access-control sized chunks, which
        # would presumably takes us more or less right down to the real register
        # bus level.  But let's not do that until we have to.
        self._common_registers_1 = \
                            AddressMap("COMMON_REGISTERS_1", access_cache_type,
                                       length = self.PIO_REG_START - self.COMMON_REG_START,
                                       layout_info=self.layout_info)
        self._common_registers_2 = \
                            AddressMap("COMMON_REGISTERS_2", access_cache_type,
                                       length = self.COMMON_REG_END - self.PIO_REG_END,
                                       layout_info=self.layout_info)
        self._sdio_host_registers = \
                            AddressMap("SDIO_HOST_REGISTERS", access_cache_type,
                                       length = self.SDIO_HOST_REG_END -
                                                  self.SDIO_HOST_REG_START, 
                                       layout_info=self.layout_info)
        # Add the banked blocks within the common registers
        # We just have to know what the bank select registers are called, how
        # long the banks are and which register they start at.
        
        bus_int_reg_bank_len_bytes = (self.layout_info.data_word_bits*
                                        self.BUS_INT_REGISTER_BANK_LENGTH_WORDS //
                                            self.layout_info.addr_unit_bits)
        self._bus_int_register_bank = BankedAddressSpace(
                                             "BUS_INT_SELECT", self,
                                             "BUS_INT_REGISTER_BANK",
                                             access_cache_type,
                                             length = bus_int_reg_bank_len_bytes, 
                                             layout_info=self.layout_info)
        
        bus_int_bank_local_start = self._bus_int_bank_start - \
                                   self.COMMON_REG_START
        bus_int_bank_local_end = (bus_int_bank_local_start + 
                          bus_int_reg_bank_len_bytes)
        self._common_registers_1.add_mapping(bus_int_bank_local_start,
                                             bus_int_bank_local_end, 
                                             self._bus_int_register_bank,
                                             autofill=True)

        self._create_rams(access_cache_type)

    def _create_rams(self, access_cache_type):
        
        self.rams = NameSpace()
        
        self.rams.p0_data_ram = AddressSlavePort("P0_DATA_RAM", access_cache_type, 
                                    length_kB=self.P0_DATA_RAM_SIZE, 
                                    layout_info=self.layout_info)

        self.rams.p1_data_ram = AddressSlavePort("P1_DATA_RAM", access_cache_type, 
                                       length_kB=self.P1_DATA_RAM_SIZE,
                                       layout_info=self.layout_info)
        
        self.rams.shared_ram = AddressSlavePort("SHARED_RAM", access_cache_type,
                                   length_kB= self.SHARED_RAM_SIZE, 
                                   layout_info=self.layout_info)
        self.rams.tcm0 = AddressSlavePort("TCM0", access_cache_type, 
                                   length_kB= self.TCM0_SIZE, 
                                   layout_info=self.layout_info)
        self.rams.tcm1 = AddressSlavePort("TCM1", access_cache_type,
                                   length_kB= self.TCM1_SIZE, 
                                   layout_info=self.layout_info)
        self.rams.p0_cache_ram_da = AddressSlavePort("P0_CACHE_RAM_DA", access_cache_type,
                                                     length_kB= self.P0_CACHE_RAM_SIZE, 
                                                     layout_info=self.layout_info)
        self.rams.p1_cache_ram_da = AddressSlavePort("P1_CACHE_RAM_DA", access_cache_type,
                                                     length_kB= self.P1_CACHE_RAM_SIZE, 
                                                     layout_info=self.layout_info)



    def _create_trb_map(self):
        """
        Set up the TRB map as a "view-enabled" AddressMap, meaning that it 
        supports mappings for different views of the same data space
        """
        trb_map = AddressMap("APPS_SUBSYSTEM_TRB", NullAccessCache,
                                  length = 0x100000000,word_bits=8,
                                  max_access_width=4,
                                  view_type=self._view_type)

        # The subsystem's TRB data map
        self._populate_trb_map(trb_map)
        
        return trb_map

    @property
    def subcomponents(self):
        cmps = HydraSubsystem.subcomponents.fget(self)
        cmps.update(HasMMU.subcomponents.fget(self))
        cmps.update({"p0" : "_p0",
                     "p1" : "_p1"})
        return cmps

    @property
    def name(self):
        return "Apps"

    @property
    def number(self):
        """
        The subsystem number (not to be confused with the SSID)
        as defined by csr.dev.hw.chip.hydra_chip.FixedSubsystemNumber.
        """
        from csr.dev.hw.chip.hydra_chip import FixedSubsystemNumber
        return FixedSubsystemNumber.APPS

    @property
    def cores(self):
        """
        We have two Kalimba cores in this subsystem, but only one is implemented
        so far
        """
        return (self.p0, self.p1)

    @property
    def core(self):
        '''
        For convenience, since this is the one we'll generally be more interested
        in.  But that might change.
        '''
        return self.p0
    
    
    def _create_apps_cores(self, access_cache_type):
        Apps0CoreType, Apps1CoreType = self._core_types
        Apps1CoreType = unique_subclass(Apps1CoreType)
        p0 = Apps0CoreType(self, access_cache_type)
        p1 = Apps1CoreType(self, access_cache_type)
        p0.populate(access_cache_type)
        p1.populate(access_cache_type)
        if access_cache_type is ExtremeAccessCache:
            # Use the built-in address-mapping mechanism to emulate the
            # hardware's windowing of PM into DM
            p0.emulate_hardware_windows(p1)
            p1.emulate_hardware_windows(p0)
        return p0, p1



    @property
    def p0(self):
        try:
            self._p0
        except AttributeError:
            self._p0, self._p1 = self._create_apps_cores(self._access_cache_type)
        return self._p0

    @property
    def p1(self):
        try:
            self._p1
        except AttributeError:
            self._p0, self._p1 = self._create_apps_cores(self._access_cache_type)
        return self._p1

    # Required overrides

    @property
    def spi_in(self):
        """\
        This subsystem's SPI AddressSlavePort.
        Used to wire up the chip's memory access model.
        
        It is not usually addressed directly but is needed
        to model the spi access route.
        
        """
        raise NotImplementedError("Apps subsystem doesn't have (conventional) SPI")

    @property
    def spi_data_map(self):
        raise NotImplementedError("Apps subsystem doesn't have (conventional) SPI")

    @property
    def _view_type(self):
        """
        Apps subsystems in general use the block ID to indicate a
        particular view of a given address (CSRA68100 D00 is the exception).
        """
        return BlockIDAccessView

    @property
    def firmware_type(self):
        from csr.dev.fw.apps_firmware import AppsFirmware
        return AppsFirmware

    @property
    def default_firmware_type(self):
        from csr.dev.fw.apps_firmware import AppsDefaultFirmware
        return AppsDefaultFirmware

    @property
    def firmware_build_info_type(self):
        #Because Apps has two processors, each with their own firmware
        #it makes no sense for the Subsystem to have this method
        raise NotImplementedError("AppsSubsystem::firmware_build_info_type makes no sense")       

    @property
    def _view_type(self):
        """
        Apps subsystems in general (CSRA68100 D00 is the exception) access
        different views of memory over TRB using the block ID to specify the
        view 
        """
        return BlockIDAccessView

    # Extensions.... but maybe these should be 
    @property
    def spi_keyhole_in(self):
        '''
        This subsystem's SPI keyhole port.  This isn't quite the same as a normal
        subsystem SPI port
        '''
        return self.spi_keyhole.port

    @property
    def spi_keyhole(self):
        try:
            self._spi_keyhole
        except AttributeError:
            self._spi_keyhole = self._create_spi_keyhole_data_map()
        return self._spi_keyhole

    def _create_spi_keyhole_data_map(self):
        """
        The Apps SPI keyhole data map may look different on different flavours of the
        subsystem 
        """
        raise PureVirtualError()

    @property
    def sqif_trb_address_block_id(self):
        '''
        Returns a list (indexed by sqif device number) of tuples of TRB address 
        and block IDs at which the SQIF contents can be read.
        See http://cognidox/vdocs/CS-301985-DD-L-Applications%20Subsystem%20Memory%20Map.pdf
        '''
        raise PureVirtualError

    # HasMMU override
    def _create_mmu(self):
        from ..mmu import AppsMMU
        return AppsMMU(self)

    #Show the state of the Apps subsystem PIO hardware
    def pios(self):
        def _get_val(register, proc):
            try:
                val = proc.fields[register]
                text = "0x%x" % (val)
            except KeyError as e:
                return register, "Error: Can't find register"
            return text
        
        def _add_row(table, owner, reg, proc):
            row = []  
            val = _get_val(reg, proc)     
            row.append(owner)
            row.append(reg)
            row.append(val)
            table.add_row(row)
        
        master_group = interface.Group("PIO registers")
        
        try:
            output_table = interface.Table(["Owner", "Register","Value"])
            _add_row(output_table, "shared", "APPS_SYS_PIO_MUX", self.p0)
            _add_row(output_table, "P0", "APPS_SYS_PIO_DRIVE", self.p0)
            _add_row(output_table, "P0", "APPS_SYS_PIO_DRIVE_ENABLE", self.p0)
            _add_row(output_table, "P0", "APPS_SYS_PIO_STATUS", self.p0)
            _add_row(output_table, "P1", "APPS_SYS_PIO_DRIVE", self.p1)
            _add_row(output_table, "P1", "APPS_SYS_PIO_DRIVE_ENABLE", self.p1)
            _add_row(output_table, "P1", "APPS_SYS_PIO_STATUS", self.p1)
            
            master_group.append(output_table)
        except AddressSpace.NoAccess:
            master_group.append(interface.Code(
                               "No or incomplete PIO register data visible"))
        return master_group

    @property
    def sqifs(self):
        try:
            self._sqifs
        except AttributeError:
            #Subsystem as two serial flash interfaces.
            self._sqifs = AppsSqifInterfaces(self.cores)
        return self._sqifs

    #The following 'helper' method may be overridden.
    def _sqif_window_select(self, offset_name):
        try:
            return getattr(self.p0.iodefs, offset_name+"_ENUM")
        except AttributeError:
            # It might only be in a P1 enumeration
            return getattr(self.p1.iodefs, offset_name+"_ENUM", None)

    def set_sqif_offset(self, offset_name, offset_value):
        '''
        Set the offset used for different accessed sources to the SQIF DATAPATH 
        '''
        offset_select = self._sqif_window_select(offset_name)
        self.core.fields["APPS_SYS_SQIF_WINDOW_CONTROL"] = offset_select
        self.core.fields["APPS_SYS_SQIF_WINDOW_OFFSET"] = offset_value

    def get_sqif_offset(self, offset_name):
        offset_select = self._sqif_window_select(offset_name)
        self.core.fields["APPS_SYS_SQIF_WINDOW_CONTROL"] = offset_select
        return self.core.fields["APPS_SYS_SQIF_WINDOW_OFFSET"]

    @property
    def tracelogger(self):
        try:
            self._tracelogger
        except AttributeError:
            self._tracelogger = TraceLogger(self.cores)
        return self._tracelogger

    def _generate_report_body_elements(self):
        group = interface.Group("Apps")
        try:
            for el in HydraSubsystem._generate_report_body_elements(self):
                group.append(el)
        except Exception as e:
            group.append(interface.Code(
                    "Error generating parent (HydraSubsystem) report body "
                    "for '%s' (%s: '%s')" % (self.title, type(e), e)))

        group.append(self.pios())
        return [group]

    def bulk_erase(self,bank=0, address = None):
        """\
        Most basic way to completely erase one of the Apps SQIFs. Note for 
        SQIF0 this will erase the Curator file-systems as well as the Apps
        firmware. 
        
        ONlY Uses register peeks and pokes so does need to have have had 
        firmware specified.
        
        SHOULD be able to erase a SQIF regardless of the system state.
        
        The 'undocumented' address parameter gives a way to erase a sector.
        
        """
        
        self.safe_state()
        
        self.sqif_clk_enable(bank)
        self.sqifs[bank].minimal_config()
        self.config_sqif_pios(bank)  #Have the SQIF HW configured before this. 
        self.sqifs[bank].bulk_erase_helper(bank = bank,byte_address = address) 

    def safe_state(self):
        """
        Force the subsystem into a 'safe' known state. This will stop
        the subsystem running and reset it.
        """

        #Get the Curator object to access it's registers
        cur = self.curator.core

        #Pause the chip to prevent it altering registers
        cur.halt_chip()
        
        #Put the APPS subsystem into a known state.
        cur.fields['CURATOR_SUBSYSTEMS_CLK_80M_ENABLED'] = 0x13
        cur.fields['CURATOR_SUBSYSTEMS_CLK_240M_ENABLED'] = 0x13
        
        #In D01 the Curator has control of the SQIF clock.
        if hasattr(cur.fields,'CURATOR_SUBSYSTEMS_SQIF_CLK_80M_ENABLED'):
            #If the register exists it needs to be set. 
            cur.fields.CURATOR_SUBSYSTEMS_SQIF_CLK_80M_ENABLED = 0xb

        cur.fields['CURATOR_SUBSYSTEMS_RUN_EN']  = 0x03
        #Power cycle the APPS subsystem
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x03
        time.sleep(0.2)
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x13
        time.sleep(0.2)

    def config_sqif_pios(self,bank=0):
        raise PureVirtualError(self)

    def sqif_clk_enable(self, bank):
        """
        Enable the SQIF clock
        """
        pass

    # CORE CLOCKS that go into apps subsystem, this is chip specific
    # but most chips use 240/80 MHz sources so this is the "default"
    # this should come from the sources CURATOR_APPS_SYS_CTRL_USE_CLK_80M selects
    _core_clks = {
        0 : 240000.0,
        1 : 80000.0
        }

    def get_nr_of_processor_clocks_per_ms(self):
        """
        Nr of processor clocks per ms
        """
        cur = self.curator.core
        if (self._chip.is_emulation):
            return 20000.0
        else:
            core_clk_in_sel = int(cur.fields.CURATOR_APPS_SYS_CTRL.CURATOR_APPS_SYS_CTRL_USE_CLK_80M.read())

            if (core_clk_in_sel == 1):
                # this clock is used as is but can changes by chip
                return self._core_clks[core_clk_in_sel]
            else:
                return (self._core_clks[core_clk_in_sel]/(2+self.core.fields.CLKGEN_CORE_CLK_RATE))


    def get_current_subsystem_clock_mhz(self):
        return self._core_clks[0]


    @property
    def raw_sqif0(self):
        """
        Direct access to the debugger view of the uncached sqif flash0 window in 
        DM RAM.  Assuming the debugger's offset hasn't been altered from its 
        normal value of 0 this gives the first 2MB of the SQIF independently of 
        the processor's view (which you can see at dm[0xb000_0000:0xd000_0000]).
        
        We could make this access more dynamic to give a virtual address range
        greater than 2MB
        """
        return self._sqif_flash0

    @property
    def raw_sqif1(self):
        """
        Direct access to the debugger view of the uncached sqif flash1 window in 
        DM RAM.  Assuming the debugger's offset hasn't been altered from its 
        normal value of 0 this gives the first 2MB of the SQIF independently of 
        the processor's view (which you can see at dm[0xd000_0000:0xf000_0000]).
        
        We could make this access more dynamic to give a virtual address range
        greater than 2MB
        """
        return self._sqif_flash1

    def _populate_trb_map(self, trb_map):

        self.p0._populate_trb_data_map(trb_map)
        self.p1._populate_trb_data_map(trb_map)

        trb_map.add_mappings(
            (0xb0000000,0xd0000000, self._sqif_flash0),
            (0xd0000000,0xf0000000, self._sqif_flash1),
            view=AccessView.RAW)

        trb_map.add_mappings(
           (self.COMMON_REG_START, self.PIO_REG_START, self._common_registers_1.port),
           (self.PIO_REG_END, self.COMMON_REG_END, self._common_registers_2.port),
            # No registers anywhere in (0xFFFFB000, 0xFFFFC000)
           (self.SDIO_HOST_REG_START, self.SDIO_HOST_REG_END, self._sdio_host_registers.port),
           view=AccessView.RAW)
        

    @contextmanager
    def ensure_powered(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is powered for an operation
        and wants to restore it to original state afterwards.
        """
        cur = self.chip.curator_subsystem.core
        powered = cur.fields.CURATOR_SUBSYSTEMS_POWERED.\
            CURATOR_SYS_POR__SYSTEM_BUS_APPS_SYS
        # Remember current state so as to restore later
        # then set up required state
        was_powered = int(powered)
        powered.write(1)
        import time
        time.sleep(0.1)
        # Let body of guarded block execute, which may raise exception
        try:
            yield
        finally:
            # On exit, restore the old state
            powered.write(was_powered)

    @contextmanager
    def ensure_clocked(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is clocked for an operation
        and wants to restore it to original state afterwards.
        Executes in a context with ensure_powered().
        """
        import time
        cur = self.chip.curator_subsystem.core
        # apps clocks are different on different chips - qcc512x_qcc302x/qcc514x_qcc304x
        # qcc512x_qcc302x, qcc514x_qcc304x
        clock = cur.fields.CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES.\
            CURATOR_SUBSYS_CORE_CLK_ENABLES_APPS
        with self.ensure_powered():
            was_clocked = int(clock)
            clock.write(1)
            time.sleep(0.1) 
            try:
                # Let body of guarded block execute
                yield
            finally:
                # On exit, restore the old state
                clock.write(was_clocked)


    def extract_debug_partition(self, output_path="debug_partition.xed"):
        """
        If a debug partition is present on the device then extract it to the given file.
        
        :param output_path: string defining the output path of the extracted partition. It is 
        recommended to use the extension ".xed" as this will be auto-detected by pydbgs frontend as a debug partition 
        """
        # Determine flash bank
        dfu_state_addr = 0x10
        dfu_state_len = 0x8F
        dfu_state_words = self.raw_sqif0[dfu_state_addr: dfu_state_addr + dfu_state_len]
        bin_string = "".join(bin(word) for word in dfu_state_words)
        bit_count = bin_string.count("1")
        # Get debug partition offset
        dfu_bank_offset_ptr = 0x4 if bit_count % 2 == 0 else 0x8
        dfu_ptr_len = 4
        dfu_bank_offset = build_le(self.raw_sqif0[dfu_bank_offset_ptr: 
                                                  dfu_bank_offset_ptr + dfu_ptr_len], 8)
        
        builder = ImageBuilder(read_from_sqif=True, apps_subsys=self)
        dbg_part_offset = builder.image_header.get_section_details("debug_partition")["offset"]

        dbg_part_addr = (dfu_bank_offset + dbg_part_offset)
        dbg_part_len_bytes = builder.image_header.get_section_details("debug_partition")["capacity"]

        # Get raw data
        dbg_part_end_addr = dbg_part_addr + dbg_part_len_bytes
        dbg_part_data = self.raw_sqif0[dbg_part_addr: dbg_part_end_addr]

        output_path = os.path.normpath(output_path) 
        with open(output_path, "wb") as f:
            iprint("Writing debug partition dump to {}".format(output_path))
            f.write(bytearray(dbg_part_data))


