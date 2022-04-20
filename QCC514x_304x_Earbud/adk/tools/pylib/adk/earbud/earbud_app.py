############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from ..caa.caa_app import CAAApp
from .tws_topology import TwsTopology
from .earbud import Earbud

class EarbudApp(CAAApp):
    """
    Container for analysis classes representing Earbud components.
    """
    @property
    def subcomponents(self):
        subcmpts = CAAApp.subcomponents.fget(self)
        subcmpts.update({
            "tws_topology" : "_tws_topology",
            "earbud" : "_earbud"
            })
        return subcmpts
    
    @property
    def tws_topology(self):
        try:
            self._tws_topology
        except AttributeError:
            self._tws_topology = self.create_component_variant((TwsTopology,), self.env, self._core, self)
        return self._tws_topology

    @property
    def earbud(self):
        try:
            self._earbud
        except AttributeError:
            self._earbud = self.create_component_variant((Earbud,), self.env, self._core, self)
        return self._earbud
