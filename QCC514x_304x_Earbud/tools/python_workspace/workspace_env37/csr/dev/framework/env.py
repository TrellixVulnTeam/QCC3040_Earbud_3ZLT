############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""\
Abstraction of current python environment for portable device tools.

Alt:-
- So far threading issues dominate - So could consider replacing the 
standard threading library instead.
"""

import time

class EnvInterface (object):
    """
    Python environment abstraction (Interface)
    
    Hides differences in standalone vs. xIDE python envrionment for python
    device tools.
    """
    def sleep(self, secs):
        raise NotImplementedError()

    def coop_yield(self):
        """\
        Yield in co-operative threading environments.
        """
        raise NotImplementedError()


class StandaloneEnv (EnvInterface):
    """
    Standalone DevTool environment.
    """
    
    # ------------------------------------------------------------------------
    # ExecEnvInterface compliance
    # ------------------------------------------------------------------------
    
    def sleep(self, secs):
        time.sleep(secs)

    def coop_yield(self):
        time.sleep(0) # "pass" might be sufficient
    

class XideEnv (EnvInterface):
    """
    xIDE DevTool environment.
    """
    
    # ------------------------------------------------------------------------
    # ExecEnvInterface compliance
    # ------------------------------------------------------------------------
    
    def sleep(self, secs):
        # Potential extension:: yield to ui/qt thread
        raise NotImplementedError()
   
    def coop_yield(self):
        # Potential extension:: yield to ui/qt thread
        raise NotImplementedError()
