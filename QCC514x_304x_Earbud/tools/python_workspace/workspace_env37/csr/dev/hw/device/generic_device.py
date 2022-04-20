############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides GenericDevice and GenericBtDevice.
"""

from csr.wheels.bitsandbobs import PureVirtualError
from .base_device import BaseDevice
from .mixins.implements_bt_protocol_stack import ImplementsBTProtocolStack

class GenericDevice(BaseDevice):
    """
    GenericDevice is a very simple type of device, which makes some common
    assumptions about the device's features in order to create a concrete
    device type which should be quite widely applicable.
     - there's just one chip
     - the name of the device is the same as the name of the chip
     - there's a jtag transport (meaning in practice something we can connect to
     via gdbserver)
    """

    def __init__(self, chip, transport):

        self._chip = chip
        self._transport = transport

    @property
    def chip(self):
        """Accessor to the device's sole chip"""

        return self._chip

    @property
    def transport(self):
        return self._transport

    @property
    def chips(self):
        """Accessor to the device's chips, there being just a sole chip"""

        return [self.chip]

    @property
    def dap(self):
        """Accessor to the sole chip's DAP debug connector"""
        return self._chip.dap

    @property
    def debug_access(self):
        return self.dap

    @property
    def gdbserver_mux(self):
        return self.chip.gdbserver_mux

    @property
    def name(self):
        return self._chip.name

    @property
    def lpc_sockets(self):
        raise PureVirtualError(self)

    def reset(self):
        for chip in self.chips:
            try:
                chip.reset
            except AttributeError:
                raise NotImplementedError("{} does not support reset".format(self.chip.__class__.__name__))
        for chip in self.chips:
            chip.reset()

class GenericBtDevice(GenericDevice, ImplementsBTProtocolStack):
    """
    A GenericDevice with addition of a BT Protocol stack.
    """

    def __init__(self, chip, transport):

        GenericDevice.__init__(self, chip, transport)
        self.register_protocol_stack(self.bt_protocol_stack)


class MSMTAP_QDSSDAPDevice(GenericDevice):
    """
    Device with a QCOM-proprietary MSM-TAP
    """

    @property
    def debug_access(self):
        return self._chip.jtag_multitap
