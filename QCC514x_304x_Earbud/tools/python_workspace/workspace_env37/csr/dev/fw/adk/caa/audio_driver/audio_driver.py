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

from .audio_use_case import AudioUseCase


@Reportable.has_subcomponents
class AudioDriver(FirmwareComponent):
    """
    Container for analysis classes for the audio driver.

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
    def audio_use_case(self):
        return self.create_component_variant((AudioUseCase,), self.env, self._core, self)

    def _generate_report_body_elements(self):

        content = []

        main_grp = interface.Group("Audio Driver")

        content.append(main_grp)

        return content
