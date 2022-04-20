############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model.base_component import Reportable


@Reportable.has_subcomponents
class ADKLibs(FirmwareComponent):
    """
    Container for analysis classes from the ADK libraries.

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
    def __init__(self, env, core, parent=None):

        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            env.vars["theCm"]
        except KeyError:
            raise self.NotDetected
