#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSP module version definition."""
import os


CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
with open(os.path.join(CURRENT_DIRECTORY, 'VERSION')) as handler:
    __version__ = handler.read().strip()

version_info = [  # pylint: disable=invalid-name
    __version__.split('.')[ind]
    if len(__version__.split('.')) > ind else 0 if ind != 3 else ''
    for ind in range(5)
]
