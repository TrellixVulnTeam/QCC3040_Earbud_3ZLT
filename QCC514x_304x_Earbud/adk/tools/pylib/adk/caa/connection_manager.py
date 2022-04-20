############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from .structs import IAdkStructHandler

class ConnectionManager(FirmwareComponent):
    ''' This class reports the connection manager state and provides some
        Bluetooth related helper functions. '''

    # Number of microseconds per BT slots
    MICROSECONDS_PER_SLOT = 625

    # Connection library lp_power_mode value for sniff
    SNIFF_MODE = 1

    @classmethod
    def slots_to_ms(cls, slots):
        ''' Convert Bluetooth slots to milliseconds '''
        return cls.slots_to_us(slots) / 1000.0

    @classmethod
    def slots_to_us(cls, slots):
        ''' Convert Bluetooth slots to microseconds '''
        return (slots * cls.MICROSECONDS_PER_SLOT)

    @property
    def connections(self):
        ''' Returns the connection manager's array of ACL connections '''
        return self.env.cu.connection_manager_list.local.connections

    @property
    def active_connections_count(self):
        ''' Returns the number of active connections in the connection manager '''
        return len(list(self._active_connections_generator()))

    @property
    def active_connections_tpaddrs(self):
        ''' Return a list of active connection tpaddrs '''
        return [active[0] for active in self._active_connections_generator()]

    def _active_connections_generator(self):
        ''' Iterates the conneciton manager list yielding active connections '''
        connections = self.connections
        with connections.footprint_prefetched():
            for conn in connections:
                tpaddr = IAdkStructHandler.handler_factory("tp_bdaddr")(self._core, conn.tpaddr)
                if not tpaddr.typed_bdaddr.bdaddr.is_zero():
                    yield (tpaddr, conn)

    def _generate_report_body_elements(self):
        ''' Report the list of connections'''
        grp = interface.Group("Connections")
        tbl = interface.Table(["TpBdAddr", "Bitfields", "Link Policy", "Details"])
        for (tpaddr, conn) in self._active_connections_generator():
            lp_state = conn.lp_state.pt_index.symbolic_value
            details = ""
            if tpaddr.is_bredr():
                if conn.mode.value == self.SNIFF_MODE:
                    si_slots = conn.sniff_interval.value
                    details = "Sniff mode {} slots ({}ms)".format(si_slots, self.slots_to_ms(si_slots))
            elif tpaddr.is_ble():
                details = "Connection Interval={}, Slave Latency={}".format(conn.conn_interval, conn.slave_latency)
            tbl.add_row([tpaddr, conn.bitfields, lp_state, details])
        grp.append(tbl)
        return [grp]
