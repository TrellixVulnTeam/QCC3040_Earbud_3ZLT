############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

class HasMMU (object):
    '''
    Mixin Base for Subsystems containing an MMU of some sort.
    
    Provides:-
    - MMU property
    - Default MMU factory (creates a SingleXapMmu)
    
    Allows:-
    - MMU factory override (e.g. for WLAN)
    '''
    
    # Public
    @property
    def subcomponents(self):
        return {}
    
    @property
    def mmu(self):
        """\
        MMU Hardware Component
        """
        try:
            self._mmu
        except AttributeError:
            self._mmu = self._create_mmu()
        return self._mmu
        
    # Protected/Overideable
    
    def _create_mmu(self):
        """\
        Create MMU Component appropriate for this Subsystem.
        """
        
        # Default to single XAP MMU
        from ...mmu import SingleXapHydraMMU
        return SingleXapHydraMMU(self)
    
