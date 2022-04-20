############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import sys, os
from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.hw.core.kal_core import KalCore
from csr.wheels import NameSpace
from ..hw.address_space import AddressMap, AddressSlavePort, NullAccessCache
import time
import csr

try:
    # Dont freak out if kalimba lab is not there. Just record it.
    scriptpath = r"c:\KalimbaLab21c\pythontools"
    sys.path.append(os.path.abspath(scriptpath))
    import kalimba_load
    kalimba_load_available = 1
except ImportError:
    kalimba_load_available = 0

class AudioCore(KalCore):
    """
        Audio functionality that extends either one of the audio cores
    """

    def __init__(self):
        KalCore.__init__(self)
        self._apps = None
        self._btcli = None
        self._patch_point = None
        self._curator = None
        self._omnicli = None
        self._tbus_interrupt = None
        self._cucmd = None
        self._fats = None
        self._is_partial = None
        self._sqif = None
    def is_silicon(self):
        if not self.is_partial() and self._subsystem.chip.version.raw == 0x46:
            return True

    def is_partial(self):
        # caching is required since trb stuff are not thread safe
        # and we are starting cucmds in haps5 with threading.
        if self._is_partial is None:
            if (self._subsystem.chip.version.variant ==
                self._subsystem.chip.version.FPGA):
                self._is_partial = False
            elif (self._subsystem.chip.version.variant ==
                self._subsystem.chip.version.FPGA_PARTIAL):
                self._is_partial = True
            else:
                self._is_partial = False # real chip
        return self._is_partial

    @property
    def core_commands(self):
        # Dictionary of commands (or other objects) you want to be registered
        # as globals in interactive shells when this core has focus.
        #
        ret = {
            #Logging 
            'log'       : "self.fw.debug_log.generate_decoded_event_report",
            'live_log'  : "self.fw.debug_log.live_log",
            # 'clear_log' : "self.fw.debug_log.clear",
            'reread_log': "self.fw.debug_log.reread",
            'log_level' : "self.fw.debug_log.log_level",
            'report'    : "self.subsystem.generate_report",
            'display_brks' : "self.brk_display",
            #Buf/MMU
            'buf_list'  : "self.subsystem.mmu.buf_list",
            'buf_read'  : "self.subsystem.mmu.buf_read",
            #Symbol lookup
            # 'psym'      : "self.sym_.psym",
            # 'dsym'      : "self.sym_.dsym",
            # 'dispsym'   : "self.sym_.dispsym",
            # 'sym'       : "self.sym_.sym",
            #Raw memory access
            # 'bitz'      : "self.bitz_report",
            'fw_ver'    : "self.fw.fw_ver",
            'patch_ver'    : "self.fw.patch_ver",
            #Misc
            # 'stack'     : "self.fw.stack_report",
            "struct" : "self.fw.env.struct"
        }

        exception_list = [FirmwareComponent.NotDetected, AttributeError]
            
        return ret, exception_list

    @property
    def apps(self):
        """
            Initialize apps object. Contains a link to apps core
        """
        if self._apps is None:
            self._apps = self._subsystem._chip.apps_subsystem.core
        return self._apps

    def run(self):
        """
            Start the audio processor.
                haps5 - this is done through kalimba_load
        """
        if self.is_partial():
            if kalimba_load_available:
                kal = kalimba_load.KalSpi()
                kal.connect("usb")
                kal.run()
                kal.disconnect()
        else:
            return super(AudioCore, self).run()

    def pause(self):
        """
            Stop the processor from running.
                Haps5 - this is done through kalimba_load
        """
        if self.is_partial():
            if kalimba_load_available:
                kal = kalimba_load.KalSpi()
                kal.connect("usb")
                kal.pause()
                kal.disconnect()
        else:
            return super(AudioCore, self).pause()

    def load_ram(self, path, verbose=0, pc=None):
        """
            @brief Loads a build into ram.
            @param path Path for the RAM build that contains pm and dm files
            @param verbose Increase verbosity: Default is set to minimum level
            @param Set the programe counter. Default is: 0x4000000
        """

        if not pc:
            pc = 0x4000000
        self.pause()
        self.fields.DOLOOP_CACHE_CONFIG = 0x0
        load_ram_contents(path, verbose=verbose)
        self.fields.REGFILE_PC = pc
        self.run()

    def setup_uart(self, verbose=0):
        """
            Setup BT uart comms if needed.
        """
        if not self.is_partial():
            return

        iprint("Setting up UART")
        fpga_reg_read = self._subsystem.chip.fpga_reg_read
        fpga_reg_write = self._subsystem.chip.fpga_reg_write

        if verbose:
            b_fpga_pio_amber_sel0_lh = fpga_reg_read("FPGA_PIO_AMBER_SEL0_LH")
            b_fpga_pio_amber_sel0_uh = fpga_reg_read("FPGA_PIO_AMBER_SEL0_UH")
            b_fpga_pio_amber_sel1_lh = fpga_reg_read("FPGA_PIO_AMBER_SEL1_LH")
            b_fpga_pio_amber_sel1_uh = fpga_reg_read("FPGA_PIO_AMBER_SEL1_UH")

        fpga_reg_write("FPGA_PIO_AMBER_SEL0_LH", 0x8000)
        fpga_reg_write("FPGA_PIO_AMBER_SEL0_UH", 0x0002)
        fpga_reg_write("FPGA_PIO_AMBER_SEL1_LH", 0x8000)
        fpga_reg_write("FPGA_PIO_AMBER_SEL1_UH", 0x0002)

        if verbose:
            a_fpga_pio_amber_sel0_lh = fpga_reg_read("FPGA_PIO_AMBER_SEL0_LH")
            a_fpga_pio_amber_sel0_uh = fpga_reg_read("FPGA_PIO_AMBER_SEL0_UH")
            a_fpga_pio_amber_sel1_lh = fpga_reg_read("FPGA_PIO_AMBER_SEL1_LH")
            a_fpga_pio_amber_sel1_uh = fpga_reg_read("FPGA_PIO_AMBER_SEL1_UH")

            iprint("UART related registers :")
            iprint("FPGA_PIO_AMBER_SEL0_LH   0x{before:04x}   0x{after:04x}".format(
                before = b_fpga_pio_amber_sel0_lh,
                after = a_fpga_pio_amber_sel0_lh))
            iprint("FPGA_PIO_AMBER_SEL0_UH   0x{before:04x}   0x{after:04x}".format(
                before = b_fpga_pio_amber_sel0_uh,
                after = a_fpga_pio_amber_sel0_uh))
            iprint("FPGA_PIO_AMBER_SEL1_LH   0x{before:04x}   0x{after:04x}".format(
                before = b_fpga_pio_amber_sel1_lh,
                after = a_fpga_pio_amber_sel1_lh))
            iprint("FPGA_PIO_AMBER_SEL1_UH   0x{before:04x}   0x{after:04x}".format(
                before = b_fpga_pio_amber_sel1_uh,
                after = a_fpga_pio_amber_sel1_uh))

    def reset_fpga(self):
        """
            Reset the fpga
        """
        if self.is_partial():
            while True:
                try:
                    self._subsystem.chip.fpga_reg_write("FPGA_RESET_ALL", 1)
                    time.sleep(0.5)
                    self._subsystem.chip.amber_reset()
                    time.sleep(0.5)
                    break
                except:
                    pass
        else:
            self._subsystem.chip.fpga_reg_write("FPGA_RESET_ALL", 1)

        # give the curator time to boot
        csr.dev.attached_device._wait_for_curator(self._subsystem._curator.core, True, True)
        self.setup_uart()

    def get_firmware_id_string(self, reset=0):
        """
            Retrieve the firmware id string from the device.
            This will attempt to setup the comms if the first get_firmware_id_string
            fails.
        """
        if(reset):
            self.cucmd.setup(reset)
        (ret, id_string) = self.omnicli.get_firmware_id_string()
        return (ret, id_string)

    def get_firmware_version(self, reset=0):
        """
            Retrieve the firmware version. Not implemented
        """
        if(reset):
            self.cucmd.setup(reset)
        (ret, fw_ver) = self.omnicli.get_firmware_version()
        return (ret, fw_ver)

    def get_patch_id(self, env=None):
        """
        Patch ID retrieval method.  Use SLT if possible, else (e.g. FakeSLT
        does not have it), if env is supplied, we could use that; otherwise
        look for the env attached to this core
        """

        from csr.dev.fw.slt import SymbolLookupError
        try:
            return self.fw.slt.patch_id_number
        except NotImplementedError:
            return None # AudioP1 typically
        except (SymbolLookupError, AttributeError):
            if not env:
                env = self.fw.env
            try:                
                # Here try to look it up by variable name
                return env.var.patched_fw_version.value
            except AttributeError:
                return None
            
class AudioVMCore(AudioCore):
    """
    Base class for Audio cores in Voice and Music chips
    """
    def populate(self, access_cache_type):
        """
        Create the core-specific memory entities ready to be mapped together
        later
        """
        self._components = NameSpace()
        comps = self._components # shorthand
        
        # Setup a minimum access width here and use to to setup all the address
        # maps since we require it for all PM/DM regions.
        maw = 4
        comps.proc_data_map = AddressMap("PROC_DATA", NullAccessCache,
                                         length = 0x100000000, 
                                         layout_info=self.info.layout_info,
                                         min_access_width=maw)
        comps.proc_pm_map = AddressMap("PROC_PM", NullAccessCache,
                                         length = 0x100000000, word_bits=8,
                                         min_access_width=maw)
        

        # Create local objects to represent the remainder of dm: in principle
        # these could be replaced with references to "real" memory spaces owned
        # by independent models of the relevant hardware if that ever becomes
        # useful
        
        comps.private_ram = AddressSlavePort("PRIVATE_RAM", access_cache_type,
                                             length=0x1000,
                                             word_bits=8,
                                             min_access_width=4)
        
        for i in range(self.subsystem.NUM_PM_NVMEM_WINDOWS):

            setattr(comps, "pm_nvmem_w%d"%i,
                    AddressMap("PM_NVMEM_W%d"%i,
                               access_cache_type,
                               length= 0x800000,
                               layout_info = self.info.layout_info,
                               min_access_width=maw))

        for i in range(self.subsystem.NUM_PM_RAM_BANKS):
            mem = AddressSlavePort("PM_BANK%d" % i, access_cache_type,
                                   length=0x2000,
                                   layout_info = self.info.layout_info,
                                   min_access_width=maw)
            setattr(comps, "pm_bank%d" % i, mem)

        # DM blocks not created here - these are owned by the subsystem as they
        # are shared between processors.

        for i in range(self.subsystem.NUM_REMOTE_BAC_WINDOWS):
            setattr(comps, "remote_bac_w%d"%i,
                    AddressMap("REMOTE_BAC_WINDOW_%d"%i,
                                access_cache_type,
                                length=0x100000,
                                layout_info = self.info.layout_info,
                                min_access_width=maw))
        
        for i in range(self.subsystem.NUM_DM_NVMEM_WINDOWS):
            setattr(comps, "dm_nvmem_w%d"%i,
                    AddressMap("DM_NVMEM_WINDOW_%d" % i,
                                access_cache_type,
                                length= 0x800000,
                                layout_info = self.info.layout_info,
                                min_access_width=maw))

            
        # Subsystem-level common regs are owned by the subsystem

        comps.cpu_regs = AddressMap("CPU_REGS",
                                access_cache_type,
                                length= 0xFFFFFE00 -0xFFFFE000,
                                layout_info = self.info.layout_info,
                                min_access_width=maw)

        comps.core_debug_regs = AddressMap("CORE_DEBUG_REGS",
                                access_cache_type,
                                length= 0x100000000 - 0xFFFFFE00 ,
                                layout_info = self.info.layout_info,
                                min_access_width=maw)

        #We have to remember this so that SPI map construction can refer to
        #it whenever it is called upon
        self.access_cache_type = access_cache_type

    def create_data_space(self):
        
        self._populate_processor_data_view(self._components.proc_data_map)

    def create_program_space(self):
        
        self._populate_processor_pm_view(self._components.proc_pm_map) 
            

    def _populate_processor_data_view(self, map, include_dm2_mappings=True):
        """
         Populate the main PROC memory map for this core.
        
         Ref: CS-205120-SP-1E
         
        """
        
        comps = self._components

        dm_bank_size = self.subsystem.RAM_BANK_SIZE

        # Add views for DM1
        for i in range(self.subsystem.NUM_120MHZ_RAM_BANKS):
            if i == 0:
                map.add_mapping(0x1000,(i+1)*dm_bank_size, self.subsystem.dm_banks[i],0x1000,
                            view=self.data_view)
            else:
                map.add_mapping(i*dm_bank_size,(i+1)*dm_bank_size, self.subsystem.dm_banks[i],
                            view=self.data_view)
        for i in range(self.subsystem.NUM_240MHZ_RAM_BANKS):
            index = i + self.subsystem.NUM_120MHZ_RAM_BANKS
            map.add_mapping(index*dm_bank_size,(index+1)*dm_bank_size, 
                            self.subsystem.dm_banks[index],
                            view=self.data_view)
            
        if include_dm2_mappings:
            # Add views for DM2
            dm2_offset = self.subsystem.DM2_OFFSET
            for i in range(self.subsystem.NUM_120MHZ_RAM_BANKS):
                if i == 0:
                    map.add_mapping(dm2_offset+i*dm_bank_size + 0x1000,dm2_offset+(i+1)*dm_bank_size, 
                                    self.subsystem.dm_banks[i], 0x1000,
                                    view=self.data_view)
                else:
                    map.add_mapping(dm2_offset+i*dm_bank_size,dm2_offset+(i+1)*dm_bank_size, 
                                    self.subsystem.dm_banks[i],
                                    view=self.data_view)
            for i in range(self.subsystem.NUM_240MHZ_RAM_BANKS):
                index = i + self.subsystem.NUM_120MHZ_RAM_BANKS
                map.add_mapping(dm2_offset+index*dm_bank_size,dm2_offset+(index+1)*dm_bank_size, 
                                self.subsystem.dm_banks[index],
                                view=self.data_view)
            
        # Map private ram in (where it appears to the processor)
        #
        # NB If private ram is not enabled, this is slightly problematic:
        # while on a live DUT both p0's and p1's private_ram spaces will route
        # calls the same way and so will act (correctly) as the same object, on a
        # coredump it could be that only one of them gets populated.  However
        # it appears that when private RAM is not enabled the coredump contains
        # a single dataset which happens to cause the coredump loader to load
        # both processors, so we get away with it.
        if self.processor == 0:
            map.add_mapping(0,0x1000, comps.private_ram, view=self.data_view)
            if include_dm2_mappings:
                map.add_mapping(self.subsystem.DM2_OFFSET,
                                self.subsystem.DM2_OFFSET+0x1000, 
                                comps.private_ram,
                                view=self.data_view)
        else:
            map.add_mapping(0,0x1000, comps.private_ram, view=self.data_view)
            if include_dm2_mappings:
                map.add_mapping(0xfff00000,0xfff01000, comps.private_ram,
                                view=self.data_view)
            
            
        start = 0x800000
        for i in range(self.subsystem.NUM_REMOTE_BAC_WINDOWS):
            end = start + 0x100000
            map.add_mapping(start, end, getattr(comps, "remote_bac_w%d"%i).port,
                            view=self.data_view)
            start = end

        start = 0xF8000000
        for i in range(self.subsystem.NUM_DM_NVMEM_WINDOWS):
            end = start + 0x800000
            map.add_mapping(start, end, getattr(comps, "dm_nvmem_w%d"%i).port,
                            view=self.data_view, group="const space")
            start = end
        
        map.add_mappings(
            (self.subsystem.MAPPED_PM_RAM_START,
             self.subsystem.MAPPED_PM_RAM_START+self.subsystem.MAPPED_PM_RAM_SIZE, 
             self.subsystem.mapped_pm_ram),
            (0xFFFF8000, 0xFFFFC000, self.subsystem.common_regs.port),
            (0xFFFFC000, 0xFFFFE000, self.subsystem.cpu_subsys_regs.port),
            (0xFFFFE000, 0xFFFFFE00, comps.cpu_regs.port),
            (0xFFFFFE00, 0xFFFFFFFF, comps.core_debug_regs.port),
            view=self.data_view)

    def _populate_processor_pm_view(self, map):
        comps = self._components

        start = 0
        for i in range(self.subsystem.NUM_PM_NVMEM_WINDOWS):
            end = start + 0x100000
            map.add_mapping(start, end, getattr(comps, "pm_nvmem_w%d"%i).port,
                            view=self.prog_view)
            start = end
            
        pm_ram_start = 0x4000000
        start = pm_ram_start
        for i in range(self.subsystem.NUM_PM_RAM_BANKS):
            bank = getattr(comps, "pm_bank%d" % i)
            map.add_mapping(start, start+len(bank), bank,
                            view=self.prog_view)
            start += len(bank)
    
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
        """
        Populate the trb data map: the differences from the processor DM and PM
        views are taken care of by the view-munging mechanism
        """
        self._populate_processor_data_view(trb_data_map, include_dm2_mappings=False)
        self._populate_processor_pm_view(trb_data_map)

    def emulate_hardware_windows(self):
        """
        When running against a coredump the hardware windows into PM from
        DM aren't available directly because we don't dump those parts of
        DM.  We could manually load them from PM, but we might as well make
        use of the address mapping mechanism to save us the trouble and the
        memory. 
        """
        comps = self._components
        start = 0
        for i in range(self.subsystem.NUM_DM_NVMEM_WINDOWS):
            end = start + 0x800000
            map = getattr(comps, "dm_nvmem_w%d"%i)
            map.add_mapping(start, end, self.program_space, start)
            start = end

    @property
    def _is_running_from_rom(self):
        return True

    def clock_rate(self):
        try:
            self.fields.CLKGEN_AUDIO_CLK_DIV_SELECT.CLKGEN_DIV_SELECT_ROOT_CLK_ACTIVE
        except AttributeError:
            is_qcc512x_qcc302x = True
        else:
            is_qcc512x_qcc302x = False
            
        if is_qcc512x_qcc302x:
            # We just look at the Curator to see what the clock to the Audio CPU
            # is and then apply the correct divider.
            cur = self.subsystem.chip.curator_subsystem.core
            audio_cpu_clk_code = cur.bitfields.CURATOR_AUDIO_CPU_CLK_SOURCE.read()
            root_clock_MHz = {0 : 2, # AOV
                              1 : 32,# XTAL
                              2 : 80, # PLL
                              3 : 120 # PLL_TURBO
                              }[audio_cpu_clk_code]
            divider_code = self.fields.CLOCK_DIVIDE_RATE.read()
            multiplier = {0 : 0, # stopped,
                          1 : 1, # max
                          2 : 0.5, # half
                          }[divider_code]
            return int(root_clock_MHz * multiplier)
        
        # For QCC514X_QCC304X we need to take account of the ADPLL
        iprint("ADPLL-based audio CPU clock rate calculation not implemented")
        return 0
        