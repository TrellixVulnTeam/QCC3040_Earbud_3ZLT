############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from .structs import IAdkStructHandler, DeviceProfiles

class HandsetService(FirmwareComponent):
    ''' This class reports the handset service '''

    @property
    def connections(self):
        ''' Returns the connection manager's array of ACL connections '''
        return self.env.cu.handset_service.local.handset_service.state_machine.capture()

    def _decode_profiles(self, value):
        return DeviceProfiles(self._core, value)

    def _active_connections_generator(self):
        ''' Iterates the handset service list yielding active connections '''
        connections = self.connections
        for conn in connections:
            bdaddr = IAdkStructHandler.handler_factory("bdaddr")(self._core, conn.handset_addr)
            if not bdaddr.is_zero():
                yield (bdaddr, conn)

    def _generate_report_body_elements(self):
        ''' Report the list of connections'''
        grp = interface.Group("Connections")
        tbl = interface.Table(["BdAddr", "State", "ACL attempts", "Disc Reason", "Profiles Req"])
        for (addr, conn) in self._active_connections_generator():
            state = conn.state.symbolic_value
            disconnect_reason = conn.disconnect_reason.symbolic_value
            tbl.add_row([addr, state, conn.acl_attempts , disconnect_reason ,self._decode_profiles(conn.profiles_requested)])
        grp.append(tbl)
        return [grp]
