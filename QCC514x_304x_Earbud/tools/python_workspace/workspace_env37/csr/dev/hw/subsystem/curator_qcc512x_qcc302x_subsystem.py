############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides base class CuratorCSRA68100Subsystem from which hardware version
specific class should be derived.
"""
from collections import OrderedDict
from contextlib import contextmanager
from csr.wheels.global_streams import iprint
from csr.dev.hw.sqif import SqifInterface
from csr.dev.hw.address_space import AddressSpace
from .curator_subsystem import CuratorSubsystem


class CuratorQCC512X_QCC302XSubsystem(CuratorSubsystem):
    """
    QCC512X_QCC302X-specific Curator functionality
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
        cur.fields['CHIP_PIO44_PIO47_MUX_CONTROL'] = 0xee00
        cur.fields['CHIP_PIO48_PIO51_MUX_CONTROL'] = 0xeeee

        # Set the pulls etc. Settings same as janitor ROM
        cur.fields['PAD_CONTROL_SQIF_4_D0'] = 0x19
        cur.fields['PAD_CONTROL_SQIF_4_D1'] = 0x19
        cur.fields['PAD_CONTROL_SQIF_4_D2'] = 0x19
        cur.fields['PAD_CONTROL_SQIF_4_D3'] = 0x19
        cur.fields['PAD_CONTROL_SQIF_4_CLK'] = 0x19
        cur.fields['PAD_CONTROL_SQIF_4_CS0'] = 0x39

    def sqif_clk_enable(self):
        """
        Enable the SQIF clock
        """

        #Get the Curator object to access it's registers
        cur = self.chip.curator_subsystem.core

        # Set both clock sources to be FOSC. Janitor ROM will have done this
        # for curator, but we dont know what state any curator code run
        # subsequently will have left them in
        cur.fields.CURATOR_SQIF_INTERFACE_CLK_SOURCES = 0

    def _get_clock_info(self):
        #pylint: disable=too-many-locals
        cur = self.chip.curator_subsystem.core
        gen_enb_dis = ["Off", "On"]
        spll_enb_dis = ["SPLL: On", "SPLL: Off"]
        mpll_enb_dis = ["MPLL: On", "MPLL: Off"]
        spll_clocks = [
            "SPLL clock o/p: 48MHz:Off, 80MHz:Off, 120MHz:Off",
            "SPLL clock o/p: 48MHz:On, 80MHz:Off, 120MHz:Off",
            "SPLL clock o/p: 48MHz:Off, 80MHz:On, 120MHz:Off",
            "SPLL clock o/p: 48MHz:On, 80MHz:On, 120MHz:Off",
            "SPLL clock o/p: 48MHz:Off, 80MHz:Off, 120MHz:On ",
            "SPLL clock o/p: 48MHz:On, 80MHz:Off, 120MHz:On",
            "SPLL clock o/p: 48MHz:Off, 80MHz:On, 120MHz:On ",
            "SPLL clock o/p: 48MHz:On, 80MHz:On, 120MHz:On"]
        lock_state = ["Unlocked", "Locked"]
        sqif_if_clk_src = ["FOSC", "XTAL", "PLL", "Undefined"]
        bt_clk_src = ["XTAL", "PLL", "NCO", "Undefined"]
        aud_eng_clk_src = ["AOV", "XTAL"]
        aud_cpu_clk_src = ["AOV", "XTAL", "PLL", "PLL_TURBO"]
        apps_clk_src = ["FOSC", "XTAL", "PLL", "Undefined"]
        host_clk_src = ["FOSC", "XTAL"]
        cur_clk_src = ["FOSC", "XTAL"]
        led_clk_src = ["SOSC", "Undefined", "FOSC", "XTAL"]
        xtal_sel_div = ["XTAL", "XTAL/2", "XTAL/4", "XTAL/8", "XTAL/16",
                        "XTAL/32", "Disabled", "Disabled"]
        state_descriptions = OrderedDict((
            ("gen_enb_dis", gen_enb_dis),
            ("spll_enb_dis", spll_enb_dis),
            ("mpll_enb_dis", mpll_enb_dis),
            ("spll_clocks", spll_clocks),
            ("lock_state", lock_state),
            ("sqif_if_clk_src", sqif_if_clk_src),
            ("bt_clk_src", bt_clk_src),
            ("aud_eng_clk_src", aud_eng_clk_src),
            ("aud_cpu_clk_src", aud_cpu_clk_src),
            ("apps_clk_src", apps_clk_src),
            ("host_clk_src", host_clk_src),
            ("cur_clk_src", cur_clk_src),
            ("led_clk_src", led_clk_src),
            ("xtal_sel_div", xtal_sel_div)
            ))

        # This has format "Clock Reg/Description",
        #                  ("Register", Offset, Mask, State description index)
        clocks = OrderedDict((
            ("CURATOR_SUBSYS_CORE_CLK_ENABLES_HOST",
             ("CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES", 1, 1, "gen_enb_dis")),
            ("CURATOR_SUBSYS_CORE_CLK_ENABLES_BT",
             ("CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES", 2, 1, "gen_enb_dis")),
            ("CURATOR_SUBSYS_CORE_CLK_ENABLES_AUDIO",
             ("CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES", 3, 1, "gen_enb_dis")),
            ("CURATOR_SUBSYS_CORE_CLK_ENABLES_APPS",
             ("CURATOR_SUBSYSTEMS_CORE_CLK_ENABLES", 4, 1, "gen_enb_dis")),
            ("(FORCE_)CLK_TBUS",
             ("CURATOR_SUBSYSTEMS_ANC_CLK_ENABLES", 0, 1, "gen_enb_dis")),
            ("CLK_BT_LO_TO_SS",
             ("CURATOR_SUBSYSTEMS_ANC_CLK_ENABLES", 1, 1, "gen_enb_dis")),
            ("CLK_AUDIO_ENGINE",
             ("CURATOR_SUBSYSTEMS_ANC_CLK_ENABLES", 2, 1, "gen_enb_dis")),
            ("CLK_CURATOR_SMPS",
             ("CURATOR_SUBSYSTEMS_ANC_CLK_ENABLES", 3, 1, "gen_enb_dis")),
            ("CLK_LED_CTRL",
             ("CURATOR_SUBSYSTEMS_ANC_CLK_ENABLES", 4, 1, "gen_enb_dis")),
            ("SQIF_CLK_CURATOR",
             ("CURATOR_SQIF_INTERFACE_CLK_ENABLES", 0, 1, "gen_enb_dis")),
            ("SQIF_CLK_BT",
             ("CURATOR_SQIF_INTERFACE_CLK_ENABLES", 1, 1, "gen_enb_dis")),
            ("SQIF_CLK_AUDIO",
             ("CURATOR_SQIF_INTERFACE_CLK_ENABLES", 2, 1, "gen_enb_dis")),
            ("SQIF_CLK_APPS0",
             ("CURATOR_SQIF_INTERFACE_CLK_ENABLES", 3, 1, "gen_enb_dis")),
            ("SQIF_CLK_APPS1",
             ("CURATOR_SQIF_INTERFACE_CLK_ENABLES", 4, 1, "gen_enb_dis")),
            ("AUX_ANA_XTAL_SEL_CLK (Bit 0 - Audio Ana. Clk)",
             ("AUX_ANA_CTRL1", 0, 1, "gen_enb_dis")),
            ("AUX_ANA_XTAL_SEL_CLK (Bit 1 - BT Ana. Clk)",
             ("AUX_ANA_CTRL1", 1, 1, "gen_enb_dis")),
            ("AUX_ANA_XTAL_DIG_CTRL_CURATOR_CLK_DIV_GATE_EN",
             ("AUX_ANA_XTAL_DIG_CTRL", 5, 1, "gen_enb_dis")),
            ("AUX_ANA_XTAL_SEL_DIV",
             ("AUX_ANA_CTRL8", 8, 7, "xtal_sel_div")),
            ("AUX_ANA_XTAL_DIG_CTRL_CURATOR_CLK_GATE_EN",
             ("AUX_ANA_XTAL_DIG_CTRL", 4, 1, "gen_enb_dis")),
            ("AUX_DI_XTL_SPARE (Bit 0 - CLK_DIG from XTAL)",
             ("AUX_ANA_CTRL33", 0, 1, "gen_enb_dis")),
            ("SQIF_INTERFACE0_CLK_SOURCE",
             ("CURATOR_SQIF_INTERFACE_CLK_SOURCES", 0, 3, "sqif_if_clk_src")),
            ("SQIF_INTERFACE1_CLK_SOURCE",
             ("CURATOR_SQIF_INTERFACE_CLK_SOURCES", 2, 3, "sqif_if_clk_src")),
            ("MILDRED_CHIP_CLKGEN_CTRL_CURATOR_CLK_SOURCE",
             ("MILDRED_CHIP_CLKGEN_CTRL", 1, 1, "cur_clk_src")),
            ("CURATOR_BT_SS_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 0, 3, "bt_clk_src")),
            ("CURATOR_AUDIO_ENGINE_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 2, 1, "aud_eng_clk_src")),
            ("CURATOR_AUDIO_CPU_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 3, 3, "aud_cpu_clk_src")),
            ("CURATOR_APPS_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 5, 3, "apps_clk_src")),
            ("CURATOR_HOST_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 7, 1, "host_clk_src")),
            ("CURATOR_LED_CTRL_CLK_SOURCE",
             ("CURATOR_SUBSYSTEMS_CLK_SOURCES", 8, 3, "led_clk_src")),
            ("AUX_ANA_SPLL_PD",
             ("AUX_ANA_CTRL10", 15, 1, "spll_enb_dis")),
            ("AUX_DO_SPLL_LOCK",
             ("AUX_ANA_STATUS1", 0, 1, "lock_state")),
            ("AUX_ANA_SYS_DIV_EN",
             ("AUX_ANA_CTRL19", 0, 7, "spll_clocks")),
            ("AUX_ANA_MPLL_PD",
             ("AUX_ANA_CTRL20", 15, 1, "mpll_enb_dis")),
            ("AUX_DO_MPLL_LOCK",
             ("AUX_ANA_STATUS2", 0, 1, "lock_state")),
            ("NCO_PLL_EN",
             ("NCO_PLL_EN", 0, 1, "gen_enb_dis")),
            ("NCO_PLL_STATUS_PLL_LOCKED",
             ("NCO_PLL_STATUS", 3, 1, "lock_state"))))

        # Create a report dictionary
        clock_info = OrderedDict()

        for index, clk_info in clocks.items():
            try:
                clk_state = cur.fields[clk_info[0]]
                raw_value = (int(clk_state) >> clk_info[1]) & clk_info[2]
                descr = state_descriptions.get(clk_info[3])
                clock_value = descr[raw_value]
                clock_info[index] = clock_value
            except KeyError:
                pass
        return clock_info

    def reset_reset_protection(self):
        """
        Reset the reset protection timer
        """
        import time
        # First, clear the reset timer in case it has just been triggered. This
        # is done by setting PMU_DA_RST_PROT_ARM_B high then low again.

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


