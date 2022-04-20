############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.model.base_component import Reportable
from csr.wheels.bitsandbobs import autolazy

from ..caa.caa_app import CAAApp
from .tws_topology import TwsTopology
from .earbud import Earbud
from .earbud_tddb import EarbudTrustedDeviceList


@Reportable.has_subcomponents
class EarbudApp(CAAApp):
    """
    Container for analysis classes representing Earbud components.

    TEMPLATE CODE FOR NEW SUBCOMPONENTS:

    @Reportable.subcomponent
        or
    @Reportable.subcomponent
    @autolazy               # for a cached subcomponent
    def subcmpt_name(self):
        return Subcmpt(self.env, self._core, self)
            or
        return self.create_component_variant((SubcmptFlavour1, SubcmptFlavour2),
                                            self.env, self._core, self)
    """

    @Reportable.subcomponent
    @autolazy
    def tws_topology(self):
        return self.create_component_variant((TwsTopology,), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def earbud(self):
        return self.create_component_variant((Earbud,), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def earbud_tddb(self):
        return self.create_component_variant((EarbudTrustedDeviceList,), self.env, self._core, self)
