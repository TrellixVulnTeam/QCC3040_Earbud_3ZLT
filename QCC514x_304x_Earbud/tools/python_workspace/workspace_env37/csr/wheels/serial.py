############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

"""
Wrapper to fallback to local copy if Pyserial library is not installed.
"""

try:
    import serial
    import serial.win32
except ImportError:
    import sys, os
    sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                    '..', '..', 'ext', 'bsd', 'serial'))

    import win32
