############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.hw.chip.hydra_vm_chip import HydraVMChip
from csr.dev.hw.chip.mixins.has_reset_transaction import HasResetTransaction
from csr.dev.hw.address_space import AddressSpace

class QCC514X_QCC304XChip (HydraVMChip, HasResetTransaction):
    
    DAP_PROPERTIES = {"speed_khz" : 4000,
                      "interface" : "swd"}
    
    # Chip compliance

    @property
    def name(self):
        
        return "QCC514X_QCC304X"


    def setup_sqif_htol(self):
        '''
        Puts the SQIF chip in quad mode. Normally the curator ROM code should
        do this but the register pokes are different for the HTOL board SQIF.
        This is meant to be called manually before patching and loading the
        Apps code.
        '''
        apps = self.apps_subsystem.p0
        cur = self.curator_subsystem.core

        cur.pause()
        
        cur.fields["CHIP_PIO9_PIO11_MUX_CONTROL"] = 0xbbb
        cur.fields["CHIP_PIO12_PIO15_MUX_CONTROL"] = 0xbbb

        # Reset the SQIF chip
        apps.fields["SQIF_POKE_LAST"] = 0xf0
        
        # Wait for it to reconfigure
        time.sleep(0.100)
        
        # Enable quad mode see datasheet for S25FL128S
        
        # Set write enable
        apps.fields["SQIF_POKE_LAST"] = 0x6
        
        # Write the config register using the WRR 01h command
        apps.fields["SQIF_POKE"] = 0x1 # Command
        apps.fields["SQIF_POKE"] = 0x0 # Status register
        apps.fields["SQIF_POKE_LAST"] = 0x2 # Config register
        
        cur.run()

    @property
    def raw_version(self):
        try:
            return HydraVMChip.raw_version.fget(self)
        except AddressSpace.NoAccess:
            # Curator isn't connected to the transport.  Just return default
            return 0xffff
            
    def bt_running_from_rom(self, bt):
        """
        Determine whether Bluetooth subsystem is running from ROM
        :param bt: bt core object
        :return: Bool - True: ROM, False: SQIF
        """
        return bt.fields.CLKGEN_REMAP_BOOT_FROM_RAM.BOOT_OPTION_ADDR_1800_0000 == 0

    def get_sqif_if_clk_rate(self, cur, ss):
        """
        Get the rate, in MHz, of QSPI clock source for 'ss'
        :param cur: curator core object
        :param ss: Name of subsystem of the interface to get the clock rate for
            Acceptable values:
                "Curator", "Apps", "BT", "Audio"
        :return: Integer - SQIF interface clock rate for 'ss'
        """

        if ss == "Audio" or ss == "Curator":
            sqif_if_clk_src_reg = cur.fields.CURATOR_SQIF_INTERFACE_CLK_SOURCES.SQIF_INTERFACE0_CLK_SOURCE
        elif ss == "BT" or ss == "Apps":
            sqif_if_clk_src_reg = cur.fields.CURATOR_SQIF_INTERFACE_CLK_SOURCES.SQIF_INTERFACE1_CLK_SOURCE
        else:
            raise ValueError("Unknown subsystem '%s'" % ss)

        if sqif_if_clk_src_reg.read() == 0:
            return 32
        elif sqif_if_clk_src_reg.read() == 1:
            return 80
