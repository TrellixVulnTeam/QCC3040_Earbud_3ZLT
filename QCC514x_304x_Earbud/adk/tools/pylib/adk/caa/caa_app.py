############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent

from .led import LEDManager
from .device_list import DeviceList
from .vm_message_queue import VmMessageQueue
from .connection_manager import ConnectionManager
from .a2dp import A2dp
from .va import VA
from .casecomms import Casecomms
from .handset_service import HandsetService


class CAAApp(FirmwareComponent):
    """
    Container for analysis classes representing generic parts of CAA applications
    """
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        # Check if we're looking at a CAA app at all.
        try:
            # Do we have variables called device_list and profile_manager present
            env.vars["device_list"]
            env.vars["profile_manager"]
        except KeyError:
            raise self.NotDetected

    @property
    def subcomponents(self):
        return {
            "led": "_led",
            "device_list": "_device_list",
            "vm_message_queue": "_vm_message_queue",
            "connection_manager": "_connection_manager",
            "a2dp": "_a2dp",
            "va": "_va",
            "casecomms" : "_casecomms",
            "handset_service" : "_handset_service"
        }

    @property
    def led(self):
        try:
            self._led
        except AttributeError:
            self._led = LEDManager(self.env, self._core, self)
        return self._led

    @property
    def device_list(self):
        try:
            self._device_list
        except AttributeError:
            self._device_list = DeviceList(self.env, self._core, self)
        return self._device_list

    @property
    def connection_manager(self):
        try:
            self._connection_manager
        except AttributeError:
            self._connection_manager = ConnectionManager(self.env, self._core, self)
        return self._connection_manager
		
    @property
    def handset_service(self):
        try:
            self._handset_service
        except AttributeError:
            self._handset_service = HandsetService(self.env, self._core, self)
        return self._handset_service

    @property
    def vm_message_queue(self):
        try:
            self._vm_message_queue
        except AttributeError:
            self._vm_message_queue = VmMessageQueue(self.env, self._core, self)
        return self._vm_message_queue

    @property
    def a2dp(self):
        try:
            self._a2dp
        except AttributeError:
            self._a2dp = A2dp(self.env, self._core, self)
        return self._a2dp

    @property
    def va(self):
        try:
            self._va
        except AttributeError:
            self._va = VA(self.env, self._core, self)
        return self._va

    @property
    def casecomms(self):
        try:
            self._casecomms
        except AttributeError:
            self._casecomms = Casecomms(self.env, self._core, self)
        return self._casecomms
