############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides one mixin class IsBluetooth.
"""
# TODO this assumes BT is based on a XAP.
# This class appears to be a fossil.
# pylint says the IsXAP__init__ call is wrong.


from .is_xap import IsXAP

class IsBluetooth(IsXAP):
    """\
    Bluetooth Core Mixin

    Implementations and extensions common to all known Bluetooth Cores.

    N.B. Including those NOT in hydra.
    """
    def __init__(self, access_cache_type):

        IsXAP.__init__(self, access_cache_type)

    # Extensions
