############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.wheels import iprint, wprint
from datetime import datetime
from ..caa.structs import DeviceProfiles

class CcWithCase(FirmwareComponent):
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self._cc_with_case = env.vars["cc_with_case"]
        except KeyError:
            raise self.NotDetected

    @property
    def lid_state(self):
        return self._cc_with_case.lid_state

    @property
    def is_lid_open(self):
        return self._cc_with_case.lid_state.value == self.env.econst.CASE_LID_STATE_OPEN

    @property
    def is_lid_closed(self):
        return self._cc_with_case.lid_state.value == self.env.econst.CASE_LID_STATE_CLOSED

    @property
    def is_lid_unknown(self):
        return self._cc_with_case.lid_state.value == self.env.econst.CASE_LID_STATE_UNKNOWN

    @property
    def case_battery_state_percent(self):
        if self._cc_with_case.case_battery_state.value != 0x7f:
            return self._cc_with_case.case_battery_state.value & 0x7f
        else:
            return "Unknown"

    def _generate_report_body_elements(self):
        with self._cc_with_case.footprint_prefetched():
            grp = interface.Group("Case Comms With Case")
            keys_in_summary = ['Case State', 'Value']
            tbl = interface.Table(keys_in_summary)
            tbl.add_row(['Lid State', self.lid_state])
            tbl.add_row(['Case Battery %', self.case_battery_state_percent])
            grp.append(tbl)
        return [grp]
            

class CcWithEarbuds(FirmwareComponent):
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self._cc_with_earbuds = env.vars["cc_with_earbuds"]
        except KeyError:
            raise self.NotDetected

    @property
    def loopback_sent(self):
        return self._cc_with_earbuds.loopback_sent.value

    @property
    def loopback_received(self):
        return self._cc_with_earbuds.loopback_recv.value

    def _generate_report_body_elements(self):
        with self._cc_with_earbuds.footprint_prefetched():
            grp = interface.Group("Case Comms With Earbuds")
            keys_in_summary = ['Earbud State', 'Value']
            tbl = interface.Table(keys_in_summary)
            tbl.add_row(['Loopback Sent', self.loopback_sent])
            tbl.add_row(['Loopback Recv', self.loopback_received])
            grp.append(tbl)
        return [grp]

class CcProtocol(FirmwareComponent):
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self._cc_protocol = env.vars["cc_protocol"]
        except KeyError:
            raise self.NotDetected

    @property
    def mode(self):
        return self._cc_protocol.mode

    @property
    def transport(self):
        return self._cc_protocol.trans

    @property
    def last_earbud_polled(self):
        return self._cc_protocol.last_earbud_polled

    @property
    def channel_outstanding_responses(self, chan):
        return str(chan.left_outstanding_response_count) + "/" + str(chan.right_outstanding_response_count)

    @property
    def supported_channels(self):
        ''' Return a list of channels enums supported in the device. '''
        chans = []
        for i in range(0,self.env.econst.CASECOMMS_CID_MAX):
            chans.append(self.env.enums["cc_cid_t"][i])
        return chans

    @property
    def channels(self):
        return self._cc_protocol.channel_cfg

    def _generate_report_body_elements(self):
        with self._cc_protocol.footprint_prefetched():
            grp = interface.Group("Case Comms Protocol")

            keys_in_summary = ['State', 'Value']
            tbl = interface.Table(keys_in_summary)
            tbl.add_row(['Mode', self.mode])
            tbl.add_row(['Transport', self.transport])
            tbl.add_row(['Supported channels', self.supported_channels])
            grp.append(tbl)

            # case specific output
            if self.mode.value == self.env.econst.CASECOMMS_MODE_CASE:
                tbl.add_row(['Last Polled Earbud', self.last_earbud_polled])
                keys_in_summary = ['Channel', 'Left EB Responses', 'Right EB Responses']
                channel_tbl = interface.Table(keys_in_summary)
                for chan in self.channels:
                    channel_tbl.add_row([chan.cid, chan.left_outstanding_response_count, chan.right_outstanding_response_count])
                grp.append(channel_tbl)

        return [grp]

class Casecomms(FirmwareComponent):
    ''' Class providing information about case communication stack.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        self._is_case = True

        try:
            self._case = CcWithCase(env, core, parent=self)
            self._is_case = False
        except self.NotDetected:
            try:
                self._earbuds = CcWithEarbuds(env, core, parent=self)
            except self.NotDetected:
                raise self.NotDetected
        
        try:
            self._cc_protocol = CcProtocol(env, core, parent=self)
        except KeyError:
            raise self.NotDetected

    @property
    def cc_with_case(self):
        return self._case

    @property
    def cc_protocol(self):
        return self._cc_protocol

    @property
    def cc_with_earbuds(self):
        return self._earbuds

    @property
    def subcomponents(self):
        subs = {"cc_protocol" : "cc_protocol"}
        if self._is_case:
            subs["cc_with_earbuds"] = "cc_with_earbuds"
        else:
            subs["cc_with_case"] = "cc_with_case"
        return subs

