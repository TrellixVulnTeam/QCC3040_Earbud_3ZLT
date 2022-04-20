############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model.base_component import Reportable
from csr.wheels.bitsandbobs import autolazy

from .led import LEDManager
from .device_list import DeviceList
from .vm_message_queue import VmMessageQueue
from .connection_manager import ConnectionManager
from .a2dp import A2dp
from .va import VA
from .casecomms import Casecomms
from .handset_service import HandsetService
from .le_advertising_manager import LeAdvertisingManager
from .le_broadcast_manager import LeBroadcastManager
from .audio_driver import AudioDriver


@Reportable.has_subcomponents
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

    @Reportable.subcomponent
    @autolazy
    def led(self):
        return self.create_component_variant((LEDManager, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def device_list(self):
        return self.create_component_variant((DeviceList, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def connection_manager(self):
        return self.create_component_variant((ConnectionManager, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def handset_service(self):
        return self.create_component_variant((HandsetService, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def vm_message_queue(self):
        return self.create_component_variant((VmMessageQueue, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def a2dp(self):
        return self.create_component_variant((A2dp, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def va(self):
        return self.create_component_variant((VA, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def casecomms(self):
        return self.create_component_variant((Casecomms, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def le_advertising_manager(self):
        return self.create_component_variant((LeAdvertisingManager, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def le_broadcast_manager(self):
        return self.create_component_variant((LeBroadcastManager, ), self.env, self._core, self)

    @Reportable.subcomponent
    @autolazy
    def audio_driver(self):
        """
            Not included in the main report yet
            Generate report with:
                >>> apps1.app.audio_driver().report()
        """
        return self.create_component_variant((AudioDriver, ), self.env, self._core, self)
