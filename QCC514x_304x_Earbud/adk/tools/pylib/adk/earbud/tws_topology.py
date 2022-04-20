############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.wheels import iprint, wprint
from datetime import datetime
from ..caa.structs import DeviceProfiles

class TwsTopology(FirmwareComponent):
    ''' Class providing information about the TWS Topology component.
        In addition to reporting this class provides helper functions to decode TWS Topology goals
        and tracing of active goals.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            self._tws_topology = env.vars["tws_topology"]
        except KeyError:
            raise self.NotDetected

    def active_goal_trace_gen(self):
        ''' Generator which provides a list of the currently active goal strings
        '''
        mask = self._tws_topology.goal_set.deref.active_goals_mask
        while True:
           yield self.decode_goal_mask(mask.value)

    def active_goal_trace(self):
        ''' Live trace of active goals with timestamps in an interactive pydbg session.
        '''
        b = []
        goals = self.active_goal_trace_gen()
        for a in goals:
            a = next(goals)
            if a != b:
                b = a
                timestamp = "[" + datetime.now().strftime("%H:%M:%S.%f") + "] "
                if len(a) != 0:
                    iprint(timestamp + str(a))
                else:
                    iprint(timestamp + "No active goals")

    def goal(self, goal_id):
        ''' Find the string representation of a TWS Topology goal ID.
        '''
        try:
            return self.env.enums["tws_topology_goal_id"][goal_id]
        except KeyError:
            wprint("Unknown goal")

    def decode_goal_mask(self, mask, numeric=False):
        ''' Convert a 64-bit goal mask into a list of TWS Topology goals
            By default the list contains the string representation of enumeration names.
            The 'numeric' parameter may be used to return a list of numeric IDs.
        '''
        goals = []
        for k, v in self.env.enums["tws_topology_goal_id"].items():
            if ((1 << v) & mask):
                goals.append(self.goal(v) if not numeric else v)
        return goals

    @property
    def acting_in_role(self):
        return self._tws_topology.acting_in_role.value != 0

    @property
    def role(self):
        return self._tws_topology.role

    @property
    def active_goals(self):
        ''' Return a dictionary of active goal strings and numeric Ids
        '''
        return dict(zip(self.active_goal_strings, self.active_goal_ids))

    @property
    def active_goal_ids(self):
        ''' Returns a list of numeric IDs, corresponding to the currently active goals.
        '''
        mask = self._tws_topology.goal_set.deref.active_goals_mask.value
        return self.decode_goal_mask(mask, True)

    @property
    def active_goal_strings(self):
        ''' Returns a list of strings, corresponding to the enumerated names of the currently active goals.
        '''
        mask = self._tws_topology.goal_set.deref.active_goals_mask.value
        return self.decode_goal_mask(mask)

    @property
    def pending_goal_queue_size(self):
        return self._tws_topology.goal_set.deref.pending_goals.queue_msg_count.value

    @property
    def pending_goals(self):
        pg = []
        num_pg = self.pending_goal_queue_size
        pg_arr = self.env.cast(self._tws_topology.goal_set.deref.pending_goals.pending_goals, "pending_goal_entry_t", array_len=num_pg)
        for goal in pg_arr:
            pg.append(self.env.enums["tws_topology_goal_id"][goal.id.value])
        return pg

    @property
    def hdma_created(self):
        return self._tws_topology.hdma_created.value == 1

    @property
    def handover_prohibited(self):
        return self._tws_topology.app_prohibit_handover.value == 1
    
    @property
    def handset_connect_prohibited(self):
        return self._tws_topology.prohibit_connect_to_handset.value == 1

    @property
    def reconnect_post_handover(self):
        return self._tws_topology.reconnect_post_handover.value == 1

    @property
    def remain_active_for_handset(self):
        return self._tws_topology.remain_active_for_handset.value == 1

    @property
    def remain_active_for_peer(self):
        return self._tws_topology.remain_active_for_peer.value == 1

    @property
    def peer_profile_connect_mask(self):
        return DeviceProfiles(self._core, self._tws_topology.peer_profile_connect_mask)

    def _generate_report_body_elements(self):
        with self._tws_topology.footprint_prefetched():
            grp = interface.Group("Summary")
            keys_in_summary = ['State', 'Value']
            tbl = interface.Table(keys_in_summary)
            acting = "(ACTING)" if self.acting_in_role else ""
            tbl.add_row(['Role', str(self.role) + acting])
            tbl.add_row(["HDMA (Handover)", "Active" if self.hdma_created else "Inactive"])
            tbl.add_row(["Handover Prohibited", "Y" if self.handover_prohibited else "N"])
            tbl.add_row(["Handset Connect Prohibited", "Y" if self.handset_connect_prohibited else "N"])
            tbl.add_row(["Reconnect Handset Post Handover", "Y" if self.reconnect_post_handover else "N"])
            tbl.add_row(["Remain Active for Handset", "Y" if self.remain_active_for_handset else "N"])
            tbl.add_row(["Remain Active for Peer", "Y" if self.remain_active_for_peer else "N"])
            tbl.add_row(["Peer Profile Connect Mask", str(self.peer_profile_connect_mask)])
            output_active = "\n".join(["{}".format(g) for g in self.active_goal_strings])
            output_queued = "\n".join(["{}".format(g) for g in self.pending_goals])
            tbl.add_row(["Active Goals", output_active])
            tbl.add_row(["Queued Goals", output_queued])
            grp.append(tbl)

        return [grp]
