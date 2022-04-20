############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent

class ADKLibs(FirmwareComponent):
    """
    Container for analysis classes representing ADK libraries
    """
    def __init__(self, env, core, parent=None):
        
        FirmwareComponent.__init__(self, env, core, parent=parent)
        
        try:
            env.vars["theCm"]
        except KeyError:
            raise self.NotDetected
    
    
    @property
    def subcomponents(self):
        return {}