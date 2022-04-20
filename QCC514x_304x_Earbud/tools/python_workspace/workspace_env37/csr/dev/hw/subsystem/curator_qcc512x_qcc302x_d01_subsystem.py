############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides hardware specific information for curator subsystem on qcc512x_qcc302x D01 chip:
CuratorQCC512X_QCC302XD01Subsystem.
"""
from .curator_qcc512x_qcc302x_subsystem import CuratorQCC512X_QCC302XSubsystem
from .mixins.is_qcc512x_qcc302x import IsQCC512X_QCC302X

class CuratorQCC512X_QCC302XD01Subsystem(CuratorQCC512X_QCC302XSubsystem, IsQCC512X_QCC302X):
    #pylint: disable=too-many-ancestors
    """\
    QCC512X_QCC302X D01 Curator Subsystem Proxy
    """
    # CuratorSubsystem Compliance

    def _create_curator_core(self, access_cache_type):

        from csr.dev.hw.core.curator_qcc512x_qcc302x_d01_core \
                                                import CuratorQCC512X_QCC302XD01Core
        core = CuratorQCC512X_QCC302XD01Core(self)
        core.populate(access_cache_type)

        return core

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
        cur.fields.MILDRED_CHIP_CLKGEN_CTRL.\
            MILDRED_CHIP_CLKGEN_CTRL_FOSC_CORE_EN = 1
