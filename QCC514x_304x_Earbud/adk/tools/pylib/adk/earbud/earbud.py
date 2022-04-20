############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from ..caa.structs import DeviceProfiles
from ..caa.connection_manager import ConnectionManager
from ..caa.device_list import DeviceList

class Earbud(FirmwareComponent):
    ''' Class providing summary information about the Earbud application.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            self._app_sm = env.vars["app_sm"]
            self._cm = ConnectionManager(env, core, parent)
            self._devlist = DeviceList(env, core, parent)
        except KeyError:
            raise self.NotDetected

    def is_peer_connection(self, tpaddr):
        return any(dev.bdaddr == tpaddr.typed_bdaddr.bdaddr for dev in self._devlist.earbuds)

    @property
    def app_state(self):
        return self._app_sm.state

    @property
    def app_role(self):
        return self._app_sm.role

    @property
    def app_phy_state(self):
        return self._app_sm.phy_state

    @property
    def is_peer_connected(self):
        for tpaddr in self._cm.active_connections_tpaddrs:
            if self.is_peer_connection(tpaddr):
                return True
        return False

    @property
    def is_handset_connected(self):
        for tpaddr in self._cm.active_connections_tpaddrs:
            for dev in self._devlist.handsets:
                if dev.bdaddr == tpaddr.typed_bdaddr.bdaddr:
                    return True
        return False

    def _generate_report_body_elements(self):
        grp = interface.Group("Summary")
        keys_in_summary = ['State', 'Value']
        tbl = interface.Table(keys_in_summary)
        found_peer_connection = False
        found_handset_connection = False

        with self._app_sm.footprint_prefetched():
            tbl.add_row(['App State', self.app_state])
            tbl.add_row(['App Phy State', self.app_phy_state])
            tbl.add_row(['App Role', self.app_role])

        tbl.add_row(['Peer Paired', "TRUE" if len(self._devlist.earbuds) != 0 else "FALSE"])
        tbl.add_row(['Handset Paired', "TRUE" if len(self._devlist.handsets) != 0 else "FALSE"])

        for tpaddr in self._cm.active_connections_tpaddrs:
            if self.is_peer_connection(tpaddr):
                tbl.add_row(['Peer Connection', tpaddr])
                found_peer_connection = True
            else:
                tbl.add_row(['Handset Connection', tpaddr])
                found_handset_connection = True
        if found_peer_connection == False:
            tbl.add_row(["Peer Connected", "FALSE"])
        if found_handset_connection == False:
            tbl.add_row(["Handset Connected", "FALSE"])

        grp.append(tbl)

        return [grp]
