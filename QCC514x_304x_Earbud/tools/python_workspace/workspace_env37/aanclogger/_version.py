#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
'''
aanclogger version definition
'''

__version__ = '0.3.0'  # pylint: disable=invalid-name

version_info = [  # pylint: disable=invalid-name
    __version__.split('.')[ind]
    if len(__version__.split('.')) > ind else 0 if ind != 3 else ''
    for ind in range(5)
]
