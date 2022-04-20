############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
class IsQCC512X_QCC302X:
    """
    Simple mixin class that provides QCC512X_QCC302X-generic functionality to mix 
    into a subsystem 
    """
    def _create_host_subsystem_view(self):

        from csr.dev.hw.subsystem.qcc512x_qcc302x_host_subsystem \
                                              import QCC512X_QCC302XHostSubsystemView
        try:
            view = QCC512X_QCC302XHostSubsystemView(self.id,
                                      self.chip.host_subsystem)
        except AttributeError:
            view = None
        return view

    def map_in_rom(self):
        '''
        Register pokes to put ROM into the memory map in place of SQIF 
        
        Currently the only reason to call this method is for re-running the 
        Curator in ROM. This will require a restart from address 0x000000 in
        which case the boot code won't expect interrupts to be already enabled
        '''
        self.core.fields["NV_MEM_ADDR_MAP_CFG"]= (
                                 self.core.info.io_map_info.misc_io_values[
                                     "NV_MEM_ADDR_MAP_CFG_HIGH_SQIF_LOW_ROM"])
                                     
        self.core.fields["INT_ENABLES"] = 0x0
