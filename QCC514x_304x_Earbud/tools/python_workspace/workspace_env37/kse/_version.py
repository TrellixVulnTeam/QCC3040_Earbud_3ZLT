#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSE module version definition"""

import os

with open(os.path.abspath(os.path.join(os.path.dirname(__file__), 'VERSION'))) as handler:
    __version__ = handler.read().strip()

version_info = [__version__.split('.')[ind]  # pylint: disable=invalid-name
                if len(__version__.split('.')) > ind
                else 0 if ind != 3 else ''
                for ind in range(5)]
