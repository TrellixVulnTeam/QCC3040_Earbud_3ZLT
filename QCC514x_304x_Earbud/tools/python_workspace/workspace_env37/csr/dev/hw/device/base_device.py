############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides BaseDevice, the base of all device classes.
"""
from csr.dev.model.base_component import BaseComponent
from csr.dev.hw.address_space import AddressSpace
from csr.dev.model.interface import Group

class BaseDevice(BaseComponent):
    """\
    CSR Device Proxy (Base)

    A Device represents everything of interest on the PCB: Chip(s), LPCs,
    ROMs. + wiring.
    """
    # BaseComponent compliance

    @property
    def title(self):
        return '%s device' % self.name

    @property
    def subcomponents(self):
        return self.chips

    # Extensions

    @property
    def name(self):
        """Well known name"""
        raise NotImplementedError()

    @property
    def chips(self):
        """\
        The set of Chips on this Device.

        Most Devices hold a single Chip under test. But there are exceptions:
        Some large emulators have 2.
        """
        raise NotImplementedError()

    @property
    def cores(self):
        "A generator for iterating over cores within all chips in the device"
        for chip in self.chips:
            try:
                for core in chip.cores:
                    yield core
            except AttributeError: #no cores, assume has hydra subsystems
                for subsystem in chip.subsystems.values():
                    for core in subsystem.cores:
                        yield core

    def find_core_component(self, comp_name):
        """
        Hunt through the device cores trying to find the component with the
        given name.
        """
        for core in self.cores:
            try:
                return getattr(core.fw, comp_name)
            except (AttributeError, AddressSpace.NoAccess):
                pass
        return None

    @property
    def transport(self):
        """
        The debug transport associated with this device
        """
        raise NotImplementedError

    @property
    def lpc_sockets(self):
        """\
        The set of LPCSockets on this Device.

        N.B. LPCSockets do not necessarily contain an LPC.
        """
        raise NotImplementedError()

    def load_register_defaults(self):
        """
        Get all the registers on all chips loaded with their defaults (used for
        setting up a simulated device model)
        """
        for chip in self.chips:
            chip.load_register_defaults()

    def halt(self):
        """Bring all chips to a halt"""
        for chip in self.chips:
            chip.halt()

    def is_coredump(self):
        '''
        returns True if this device (transport url) is a coredump
        '''
        return self.transport is None

    @property
    def protocol_stacks(self):
        'provides a list of protocol stacks, e.g. BT Protocol stack'
        try:
            return self._protocol_stacks
        except AttributeError:
            self._protocol_stacks = []
        return self._protocol_stacks

    def register_protocol_stack(self, protocol_stack):
        'adds a protocol stack available on this device'
        self.protocol_stacks.append(protocol_stack)

    def _generate_report_body_elements(self):
        if len(self.protocol_stacks) > 1:
            report = [Group('PROTOCOL STACKS')]
        else:
            report = []
        for stack in self.protocol_stacks:
            report.append(stack.generate_report())
        return report
