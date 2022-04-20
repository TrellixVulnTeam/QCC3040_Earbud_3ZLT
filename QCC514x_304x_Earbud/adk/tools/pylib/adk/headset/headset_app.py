############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from ..caa.caa_app import CAAApp

class HeadsetApp(CAAApp):
    """
    Container for analysis classes representing code specific to the headset
    app
    """
    @property
    def subcomponents(self):
        subcmpts = CAAApp.subcomponents.fget(self)
        subcmpts.update({
            # Add headset-specific subcomponents here
            # "subcmt" : "_subcmpt"
            })
        return subcmpts
    
    # TEMPLATE CODE FOR NEW SUBCOMPONENTS
    #@property
    #def subcmpt(self):
    #    try:
    #        self._subcmpt
    #    except AttributeError:
    #        self._subcmpt = Subcmpt(self.env, self._core, self)
    #         or
    #        self._subcmpt = self.create_component_variant((SubcmptFlavour1, 
    #                                                       SubcmptFlavour2),
    #                                                       self.env, self._core, self)
    #    return self._subcmpt
