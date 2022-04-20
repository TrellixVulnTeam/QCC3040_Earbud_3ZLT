############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .mixins.is_qcc514x_qcc304x import IsQCC514X_QCC304X
from .audio_subsystem import AudioVMSubsystem
from csr.dev.hw.address_space import AddressMap, NullAccessCache, AccessView, \
                                    AddressMungingAccessView, ExtremeAccessCache
import time

class QCC514X_QCC304XAudioSubsystem(AudioVMSubsystem, IsQCC514X_QCC304X):
    '''
    Class representing a QCC514X_QCC304X-flavoured Audio subsystem
    
    Note: we inherit from IsQCC514X_QCC304X on principle, but in fact the only
    thing this gives us is _create_host_subsystem_view, which isn't useful
    because the Audio memory map has all the HIF subsystems mapped in separately.
    '''

    NUM_120MHZ_RAM_BANKS = 14
    NUM_240MHZ_RAM_BANKS = 0
    RAM_BANK_SIZE = 0x8000
    DM2_OFFSET = 0xfff00000
    NUM_REMOTE_BAC_WINDOWS = 4
    NUM_DM_NVMEM_WINDOWS = 8
    NUM_PM_NVMEM_WINDOWS = 4
    NUM_PM_RAM_BANKS = 5
    MAPPED_PM_RAM_SIZE = 0xc0000
    MAPPED_PM_RAM_START = 0x100000

    def __init__(self, chip, ss_id, access_cache_type, hw_version):
        
        super(QCC514X_QCC304XAudioSubsystem, self).__init__(chip, ss_id, access_cache_type)
        self._hw_version = hw_version
        

    def _create_trb_map(self):        
        trb_data_map = AddressMap("TRB_DATA", NullAccessCache,
                                  length = 0x100000000,word_bits=8,
                                  max_access_width=4,
                                  view_type=self._view_type)

        # The subsystem's TRB data map
        self.p0._populate_trb_data_map(trb_data_map)
        self.p1._populate_trb_data_map(trb_data_map)        
        
        # Add a mapping for the tbus access (that doesn't go through the 
        # processor) 
        tbus_non_proc_map = AddressMap("TBUS_NON_PROC_DATA", NullAccessCache,
                                   length = 0x080000000, word_bits=8)
        trb_data_map.add_mapping(0x00000000, 0x08000000, tbus_non_proc_map.port,
                                 view=AccessView.RAW)
        
        return trb_data_map


    def config_sqif_pios(self,bank=0):
        """
        The firmware should request these as resources from the Curator.
        During testing it may be necessary to forcible take these PIOs
        """
        #Get the Curator object to access it's registers
        cur = self.curator.core
       
        # SQIF 2 AUDIO -> PIO34..PIO39
        cur.fields['CHIP_PIO32_PIO35_MUX_CONTROL'] = 0xbb00
        cur.fields['CHIP_PIO36_PIO39_MUX_CONTROL'] = 0xbbbb 

    def safe_state(self):
        """
        Force the subsystem into a 'safe' known state. This will power
        off all subsystems, and only repower AUDIO without letting it run.
        """
        
        #Get the Curator object to access it's registers
        cur = self.curator.core
    
        # QCC512X_QCC302X needs an additional clock enable - clock sources possibly in future too
        cur.fields.CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES.CURATOR_SUBSYS_CORE_CLK_ENABLES_AUDIO = 1
    
        cur.fields['CURATOR_SUBSYSTEMS_RUN_EN']  = 0x03
        #Power cycle the Audio subsystem
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0x03
        time.sleep(0.2)
        cur.fields['CURATOR_SUBSYSTEMS_POWERED']  = 0xb
        time.sleep(0.2)

    @property
    def sqif_trb_address_block_id(self):
        '''
        Returns a list (indexed by sqif device number) of tuples of TRB address 
        and block IDs at which the SQIF contents can be read. See CS-205120-SP
        '''
        return [ (0x40000000,0) ]
    
    @property
    def dm_total_size(self):
        return 448 * 1024
    
    
    def _create_audio_cores(self, access_cache_type):
        from ..core.qcc514x_qcc304x_audio_core import QCC514X_QCC304XAudioP0Core, QCC514X_QCC304XAudioP1Core
        p0 = QCC514X_QCC304XAudioP0Core(self, access_cache_type, self._hw_version)
        p1 = QCC514X_QCC304XAudioP1Core(self, access_cache_type, self._hw_version)
        # Populate with memory entities
        p0.populate(access_cache_type)
        p1.populate(access_cache_type)
        # Wire everything together to create coherent views of memory
        
        p0.create_data_space()
        p0.create_program_space()
        p1.create_data_space()
        p1.create_program_space()

        if access_cache_type is ExtremeAccessCache:
            # Use the built-in address-mapping mechanism to emulate the 
            # hardware's windowing of PM into DM
            p0.emulate_hardware_windows()
            p1.emulate_hardware_windows()
    
        return p0,p1
