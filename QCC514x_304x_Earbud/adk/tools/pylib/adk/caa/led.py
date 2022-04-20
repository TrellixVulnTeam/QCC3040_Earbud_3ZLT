############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface

class LEDManager(FirmwareComponent):
    """
    Noddy example of analysis of a firmware module, focused on the LED manager
    in the earbud app.
    """
    
    def __init__(self, env, core, parent=None):
        
        FirmwareComponent.__init__(self, env, core, parent=parent)
        
        try:
            self._led_mgr = env.vars["led_mgr"]
        except KeyError:
            # If there's no LED manager compiled in, say so?
            raise self.NotDetected

    @property
    def enabled(self):
        return self._led_mgr.enable.value == 1
    
    @property
    def force_enabled(self):
        return self._led_mgr.force_enabled.value == 1

    @property
    def pattern(self):
        pattern_name = self.env.vars[self._led_mgr.priority_state[0].pattern.value]
        return pattern_name[len("app_led_pattern_"):]
    
    
    
    def _generate_report_body_elements(self):
        
        content = [] # Construct a list of Renderables
        
        # Prefetch current led_mgr value to reduce risk of tearing
        with self._led_mgr.footprint_prefetched():
            # Groups are a way of providing titles
            grp = interface.Group("Enable status")
            tbl = interface.Table(["Enabled", "Force enabled"])
            tbl.add_row(["Y" if self.enabled else "N",
                         "Y" if self.force_enabled else "N"])
            grp.append(tbl)
            
            content.append(grp)

            if self.enabled or self.force_enabled:
                content.append(interface.Text("Pattern: {}".format(self.pattern)))
                
            content.append(interface.Text("(default Pydbg code)"))
                
        return content