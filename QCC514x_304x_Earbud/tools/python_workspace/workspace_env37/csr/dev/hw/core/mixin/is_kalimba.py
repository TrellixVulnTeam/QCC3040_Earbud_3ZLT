############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
class IsKalimba (object):
    """\
    Kalimba Core Mixin
    
    Implementations and extensions common to all known Kalimba Cores.
    """
    
    # Core Compliance
    
    def map_lpc_slave_regs_into_prog_space(self):
        
        raise NotImplementedError();
    
