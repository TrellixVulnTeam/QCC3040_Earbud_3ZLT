############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.model.base_component import Reportable
from ..caa.caa_app import CAAApp


@Reportable.has_subcomponents
class ChargerCaseApp(CAAApp):
    """
    Container for analysis classes representing code specific to the charger
    case app.

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
