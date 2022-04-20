#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Utility library for period computation"""


def compute_period(period, remainder, resolution=1e-6):
    """Helper function to compute next timer period based of the nominal period, a remainder
    value and the resolution of the timer

    Example:
        Overflow period example::

            period = 0.00000166667
            new_period, remain = self._compute_period(period, 0)
            print(new_period, remain)
            new_period, remain = self._compute_period(period, remain)
            print(new_period, remain)

    Args:
        period (float): Timer nominal period
        remainder (float): Carried remainder from previous timer
        resolution (float): Timer resolution f.i. 0.001 for msecs, 0.000001 for usecs

    Returns:
        tuple:
            float: Timer period
            float: Carried remainder
    """
    inv_res = 1.0 / resolution
    period_new = int((period + remainder) * inv_res) / inv_res
    remainder_new = (int(int((period + remainder) * 1e9) % int(1e9 / inv_res))) / 1e9
    return period_new, remainder_new
