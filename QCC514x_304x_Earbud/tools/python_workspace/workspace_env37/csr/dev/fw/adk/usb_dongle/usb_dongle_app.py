############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from ..caa.caa_app import CAAApp
from .usb_dongle import UsbDongle
from ..caa.usb import Usb

class UsbDongleApp(CAAApp):
    """
    Container for analysis classes representing code specific to the usb dongle
    app.
    """
    @property
    def subcomponents(self):
        subcmpts = CAAApp.subcomponents.fget(self)
        subcmpts.update({
            # Add usb_dongle-specific subcomponents here
            "usb_dongle" : "_usb_dongle",
            "usb" : "_usb"
            })
        return subcmpts
        
    @property
    def usb(self):
        try:
            self._usb
        except AttributeError:
            self._usb = self.create_component_variant((Usb,), self.env, self._core, self)
        return self._usb    
        
    @property
    def usb_dongle(self):
        try:
            self._usb_dongle
        except AttributeError:
            self._usb_dongle = self.create_component_variant((UsbDongle,), self.env, self._core, self)
        return self._usb_dongle

    
