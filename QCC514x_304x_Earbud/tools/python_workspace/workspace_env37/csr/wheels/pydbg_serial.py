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
    del serial  # Only interested in the existence of serial here.
except ImportError:
    import sys, os
    sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                    '..', '..', 'ext', 'bsd'))

# Serial should now be importable in either case.
from serial import win32
