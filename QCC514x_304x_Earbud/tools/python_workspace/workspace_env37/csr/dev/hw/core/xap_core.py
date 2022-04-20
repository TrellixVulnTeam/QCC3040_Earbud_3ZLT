############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class XAPCore to represent XAP processor core.
"""
import time
from .base_core import BaseCore
from .execution_state import XapExecState

class XAPCore(BaseCore):
    """\
    XAP Core Proxy (Base - or maybe a mixin)

    Implementations and extensions common to all XAP cores.
    """

    def cpu_shallow_sleep_time(self, sample_ms=1000):
        """
        Returns the % of time the CPU spends in shallow sleep
        """
        asleep = 0
        awake = 0
        initial_time = time.time()
        final_time = initial_time + (sample_ms/1000.0)
        while time.time() < final_time:
            if (self.fields.CLKGEN_MINMAX_MMU_RATE.CLKGEN_MIN_MMU_RATE.read()
                    != 0):
                asleep += 1
            else:
                awake += 1
        total_samples = asleep + awake
        if total_samples < 10:
            self.logger.warning(
                'total samples is less than 10: increase sample_ms.')
        return (asleep * 100.0) / total_samples

    @property
    def bad_read_reg_name(self):
        return "MMU_REG_ACCESS_TIMEOUT_VALUE"

    @property
    def execution_state(self):
        """
        Returns an object representing the execution state of the core.
        """
        try:
            return self._execution_state
        except AttributeError:
            self._execution_state = XapExecState(core=self)
        return self._execution_state

    def _all_subcomponents(self):
        sub_dict = BaseCore._all_subcomponents(self)
        sub_dict.update({"execution_state": "_execution_state"})
        return sub_dict
