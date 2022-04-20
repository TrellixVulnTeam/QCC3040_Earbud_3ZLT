############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface

from .gaa import Gaa
from .ama import Ama


class VA(FirmwareComponent):
    """
    Container for analysis classes representing generic parts of CAA applications
    """
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

    @property
    def subcomponents(self):
        return {
            "gaa": "_gaa",
            "ama": "_ama",
        }

    @property
    def gaa(self):
        try:
            self._gaa
        except AttributeError:
            self._gaa = self.create_component_variant((Gaa,), self.env, self._core, self)
        return self._gaa

    @property
    def ama(self):
        try:
            self._ama
        except AttributeError:
            self._ama = self.create_component_variant((Ama,), self.env, self._core, self)
        return self._ama

    @property
    def current_state(self):
        return self.env.cus['kymera_va.c'].localvars['current_state']

    @staticmethod
    def y_or_n(condition):
        return "Y" if condition else "N"

    def _generate_report_body_elements(self):

        content = []

        main_grp = interface.Group("Voice Assistants")
        in_build_grp = interface.Group("Included in the build")
        assistants_in_build = interface.Table([
            "Gaa",
            "Ama"
        ])
        assistants_in_build.add_row([
            self.y_or_n(self.gaa),
            self.y_or_n(self.ama)
        ])
        in_build_grp.append(assistants_in_build)
        main_grp.append(in_build_grp)

        state_grp = interface.Group("VA State")
        state = interface.Text("Current state: {}".format(self.current_state))
        state_grp.append(state)
        main_grp.append(state_grp)

        content.append(main_grp)

        return content
