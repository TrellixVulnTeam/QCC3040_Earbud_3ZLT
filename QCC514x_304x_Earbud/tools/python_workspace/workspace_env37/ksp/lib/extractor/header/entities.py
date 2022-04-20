#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Represents the header of a packet received from a KSP capability."""
from ksp.lib.data_types import KspDataType


# This class is to encapsulate and present the raw header. It's expected
# not to have many public methods.
# pylint: disable=too-few-public-methods
class Header():
    """Decode the header value and present it as object.

    Args:
        header (int): Header of a packet in an lrw file and/or received
            from a KSP capability.
    """

    def __init__(self, header):
        self.sync = header & 0xFF
        self.seq = (header >> 8) & 0xFF
        self.channel_info = (header >> 16) & 0x7
        self.samples = (((header >> 19) & 0x1F) + 1) * 8
        self.n_chans = ((header >> 24) & 0x7) + 1
        self.data_type = (header >> 27) & 0x7
        self.stream_id = (header >> 30) & 0x3

        self._check_data_type()

    def _check_data_type(self):
        try:
            self.data_type = KspDataType(self.data_type)

        except ValueError:
            raise ValueError("Unexpected data type: %s" % self.data_type)
