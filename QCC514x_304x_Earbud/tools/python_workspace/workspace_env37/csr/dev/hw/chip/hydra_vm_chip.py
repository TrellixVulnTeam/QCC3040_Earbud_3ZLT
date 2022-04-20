############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import gstrm, autolazy
from csr.dev.hw.chip.hydra_chip import HydraChip
from csr.dev.hw.subsystem.hydra_subsystem import SimpleHydraSubsystem
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.nvm.hydra_apps_sqif import HydraAppsSQIF
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from ..pins import Pins
import sys
from collections import OrderedDict

class HydraVMChip(HydraChip):
    """
    Generic Hydra Voice and Music chip
    """
    num_subsystems = 6
    
    class _SSID(HydraChip._SSID):
        """\
        Subsystem IDs
        """
        @property
        def BT(self):
            return 2 + self._offset

        @property
        def AUDIO(self):
            return 3 + self._offset

        @property
        def APPS(self):
            return 4 + self._offset
        
        @property        
        def DEBUGGER(self):
            """
            Trb Debugger Id, usually equals to SYSTEM_BUS_NUM_SUBSYSTEMS.
            """
            return self._num_subsystems + self._offset
    
    # HydraChip compliance

    @property
    def subsystems(self):

        # There must be a better way to union two dictionaries
        # to creat a new one.
        #
        sss = HydraChip.subsystems.fget(self).copy()
        sss.update({
            self.SSID.BT: self.bt_subsystem,
            self.SSID.AUDIO: self.audio_subsystem,
            self.SSID.APPS: self.apps_subsystem,
        })
        return sss
        
    @property
    def subcomponents(self):
        cmps = HydraChip.subcomponents.fget(self)
        cmps.update({"bt_subsystem" : "_bt_subsystem",
                     "audio_subsystem" : "_audio_subsystem",
                     "apps_subsystem" : "_apps_subsystem",
                     "pins" : "_pins"})
        return cmps
    # Extensions    
      
    @property
    def hw_block_ids(self):
        return {"UART" : 0,
                "USB2" : 1,
                "SDIO" : 2,
                "BITSERIAL0" : 3,
                "BITSERIAL1" : 4,
                "HOST_SYS" : 0xd}
    
    @property
    def bt_subsystem(self):
        """\
        BT Subsystem Proxy
        """
        # COD...
        try:
            self._bt_subsystem
        except AttributeError:
            self._bt_subsystem = self._create_bt_subsystem()
        return self._bt_subsystem

    @property
    def audio_subsystem(self):
        """\
        AUDIO Subsystem Proxy
        """
        # COD...
        try:
            self._audio_subsystem
        except AttributeError:
            self._audio_subsystem = self._create_audio_subsystem()
        return self._audio_subsystem

    @property
    def apps_subsystem(self):
        """\
        AUDIO Subsystem Proxy
        """
        # COD...
        try:
            self._apps_subsystem
        except AttributeError:
            self._apps_subsystem = self._create_apps_subsystem()
        return self._apps_subsystem

    @property
    def pins(self):
        try:
            self._pins
        except AttributeError:
            self._pins = Pins(self.curator_subsystem.core)
        return self._pins

    # Overrideable
    
    # Required
    
    def _create_bt_subsystem(self):
        """\
        Create BTSubsystem Proxy.
        
        Derived classes must override to create appropriate variant.
        
        Called on first request for the Proxy.
        """
        return SimpleHydraSubsystem(self, self.SSID.BT, self._access_cache_type)

    def _create_audio_subsystem(self):
        """\
        Create AudioSubsystem Proxy.
        
        Derived classes must override to create appropriate variant.
        
        Called on first request for the Proxy.
        """
        return SimpleHydraSubsystem(self, self.SSID.AUDIO, self._access_cache_type)

    @property
    @autolazy
    def sqif(self):
        return HydraAppsSQIF(self.apps_subsystem)
    
    def sqif_info(self, report=False):
        '''
        This is a meta-method which pulls all the SQIF/QSPI state for all subsystems
        together into a single report
        '''
        subsystem_sqifs = OrderedDict((("Curator", self.curator_subsystem.sqif),
                                      ("Apps", self.apps_subsystem.sqifs[0]),
                                      ("BT", self.bt_subsystem.sqif),
                                      ("Audio", self.audio_subsystem.sqif)))

        # Do we run from SQIF or ROM
        try:
            #Force a read to see if the Apps is powered
            self.apps_subsystem.core.data[0]
            sqif_or_rom = {"Apps": "QSPI"}
        except AddressSpace.ReadFailure:
            sqif_or_rom = {"Apps": "OFF"}

        cur = self.curator_subsystem.core
        bt = self.bt_subsystem.core
        audio = self.audio_subsystem.core
        try:
            if self.cur_running_from_rom(cur):
                sqif_or_rom["Curator"] = "ROM"
            else:
                sqif_or_rom["Curator"] = "QSPI"
        except AddressSpace.ReadFailure:
                sqif_or_rom["Curator"] = "OFF"
        try:
            if self.bt_running_from_rom(bt):
                sqif_or_rom["BT"] = "ROM"
            else:
                sqif_or_rom["BT"] = "QSPI"
        except AddressSpace.ReadFailure:
                sqif_or_rom["BT"] = "OFF"
        try:
            if self.audio_running_from_rom(audio):
                sqif_or_rom["Audio"] = "QSPI"
            else:
                sqif_or_rom["Audio"] = "ROM"
        except AddressSpace.ReadFailure:
            sqif_or_rom["Audio"] = "OFF"

        # Create the table
        output_table = interface.Table(["Subsystem", "QSPI/ROM", "SQIF_EN", 
                                        "DDR_EN", "CLK DIV",
                                        "CLK Rate MHz", "Continuous", "Width"])

        for ss, sqif in subsystem_sqifs.items():
            if sqif_or_rom[ss] == "QSPI":
                clk_rate = self.get_sqif_if_clk_rate(cur, ss)
                output_table.add_row([ss, sqif_or_rom[ss],
                                      sqif.sqif_enabled,
                                      sqif.ddr_enabled,
                                      sqif.clk_divider,
                                      "-" if sqif.clk_divider == 0 else clk_rate / sqif.clk_divider,
                                      sqif.in_continuous_mode,
                                      sqif.sqif_width
                                      ])
            else:
                output_table.add_row([ss, sqif_or_rom[ss],
                                      "-",
                                      "-",
                                      "-",
                                      "-",
                                      "-",
                                      "-"
                                      ])
        if report is True:
            return output_table
        TextAdaptor(output_table, gstrm.iout)

    def get_sqif_if_clk_rate(self, cur, ss):
        """
        Get the rate, in MHz, of QSPI clock source for 'ss'
        :param cur: curator core object
        :param ss: Name of subsystem of the interface to get the clock rate for
            Acceptable values:
                "Curator", "Apps", "BT", "Audio"
        :return: Integer - SQIF interface clock rate for 'ss'
        """
        # This is chip specific and needs to be overridden.
        raise PureVirtualError

    def cur_running_from_rom(self, cur):
        """
        Determine whether Curator subsystem is running from ROM
        :param cur: curator core object
        :return: Bool - True: ROM, False: SQIF
        """
        return cur.fields.NV_MEM_ADDR_MAP_CFG_STATUS == 0x0004

    def bt_running_from_rom(self, bt):
        """
        Determine whether Bluetooth subsystem is running from ROM
        :param bt: bt core object
        :return: Bool - True: ROM, False: SQIF
        """
        return bt.fields.NV_MEM_ADDR_MAP_CFG_STATUS == 0x0004

    def audio_running_from_rom(self, audio):
        """
        Determine whether Audio subsystem is running from ROM
        :param audio: audio core object
        :return: Bool - True: ROM, False: SQIF
        """
        return audio.fields.NVMEM_WIN0_CONFIG == 0x0001

    def _generate_report_body_elements(self):
        sqif_group = interface.Group("SQIF interface configuration")
        sqif_group.append(self.sqif_info(report = True))
        return [sqif_group]
