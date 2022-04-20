############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import time
from contextlib import contextmanager
from csr.wheels.bitsandbobs import PureVirtualError, NameSpace
from csr.dev.hw.address_space import BlockIDAccessView, AddressMap, \
AddressSlavePort, BankedAddressSpace
from csr.dev.hw.subsystem.hydra_subsystem import HydraSubsystem
from csr.dev.hw.subsystem.mixins.has_mmu import HasMMU
from csr.dev.hw.sqif import AudioSqifInterface
from csr.dev.hw.trace import TraceLogger
from ..core.meta.i_layout_info import Kalimba32DataInfo


class AudioSubsystem(HydraSubsystem, HasMMU):
    '''
    Class representing a generic Audio subsystem
    '''

    @property
    def name(self):
        return "Audio"

    @property
    def subcomponents(self):
        cmps = HydraSubsystem.subcomponents.fget(self)
        cmps.update(HasMMU.subcomponents.fget(self))
        cmps.update({"p0" : "_p0",
                     "p1" : "_p1"})
        return cmps

    @property
    def number(self):
        """
        The subsystem number (not to be confused with the SSID)
        as defined by csr.dev.hw.chip.hydra_chip.FixedSubsystemNumber.
        """
        from csr.dev.hw.chip.hydra_chip import FixedSubsystemNumber
        return FixedSubsystemNumber.AUDIO

    @property
    def cores(self):
        """
        We have two Kalimba cores in this subsystem, but only one is implemented
        so far
        """
        return [self.p0, self.p1]

    @property
    def core(self):
        '''
        For convenience, since this is the one we'll generally be more interested
        in.  But that might change.
        '''
        return self.p0

    @property
    def p0(self):
        try:
            self._p0
        except AttributeError:
            self._p0, self._p1 = self._create_audio_cores(self._access_cache_type)
        return self._p0

    @property
    def p1(self):
        try:
            self._p1
        except AttributeError:
            self._p0, self._p1 = self._create_audio_cores(self._access_cache_type)
        return self._p1

    # Required overrides
    @property
    def _view_type(self):
        """
        Apps subsystems in general (CSRA68100 D00 is the exception) access
        different views of memory over TRB using the block ID to specify the
        view 
        """
        return BlockIDAccessView

    @property
    def spi_in(self):
        """\
        This subsystem's SPI AddressSlavePort.
        Used to wire up the chip's memory access model.
        
        It is not usually addressed directly but is needed
        to model the spi access route.
        
        """
        raise NotImplementedError("audio subsystem doesn't have (conventional) SPI")

    @property
    def spi_data_map(self):
        raise NotImplementedError("audio subsystem doesn't have (conventional) SPI")

    def _create_trb_map(self):
        '''
        The TRB data map looks different on different subsystems
        '''
        raise PureVirtualError()

    def _populate_trb_data_map(self, trb_data_map):
        """
        Add the memory blocks that the subsystem owns
        """

    @property
    def firmware_type(self):
        #Because Audio has two processors, each with their own firmware
        #it makes no sense for the Subsystem to have this method
        raise NotImplementedError("AudioSubsystem::firmware_type makes no sense")

    @property
    def default_firmware_type(self):
        from csr.dev.fw.audio_firmware import AudioDefaultFirmware
        return AudioDefaultFirmware

    @property
    def patch_type(self):
        # Although Audio has two processors, each with their own firmware
        # it makes some sense for the Subsystem to have this method
        # because patch types are very high level - "Hydra", "Zeagle", etc.
        # You aren't going to get different patch types at the same time within
        # the same subsystem.
        from csr.dev.fw.patch import HydraAudioPatch
        return HydraAudioPatch

    @property
    def firmware_build_info_type(self):
        #Because Audio has two processors, each with their own firmware
        #it makes no sense for the Subsystem to have this method
        raise NotImplementedError("AudioSubsystem::firmware_build_info_type makes no sense")       

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
        The Audio SPI keyhole data map may look different on different flavours of the
        subsystem 
        """
        raise PureVirtualError()

    @property
    def sqif(self):
        try:
            self._sqifs
        except AttributeError:
            #Subsystem has one serial flash interface.
            self._sqifs = AudioSqifInterface(self.core)
        return self._sqifs
    @property

    def tracelogger(self):
        try:
            self._tracelogger
        except AttributeError:
            self._tracelogger = TraceLogger(self.cores)
        return self._tracelogger

    # HasMMU override
    def _create_mmu(self):
        from ..mmu import AudioBAC
        return AudioBAC(self)

    def bulk_erase(self,bank=0,address = None):
        """\
        Most basic way to completely erase a SQIF. 
        
        ONlY Uses register peeks and pokes so doesn't need to have have had 
        firmware specified.
        
        SHOULD be able to erase a SQIF regardless of the system state.
        """
        
        self.safe_state()
        self.sqif_clk_enable()
        self.sqif.minimal_config()
        self.config_sqif_pios(bank) #Have the SQIF HW configured before this.
        self.sqif.bulk_erase_helper(byte_address = address) 

    def safe_state(self):
        """
        Force the subsystem into a 'safe' known state. This will power
        off all subsystems, and only repower AUDIO without letting it run.
        """
        
        #Get the Curator object to access it's registers
        cur = self.curator.core

        #Put the Audio subsystem into a known state.
        cur.fields['CURATOR_SUBSYSTEMS_CLK_80M_ENABLED'] = 0xb
        cur.fields['CURATOR_SUBSYSTEMS_CLK_240M_ENABLED'] = 0xb
        
        #In D01 the Curator has control of the SQIF clock.
        if hasattr(cur.fields,'CURATOR_SUBSYSTEMS_SQIF_CLK_80M_ENABLED'):
            #If the register exists it needs to be set. 
            cur.fields.CURATOR_SUBSYSTEMS_SQIF_CLK_80M_ENABLED = 0xb
            
        cur.fields['CURATOR_SUBSYSTEMS_RUN_EN']  = 0x03
        #Power cycle the Audio subsystem
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x03
        time.sleep(0.2)
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0xb
        time.sleep(0.2)
        
    def config_sqif_pios(self,bank=0):
        raise PureVirtualError()
        

    def sqif_clk_enable(self):
        """
        Enable the SQIF clock
        """
        pass

    def _adjust_memory_report(self, report):
        """\
        In the Audio subsystem we modify the memory report output to show all the memory areas in a linear
        sequence rather than split between DM1 0x000x_xxxx and DM2 0xfffx_xxxx. We put the DM2 addresses in
        as comments.
        """
        for item in report:
            if item["start"] >= 0xfff00000:
                new_comment = "Logical Addr: 0x%08x - 0x%08x" % (item["start"], item["end"])
                try:
                    current_comment = item["comment"]
                    new_comment += ", " + current_comment
                except KeyError:
                    pass
                item["comment"] = new_comment

                item["start"] = item["start"] & 0x000fffff
                item["end"] = item["end"] & 0x000fffff
        return report


class AudioVMSubsystem(AudioSubsystem):
    
    def __init__(self, chip, ss_id, access_cache_type):
        
        super(AudioVMSubsystem, self).__init__(chip, ss_id, access_cache_type)
        
        self.dm_banks = []
        maw = 4
        layout_info = Kalimba32DataInfo()
        
        # Create the memory entities that are shared between the processors
        
        for i in range(self.NUM_120MHZ_RAM_BANKS):
            
            # I *think* slow RAM is byte-accessible
            mem = AddressSlavePort("DM_BANK%d" % i, access_cache_type,
                                   length=0x8000,
                                   layout_info=layout_info,
                                   min_access_width=4)
            self.dm_banks.append(mem)
            
        for i in range(self.NUM_240MHZ_RAM_BANKS):
            index = (i + self.NUM_120MHZ_RAM_BANKS)
            mem = AddressSlavePort("DM_BANK%d" % index,
                                    access_cache_type,
                                    length= 0x8000,
                                    layout_info = layout_info,
                                    min_access_width=maw)
            self.dm_banks.append(mem)

        self.mapped_pm_ram = AddressSlavePort("MAPPED_PM_RAM",
                                              access_cache_type,
                                              length= self.MAPPED_PM_RAM_SIZE,
                                              layout_info = layout_info)

        self.common_regs = AddressMap("COMMON_REGS",
                                      access_cache_type,
                                      length= 0xFFFFC000 - 0xFFFF8000 ,
                                      layout_info = layout_info,
                                      min_access_width=maw)
        self.common_regs_pcm_bank = BankedAddressSpace("AUDIO_PCM_REG_BANK_SELECT", 
                                                       self, 
                                                       "COMMON_REGS__PCM_BANK",
                                                       access_cache_type, 
                                                       length=0xcb8-0xc40,
                                                       layout_info=layout_info)
        self.common_regs_spdif_bank = BankedAddressSpace("AUDIO_SPDIF_REG_BANK_SELECT",
                                                         self,
                                                         "COMMON_REGS__SPDIF_BANK",
                                                         access_cache_type,
                                                         length=0xd2c-0xcc0)
        
        self.common_regs.add_mappings((0xc40, 0xcb8, self.common_regs_pcm_bank),
                                      (0xcc0, 0xd2c, self.common_regs_spdif_bank))

        self.cpu_subsys_regs = AddressMap("CPU_SUBSYS_REGS",
                                          access_cache_type,
                                          length= 0xFFFFE000 - 0xFFFFC000,
                                          layout_info = layout_info,
                                          min_access_width=maw)

    @property
    def has_per_core_firmware(self):
        """
        Audio subsystem on QCC512X_QCC302X and CSRA68100 runs the same firmware for both
        P0 and P1
        """
        return False

    @contextmanager
    def ensure_powered(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is powered for an operation
        and wants to restore it to original state afterwards.
        """
        cur = self.chip.curator_subsystem.core
        powered = cur.fields.CURATOR_SUBSYSTEMS_POWERED.\
            CURATOR_SYS_POR__SYSTEM_BUS_AUDIO_SYS
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
        clock = cur.fields.CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES.\
            CURATOR_SUBSYS_CORE_CLK_ENABLES_AUDIO
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

    @property
    def patch_build_info_type(self):
        from csr.dev.fw.meta.i_firmware_build_info import\
            HydraAudioP0PatchBuildInfo
        return HydraAudioP0PatchBuildInfo