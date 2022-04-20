############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model.base_component import Reportable
from csr.wheels.bitsandbobs import autolazy
from csr.dev.model import interface


@Reportable.has_subcomponents
class AudioUseCase(FirmwareComponent):
    """
    Container for analysis classes for the audio_use_case component of audio driver.

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