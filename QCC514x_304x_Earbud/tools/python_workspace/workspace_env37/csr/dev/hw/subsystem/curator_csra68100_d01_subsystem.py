# Copyright (c) 2016-2019 Qualcomm Technologies International, Ltd.
#   %%version
"""
Provides hardware specific information for curator subsystem on qcc512x_qcc302x D01 chip:
CuratorCSRA68100D01Subsystem.
"""
from .curator_csra68100_subsystem import CuratorCSRA68100Subsystem
from .mixins.is_csra68100 import IsCSRA68100

class CuratorCSRA68100D01Subsystem(CuratorCSRA68100Subsystem, IsCSRA68100):
    #pylint: disable=too-many-ancestors
    """\
    CSRA68100 Dev Curator Subsystem Proxy
    """

    # CuratorSubsystem Compliance

    def _create_curator_core(self, access_cache_type):

        from csr.dev.hw.core.curator_csra68100_d01_core \
                                                import CuratorCSRA68100D01Core
        core = CuratorCSRA68100D01Core(self)
        core.populate(access_cache_type)
        return core

    def sqif_clk_enable(self):
        """
        Enable the SQIF clock
        """
        cur = self.chip.curator_subsystem.core
        cur.fields.CURATOR_SUBSYSTEMS_SQIF_CLK_ENABLE.\
            CURATOR_SUBSYSTEMS_SQIF_CLK_ENABLE_CURATOR = 1
