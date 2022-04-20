############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import InvalidDereference
from csr.dev.model import interface

class Usb(FirmwareComponent):
    ''' This class reports the usb state '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            self._usb_data = env.vars["usbaudio_globaldata"]

        except KeyError:
            raise self.NotDetected
   
    @property
    def headphone_sample_rate(self):
        ''' Returns the headphone sample rate '''
        try:
            return self._usb_data.deref.headphone.deref.spkr_sample_rate.get_value()

        except InvalidDereference:
            return None

    @property
    def mic_sample_rate(self):
        ''' Returns the microphone sample rate '''
        try:
            return self._usb_data.deref.headset.deref.mic_sample_rate.get_value()

        except InvalidDereference:
            return None

    @property
    def mic_enabled(self):
        ''' Returns whether the usb mic is enabled '''
        try:
            return self._usb_data.deref.headset.deref.mic_enabled.get_value()

        except InvalidDereference:
            return None

    @property
    def mic_active(self):
        ''' returns whether the usb mic is active '''
        try:
            return self._usb_data.deref.headset.deref.mic_active.get_value()

        except InvalidDereference:
            return None

    @property
    def target_lantency(self):
        ''' Returns the configured usb target latency '''
        try:
            return self._usb_data.deref.config.deref.target_latency_ms.get_value()

        except InvalidDereference:
            return None

    @property
    def min_lantency(self):
        ''' Returns the configured usb min latency '''
        try:
            return self._usb_data.deref.config.deref.min_latency_ms.get_value()

        except InvalidDereference:
            return None

    @property
    def max_lantency(self):
        ''' Returns the configured usb max latency '''
        try:
            return self._usb_data.deref.config.deref.max_latency_ms.get_value()

        except InvalidDereference:
            return None
            
    def _generate_report_body_elements(self):
        ''' Generates a summary report for usb. '''
        
        grp = interface.Group("Summary")

        keys_in_summary = ['Variable', 'Value']
        tbl = interface.Table(keys_in_summary)

        tbl.add_row(['Headphone Sample Rate', str(self.headphone_sample_rate)])
        tbl.add_row(['Microphone Sample Rate', str(self.mic_sample_rate)])
        tbl.add_row(['Microphone Enabled', "TRUE" if self.mic_enabled else "FALSE"])
        tbl.add_row(['Microphone Active', "TRUE" if self.mic_active else "FALSE"])
        tbl.add_row(['Target Latency', str(self.target_lantency)])
        tbl.add_row(['Min Latency', str(self.target_lantency)])
        tbl.add_row(['Max Latency', str(self.max_lantency)])

        grp.append(tbl)
        return [grp]