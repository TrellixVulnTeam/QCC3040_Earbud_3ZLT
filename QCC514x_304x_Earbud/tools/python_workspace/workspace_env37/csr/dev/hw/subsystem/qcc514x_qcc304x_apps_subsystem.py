############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .mixins.is_qcc514x_qcc304x import IsQCC514X_QCC304X
from .apps_subsystem import AppsSubsystem
from .hydra_subsystem import HydraSubsystem
from csr.dev.hw.address_space import AddressMap, NullAccessCache, \
BankedAddressSpace, AccessView
from csr.dev.hw.core.meta.i_core_info import Kalimba32CoreInfo
import time

class QCC514X_QCC304XAppsSubsystem(AppsSubsystem, IsQCC514X_QCC304X):
    '''
    Class representing an QCC514X_QCC304X-flavoured Apps subsystem
    
    Note: we inherit from IsCSRA68100 on principle, but in fact the only 
    thing this gives us is _create_host_subsystem_view, which isn't useful
    because the Apps memory map has all the HIF subsystems mapped in separately.
    '''
    P0_DATA_RAM_SIZE  = 40
    P1_DATA_RAM_SIZE  = 40
    SHARED_RAM_SIZE   = 32
    TCM0_SIZE         = 24
    TCM1_SIZE         = 16
    P0_CACHE_RAM_SIZE = 32
    P1_CACHE_RAM_SIZE = 16
    P0_CACHE_TAG_RAM_SIZE = 2
    P1_CACHE_TAG_RAM_SIZE = 1
    
    def __init__(self, chip, ss_id, access_cache_type):
        
        AppsSubsystem.__init__(self, chip, ss_id, access_cache_type)


    def safe_state(self):
        """
        Force the subsystem into a 'safe' known state. This will stop
        the subsystem running, reset it and 
        """
    
        #Get the Curator object to access it's registers
        cur = self.curator.core
    
        #Pause the Curator to prevent it altering registers
        cur.pause()
        
        #Put the APPS subsystem into a known state.
        # QCC512X_QCC302X needs an additional clock enable - clock sources possibly in future too
        cur.fields.CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES.CURATOR_SUBSYS_CORE_CLK_ENABLES_APPS = 1
    
        cur.fields['CURATOR_SUBSYSTEMS_RUN_EN']  = 0x03
        #Power cycle the APPS subsystem
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x03
        time.sleep(0.2)
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x13
        time.sleep(0.2)
        
    def config_sqif_pios(self,bank=0):
        """
        The firmware should request these as resources from the Curator.
        During testing it may be necessary to forcible take these PIOs
        """
        #Get the Curator object to access it's registers
        cur = self.curator.core
        
        #Configure the SQIF's PIOs
        # This is QCC512X_QCC302X specific
        if bank == 0:
            # SQIF 1 APPS0 -> PIO09..PIO14
            cur.fields['CHIP_PIO9_PIO11_MUX_CONTROL'] = 0x0bbb
            cur.fields['CHIP_PIO12_PIO15_MUX_CONTROL'] = 0x0bbb
        
            #SQIF register clock enable, this SHOULD be unnecessary
            self.p0.fields.CLKGEN_ENABLES.CLKGEN_SQIF0_REGS_EN = 1
        else:
            # SQIF 2 APPS1 -> PIO34..PIO39
            cur.fields['CHIP_PIO32_PIO35_MUX_CONTROL'] = 0xbb00
            cur.fields['CHIP_PIO36_PIO39_MUX_CONTROL'] = 0xbbbb
        
            #SQIF register clock enable, this SHOULD be unnecessary
            self.p1.fields.CLKGEN_ENABLES.CLKGEN_SQIF1_REGS_EN = 1

    # CORE CLOCKS vaues that go into apps subsystem
    _core_clks = {
        0 : 32000.0, # Scaled
        1 : 80000.0, # PLL
        }
    
    def get_nr_of_processor_clocks_per_ms(self):
        """
        Nr of processor clocks per ms
        """
        cur = self.curator.core
        if (self._chip.is_emulation):
            return 20000.0
        else:
            core_clk_in_sel = int(cur.fields.CURATOR_SUBSYSTEMS_CLK_SOURCES.CURATOR_APPS_CLK_SOURCE.read())
            return self._core_clks[core_clk_in_sel]
    
    def get_current_subsystem_clock_mhz(self):
        # same as cpu clock on qcc512x_qcc302x
        return self.get_nr_of_processor_clocks_per_ms()/1000.0


class QCC514X_QCC304XAppsD00Subsystem(QCC514X_QCC304XAppsSubsystem):
    def __init__(self, chip, ss_id, access_cache_type):
        # cache the bus interrupt registers bank start address,
        # we will need it in QCC512X_QCC302XAppsSubsystem.__init__
        from csr.dev.hw.core.qcc514x_qcc304x_apps_core import QCC514X_QCC304XAppsD00CoreInfo
        core_info = QCC514X_QCC304XAppsD00CoreInfo(custom_digits=chip.emulator_build)
        if core_info.custom_io_struct:
            BUS_INT_MASK = getattr(core_info.custom_io_struct, "BUS_INT_MASK")
        else:
            from csr.dev.hw.io.qcc514x_qcc304x_apps_d00_io_struct import BUS_INT_MASK
        self._bus_int_bank_start = BUS_INT_MASK.addr
        
        QCC514X_QCC304XAppsSubsystem.__init__(self, chip, ss_id, access_cache_type)

    @property
    def _core_types(self):
        from csr.dev.hw.core.qcc514x_qcc304x_apps_core import \
            QCC514X_QCC304XAppsD00P0Core, QCC514X_QCC304XAppsD00P1Core
        return QCC514X_QCC304XAppsD00P0Core, QCC514X_QCC304XAppsD00P1Core


    @property
    def sqif_trb_address_block_id(self):
        '''
        Returns a list (indexed by sqif device number) of tuples of TRB address
        and block IDs at which the SQIF contents can be read.
        See http://cognidox/vdocs/CS-333748-DC-LATEST.pdf
        '''
        return [(0xB0000000, 0), (0xD0000000, 0)]
