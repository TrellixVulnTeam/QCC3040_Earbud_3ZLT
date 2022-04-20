############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides base class CuratorCSRA68100Subsystem from which hardware version
specific class should be derived.
"""
from contextlib import contextmanager
from csr.wheels.global_streams import iprint
from csr.dev.hw.sqif import SqifInterface
from csr.dev.hw.address_space import AddressSpace
from .curator_subsystem import CuratorSubsystem

class CuratorCSRA68100Subsystem(CuratorSubsystem):
    """
    CSRA68100-specific Curator functionality
    """
    @property
    def sqif(self):
        '''
        The represents the SQIF hardware block on the Curator subsystem,
        which is used as a ROM replacement during development.
        '''
        # Construct lazily...
        try:
            self._sqif
        except AttributeError:
            self._sqif = SqifInterface(self.core)

        return self._sqif

    @property
    def siflash(self):
        '''
        The represents the Serial flash support for ALL subsystems
        implemented in the Curator firmware using the Hydra Protocols.

        Since this is a chip wide function it is implemented now in the
        CSRA68100Chip object but this 'softlink' from the Curator Object
        is retained for backwards compatibility.
        '''
        return self.chip.siflash

    def bulk_erase(self, bank=0):
        """
        Most basic way to completely erase the Curator SQIF.

        ONlY Uses register peeks and pokes so does need to have have had
        firmware specified.

        SHOULD be able to erase a SQIF regardless of the system state.
        """

        iprint("About to erase the Curator SQIF")
        self.core.halt_chip()
        self.sqif_clk_enable()
        self.sqif.minimal_config()
        self.config_sqif_pios()  #Have the SQIF HW configured before this.
        self.sqif.bulk_erase_helper()

    def config_sqif_pios(self):
        """
        The Janitor should have configured before the Curator boots.
        During testing it may be necessary to forcible take these PIOs
        """

        #Get the Curator object to access it's registers
        cur = self.chip.curator_subsystem.core

        #Configure the SQIF PIOs
        cur.fields['CHIP_PIO44_PIO47_MUX_CONTROL'] = 0xb000 #CLK
        cur.fields['CHIP_PIO48_PIO51_MUX_CONTROL'] = 0x0bbb
        cur.fields['CHIP_PIO52_PIO55_MUX_CONTROL'] = 0x000b
        cur.fields['CHIP_PIO56_PIO59_MUX_CONTROL'] = 0x0b00 #CS

    def sqif_clk_enable(self):
        """
        Enable the SQIF clock
        """
        pass

    def reset_reset_protection(self):
        """
        Reset the reset protection timer
        """
        import time
        # First, clear the reset timer in case it has just been triggered.
        # This is done by setting PMU_DA_RST_PROT_ARM_B high then low again.
        self.core.fields.PMU_CTRL5 |= 0x10
        time.sleep(0.001)
        self.core.fields.PMU_CTRL5 &= ~0x10
        time.sleep(0.001)

    def enable_reset_protection(self):
        """
        Enable the reset protection circuit in the PMU
        """
        #
        # We're going to enable the pmu reset protection so that the chip will
        # keep the ka system (ie janitor power) on for about 2 seconds (1 second
        # min), when we reset the chip. Otherwise the reset will turn the chip
        # off. We could wiggle the sys ctrl line to turn the chip back on, but
        # that requires the sys ctrl to work. Our chosen method only needs the
        # reset line to work (and the TRB interface).
        #
        try:
            self.reset_reset_protection()
            self.core.fields.PMU_CTRL5 |= 0x20
        except (AddressSpace.ReadFailure, AddressSpace.WriteFailure):
            # The curator may not be responsive. Don't prevent resetting
            # in that case.
            pass

    def get_temperature(self):
        """
        Get the curator recorded last measured temperature in Celsius degree.
        """
        return self.core.fw.gbl.last_temp_meas_data.temp_deg_c.get_value()

    @contextmanager
    def reset_protection(self):
        """
        Context manager for reset protection circuit when
        resetting the chip.
        """
        self.enable_reset_protection()
        try:
            yield
        finally:
            self.reset_reset_protection()
