############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import InvalidDereference
from csr.dev.model import interface


class Ama(FirmwareComponent):
    """
    Ama Voice Assistant analysis class
    """

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self._ama = env.econst.voice_ui_provider_ama
        except AttributeError:
            raise self.NotDetected("AMA is not included in this build")

    @property
    def active(self):
        try:
            active_va = self.env.vars['active_va'].deref.voice_assistant.deref.va_provider.value
            return active_va == self._ama
        except InvalidDereference:
            return False

    def _generate_report_body_elements(self):

        content = []

        grp = interface.Group("Ama status")
        tbl = interface.Table(["Active"])
        tbl.add_row([
            "Y" if self.active else "N"
        ])
        grp.append(tbl)

        content.append(grp)

        return content
