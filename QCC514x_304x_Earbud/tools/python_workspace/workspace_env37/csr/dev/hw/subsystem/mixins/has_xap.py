############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .has_mmu import HasMMU
from csr.wheels.bitsandbobs import PureVirtualError

class HasXAP (HasMMU):
    '''
    Mixin Base for Subsystems that have one or more XAP cores.
    
    Declares a number of features common to all known XAP-based subsystems.
    '''

    # Extensions
    
    @property
    def baseline_slt(self):
        """\
        Interface to baseline firmware SLT. 
        
        This interface is valid for all firmware versions, and can be used for
        detecting the currently installed firmware version amongst other
        things.
        
        The SLT returned will vary with subsystem type.
        
        Where firmware version is known subsystem.fw.slt may present an
        extended SLT interface.
        """
        try:
            self._has_xap_baseline_slt
        except AttributeError:
            self._has_xap_baseline_slt = self._create_baseline_slt()            
        return self._has_xap_baseline_slt
        
    # Protected / Required
    
    def _create_baseline_slt(self):
        """\
        Subsystems must override to create suitable baseline SLT. 
        
        Typically this will be delegated to a static method on respective
        Firmware base class.
        """
        raise PureVirtualError(self)