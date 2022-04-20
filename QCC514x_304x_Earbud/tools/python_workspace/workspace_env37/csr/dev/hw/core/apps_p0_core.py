# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressMap, NullAccessCache, BankedAddressSpace, AddressRange, AccessView
from csr.dev.hw.subsystem.host_subsystem import AppsHifTransform
from csr.dev.hw.core.kal_core import KalCore
from csr.dev.hw.core.mixin.is_in_hydra import IsInHydra
from csr.wheels.bitsandbobs import NameSpace, null_context
from csr.dev.hw.interrupt import Interrupt
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dwarf.read_dwarf import  Dwarf_Reader
import sys
from csr.dev.model import interface
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.mmu import AppsVMWindow
import contextlib
import struct
from io import open

if sys.version_info > (3,):
    # Python 3
    int_type = int
else:
    # Python 2
    int_type = (int, long)


class AppsCore(KalCore, IsInHydra):
    '''
    Functionality common to the two processors in the Apps subsystem
    '''
    P0_DATA_RAM_START =    0x00000000
    P1_DATA_RAM_START =    0x00010000
    SHARED_RAM_START =     0x10000000
    TCM_START =            0x20000000
    P0_CACHE_RAM_START =   0x30000000
    P1_CACHE_RAM_START =   0x30030000
    INT_REGISTER_BANK_LENGTH_WORDS = 1
    K32_REG_START = 0xffff8000
    
    
    def __init__(self, subsystem):
        KalCore.__init__(self)
        IsInHydra.__init__(self, subsystem)


    @property
    def program_memory(self):
        return self._program_memory.port
    
    # BaseCore Compliance

    @property
    def data(self):
        return self._components.proc_data_map.port
                        
    @property
    def program_space(self):
        return self._components.proc_pm_map.port

    @property
    def register_space(self):
        '''
        Registers appear in data space at their "well known" addresses, i.e.
        starting from 0xfff00000
        '''
        return self.data

    def _add_int_banked_regs(self, access_cache_type):
        """
        Add a special address map for handling the banked k32_interrupt 
        register(s).  This is a sub-space of the k32_registers map that exists
        in both cores.
        """
        
        layout_info = self.info.layout_info
        comps = self._components
        
        int_reg_bank_len_bytes = (layout_info.data_word_bits*
                                    self.INT_REGISTER_BANK_LENGTH_WORDS //
                                        layout_info.addr_unit_bits)
        comps.int_register_bank = BankedAddressSpace(
                                             "INT_SELECT", self,
                                             "INT_REGISTER_BANK", 
                                             access_cache_type,
                                             length = int_reg_bank_len_bytes,
                                             layout_info=layout_info)
        # This block must be added in terms of addresses relative to 
        # k32_registers' internal addressing, which is from 0 even though it
        # appears in TRB at 0x7fff8000 and processor 0xffff8000.
        # respectively.
        int_bank_local_start = (
            self.field_refs["INT_PRIORITY"]._info.start_addr -  
                                                        self.K32_REG_START)
        int_bank_local_end = int_bank_local_start + int_reg_bank_len_bytes

        comps.k32_registers.add_mapping(
            int_bank_local_start, int_bank_local_end, comps.int_register_bank,
            autofill=True)

    def populate(self, access_cache_type):

        is_p0 = self.processor_number == 0
        self._components = NameSpace()
        comps = self._components # shorthand
        
        # Memory model for access routing purposes
        # 
        # This is OO equivalent of "scripted memory map" (and some)
        #
        # Standard Apps P0 Kalimba Address spaces.
        #

        # Components needed to populate the memory maps.
        #

        comps.proc_data_map = AddressMap("PROC_DATA", NullAccessCache,
                                         length = 0x100000000, 
                                         layout_info=self.info.layout_info)
        comps.proc_pm_map = AddressMap("PROC_PM", NullAccessCache,
                                         length = 0x810000, word_bits=8)
        

        # Create local objects to represent the remainder of dm: in principle
        # these could be replaced with references to "real" memory spaces owned
        # by independent models of the relevant hardware if that ever becomes
        # useful
        if is_p0:
            comps.cross_cpu_regs = AddressMap("CROSS_CPU_REGS", access_cache_type,
                                       length = 0x10000, 
                                       layout_info=self.info.layout_info)
        comps.remote_registers = AddressMap("REMOTE_REGISTERS", access_cache_type,
                                            length = 0x50000, 
                                            layout_info=self.info.layout_info)
        comps.p0_cached_sqif_flash_0 = AddressMap("P0_CACHED_SQIF_FLASH_0",
                                                  access_cache_type,
                                                  length = 0x800000,
                                            layout_info=self.info.layout_info)
        comps.p1_cached_sqif_flash_0 = AddressMap("P1_CACHED_SQIF_FLASH_0",
                                                  access_cache_type,
                                                  length = 0x800000, 
                                            layout_info=self.info.layout_info) 
        comps.sqif_0_sram = AddressMap("SQIF_0_SRAM", access_cache_type,
                                       length = 0x800000,
                                       layout_info=self.info.layout_info)
        comps.sqif_1_sram = AddressMap("SQIF_1_SRAM", access_cache_type,
                                       length = 0x800000,
                                       layout_info=self.info.layout_info)
        comps.vm_buffer_window = AppsVMWindow(self, 
                                              "VM_BUFFER_WINDOW", NullAccessCache,
                                              length = 0x10000000,
                                              layout_info=self.info.layout_info)

        # Reduced length of memory required. This is enough for
        # Apps SQIF0 window with both Bank A and Bank B image. 
        comps.direct_sqif_flash0_window = \
                                AddressMap("DIR_SQIF_0_WIN", access_cache_type,
                                           length = 0x1000000, 
                                           layout_info=self.info.layout_info)
        if is_p0:
            comps.direct_sqif_flash1_window = \
                                    AddressMap("DIR_SQIF_1_WIN", access_cache_type,
                                               length = 0x20000000, 
                                               layout_info=self.info.layout_info)
        comps.remote_subsys_access_window = \
                            AddressMap("REMOTE_SUBSYS_WIN", access_cache_type,
                                       length = 0x8000000, 
                                       layout_info=self.info.layout_info)
        comps.k32_registers = \
                            AddressMap("K32_REGISTERS", access_cache_type,
                                       length = 0x1000, 
                                       layout_info=self.info.layout_info)
        comps.pio_registers = \
                            AddressMap("PIO_REGISTERS", access_cache_type,
                                       length = 0x24,
                                       layout_info=self.info.layout_info)
        comps.k32_debug_registers = \
                            AddressMap("K32_DEBUG_REGISTERS", access_cache_type,
                                       length = 0x200, 
                                       layout_info=self.info.layout_info)
            
        if is_p0:
            hif_view = self._hif_subsystem_view
            if hif_view:
                comps.hif_uart_view = AppsHifTransform(hif_view.uart.port, self)
                comps.hif_usb2_view = AppsHifTransform(hif_view.usb2.port, self)
                comps.hif_sdio_view = AppsHifTransform(hif_view.sdio.port, self)
                comps.hif_bitserial0_view = AppsHifTransform(hif_view.bitserial0.port, self) 
                comps.hif_bitserial1_view = AppsHifTransform(hif_view.bitserial1.port, self)
                comps.hif_host_sys_view = AppsHifTransform(hif_view.host_sys.port, self)

        self._add_int_banked_regs(access_cache_type)

        self._populate_processor_data_view(comps.proc_data_map) 
        self._populate_processor_pm_view(comps.proc_pm_map) 
            
        #We have to remember this so that SPI map construction can refer to
        #it whenever it is called upon
        self.access_cache_type = access_cache_type
        
    def emulate_hardware_windows(self, other_core):
        """
        When running against a coredump the hardware windows into PM from
        DM aren't available directly because we don't dump those parts of
        DM.  We could manually load them from PM, but we might as well make
        use of the address mapping mechanism to save us the trouble and the
        memory. 
        """
        comps = self._components
        
        iprint("Adding mapping from DM window into PM")
        if self.processor_number == 0:
            p0 = self
            p1 = other_core
        else:
            p1 = self
            p0 = other_core
            
        comps.p0_cached_sqif_flash_0.add_mapping(0x0,0x800000, p0.program_space)
        comps.direct_sqif_flash0_window.add_mapping(0x0,0x800000, p0.program_space)
        comps.p1_cached_sqif_flash_0.add_mapping(0x0,0x800000, p1.program_space)
        if self.processor_number == 0:
            comps.direct_sqif_flash1_window.add_mapping(0x0,0x800000, p1.program_space)
        
    def _restore_hif_local_map(self):
        """
        Only for testing purposes - removes HIF view blocks from the
        proc_data_map, restoring local access to the HIF registers.
        We won't be able to install these back because the order counts,
        pydbg session restart will be needed.
        """
        comps = self._components
        maps = comps.proc_data_map
        
        regions = [(0x00140000, 0x00140FFF),
                   (0x00141000, 0x00141FFF),
                   (0x00142000, 0x00142FFF),
                   (0x00143000, 0x00143FFF),
                   (0x00144000, 0x00144FFF),
                   (0x0014d000, 0x0014dFFF)]
        ranges = [AddressRange(r[0],r[1]) for r in regions]
        
        for r in ranges:
            for mapping in maps.mappings:
                if mapping.does_span(r):
                    break
            if mapping.does_span(r):
                maps.mappings.remove(mapping)
                iprint("Removed mapping for range %s" % str(r)) 
    
    def _populate_processor_data_view(self, map):
        """
         Populate the main PROC memory map for this core.
        
         Ref: CS-301985-DD-F
        """
        
        comps = self._components
        ss = self.subsystem
        rams = ss.rams
        is_p0 = self.processor_number == 0 

        map.add_mapping(self.P0_DATA_RAM_START, 
                        self.P0_DATA_RAM_START + ss.P0_DATA_RAM_SIZE*0x400, 
                        rams.p0_data_ram, 
                        group="local ram" if is_p0 else None)
        map.add_mapping(self.P1_DATA_RAM_START,
                        self.P1_DATA_RAM_START + ss.P1_DATA_RAM_SIZE*0x400,
                        rams.p1_data_ram,
                        group="local ram" if not is_p0 else None)
        map.add_mapping(self.SHARED_RAM_START,
                        self.SHARED_RAM_START + ss.SHARED_RAM_SIZE*0x400, 
                        rams.shared_ram, group="shared ram")
        TCM_START = self.TCM_START
        TCM1_START = self.TCM_START + ss.TCM0_SIZE*0x400
        map.add_mappings(
            (TCM_START, TCM1_START, rams.tcm0),
            (TCM1_START, TCM1_START + ss.TCM1_SIZE*0x400, rams.tcm1))
        if is_p0:
            map.add_mapping(
             self.P0_CACHE_RAM_START, 
             self.P0_CACHE_RAM_START + (ss.P0_CACHE_RAM_SIZE + ss.P0_CACHE_TAG_RAM_SIZE)*0x400,
             rams.p0_cache_ram_da)
        map.add_mapping(
             self.P1_CACHE_RAM_START, 
             self.P1_CACHE_RAM_START + (ss.P1_CACHE_RAM_SIZE + ss.P1_CACHE_TAG_RAM_SIZE)*0x400,
             rams.p1_cache_ram_da)
        try:
            nfc_ram = rams.nfc_ram
        except AttributeError:
            pass
        else:
            map.add_mapping(0x40000000, 0x50000000, rams.nfc_ram.port)
        if is_p0:
            map.add_mapping(0x50000000, 0x60000000, comps.cross_cpu_regs.port)
            
        try:
            ss._usb_host_registers
        except AttributeError:
            pass
        else:
            map.add_mapping(ss.USB_HOST_REG_START,
                            ss.USB_HOST_REG_END, 
                            ss._usb_host_registers.port)
            #0x00190000, 0x00800000 unmapped
        map.add_mappings(
            (0x70000000, 0x70800000, comps.p0_cached_sqif_flash_0.port),
            (0x78000000, 0x78800000, comps.p1_cached_sqif_flash_0.port),
            group="const space")
        map.add_mappings(
            (0x80000000, 0x80800000, comps.sqif_0_sram.port),
            (0x88000000, 0x88800000, comps.sqif_1_sram.port),
            (0x90000000, 0xA0000000, comps.remote_subsys_access_window.port),
            (0xA0000000, 0xB0000000, comps.vm_buffer_window))
        if is_p0:
            map.add_mapping(
                0xB0000000, 0xD0000000, comps.direct_sqif_flash0_window.port)
        else:
            map.add_mapping(
                0xD0000000, 0xF0000000, comps.direct_sqif_flash0_window.port)
        if is_p0:
            map.add_mapping(
                0xD0000000, 0xF0000000, comps.direct_sqif_flash1_window.port)
        map.add_mapping(
            0xFFFF8000, 0xFFFF9000, comps.k32_registers.port)
        map.add_mapping(
            ss.COMMON_REG_START, ss.PIO_REG_START, ss._common_registers_1.port)
        map.add_mapping(
            ss.PIO_REG_START, ss.PIO_REG_END, comps.pio_registers.port)
        map.add_mapping(
            ss.PIO_REG_END, 
            ss.COMMON_REG_END, ss._common_registers_2.port)
        try:
            ss._nfc_registers
        except AttributeError:
            pass
        else:
            map.add_mapping(
                ss.NFC_REG_START, ss.NFC_REG_END, ss._nfc_registers.port)
            # No registers anywhere in (0xFFFFB000, 0xFFFFC000)
        try:
            ss._aux_data_conv_registers
        except AttributeError:
            pass
        else:
            map.add_mapping(
                ss.AUX_DATA_CONV_REG_START, ss.AUX_DATA_CONV_REG_END, 
                ss._aux_data_conv_registers.port)
        try:
            ss._sdio_host_registers
        except AttributeError:
            pass
        else:
            map.add_mapping(
                ss.SDIO_HOST_REG_START, ss.SDIO_HOST_REG_END, ss._sdio_host_registers.port)
        map.add_mapping(
            0xFFFFFE00, 0x100000000,comps.k32_debug_registers.port)
        
        if is_p0:
            # Windows for HIF blocks
            try:
                map.add_mappings(
                    # windows for HIF blocks registers
                    (0x60040000, 0x60040FFF, comps.hif_uart_view),
                    (0x60041000, 0x60041FFF, comps.hif_usb2_view),
                    (0x60043000, 0x60043FFF, comps.hif_bitserial0_view),
                    (0x60044000, 0x60044FFF, comps.hif_bitserial1_view),
                    (0x6004d000, 0x6004dFFF, comps.hif_host_sys_view))
            except AttributeError:
                # host subsystem is not present
                pass
            
        if is_p0:
            #map the rest of remote registers range
            map.add_mapping(0x60000000, 0x60050000, comps.remote_registers.port)
        
    def _populate_processor_pm_view(self, map):
        comps = self._components

        map.add_mapping(0x00000000, 0x00810000, self.program_memory)
    
    def _create_spi_keyhole_data_map(self):
        '''
        Since this goes through a keyhole I guess it just sees everything that
        the Kalimba sees.  At least that's a good start.
        '''
        spi_data_map = AddressMap("SPI_KEYHOLE_DATA", NullAccessCache,
                                  length = 0x100000000)
        self._populate_processor_data_view(spi_data_map)

        return spi_data_map

    def _populate_trb_data_map(self, trb_data_map):
        
        ss = self.subsystem
        is_p0 = self.processor_number == 0
        
        comps = self._components
        rams = ss.rams
        proc_data_view = AccessView.PROC_0_DATA if self.processor_number == 0 else AccessView.PROC_1_DATA
        proc_prog_view = AccessView.PROC_0_PROG if self.processor_number == 0 else AccessView.PROC_1_PROG

        
        TCM_START = self.TCM_START
        TCM1_START = self.TCM_START + ss.TCM0_SIZE*0x400
        trb_data_map.add_mappings(
            (self.P0_DATA_RAM_START, 
             self.P0_DATA_RAM_START + ss.P0_DATA_RAM_SIZE*0x400, rams.p0_data_ram),
            (self.P1_DATA_RAM_START, 
             self.P1_DATA_RAM_START + ss.P1_DATA_RAM_SIZE*0x400, rams.p1_data_ram),
            (self.SHARED_RAM_START, 
             self.SHARED_RAM_START + ss.SHARED_RAM_SIZE*0x400, rams.shared_ram),
            (TCM_START, TCM1_START, rams.tcm0),
            (TCM1_START, TCM1_START + ss.TCM1_SIZE*0x400, rams.tcm1),
             view=AccessView.RAW)
        if is_p0:
            trb_data_map.add_mappings(
                (self.P0_CACHE_RAM_START, 
                 self.P0_CACHE_RAM_START + (ss.P0_CACHE_RAM_SIZE + ss.P0_CACHE_TAG_RAM_SIZE)*0x400,
                 rams.p0_cache_ram_da),
                 view=AccessView.RAW)
        trb_data_map.add_mappings(
            (self.P1_CACHE_RAM_START, 
             self.P1_CACHE_RAM_START + (ss.P1_CACHE_RAM_SIZE + ss.P1_CACHE_TAG_RAM_SIZE)*0x400,
             rams.p1_cache_ram_da),
             view=AccessView.RAW)
        try:
            nfc_ram = rams.nfc_ram
        except AttributeError:
            pass
        else:
            trb_data_map.add_mappings(
                (0x40000000, 0x50000000, nfc_ram.port),
                view=AccessView.RAW)
            
        if is_p0:
            trb_data_map.add_mappings(
                (0x50000000, 0x60000000, comps.cross_cpu_regs.port),
                (0x60000000, 0x60050000, comps.remote_registers.port),
                view=proc_data_view)
        trb_data_map.add_mappings(
            (0x70000000, 0x70800000, comps.p0_cached_sqif_flash_0.port),
            (0x78000000, 0x78800000, comps.p1_cached_sqif_flash_0.port),
            (0x80000000, 0x80800000, comps.sqif_0_sram.port),
            (0x88000000, 0x88800000, comps.sqif_1_sram.port),
            (0x90000000, 0xA0000000, comps.remote_subsys_access_window.port),
            view=proc_data_view)
        if is_p0:
            trb_data_map.add_mappings(
                (0xB0000000, 0xD0000000, comps.direct_sqif_flash0_window.port),
                (0xD0000000, 0xF0000000, comps.direct_sqif_flash1_window.port),
                view=proc_data_view)
        else:
            trb_data_map.add_mappings(
                (0xD0000000, 0xF0000000, comps.direct_sqif_flash0_window.port),
                view=proc_data_view)
        trb_data_map.add_mappings(
            (0xFFFF8000, 0xFFFF9000, comps.k32_registers.port),
            (ss.PIO_REG_START, ss.PIO_REG_END, comps.pio_registers.port),
            (0xFFFFFE00, 0x100000000, comps.k32_debug_registers.port),
             view=proc_data_view)
        trb_data_map.add_mapping(
             0x00000000, 0x10000000, self.program_memory,
             view=proc_prog_view)






    @property
    def core_commands(self):
        return self._common_core_commands()

    def _common_core_commands(self):
        '''        
        Dictionary of commands (or other objects) you want to be registered
        as globals in interactive shells when this core has focus.
        '''
        cmds = {
            'report'    : "self.subsystem.generate_report",
            #Misc
            'pios'      : "self.subsystem.pios",
            'display_brks' : "self.brk_display",
            'extract_debug_partition' : "self.subsystem.extract_debug_partition"}
        cmds["fw_ver"] = "self.fw.fw_ver" 
            
        # Now try adding the commands that require real firmware (i.e. an ELF
        # file).  These might not be available.
        core_fw_cmds = {            
             #Logging 
            'log'       : "self.fw.debug_log.generate_decoded_event_report",
            'live_log'  : "self.fw.debug_log.live_log",
            'trb_live_log': "self.fw.debug_log.trb_live_log",
            'clear_log' : "self.fw.debug_log.clear",
            'reread_log': "self.fw.debug_log.reread",
            'log_level' : "self.fw.debug_log.log_level",
            #Symbol lookup
            'psym'      : "self.sym_.psym",
            'dsym'      : "self.sym_.dsym",
            'dispsym'   : "self.sym_.dispsym",
            'sym'       : "self.sym_.sym",
            'struct'    : "self.fw.env.struct",
            #Misc         
            'stack'     : "self.fw.stack_report",
            'irqs'      : "self.fw.irqs", 
            'mib_dump'  : "self.fw.mib.dump"
        }
        exception_list = [AttributeError]
                
        cmds.update(core_fw_cmds)
        
        prim_commands = {
            'prim_log'          : "self.fw.prim_log.generate_decoded_event_report",
            'prim_live_log'     : "self.fw.prim_log.live_log",
            'prim_log_xml'      : "self.fw.prim_log.generate_decoded_event_report_xml",
            'prim_live_log_xml' : "self.fw.prim_log.live_log_xml",
           }
        cmds.update(list(prim_commands.items()))

        return cmds, exception_list

    
    def cache_counters(self, report=False):
        if self.fields.CLOCK_DIVIDE_RATE == 0:
            output = interface.Code("Cache counters can't be read when "
                                    "the processor is shallow sleeping")
            if report:
                return output
            TextAdaptor(output, gstrm.iout)
            return
        self.fields["APPS_SYS_CACHE_SEL"] = self.processor_number
        return self._print_list_regs("Cache Counters",
               [("PM Hits",     "KALIMBA_READ_CACHE_PM_HIT_COUNTER", ",d"),
                ("PM Misses",   "KALIMBA_READ_CACHE_PM_MISS_COUNTER", ",d"),
                ("DM Hits",     "KALIMBA_READ_CACHE_DM_HIT_COUNTER", ",d"),
                ("DM Misses",   "KALIMBA_READ_CACHE_DM_MISS_COUNTER", ",d"),
                ("Slave Waits", "KALIMBA_READ_CACHE_SLAVE_WAIT_COUNTER", ",d") ],
                                     report)
          
    def _generate_report_body_elements(self):
        """
        Output useful firmware information
        """
        elements = []
        elements.extend(super(AppsCore, self)._generate_report_body_elements())
        elements.append(self.counters(True))
        elements.append(self.prefetch_counters(True))
        try:
            elements.append(self.cache_counters(True))
        except AddressSpace.NoAccess:
            elements.append(interface.Warning("Cache counter regs not available for reading"))
        elements.append(self.interrupt_state(True))
        elements.append(self.sqif_state(report=True))

        return elements
    
    @contextlib.contextmanager
    def sqif_readable(self):
        """
        Context manager for temporarily putting both the Apps cores into a state 
        where the SQIF is safe to read.
        """
        def stop_proc(p):
            if p.is_running:
                p.pause()
                return True
            return False
        
        p0 = self.subsystem.p0
        p1 = self.subsystem.p1
        p0_was_running = stop_proc(p0)
        p1_was_running = stop_proc(p1)
            
        # Remember what the divide rate was
        p0_clk_div = p0.fields.CLOCK_DIVIDE_RATE.read()
        p1_clk_div = p1.fields.CLOCK_DIVIDE_RATE.read()
        # Set it to a safe value
        p0.fields.CLOCK_DIVIDE_RATE = 1
        p1.fields.CLOCK_DIVIDE_RATE = 1
        # Now the context is set up
        yield
        # Finished with the context: put things back
        p0.fields.CLOCK_DIVIDE_RATE = p0_clk_div
        p1.fields.CLOCK_DIVIDE_RATE = p1_clk_div
        if p0_was_running:
            p0.run()
        if p1_was_running:
            p1.run()
    
    def get_sqif_window_offset(self, index_or_indices, in_readable_state=False):
        """
        Return the SQIF window offset(s) for a given index or list of indices.
        Returns an int if passed a single index or a list if passed a list.
        """
        with self.sqif_readable() if not in_readable_state else null_context():
            
            if isinstance(index_or_indices, int_type):
                index = index_or_indices
                self.fields.APPS_SYS_SQIF_WINDOW_CONTROL = index
                return self.fields.APPS_SYS_SQIF_WINDOW_OFFSET.read()
            else:
                indices = index_or_indices
                offsets = []
                for index in indices:
                    self.fields.APPS_SYS_SQIF_WINDOW_CONTROL = index
                    offsets.append(self.fields.APPS_SYS_SQIF_WINDOW_OFFSET.read())
                return offsets
    
    def sqif_state(self, pause_processors=False, report=False):
        '''
        Output text or a report showing the sqif decrypt and window offset
        settings. 
        '''
        output = interface.Group("SQIF state")
        
        p0 = self.subsystem.p0
        p1 = self.subsystem.p1

        if pause_processors or (not p0.is_running and not p1.is_running):

            @contextlib.contextmanager
            def sqif_selected(isqif):
                """
                Simple context manager that ensures the chip's SQIF selection is
                always restored.
                """
                sqif_sel = self.fields.APPS_SYS_SQIF_SEL.read()
                self.fields.APPS_SYS_SQIF_SEL = isqif
                yield
                self.fields.APPS_SYS_SQIF_SEL = sqif_sel

            with self.sqif_readable():
                for sqif in range(2):
                    with sqif_selected(sqif):
                        output.append(self._print_list_regs("SQIf %d" % sqif, [
                            ("Decrypt Key","READ_DECRYPT_KEY", "x"),
                            ("Decrypt nonce", "READ_DECRYPT_NONCE", "x"),
                            ("Enable Key", "READ_DECRYPT_KEY_ENABLE", "x"),
                            ("Decrypt Control", "READ_DECRYPT_CONTROL", "x"),
                            ("ClearText Base", "READ_DECRYPT_CLEARTEXT_BASE", "x"),
                            ("ClearText Size", "READ_DECRYPT_CLEARTEXT_SIZE", "x"),
                            ], True))
                
                # Grab the offset names from the register enums
                offset_dict = {getattr(self.iodefs,k):k[14:-5] 
                                  for k in dir(self.iodefs) 
                                      if k.startswith("APPS_SYS_SYS__SQIF")}
                max_value = 17
                    
                offsets = self.get_sqif_window_offset(list(range(max_value+1)), 
                                                      in_readable_state=True)
                for i in range(max_value+1):
                    try:
                        name = offset_dict[i]
                    except KeyError:
                        name = "Index %d" % i
                    output.append(interface.Code("%36s : 0x%x\n" % (name, offsets[i])))
        else:
            output.append(interface.Code(
                 "SQIF window offsets can only be read with processors paused"))
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)
         
    @property
    def num_brk_regs(self):
        return 8
    
    @property
    def _hif_subsystem_view(self):
        return self.subsystem.hif

    @property
    def interrupt(self):
        try:
            return self._interrupt
        except AttributeError:
            self._interrupt = Interrupt(self)
        return self._interrupt
    
    def interrupt_state(self, report=False):
        
        output = interface.Group("INTERRUPT STATE")
        
        try:
            # Oxygen
            block_count = self.fw.env.cast(self.fw.env.globalvars["$interrupt.block_count"].address, "uint32")
            output.append(interface.Code("$interrupt.block_count  : %14s" % format(block_count.value)))
        except KeyError:
            try:
                # FreeRTOS
                # The top bit in the interrupt block count byte stores whether the scheduler should yield at the end
                # of the critical section. See port.c.
                yield_and_block_count = self.fw.env.var.yield_and_block_count.value

                block_count = yield_and_block_count & 0x7f
                output.append(interface.Code("interrupt block count   : {:>14}".format(block_count)))

                _yield = bool(yield_and_block_count & 0x80)
                output.append(interface.Code("yield on unblock        : {:>14s}".format(str(_yield))))
            except AttributeError:
                output.append(interface.Code("Cannot access interrupt block count. No firmware information available."))

        output.append(self.interrupt.state(True))

        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)

    @property
    def _is_running_from_rom(self):
        """
        The Apps firmware is always in SQIF
        """
        return False
    
    def _set_cache_sel(self):
        orig_value = self.fields.APPS_SYS_CACHE_SEL.read()
        self.fields.APPS_SYS_CACHE_SEL = self.processor_number
        return orig_value
    

    def clock_rate(self):
        # We just look at the Curator to see what the clock to the Apps
        # is and then apply the correct divider.
        cur = self.subsystem.chip.curator_subsystem.core
        apps_cpu_clk_code = cur.bitfields.CURATOR_APPS_CLK_SOURCE.read()
        root_clock_MHz = {0 : 32, #?? FOSC
                          1 : 32,# XTAL
                          2 : 80, # PLL
                          }[apps_cpu_clk_code]
        divider_code = self.fields.CLOCK_DIVIDE_RATE.read()
        multiplier = {0 : 0, # stopped,
                      1 : 1, # max
                      2 : 0.5, # half
                      }[divider_code]
        return int(root_clock_MHz * multiplier)


         
class AppsP0Core(AppsCore):
    """
    This class represents the general case of the Apps P0 memory map.  The
    CSRA68100 D00 memory map is somewhat different, however, and is implemented
    via an overriding subclass.
    """
    def __init__(self, subsystem, access_cache_type):
        '''
        Create the fundamental memory blocks
        '''
        AppsCore.__init__(self, subsystem)
        self.processor_number = 0
        self._program_memory = AddressMap("P0_PROGRAM_MEMORY", access_cache_type, 
                                    length= 0x810000, word_bits=8)
        
    nicknames = ("apps0", "apps")

    @property
    def firmware_type(self):
        return self.default_firmware_type
    
    @property
    def default_firmware_type(self):
        """
        Return a suitable type for the default firmware object, which provides
        the limited information that's available about the firmware just from
        the SLT, without any ELF/DWARF info.
        """
        return self.subsystem.default_firmware_type

    @property
    def core_commands(self):
        '''
        Dictionary of commands (or other objects) you want to be registered
        as globals in interactive shells when this core has focus.
        '''
        common_core_cmds, exception_list = self._common_core_commands()
        
        p0_only_commands = {
            #Buf/MMU
            'buf_list'      : "self.subsystem.mmu.buf_list",
            'buf_read'      : "self.subsystem.mmu.buf_read",
           }
        
        
        commands_dict = dict(list(p0_only_commands.items()) +
                             list(common_core_cmds.items()))

        # Only available if the apps0 elf is provided
        p0_only_prim_commands = {
            # Bluestack primitive logging
            'prim_log'          : "self.fw.prim_log.generate_decoded_event_report",
            'prim_live_log'     : "self.fw.prim_log.prim_live_log",
            'prim_log_xml'      : "self.fw.prim_log.generate_decoded_event_report_xml",
            'prim_live_log_xml' : "self.fw.prim_log.prim_live_log_xml",
           }
        commands_dict.update(list(p0_only_prim_commands.items()))

        exception_list += [FirmwareComponent.NotDetected, AttributeError,
                Dwarf_Reader.NotAGlobalException, KeyError]

        return commands_dict, exception_list
    
    @property
    def firmware_build_info_type(self):
        from csr.dev.fw.meta.i_firmware_build_info import HydraAppsP0FirmwareBuildInfo
        return HydraAppsP0FirmwareBuildInfo

