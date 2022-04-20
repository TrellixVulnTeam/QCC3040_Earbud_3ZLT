#
# Copyright (c) 2021 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Base class for the firmware objects."""


# pylint: disable=too-few-public-methods
class Firmware:
    """Base class for the firmware objects.

    Args:
        connection (object): A connection object that contains commands
            to communicate with the chip.
    """
    def __init__(self, connection):
        self._connection = connection
