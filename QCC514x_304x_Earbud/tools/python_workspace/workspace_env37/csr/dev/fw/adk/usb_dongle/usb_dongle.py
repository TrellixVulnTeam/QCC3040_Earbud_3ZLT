############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.dev.env.env_helpers import InvalidDereference
from ..caa.structs import DeviceProfiles
from ..caa.connection_manager import ConnectionManager
from ..caa.device_list import DeviceList
from ..caa.a2dp import A2dp
from ..caa.usb import Usb


class UsbDongle(FirmwareComponent):
    ''' Class providing summary information about the USB Dongle application.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            self._app_sm = env.vars["usb_dongle_sm"]
            self._app_audio = env.vars["usb_dongle_audio_data"]
            self._cm = ConnectionManager(env, core, parent)
            self._devlist = DeviceList(env, core, parent)
            self._a2dp = A2dp(env, core, parent)
            self._usb = Usb(env, core, parent)
            self._input_enum = env.enum.usb_dongle_audio_input_t
            self._kymera_data = env.vars["app_kymera"]
            
        except KeyError:
            raise self.NotDetected

    @property
    def app_state(self):
        return self._app_sm.state

    @property
    def connected_audio_inputs(self):
        if self._app_audio.connected_inputs.get_value() == 0:
            return None
        input_list = []
        for data in self._input_enum:
            if (self._app_audio.connected_inputs.get_value() & self._input_enum[data]):
                input_list.append(data)
        return input_list

    @property
    def is_sink_connected(self):
        for tpaddr in self._cm.active_connections_tpaddrs:
            for dev in self._devlist.sinks:
                if dev.bdaddr == tpaddr.typed_bdaddr.bdaddr:
                    return True
        return False
        
    @property
    def current_codec(self):
        current_seid = 0
        for device in self._devlist.devices:
            if "av_instance" in device:
                current_seid = device["av_instance"].a2dp.current_seid.get_value()
        
        codec_dict = {0x01:"SBC", 0x02:"AptX Classic", 0x03:"AptX Adaptive"}
        
        return codec_dict.get(current_seid, None)
                        
    @property
    def a2dp_sample_rate(self):
        try:
            return self._kymera_data.a2dp_output_params.deref.rate.get_value()
        except InvalidDereference:
            return None
    
    def extended_report(self):
        self.report()
        self._usb.report()
        self._devlist.report()

    def _generate_report_body_elements(self):
        grp = interface.Group("Summary")
        keys_in_summary = ['Variable', 'Value']
        tbl = interface.Table(keys_in_summary)
        found_sink_connection = False

        with self._app_sm.footprint_prefetched():
            tbl.add_row(['App State', self.app_state])

        tbl.add_row(['Connected Audio Inputs', str(self.connected_audio_inputs)])
        tbl.add_row(['Sink Paired', "TRUE" if len(self._devlist.sinks) != 0 else "FALSE"])

       
        for active in self._cm._active_connections_generator():
            tbl.add_row(['Sink Connection', active[0].typed_bdaddr.bdaddr])
            
            tbl.add_row(['QHS Connected', "TRUE" if active[1].bitfields.qhs_supported.get_value() else "FALSE"])
            found_sink_connection = True
        if found_sink_connection:
            tbl.add_row(['Current Codec', str(self.current_codec)])
            tbl.add_row(['A2DP Sample Rate', str(self.a2dp_sample_rate)])
        else:
            tbl.add_row(["Sink Connected", "FALSE"])
        
        grp.append(tbl)

        return [grp]
